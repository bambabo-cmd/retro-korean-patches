/* Gens KO playback timing, 2026-11-20. GPL-2.0; see LICENSE.txt. */
#ifndef GENS_KO_PLAYBACK_CLOCK_H
#define GENS_KO_PLAYBACK_CLOCK_H
#include <stdint.h>
#include <algorithm>
#include <cmath>

namespace GensPlayback {
/* One call never requests more than eight frames. Long pauses discard excess
   debt, rather than freezing the message pump while catching up minutes later.
   Fractional frames survive ordinary calls, including slow rates below 1x. */
class FrameClock {
    uint64_t last_;
    long double credit_;
    bool started_;
public:
    FrameClock():last_(0),credit_(0),started_(false){}
    void Reset() { last_=0; credit_=0; started_=false; }
    void ResetAt(uint64_t tick) {last_=tick;credit_=0;started_=true;}
    int Due(uint64_t tick, uint64_t frequency, int fps, int percent,
            int maximum=8) {
        if(!frequency || fps<=0 || percent<=0 || maximum<=0) {ResetAt(tick);return 0;}
        if(!started_ || tick<last_) {ResetAt(tick);return 0;}
        uint64_t elapsed=tick-last_;last_=tick;
        long double seconds=(long double)elapsed/(long double)frequency;
        seconds=std::min(seconds,0.25L);
        credit_+=seconds*(long double)fps*(long double)percent/100.0L;
        credit_=std::min(credit_,(long double)(maximum*2));
        int due=std::min(maximum,(int)std::floor(credit_+1e-10L));
        credit_-=due;
        if(credit_<0)credit_=0;
        return due;
    }
};

/* The requested cursor is independent of the last restored snapshot. If FF
   captured only every fourth frame, REW must wait for the cursor to cross that
   gap instead of jumping four frames for each one-frame request. */
class RewindCursor {
    uint64_t target_;
public:
    RewindCursor():target_(0){}
    void Start(uint64_t current) {target_=current;}
    uint64_t Advance(uint64_t frames) {
        target_=frames>target_?0:target_-frames;return target_;
    }
    uint64_t Target() const {return target_;}
};

inline int SelectedRate(bool rewind,bool fast,bool slow,
                        int normalRate,int fastRate,int slowRate,int rewindRate) {
    return rewind?rewindRate:fast?fastRate:slow?slowRate:normalRate;
}
}
#endif
