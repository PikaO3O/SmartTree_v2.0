#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits>
#include "../SmartTree/PlantMonitor.h"
#include "../SmartTree/DetectionSnapshot.h"
#include "../SmartTree/CommandParser.h"

static void run(PlantMonitor& m,uint32_t start,unsigned ms,bool present) {
    for (unsigned d=0;d<=ms;d+=200) m.observe(start+d,present);
}
static DetectionSnapshot frame(uint32_t at,bool hit) {
    DetectionSnapshot s={}; s.at=at; s.sequence=at/200+1;
    if (hit) {
        s.plantCount=1; s.bestScore=70; s.boxCount=1;
        s.boxes[0]=DisplayBox{58,70,0.1f,0.5f,0.2f,0.8f};
    }
    return s;
}
int main() {
    {
        PlantMonitor m(2000,5000,1500,1000);
        run(m,0,1800,true); m.observe(2000,false); m.observe(2200,true);
        assert(m.positiveEvidenceMs()==1800 && m.confirms()==0);
        m.observe(2400,true);
        assert(m.confirms()==1 && m.state()==PlantMonitor::CONFIRMED);
    }
    {
        PlantMonitor m(2000,5000,1500,1000);
        run(m,0,2000,true);
        m.observe(2200,false); m.observe(2800,false);
        assert(m.state()==PlantMonitor::CONFIRMED);
        m.observe(3000,true); assert(m.confirms()==1 && m.state()==PlantMonitor::CONFIRMED);
        run(m,3200,1000,false); assert(m.state()==PlantMonitor::CHECKING);
        run(m,4400,3800,false);
        assert(m.alerts()==1 && m.state()==PlantMonitor::UNCONFIRMED);
        run(m,8400,1000,false); assert(m.alerts()==1);
        run(m,9600,2000,true); assert(m.confirms()==2);
    }
    {
        PlantMonitor m(2000,5000,1500,1000);
        run(m,0,1800,true); run(m,2000,1000,false); run(m,3200,1800,true);
        assert(m.confirms()==0 && m.positiveEvidenceMs()==1800);
        m.observe(5200,true); assert(m.confirms()==1);
    }
    {
        PlantMonitor m(2000,5000,1500,1000);
        run(m,0,2000,true); m.observe(2200,false);
        assert(m.tick(3199)==PlantMonitor::NONE && m.state()==PlantMonitor::CONFIRMED);
        m.tick(3200); assert(m.state()==PlantMonitor::CHECKING);
        assert(m.tick(3700)==PlantMonitor::INFERENCE_TIMEOUT);
        assert(m.alerts()==0); // silence is not a real negative inference
    }
    {
        PlantMonitor m(2000,5000,1500,1000); run(m,0,2000,true);
        m.observe(2200,true,true);
        assert(m.state()==PlantMonitor::CHECKING && m.confirms()==1);
        run(m,2400,1800,true); assert(m.confirms()==2);
        m.offline(); m.reconnect(5000); run(m,5000,2000,true); assert(m.confirms()==3);
    }
    {
        PlantMonitor m(2000,5000,1500,1000);
        m.observe(0,true); m.observe(1499,true); m.observe(2000,true);
        assert(m.confirms()==1); // minimum three independent callbacks still applies
        m.reset(0); m.observe(0,true); m.observe(2000,true);
        assert(m.confirms()==0); // stale gap must reset evidence
    }
    {
        BoxHold h; h.observe(frame(0,true));
        assert(h.mode(0)==BoxHold::DETECTED && h.count()==1);
        h.observe(frame(200,false));
        assert(h.mode(599)==BoxHold::HOLD && h.box(0).score==70);
        assert(h.mode(600)==BoxHold::EMPTY);
        h.observe(frame(1000,true)); assert(h.mode(1599)==BoxHold::DETECTED);
        assert(h.mode(1600)==BoxHold::EMPTY); // callback silence cannot keep a box forever
    }
    {
        BoxHold h; h.observe(frame(0,true)); h.observe(frame(400,true));
        h.observe(frame(600,false));
        assert(h.mode(999)==BoxHold::HOLD && h.mode(1000)==BoxHold::EMPTY);
        DetectionSnapshot s=frame(1100,false); s.discontinuous=true; h.observe(s);
        assert(h.mode(1100)==BoxHold::EMPTY);
        h.observe(frame(1200,true)); h.clear(); assert(h.mode(1201)==BoxHold::EMPTY);
    }
    {
        BoxHold h; DetectionSnapshot s=frame(0,true);
        s.boxes[0].xmin=std::numeric_limits<float>::quiet_NaN(); h.observe(s);
        assert(h.mode(0)==BoxHold::EMPTY);
        s=frame(100,true); s.boxes[0].type=14; h.observe(s);
        assert(h.mode(100)==BoxHold::EMPTY); // a bird cannot become a retained plant
        s=frame(200,true); s.boxes[0].xmax=s.boxes[0].xmin;
        assert(!validBox(s.boxes[0]));
    }
    {
        BoxHold h; PlantMonitor m(2000,5000,1500,1000);
        DetectionSnapshot s=frame(0,true); h.observe(s); m.observe(0,true);
        for (uint32_t at=200;at<=6000;at+=200) {
            s=frame(at,false); h.observe(s); m.observe(at,false);
            if (at<600) assert(h.mode(at)==BoxHold::HOLD);
        }
        assert(m.confirms()==0 && m.alerts()==1); // retained rectangle never feeds monitor
    }
    {
        const uint32_t start=UINT32_MAX-100;
        BoxHold h; h.observe(frame(start,true)); h.observe(frame(start+200,false));
        assert(h.mode(start+599)==BoxHold::HOLD && h.mode(start+600)==BoxHold::EMPTY);
        PlantMonitor m(2000,5000,1500,1000); m.reset(start); run(m,start,2000,true);
        assert(m.confirms()==1); run(m,start+2200,5000,false); assert(m.alerts()==1);
    }
    {
        BoxHold h(0); h.observe(frame(0,true)); assert(h.mode(100)==BoxHold::DETECTED);
        h.observe(frame(200,false)); assert(h.mode(200)==BoxHold::EMPTY);
    }
    {
        CommandParser p;
        for (const char* s="PING\r";*s;++s) assert(p.feed(*s)==CommandParser::WAITING);
        assert(p.feed('\n')==CommandParser::LINE && strcmp(p.line(),"PING")==0);
        for (unsigned i=0;i<1000;++i) p.feed('A');
        assert(p.feed('\n')==CommandParser::TOO_LONG);
        for (const char* s="STATUS";*s;++s) p.feed(*s);
        assert(p.feed('\n')==CommandParser::LINE && strcmp(p.line(),"STATUS")==0);
        for (unsigned i=0;i<47;++i) p.feed('B');
        assert(p.feed('\n')==CommandParser::LINE && strlen(p.line())==47);
    }
    puts("PASS: 13 v2 scenario groups (grace/evidence, counts, timeout, gaps, HOLD expiry, invalid boxes, wrap, parser)");
}
