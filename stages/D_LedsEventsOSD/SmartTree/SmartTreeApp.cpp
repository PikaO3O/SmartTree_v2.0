#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "StreamIO.h"
#include "VideoStream.h"
#include "UVCD.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "SmartTreeApp.h"
#include "SmartTreeConfig.h"
#include "PlantMonitor.h"
#include "DetectionSnapshot.h"
#include "CommandParser.h"
#include "ObjectClassList.h"
extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
}

void selectSmartTreeModel(NNObjectDetection& detector);
void configureSmartTreeCamera(Video& camera,VideoSetting& streamConfig,VideoSetting& nnConfig);
// Public SDK struct from module_uvcd.h; also used by UVCD.cpp in 4.0.9.
extern struct uvc_format* uvc_format_ptr;

namespace {
enum Phase { BOOT, CONFIGURING, WAIT_HOST, RUNNING, MEDIA_ERROR };
enum OverlayMode { OVERLAY_OFF, OVERLAY_BOXES, OVERLAY_FULL };
enum EventKind { EV_CONFIRM, EV_ALERT, EV_TIMEOUT, EV_CONNECT, EV_DISCONNECT, EV_GAP, EV_ERROR };
struct EventRecord { uint32_t at; EventKind kind; int score,count; };
struct MediaStatus {
    Phase phase;
    bool nnContextReady,osdReady,active,outputReady;
    int hostState,negotiatedWidth,negotiatedHeight,negotiatedFps,negotiatedFormat;
    uint32_t epoch;
    int error;
};
struct CallbackStats {
    uint32_t callbacks,lastAt,activeSamples,hits,drops,invalidBoxes;
    uint32_t scoreBins[10]; // only scores surviving the model confidence gate
    int lastScore;
};
struct RenderView {
    uint32_t epoch,sequence,confirms,alerts,sampleAt,hitAt;
    PlantMonitor::State state;
    BoxHold::Mode mode;
    OverlayMode overlay;
    unsigned count;
    int plants,score;
    DisplayBox boxes[MAX_DISPLAY_BOXES];
};

// MMFModule intentionally exposes only a protected context. A base COPY into this
// derived inspection object is legal C++; it neither casts objects nor owns/frees
// the context. This avoids 4.0.9 cameraGetCtx()'s malloc-then-overwrite leak.
class ModuleView : public MMFModule {
public:
    explicit ModuleView(const MMFModule& module) : MMFModule(module) {}
    mm_context_t* context() const { return _p_mmf_context; }
};

Video camera;
NNObjectDetection detector;
UVCD usb;
StreamIO displayLink(1,1),nnLink(1,1);
#if SMARTTREE_DISPLAY_PROFILE == 0
VideoSetting streamConfig(USB_UVCD_STREAM_PRESET);
#else
VideoSetting streamConfig(VIDEO_FHD,24,VIDEO_H264,0);
#endif
VideoSetting nnConfig(Config::NN_WIDTH,Config::NN_HEIGHT,Config::NN_SOURCE_FPS,VIDEO_RGB,0);
QueueHandle_t samples=NULL;
MediaStatus sharedMedia={};
CallbackStats sharedCallbacks={};
RenderView sharedView={};
uint32_t retryRequest=0; // main writes; media consumes; protected with other mailboxes

// All shared reads AND writes use brief task critical sections. No MMF, Serial,
// OSD, heap allocation, queue wait, or image operation is performed while locked.
MediaStatus readMedia() {
    taskENTER_CRITICAL(); MediaStatus s=sharedMedia; taskEXIT_CRITICAL(); return s;
}
CallbackStats readCallbacks() {
    taskENTER_CRITICAL(); CallbackStats s=sharedCallbacks; taskEXIT_CRITICAL(); return s;
}
void publishMedia(const MediaStatus& s) {
    taskENTER_CRITICAL(); sharedMedia=s; taskEXIT_CRITICAL();
}
void publishView(const RenderView& v) {
    taskENTER_CRITICAL(); sharedView=v; taskEXIT_CRITICAL();
}
RenderView readView() {
    taskENTER_CRITICAL(); RenderView v=sharedView; taskEXIT_CRITICAL(); return v;
}
int outputReadyNoAllocation() {
    mm_context_t* context=ModuleView(camera.getStream(Config::STREAM_CHANNEL)).context();
    if (context==NULL || context->port[Config::STREAM_CHANNEL].output_recycle==NULL) return 0;
    mm_queue_item_t* item=NULL;
    return xQueuePeek(context->port[Config::STREAM_CHANNEL].output_recycle,&item,0)==pdTRUE;
}
void readNegotiated(MediaStatus& s) {
    taskENTER_CRITICAL();
    if (uvc_format_ptr!=NULL) {
        s.hostState=uvc_format_ptr->state;
        s.negotiatedWidth=uvc_format_ptr->width;
        s.negotiatedHeight=uvc_format_ptr->height;
        s.negotiatedFps=uvc_format_ptr->fps;
        s.negotiatedFormat=uvc_format_ptr->format;
    }
    taskEXIT_CRITICAL();
}

void onDetection(std::vector<ObjectDetectionResult> results) {
    static bool failedPublish=false; // callback has one SDK owner
    DetectionSnapshot s={};
    s.at=millis(); s.discontinuous=failedPublish;
    unsigned invalid=0;
    for (size_t i=0;i<results.size();++i) {
        ObjectDetectionResult item=results[i];
        const int type=item.type(),score=item.score();
        if (type<0 || type>=80) continue;
        if (type==Config::PLANT_CLASS) {
            if (score>s.bestScore) s.bestScore=score;
            if (score>=Config::CONFIDENCE_PERCENT) ++s.plantCount;
        }
        const bool selected=(type==Config::PLANT_CLASS || (Config::SHOW_BIRD && type==Config::BIRD_CLASS));
        if (!selected || !itemList[type].filter || score<Config::CONFIDENCE_PERCENT) continue;
        DisplayBox box={type,score,item.xMin(),item.xMax(),item.yMin(),item.yMax()};
        if (!validBox(box)) { ++invalid; continue; }
        if (s.boxCount<MAX_DISPLAY_BOXES) s.boxes[s.boxCount++]=box;
    }
    bool active;
    taskENTER_CRITICAL();
    s.sequence=++sharedCallbacks.callbacks;
    sharedCallbacks.lastAt=s.at; sharedCallbacks.lastScore=s.bestScore;
    sharedCallbacks.invalidBoxes+=invalid;
    active=sharedMedia.active; s.epoch=sharedMedia.epoch;
    if (active) {
        ++sharedCallbacks.activeSamples;
        if (s.plantCount>0) {
            ++sharedCallbacks.hits;
            unsigned bin=(unsigned)s.bestScore/10;
            if (bin>9) bin=9;
            ++sharedCallbacks.scoreBins[bin];
        }
    }
    taskEXIT_CRITICAL();
    if (!active) { failedPublish=false; return; }
    failedPublish=(xQueueSend(samples,&s,0)!=pdPASS);
    if (failedPublish) {
        taskENTER_CRITICAL(); ++sharedCallbacks.drops; taskEXIT_CRITICAL();
    }
}

int pixel(float value,int extent) {
    if (value<=0) return 0;
    if (value>=1) return extent-1;
    return (int)(value*extent);
}
const char* screenState(PlantMonitor::State s) {
    switch (s) {
        case PlantMonitor::CONFIRMED:return "PLANT CONFIRMED";
        case PlantMonitor::UNCONFIRMED:return "NOT CONFIRMED";
        case PlantMonitor::NN_TIMEOUT:return "NN DATA TIMEOUT";
        case PlantMonitor::OFFLINE:return "USB OFFLINE";
        default:return "CHECKING";
    }
}
const char* boxName(BoxHold::Mode m) {
    return m==BoxHold::DETECTED ? "DETECTED" : m==BoxHold::HOLD ? "HOLD" : "NONE";
}
const char* stateName(PlantMonitor::State s) {
    switch (s) {
        case PlantMonitor::CONFIRMED:return "PLANT_CONFIRMED";
        case PlantMonitor::UNCONFIRMED:return "UNCONFIRMED_CHECK_VIEW";
        case PlantMonitor::NN_TIMEOUT:return "INFERENCE_TIMEOUT";
        case PlantMonitor::OFFLINE:return "USB_OFFLINE";
        default:return "CHECKING";
    }
}
bool sameView(const RenderView& a,const RenderView& b) {
    if (a.overlay==OVERLAY_OFF && b.overlay==OVERLAY_OFF && a.epoch==b.epoch) return true;
    return a.epoch==b.epoch && a.sequence==b.sequence && a.mode==b.mode && a.state==b.state
        && a.overlay==b.overlay && a.confirms==b.confirms && a.alerts==b.alerts && a.count==b.count;
}
void drawView(const RenderView& v) {
    const int w=streamConfig.width(),h=streamConfig.height();
    OSD.createBitmap(Config::STREAM_CHANNEL);
    if (v.overlay!=OVERLAY_OFF) {
        for (unsigned i=0;i<v.count && i<MAX_DISPLAY_BOXES;++i) {
            const DisplayBox& b=v.boxes[i];
            int x1=pixel(b.xmin,w),x2=pixel(b.xmax,w),y1=pixel(b.ymin,h),y2=pixel(b.ymax,h);
            if (x2<=x1 || y2<=y1) continue;
            const bool held=v.mode==BoxHold::HOLD && b.type==Config::PLANT_CLASS;
            const uint32_t color=held ? OSD_COLOR_YELLOW : OSD_COLOR_GREEN;
            OSD.drawRect(Config::STREAM_CHANNEL,x1,y1,x2,y2,3,color);
            char label[40];
            snprintf(label,sizeof(label),"%s %d %s",itemList[b.type].objectName,b.score,held ? "HOLD" : "LIVE");
            const int labelWidth=(int)strlen(label)*OSD.getTextWidth(Config::STREAM_CHANNEL);
            if (x1+labelWidth>w) x1=(w>labelWidth) ? w-labelWidth : 0;
            y1-=OSD.getTextHeight(Config::STREAM_CHANNEL);
            if (y1<0) y1=0;
            OSD.drawText(Config::STREAM_CHANNEL,x1,y1,label,color);
        }
        if (v.overlay==OVERLAY_FULL && Config::USE_LEDS_AND_EVENTS) {
            const int step=OSD.getTextHeight(Config::STREAM_CHANNEL)+4;
            char line[32];
            OSD.drawText(Config::STREAM_CHANNEL,20,20,screenState(v.state),OSD_COLOR_CYAN);
            snprintf(line,sizeof(line),"Plants:%d Score:%d",v.plants,v.score);
            OSD.drawText(Config::STREAM_CHANNEL,20,20+step,line,OSD_COLOR_WHITE);
            snprintf(line,sizeof(line),"Box:%s",boxName(v.mode));
            OSD.drawText(Config::STREAM_CHANNEL,20,20+step*2,line,OSD_COLOR_WHITE);
            snprintf(line,sizeof(line),"Confirm:%lu",(unsigned long)v.confirms);
            OSD.drawText(Config::STREAM_CHANNEL,20,20+step*3,line,OSD_COLOR_WHITE);
            snprintf(line,sizeof(line),"Alert:%lu",(unsigned long)v.alerts);
            OSD.drawText(Config::STREAM_CHANNEL,20,20+step*4,line,OSD_COLOR_WHITE);
            snprintf(line,sizeof(line),"Confirm%lus Miss%lus",(unsigned long)(Config::CONFIRM_MS/1000),(unsigned long)(Config::MISSING_MS/1000));
            const char* hint=(v.state==PlantMonitor::UNCONFIRMED || v.state==PlantMonitor::NN_TIMEOUT) ? "Check camera view" : line;
            OSD.drawText(Config::STREAM_CHANNEL,20,20+step*5,hint,OSD_COLOR_CYAN);
        }
    }
    OSD.update(Config::STREAM_CHANNEL);
}

bool mediaInit(MediaStatus& m) {
    m.phase=CONFIGURING; publishMedia(m);
    // Original NN example ordering; USB preset selects cameraOpenUVCD internally.
    configureSmartTreeCamera(camera,streamConfig,nnConfig);
    usb.configVideo(streamConfig);
    if (ModuleView(usb).context()==NULL || uvc_format_ptr==NULL) { m.error=1; return false; }
    camera.videoInit();
    if (ModuleView(camera.getStream(Config::STREAM_CHANNEL)).context()==NULL ||
        ModuleView(camera.getStream(Config::NN_CHANNEL)).context()==NULL) { m.error=2; return false; }
    detector.configVideo(nnConfig);
    selectSmartTreeModel(detector);
    detector.configThreshold(Config::CONFIDENCE_PERCENT/100.0f,Config::NMS_THRESHOLD);
    detector.setResultCallback(onDetection);
    detector.begin();
    m.nnContextReady=ModuleView(detector).context()!=NULL;
    if (!m.nnContextReady) { m.error=3; return false; }
    publishMedia(m); // context ready != successful model inference; STATUS reports both
    displayLink.registerInput(camera.getStream(Config::STREAM_CHANNEL));
    displayLink.registerOutput(usb);
    if (displayLink.begin()!=0) { m.error=4; return false; }
    camera.channelBegin(Config::STREAM_CHANNEL);
    nnLink.registerInput(camera.getStream(Config::NN_CHANNEL));
    nnLink.setStackSize(); nnLink.setTaskPriority(); nnLink.registerOutput(detector);
    if (nnLink.begin()!=0) { m.error=5; return false; }
    camera.channelBegin(Config::NN_CHANNEL);
    m.phase=WAIT_HOST; publishMedia(m);
    // Blocks only this task. Never replace it with the forever-loop UVCD::begin().
    usb.nnbegin(camera.getStream(Config::STREAM_CHANNEL),displayLink.linker,
                Config::STREAM_CHANNEL,outputReadyNoAllocation());
    OSD.configVideo(Config::STREAM_CHANNEL,streamConfig);
    OSD.begin(); m.osdReady=true; m.phase=RUNNING;
    publishMedia(m);
    return true;
}

struct MediaRuntime {
    MediaStatus status;
    bool paused,haveDrawn;
    RenderView previous;
    uint32_t handledRetry;
};
MediaRuntime mediaRuntime={}; // only the media owner accesses this object
void serviceMedia(MediaRuntime& runtime) {
        MediaStatus& m=runtime.status;
        uint32_t request;
        taskENTER_CRITICAL(); request=retryRequest; taskEXIT_CRITICAL();
        if (request!=runtime.handledRetry) {
            runtime.handledRetry=request;
            // Explicit recovery only: one API wait per USB RETRY request, no busy loop.
            m.active=false; m.outputReady=false; ++m.epoch; m.phase=WAIT_HOST; publishMedia(m);
            usb.nnbegin(camera.getStream(Config::STREAM_CHANNEL),displayLink.linker,
                        Config::STREAM_CHANNEL,outputReadyNoAllocation());
            m.phase=RUNNING; runtime.haveDrawn=false;
        }
        readNegotiated(m);
        m.outputReady=usb.isUsbUvcConnected(outputReadyNoAllocation())!=0;
        // isUsbUvcConnected also tests recycle-queue occupancy, which can be
        // momentarily empty. Only a real host-state=0 stops/restarts NN.
        const bool hostStreaming=m.hostState==1;
        if (!hostStreaming) {
            if (m.active) { m.active=false; ++m.epoch; publishMedia(m); }
            if (!runtime.paused) {
                camera.channelEnd(Config::NN_CHANNEL);
                // Official disconnectNN() calls channelBegin here (its comment
                // says "Stop", but the implementation starts the video channel).
                camera.channelBegin(Config::STREAM_CHANNEL);
                runtime.paused=true;
            }
            runtime.haveDrawn=false;
        } else {
            if (runtime.paused) {
                camera.channelBegin(Config::NN_CHANNEL);
                camera.channelBegin(Config::STREAM_CHANNEL);
                runtime.paused=false;
            }
            if (!m.active) { ++m.epoch; m.active=true; }
            publishMedia(m);
            RenderView view=readView();
            if (view.epoch==m.epoch) {
                // Expire even if the Arduino loop stops publishing a new view.
                const uint32_t at=millis();
                const uint32_t liveLimit=Config::BOX_HOLD_MS>0 ? Config::BOX_HOLD_MS : Config::NN_FRESH_MS;
                const bool plantExpired=Config::BOX_HOLD_MS>0 && uint32_t(at-view.hitAt)>=Config::BOX_HOLD_MS;
                unsigned kept=0;
                for (unsigned i=0;i<view.count;++i) {
                    const bool plant=view.boxes[i].type==Config::PLANT_CLASS;
                    if ((plant && plantExpired) || (!plant && uint32_t(at-view.sampleAt)>=liveLimit)) continue;
                    view.boxes[kept++]=view.boxes[i];
                }
                view.count=kept;
                if (plantExpired) view.mode=BoxHold::EMPTY;
                if (!runtime.haveDrawn || !sameView(view,runtime.previous)) {
                    drawView(view); runtime.previous=view; runtime.haveDrawn=true;
                }
            }
        }
        publishMedia(m);
}
#if SMARTTREE_STAGE >= 5
void mediaTask(void*) {
    if (!mediaInit(mediaRuntime.status)) {
        mediaRuntime.status.phase=MEDIA_ERROR; mediaRuntime.status.active=false;
        publishMedia(mediaRuntime.status);
        vTaskDelete(NULL); return;
    }
    for (;;) {
        serviceMedia(mediaRuntime);
        vTaskDelay(pdMS_TO_TICKS(Config::MEDIA_POLL_MS));
    }
}
#endif

// The following data have exactly one owner: Arduino setup/loop.
PlantMonitor monitor(Config::CONFIRM_MS,Config::MISSING_MS,Config::NN_FRESH_MS,Config::MISS_GRACE_MS);
BoxHold hold(Config::BOX_HOLD_MS,Config::NN_FRESH_MS);
DetectionSnapshot latest={};
bool haveLatest=false,wasActive=false,logEvents=Config::DEFAULT_EVENT_LOG;
bool autoStatus=Config::DEFAULT_AUTO_STATUS,sampleLog=false,setupFailed=false;
uint32_t epoch=0,lastSeq=0,lastGoodAt=0;
bool haveGood=false;
CommandParser parser;
constexpr unsigned HISTORY_SIZE=32;
EventRecord history[HISTORY_SIZE];
unsigned historyHead=0,historyCount=0,historySendIndex=0,historySendCount=0;
EventRecord historyCopy[HISTORY_SIZE]; // bounded snapshot so paging cannot be corrupted
bool sendingHistory=false;
OverlayMode overlay=Config::USE_LEDS_AND_EVENTS ? OVERLAY_FULL : OVERLAY_BOXES;
uint32_t fpsAt=0,fpsCallbacks=0,fps100=0,lastAutoAt=0,statsAt=0;
CallbackStats statsBase={};
uint32_t visibleMs=0,holdMs=0,displayMs=0,displayAt=0;
BoxHold::Mode previousMode=BoxHold::EMPTY;
uint32_t previousAccepted=0;

const char* eventName(EventKind k) {
    switch (k) {
        case EV_CONFIRM:return "PLANT_CONFIRMED";
        case EV_ALERT:return "PLANT_UNCONFIRMED";
        case EV_TIMEOUT:return "INFERENCE_TIMEOUT";
        case EV_CONNECT:return "USB_CONNECTED";
        case EV_DISCONNECT:return "USB_DISCONNECTED";
        case EV_GAP:return "RESULT_QUEUE_GAP";
        default:return "MEDIA_ERROR";
    }
}
void printEvent(const EventRecord& e) {
    char line[112];
    snprintf(line,sizeof(line),"EVENT,%lu,%s,%d,%d",(unsigned long)e.at,eventName(e.kind),e.score,e.count);
    Serial.println(line);
}
void addEvent(EventKind kind,uint32_t at,int score=0,int count=0) {
    EventRecord e={at,kind,score,count};
    history[historyHead]=e; historyHead=(historyHead+1)%HISTORY_SIZE;
    if (historyCount<HISTORY_SIZE) ++historyCount;
    if (logEvents) printEvent(e);
}
void decision(PlantMonitor::Event e,uint32_t at) {
    if (!Config::USE_LEDS_AND_EVENTS) return;
    if (e==PlantMonitor::PLANT_CONFIRMED) addEvent(EV_CONFIRM,at,latest.bestScore,latest.plantCount);
    else if (e==PlantMonitor::PLANT_UNCONFIRMED) addEvent(EV_ALERT,at,latest.bestScore,latest.plantCount);
    else if (e==PlantMonitor::INFERENCE_TIMEOUT) addEvent(EV_TIMEOUT,at);
}
const char* phaseName(Phase p) {
    switch (p) {
        case CONFIGURING:return "CONFIGURING";
        case WAIT_HOST:return "WAIT_HOST_STREAM";
        case RUNNING:return "RUNNING";
        case MEDIA_ERROR:return "ERROR";
        default:return "BOOT";
    }
}
void printStatus() {
    const uint32_t now=millis();
    const MediaStatus m=readMedia();
    const CallbackStats cb=readCallbacks();
    // First seven fields preserve the complete v1.1 STATUS record.
    char line[256];
    const bool fresh=haveLatest && uint32_t(now-latest.at)<Config::NN_FRESH_MS && m.active;
    snprintf(line,sizeof(line),"STATUS,%lu,%s,%d,%d,%lu,%lu",(unsigned long)now,stateName(monitor.state()),
             fresh ? latest.bestScore : 0,fresh ? latest.plantCount : 0,
             (unsigned long)monitor.confirms(),(unsigned long)monitor.alerts());
    Serial.println(line);
    snprintf(line,sizeof(line),"USB,phase=%s,host_stream=%d,stream_ready=%d,pipeline_active=%d,osd_ready=%d,epoch=%lu,error=%d,format=%d,width=%d,height=%d,fps=%d",
             phaseName(m.phase),m.hostState,m.outputReady,m.active,m.osdReady,(unsigned long)m.epoch,m.error,
             m.negotiatedFormat,m.negotiatedWidth,m.negotiatedHeight,m.negotiatedFps);
    Serial.println(line);
    snprintf(line,sizeof(line),"NN,context_ready=%d,callback_seen=%d,fresh=%d,fps=%lu.%02lu,last_callback_ms=%lu,callbacks=%lu,queue_drops=%lu",
             m.nnContextReady,cb.callbacks>0,cb.callbacks>0 && uint32_t(now-cb.lastAt)<Config::NN_FRESH_MS,
             (unsigned long)(fps100/100),(unsigned long)(fps100%100),(unsigned long)cb.lastAt,
             (unsigned long)cb.callbacks,(unsigned long)cb.drops);
    Serial.println(line);
    snprintf(line,sizeof(line),"DETECTION,raw=%d,box=%s,last_valid_seen=%d,last_valid_ms=%lu,age_ms=%lu,evidence_ms=%lu,overlay=%d,version=2.0,stage=%d",
             fresh && latest.plantCount>0,boxName(m.active ? hold.mode(now) : BoxHold::EMPTY),haveGood,
             (unsigned long)lastGoodAt,(unsigned long)(haveGood ? uint32_t(now-lastGoodAt) : 0),
             (unsigned long)monitor.positiveEvidenceMs(),(int)overlay,SMARTTREE_STAGE);
    Serial.println(line);
    snprintf(line,sizeof(line),"MEM,free_heap=%lu,min_free_heap=%lu",(unsigned long)xPortGetFreeHeapSize(),
             (unsigned long)xPortGetMinimumEverFreeHeapSize());
    Serial.println(line);
}
void resetStats(uint32_t now) {
    statsBase=readCallbacks(); statsAt=now;
    visibleMs=holdMs=displayMs=0; displayAt=now;
}
void printStats() {
    const CallbackStats cb=readCallbacks();
    char line[240];
    snprintf(line,sizeof(line),"STATS,elapsed_ms=%lu,raw_samples=%lu,raw_hits=%lu,queue_drops=%lu,invalid_boxes=%lu,display_ms=%lu,visible_ms=%lu,hold_ms=%lu,threshold=%d",
             (unsigned long)uint32_t(millis()-statsAt),(unsigned long)uint32_t(cb.activeSamples-statsBase.activeSamples),
             (unsigned long)uint32_t(cb.hits-statsBase.hits),(unsigned long)uint32_t(cb.drops-statsBase.drops),
             (unsigned long)uint32_t(cb.invalidBoxes-statsBase.invalidBoxes),(unsigned long)displayMs,
             (unsigned long)visibleMs,(unsigned long)holdMs,Config::CONFIDENCE_PERCENT);
    Serial.println(line);
    Serial.print("SCORE_BINS");
    for (unsigned i=0;i<10;++i) { Serial.print(','); Serial.print(uint32_t(cb.scoreBins[i]-statsBase.scoreBins[i])); }
    Serial.println();
}
void command(const char* line) {
    if (strcmp(line,"PING")==0) Serial.println("SMARTTREE_READY 1");
    else if (strcmp(line,"STATUS")==0) printStatus();
    else if (strcmp(line,"HISTORY")==0) {
        const unsigned first=(historyHead+HISTORY_SIZE-historyCount)%HISTORY_SIZE;
        for (unsigned i=0;i<historyCount;++i) historyCopy[i]=history[(first+i)%HISTORY_SIZE];
        historySendCount=historyCount; historySendIndex=0; sendingHistory=true;
        Serial.println("HISTORY_BEGIN,time_ms,event,score,plant_count");
    } else if (strcmp(line,"LOG ON")==0 || strcmp(line,"LOG OFF")==0) {
        logEvents=autoStatus=(strcmp(line,"LOG ON")==0); Serial.println(logEvents ? "OK,LOG_ON" : "OK,LOG_OFF");
    } else if (strcmp(line,"SAMPLES ON")==0 || strcmp(line,"SAMPLES OFF")==0) {
        sampleLog=strcmp(line,"SAMPLES ON")==0;
        Serial.println(sampleLog ? "SAMPLE_COLUMNS,sequence,time_ms,plant_count,score,box,state,epoch" : "OK,SAMPLES_OFF");
    } else if (strcmp(line,"STATS")==0) printStats();
    else if (strcmp(line,"STATS RESET")==0) { resetStats(millis()); Serial.println("OK,STATS_RESET"); }
    else if (strcmp(line,"OSD OFF")==0) { overlay=OVERLAY_OFF; Serial.println("OK,OSD_OFF"); }
    else if (strcmp(line,"OSD BOXES")==0) { overlay=OVERLAY_BOXES; Serial.println("OK,OSD_BOXES"); }
    else if (strcmp(line,"OSD FULL")==0) { overlay=OVERLAY_FULL; Serial.println("OK,OSD_FULL"); }
    else if (strcmp(line,"USB RETRY")==0) {
        if (readMedia().phase!=RUNNING) Serial.println("ERR,USB_NOT_READY_FOR_RETRY");
        else { taskENTER_CRITICAL(); ++retryRequest; taskEXIT_CRITICAL(); Serial.println("OK,USB_RETRY_OPEN_CAMERA"); }
    } else if (strcmp(line,"HELP")==0) {
        Serial.println("COMMANDS:PING STATUS HISTORY LOG ON/OFF SAMPLES ON/OFF STATS STATS RESET OSD OFF/BOXES/FULL USB RETRY");
    } else if (*line) Serial.println("ERR,UNKNOWN_COMMAND");
}
void handleSerial() {
    // Bounded work, even if a host floods UART. Serial/LED never touch the camera.
    for (unsigned budget=0;budget<64 && Serial.available()>0;++budget) {
        CommandParser::Result r=parser.feed((char)Serial.read());
        if (r==CommandParser::TOO_LONG) Serial.println("ERR,LINE_TOO_LONG");
        else if (r==CommandParser::LINE) command(parser.line());
    }
    if (sendingHistory) {
        if (historySendIndex<historySendCount) printEvent(historyCopy[historySendIndex++]);
        else { Serial.println("HISTORY_END"); sendingHistory=false; }
    }
}
void updateLeds(uint32_t now) {
    bool blue=false,green=false;
    if (Config::USE_LEDS_AND_EVENTS) {
        switch (monitor.state()) {
            case PlantMonitor::CONFIRMED:green=true;break;
            case PlantMonitor::UNCONFIRMED:blue=true;break;
            case PlantMonitor::CHECKING:blue=((now/500)%2)==0;break;
            case PlantMonitor::NN_TIMEOUT:blue=((now/150)%2)==0;break;
            default:break;
        }
    }
    if (setupFailed || readMedia().phase==MEDIA_ERROR) blue=((now/150)%2)==0;
    digitalWrite(LED_B,blue ? HIGH : LOW); digitalWrite(LED_G,green ? HIGH : LOW);
}
void serviceApp() {
    const uint32_t now=millis();
    const MediaStatus m=readMedia();
    if (m.active!=wasActive || m.epoch!=epoch) {
        if (wasActive) addEvent(EV_DISCONNECT,now);
        epoch=m.epoch; wasActive=m.active;
        hold.clear(); latest=DetectionSnapshot{}; haveLatest=false; lastSeq=0;
        if (m.active) { monitor.reconnect(now); addEvent(EV_CONNECT,now); }
        else monitor.offline();
    }
    DetectionSnapshot s;
    for (unsigned budget=0;budget<Config::QUEUE_LENGTH && samples!=NULL && xQueueReceive(samples,&s,0)==pdPASS;++budget) {
        if (!m.active || s.epoch!=epoch || uint32_t(millis()-s.at)>=Config::NN_FRESH_MS) continue;
        if (lastSeq!=0 && uint32_t(s.sequence-lastSeq)!=1) s.discontinuous=true;
        lastSeq=s.sequence;
        latest=s; haveLatest=true;
        if (s.discontinuous) addEvent(EV_GAP,s.at,s.bestScore,s.plantCount);
        if (s.plantCount>0) { lastGoodAt=s.at; haveGood=true; }
        hold.observe(s,Config::PLANT_CLASS);
        if (Config::USE_MONITOR) decision(monitor.observe(s.at,s.plantCount>0,s.discontinuous),s.at);
        if (sampleLog) {
            char line[160];
            snprintf(line,sizeof(line),"SAMPLE,%lu,%lu,%d,%d,%s,%s,%lu",(unsigned long)s.sequence,
                     (unsigned long)s.at,s.plantCount,s.bestScore,boxName(hold.mode(millis())),
                     stateName(monitor.state()),(unsigned long)s.epoch);
            Serial.println(line);
        }
    }
    if (m.active && Config::USE_MONITOR) decision(monitor.tick(millis()),millis());
    const uint32_t current=millis();
    if (monitor.state()==PlantMonitor::NN_TIMEOUT && Config::USE_MONITOR) hold.clear();
    RenderView v={};
    v.epoch=epoch; v.sequence=lastSeq; v.state=monitor.state(); v.overlay=overlay;
    v.confirms=monitor.confirms(); v.alerts=monitor.alerts(); v.mode=m.active ? hold.mode(current) : BoxHold::EMPTY;
    v.hitAt=hold.hitAt(); v.sampleAt=latest.at;
    v.plants=haveLatest ? latest.plantCount : 0; v.score=haveLatest ? latest.bestScore : 0;
    if (v.mode!=BoxHold::EMPTY) {
        for (unsigned i=0;i<hold.count();++i) v.boxes[v.count++]=hold.box(i);
    }
    // Optional v1.1 bird display is live only and never enters BoxHold/monitor.
    const uint32_t liveLimit=Config::BOX_HOLD_MS>0 ? Config::BOX_HOLD_MS : Config::NN_FRESH_MS;
    if (Config::SHOW_BIRD && m.active && haveLatest && uint32_t(current-latest.at)<liveLimit) {
        for (unsigned i=0;i<latest.boxCount && v.count<MAX_DISPLAY_BOXES;++i)
            if (latest.boxes[i].type==Config::BIRD_CLASS) v.boxes[v.count++]=latest.boxes[i];
    }
    publishView(v);
    const uint32_t dt=uint32_t(current-displayAt); displayAt=current;
    if (previousAccepted) {
        displayMs+=dt;
        if (previousMode!=BoxHold::EMPTY) visibleMs+=dt;
        if (previousMode==BoxHold::HOLD) holdMs+=dt;
    }
    previousAccepted=m.active && overlay!=OVERLAY_OFF;
    previousMode=v.mode;
    const CallbackStats cb=readCallbacks();
    if (uint32_t(current-fpsAt)>=1000) {
        fps100=(uint32_t(cb.callbacks-fpsCallbacks)*100000UL)/uint32_t(current-fpsAt);
        fpsCallbacks=cb.callbacks; fpsAt=current;
    }
    static bool reportedError=false;
    if (m.phase==MEDIA_ERROR && !reportedError) { addEvent(EV_ERROR,current,m.error); reportedError=true; }
    updateLeds(current);
}
} // namespace

void smartTreeSetup() {
    Serial.begin(115200);
    pinMode(LED_B,OUTPUT); pinMode(LED_G,OUTPUT);
    digitalWrite(LED_B,LOW); digitalWrite(LED_G,LOW);
    monitor.offline(); displayAt=fpsAt=statsAt=millis();
    samples=xQueueCreate(Config::QUEUE_LENGTH,sizeof(DetectionSnapshot));
    Serial.println("SMARTTREE_READY 1"); // command endpoint ready; not a USB/model success claim
    Serial.println("SMARTTREE_VERSION,2.0");
    Serial.println("STATUS_COLUMNS,time_ms,state,score,plant_count,confirm_events,alert_events");
    if (samples==NULL) {
        setupFailed=true; MediaStatus m={}; m.phase=MEDIA_ERROR; m.error=6; publishMedia(m); return;
    }
#if SMARTTREE_STAGE >= 5
    if (xTaskCreate(mediaTask,"SmartTreeMedia",Config::MEDIA_STACK_WORDS,NULL,1,NULL)!=pdPASS) {
        setupFailed=true; MediaStatus m={}; m.phase=MEDIA_ERROR; m.error=7; publishMedia(m);
    }
#else
    // B/C/D intentionally reproduce the single-task initialization dependency.
    // Use E/F for UART-only operation. Stage A is a separate official sketch.
    if (!mediaInit(mediaRuntime.status)) {
        mediaRuntime.status.phase=MEDIA_ERROR; publishMedia(mediaRuntime.status); setupFailed=true;
    }
#endif
}

void smartTreeLoop() {
#if SMARTTREE_STAGE < 5
    static uint32_t lastMediaAt=0;
    if (!setupFailed && uint32_t(millis()-lastMediaAt)>=Config::MEDIA_POLL_MS) {
        serviceMedia(mediaRuntime); lastMediaAt=millis();
    }
#endif
    handleSerial();
    serviceApp();
    const uint32_t now=millis();
    if (autoStatus && uint32_t(now-lastAutoAt)>=1000) { printStatus(); lastAutoAt=now; }
    delay(Config::APP_POLL_MS);
}
