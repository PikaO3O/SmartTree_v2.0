#ifndef SMARTTREE_PLANT_MONITOR_H
#define SMARTTREE_PLANT_MONITOR_H
#include <stdint.h>

// One owner (Arduino loop). observe() is called ONLY for a new real NN callback.
// Default grace=0 preserves v1.1 tests; v2 explicitly enables 1000 ms in Config.
class PlantMonitor {
public:
    enum State { CHECKING, CONFIRMED, UNCONFIRMED, NN_TIMEOUT, OFFLINE };
    enum Event { NONE, PLANT_CONFIRMED, PLANT_UNCONFIRMED, INFERENCE_TIMEOUT };
    PlantMonitor(uint32_t confirmMs=2000, uint32_t missingMs=5000,
                 uint32_t freshnessMs=1500, uint32_t graceMs=0)
        : confirmMs_(confirmMs), missingMs_(missingMs), freshnessMs_(freshnessMs),
          graceMs_(graceMs) { reset(0); }

    void reset(uint32_t now) {
        confirmedEvents_=alertEvents_=0;
        reconnect(now);
    }
    void reconnect(uint32_t now) {
        state_=committed_=CHECKING;
        startedAt_=lastSampleAt_=missingSince_=now;
        hasSample_=candidate_=missing_=lastPositive_=false;
        positiveMs_=0; positiveSamples_=0;
    }
    void offline() {
        state_=OFFLINE; committed_=CHECKING;
        hasSample_=candidate_=missing_=lastPositive_=false;
    }
    Event observe(uint32_t at, bool present, bool discontinuous=false) {
        const bool gap=discontinuous || (hasSample_ && uint32_t(at-lastSampleAt_)>=freshnessMs_);
        if (gap || state_==NN_TIMEOUT || state_==OFFLINE) {
            candidate_=missing_=lastPositive_=false;
            committed_=CHECKING;
        }
        const uint32_t step=uint32_t(at-lastSampleAt_);
        const bool hadSample=hasSample_;
        lastSampleAt_=at; hasSample_=true;
        if (present) {
            if (missing_ && (graceMs_==0 || uint32_t(at-missingSince_)>=graceMs_))
                candidate_=false;
            if (!candidate_) {
                candidate_=true; positiveMs_=0; positiveSamples_=0;
            } else if (hadSample && lastPositive_) {
                // Pause evidence time across misses: HOLD is never positive evidence.
                if (positiveMs_<confirmMs_) {
                    const uint32_t remaining=confirmMs_-positiveMs_;
                    positiveMs_ += step>=remaining ? remaining : step;
                }
            }
            missing_=false; lastPositive_=true;
            if (positiveSamples_<65535) ++positiveSamples_;
            state_=CHECKING;
            if (positiveMs_>=confirmMs_ && positiveSamples_>=3) {
                state_=CONFIRMED;
                if (committed_!=CONFIRMED) {
                    committed_=CONFIRMED; ++confirmedEvents_;
                    return PLANT_CONFIRMED;
                }
            }
        } else {
            if (!missing_) { missing_=true; missingSince_=at; }
            lastPositive_=false;
            const uint32_t absent=uint32_t(at-missingSince_);
            if (graceMs_==0 || absent>=graceMs_) candidate_=false;
            state_=(committed_==CONFIRMED && graceMs_>0 && absent<graceMs_) ? CONFIRMED : CHECKING;
            if (absent>=missingMs_) {
                state_=UNCONFIRMED;
                if (committed_!=UNCONFIRMED) {
                    committed_=UNCONFIRMED; ++alertEvents_;
                    return PLANT_UNCONFIRMED;
                }
            }
        }
        return NONE;
    }
    Event tick(uint32_t now) {
        if (state_==OFFLINE) return NONE;
        const uint32_t reference=hasSample_ ? lastSampleAt_ : startedAt_;
        if (uint32_t(now-reference)>=freshnessMs_) {
            if (state_!=NN_TIMEOUT) {
                state_=NN_TIMEOUT; committed_=CHECKING;
                candidate_=missing_=lastPositive_=false;
                return INFERENCE_TIMEOUT;
            }
        } else if (missing_ && state_==CONFIRMED && uint32_t(now-missingSince_)>=graceMs_) {
            state_=CHECKING;
        }
        return NONE;
    }
    State state() const { return state_; }
    uint32_t confirms() const { return confirmedEvents_; }
    uint32_t alerts() const { return alertEvents_; }
    uint32_t freshnessMs() const { return freshnessMs_; }
    uint32_t positiveEvidenceMs() const { return positiveMs_; }
private:
    uint32_t confirmMs_,missingMs_,freshnessMs_,graceMs_;
    uint32_t startedAt_,lastSampleAt_,missingSince_,positiveMs_;
    uint32_t confirmedEvents_,alertEvents_;
    uint16_t positiveSamples_;
    bool hasSample_,candidate_,missing_,lastPositive_;
    State state_,committed_;
};
#endif
