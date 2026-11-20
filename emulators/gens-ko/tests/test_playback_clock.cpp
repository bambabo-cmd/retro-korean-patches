/* Gens KO timing regression tests. GPL-2.0; 2026-11-20. */
#include "../src/common/src/playback_clock.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>

static int checks=0;
static void check(bool ok) {++checks;assert(ok);}
static uint64_t run(int fps,int rate,uint64_t frequency,int seconds,
                    uint64_t firstTick=0) {
    GensPlayback::FrameClock c;
    check(c.Due(firstTick,frequency,fps,rate)==0);
    uint64_t count=0;
    for(int i=1;i<=seconds*1000;i++) {
        int due=c.Due(firstTick+(frequency*(uint64_t)i)/1000,frequency,fps,rate);
        check(due>=0&&due<=8);count+=(unsigned)due;
    }
    return count;
}
int main() {
    // Ten seconds of host time, arbitrary QPC frequencies, 50/60Hz and long
    // sessions beyond the 32-bit tick wrap. All rates are absolute percentages.
    for(int fps: {50,60})for(int rate:{5,10,25,50,75,100,125,200,400,800,1000,1600})
    for(uint64_t frequency: {1000ULL,1000000ULL,3579545ULL,10000000ULL}) {
        uint64_t frames=run(fps,rate,frequency,10,frequency*100000ULL);
        uint64_t expected=(uint64_t)fps*(uint64_t)rate/10;
        check(frames==expected);
    }
    GensPlayback::FrameClock c;
    check(c.Due(0,1000000,60,100)==0);
    check(c.Due(10000,1000000,60,100)==0);
    check(c.Due(20000,1000000,60,100)==1);
    check(c.Due(30000,1000000,60,100)==0);
    check(c.Due(40000,1000000,60,100)==1);
    // Pause/resume never accrues a hidden frame burst.
    c.Reset();check(c.Due(1000000000,1000000,60,400)==0);
    check(c.Due(1000004000,1000000,60,400)==0);
    check(c.Due(1000005000,1000000,60,400)==1);
    // A stalled renderer/audio device may create at most sixteen debt frames.
    c.Reset();check(c.Due(0,1000000,60,1600)==0);
    check(c.Due(600000000,1000000,60,1600)==8);
    check(c.Due(600000000,1000000,60,1600)==8);
    check(c.Due(600000000,1000000,60,1600)==0);
    // Counter rollback and invalid clocks drop debt instead of unsigned wraps.
    check(c.Due(100,1000000,60,400)==0);
    check(c.Due(101,0,60,400)==0);
    check(c.Due(102,1000000,0,400)==0);
    check(c.Due(103,1000000,60,0)==0);
    c.ResetAt(500);check(c.Due(500,1000000,60,50)==0);
    // Explicit priority remains REW > FF > slow > base rate.
    check(GensPlayback::SelectedRate(false,false,false,125,400,50,200)==125);
    check(GensPlayback::SelectedRate(false,false,true,125,400,50,200)==50);
    check(GensPlayback::SelectedRate(false,true,true,125,400,50,200)==400);
    check(GensPlayback::SelectedRate(true,true,true,125,400,50,200)==200);
    // Sparse captures must not turn 2x REW into 8x REW: 120 requested frames
    // traverse 120 emulated frames even with a four-frame snapshot interval.
    GensPlayback::RewindCursor cursor;cursor.Start(240);
    uint64_t restored=240;
    std::vector<uint64_t> history;for(uint64_t f=0;f<=240;f+=4)history.push_back(f);
    for(unsigned tick=0;tick<120;++tick) {
        uint64_t target=cursor.Advance(1);
        auto candidate=std::upper_bound(history.begin(),history.end(),target);
        if(candidate!=history.begin()) {
            --candidate;
            if(*candidate<restored) {
                restored=*candidate;history.erase(candidate+1,history.end());
            }
        }
        check(restored<=target&&target-restored<4);
    }
    check(restored==120&&cursor.Target()==120);
    check(cursor.Advance(UINT64_MAX)==0);
    check(cursor.Advance(1)==0);
    // Branch resumes from restored state; future old snapshots are gone.
    history.push_back(restored+1);check(history.back()==121);
    check(std::find(history.begin(),history.end(),240)==history.end());
    std::printf("playback timing: %d checks passed\n",checks);
}
