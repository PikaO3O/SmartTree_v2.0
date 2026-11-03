#ifndef SMARTTREE_DETECTION_SNAPSHOT_H
#define SMARTTREE_DETECTION_SNAPSHOT_H
#include <stdint.h>
#include <math.h>
constexpr unsigned MAX_DISPLAY_BOXES=16;
struct DisplayBox {
    int type,score;
    float xmin,xmax,ymin,ymax;
};
struct DetectionSnapshot {
    uint32_t at,sequence,epoch;
    bool discontinuous;
    int plantCount,bestScore;
    unsigned boxCount;
    DisplayBox boxes[MAX_DISPLAY_BOXES];
};
inline bool validBox(const DisplayBox& b) {
    return isfinite(b.xmin) && isfinite(b.xmax) && isfinite(b.ymin) && isfinite(b.ymax)
        && b.xmax>b.xmin && b.ymax>b.ymin && b.xmax>0 && b.ymax>0 && b.xmin<1 && b.ymin<1;
}
// Only metadata/coordinates, never a raster image buffer.
class BoxHold {
public:
    enum Mode { EMPTY, DETECTED, HOLD };
    BoxHold(uint32_t holdMs=600, uint32_t liveMs=1500) : holdMs_(holdMs),liveMs_(liveMs) { clear(); }
    void clear() { count_=0; hitAt_=sampleAt_=0; hasSample_=raw_=false; }
    void observe(const DetectionSnapshot& sample, int plantClass=58) {
        if (sample.discontinuous) clear();
        hasSample_=true; sampleAt_=sample.at; raw_=sample.plantCount>0;
        if (raw_) {
            count_=0;
            for (unsigned i=0;i<sample.boxCount && i<MAX_DISPLAY_BOXES;++i) {
                if (sample.boxes[i].type==plantClass && validBox(sample.boxes[i]))
                    boxes_[count_++]=sample.boxes[i];
            }
            hitAt_=sample.at;
        }
        if (holdMs_==0 && !raw_) count_=0;
    }
    Mode mode(uint32_t now) const {
        if (!hasSample_ || count_==0) return EMPTY;
        if (holdMs_==0) return raw_ && uint32_t(now-sampleAt_)<liveMs_ ? DETECTED : EMPTY;
        if (uint32_t(now-hitAt_)>=holdMs_) return EMPTY;
        return raw_ ? DETECTED : HOLD;
    }
    unsigned count() const { return count_; }
    uint32_t hitAt() const { return hitAt_; }
    const DisplayBox& box(unsigned i) const { return boxes_[i]; }
private:
    uint32_t holdMs_,liveMs_,hitAt_,sampleAt_;
    unsigned count_;
    bool hasSample_,raw_;
    DisplayBox boxes_[MAX_DISPLAY_BOXES];
};
#endif
