/* Gens KO transport scheduler, 2026-11-20. GPL-2.0; see LICENSE.txt.
   Normal 1x retains DirectSound ring pacing. Other rates use the performance
   counter and muted output; this is not a pitch-preserving audio processor. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef GENS_KO_PLAYBACK_TEST
#include "playback_test_runtime.h"
#else
#include "playback.h"
#include "hotkeys.h"
#include "rewind_engine.h"
#include "G_ddraw.h"
#include "G_dsound.h"
#include "G_Input.h"
#include "G_main.h"
#include "gens.h"
#include "vdp_io.h"
#include "mem_M68K.h"
#include "renderer_gl.h"
#endif
#include "playback_clock.h"
#include <cstdio>
#include <cstring>

namespace {
enum Mode {Unset=-1,Normal,Fast,Slow,Reverse};
GensPlayback::FrameClock clockFrames;
GensPlayback::RewindCursor reverseCursor;
uint64_t frequency=0,frameSerial=0,lastPresent=0,lastAudioProgress=0;
int lastAudioSegment=-1,activeRate=100,activeMode=Unset,activeFps=60;
bool audioMuted=false,audioStalled=false,vsyncAllowed=true,rewindUnavailable=false;
bool rewindAtStart=false;

uint64_t Now() {
    LARGE_INTEGER value;
    if(!frequency) {
        if(QueryPerformanceFrequency(&value)&&value.QuadPart>0)frequency=(uint64_t)value.QuadPart;
        else frequency=1000;
    }
    if(frequency==1000)return GetTickCount64();
    QueryPerformanceCounter(&value);return (uint64_t)value.QuadPart;
}
void ClearMix() {
    memset(Seg_L,0,sizeof(Seg_L));memset(Seg_R,0,sizeof(Seg_R));
}
void MuteOutput(bool muted,uint64_t now) {
    if(muted) {
        if(!audioMuted || Sound_Is_Playing) {
            if(Sound_Initialised) {Stop_Sound();Clear_Sound_Buffer();}
            ClearMix();
        }
        audioMuted=true;
    } else if(audioMuted) {
        if(Sound_Initialised) {
            Clear_Sound_Buffer();
            if(Sound_Enable)Play_Sound();
        }
        ClearMix();audioMuted=false;audioStalled=false;
        int segment=0;
        if(Sound_GetPlaybackSegment(&segment))WP=RP=segment;
        lastAudioSegment=segment;lastAudioProgress=now;
    }
}
void Overlay(const char*text,int duration=1600) {
    if(Show_Message)Renderer_SetOverlayUtf8(text,duration);
}
void SetMode(int mode,int rate,int fps,uint64_t now) {
    if(activeMode==mode&&activeRate==rate&&activeFps==fps)return;
    bool wasReverse=activeMode==Reverse;
    bool first=activeMode==Unset;
    activeMode=mode;activeRate=rate;activeFps=fps;
    clockFrames.ResetAt(now);lastPresent=0;
    lastAudioProgress=now;lastAudioSegment=-1;audioStalled=false;
    vsyncAllowed=rate<=100 && mode!=Reverse;
    if(mode==Reverse)reverseCursor.Start(frameSerial);
    rewindAtStart=false;
    MuteOutput(mode==Reverse||rate!=100,now);
    if(mode!=Reverse&&rate==100) {
        /* Menus and pauses leave a silent DirectSound ring running. Reanchor
           after that host-time gap, instead of emulating its pending segments. */
        if(Sound_Enable&&Sound_Initialised&&!Sound_Is_Playing)Play_Sound();
        int segment=0;
        if(Sound_GetPlaybackSegment(&segment))WP=RP=segment;
        lastAudioSegment=segment;
    }
    if(!first || mode!=Normal || rate!=100) {
        char text[180];
        const char*label=mode==Reverse?"되감기":mode==Fast?"빨리 감기":mode==Slow?"슬로 모션":"기본 속도";
        std::snprintf(text,sizeof(text),"%s  %.2f×%s",label,rate/100.0,
                      (mode==Reverse||rate!=100)?" · 소리 꺼짐":"");
        Overlay(text);
    }
    if(wasReverse&&mode!=Reverse)ClearMix();
}
bool PresentationDue(uint64_t now) {
    return !lastPresent || now<lastPresent ||
        now-lastPresent>=frequency/(uint64_t)activeFps;
}
void EmulateFrames(HWND window,int frames,bool useAudio,uint64_t now) {
    if(frames<=0)return;
    const bool allowPresent=activeRate<=100||PresentationDue(now);
    for(int i=0;i<frames;i++) {
        if(useAudio) {
            if(!Write_Sound_Buffer(NULL))ClearMix();
            WP=(WP+1)&(Sound_Segs-1);
        } else ClearMix();
        Update_Controllers();
        ++Frame_Number;
        const bool render=i==frames-1 && allowPresent &&
            (Frame_Skip<0 || Frame_Number>Frame_Skip);
        if(render)Update_Frame();else Update_Frame_Fast();
        ++frameSerial;
        if(render) {
            Frame_Number=0;
            Rewind_Capture(frameSerial);
            Flip(window);lastPresent=Now();
        }
    }
}
/* This routine returns promptly even when the sound device stops advancing.
   The historical blocking WP==RP loop could otherwise freeze all hotkeys. */
bool AudioFrames(HWND window,uint64_t now) {
    int segment=0;
    if(!Sound_Enable||!Sound_Initialised||!Sound_Is_Playing)return false;
    if(!Sound_GetPlaybackSegment(&segment)) {
        if(!audioStalled)Overlay("사운드 장치 대기 중 · 시간 기준 재생",2000);
        /* Remember that QPC advanced the core while no device position was
           available. On recovery, old ring debt must not run those frames twice. */
        audioStalled=true;lastAudioSegment=-1;return false;
    }
    if(segment!=lastAudioSegment) {
        if(audioStalled) {
            Clear_Sound_Buffer();ClearMix();WP=segment;
            audioStalled=false;clockFrames.ResetAt(now);
        }
        lastAudioSegment=segment;lastAudioProgress=now;
    }
    if(now-lastAudioProgress>frequency/4) {
        if(!audioStalled) {
            audioStalled=true;clockFrames.ResetAt(now);
            Overlay("사운드 장치 대기 중 · 시간 기준 재생",2000);
        }
        return false;
    }
    RP=segment;
    int due=(RP-WP+Sound_Segs)&(Sound_Segs-1);
    if(due>8)due=8;
    if(due) {
        EmulateFrames(window,due,true,now);clockFrames.ResetAt(Now());
    } else Sleep(1);
    return true;
}
void ReverseFrames(HWND window,int due,uint64_t now) {
    if(due>0)reverseCursor.Advance((uint64_t)due);
    if(!PresentationDue(now)) {Sleep(1);return;}
    uint64_t restored=frameSerial;
    if(Rewind_Step(reverseCursor.Target(),&restored)) {
        frameSerial=restored;ClearMix();Flip(window);lastPresent=Now();
    } else {
        if(!rewindAtStart && (!Rewind_HasHistory()||reverseCursor.Target()<=Rewind_OldestFrame())) {
            Overlay(Rewind_HasHistory()?"되감기 기록의 처음입니다":"되감기 기록이 없습니다",1800);
            rewindAtStart=true;
        }
        Sleep(1);
    }
}
}

bool Playback_UsesVSync(void) {return vsyncAllowed;}

void Playback_Suspend(void) {
    Hotkeys_ClearActive();clockFrames.Reset();
    activeMode=Unset;lastPresent=0;vsyncAllowed=true;
    /* Keep an empty ring playing: WAV/GYM start and the independent netplay
       pump require Sound_Is_Playing even before their mode flags are set.
       Stopping the device here breaks commands invoked from menus or FF. */
    MuteOutput(false,Now());audioStalled=false;
    if(Sound_Initialised) {
        Clear_Sound_Buffer();
        if(Sound_Enable&&!Sound_Is_Playing)Play_Sound();
        int segment=0;
        if(Sound_GetPlaybackSegment(&segment))WP=RP=segment;
    }
    ClearMix();
    if(Net_Play||GYM_Playing) {Rewind_Clear();frameSerial=0;}
}

void Playback_ResetTimeline(void) {
    Playback_Suspend();Rewind_Clear();frameSerial=0;Frame_Number=0;
}

int Playback_Update(HWND window) {
    Hotkeys_Poll();
    const PlaybackSettings&s=Hotkeys_Settings();
    Rewind_Configure(s.rewindEnabled,s.rewindSeconds,s.rewindMemoryMiB);
    bool reverse=Hotkeys_IsActive(HK_REWIND);
    bool fast=Hotkeys_IsActive(HK_FAST_FORWARD);
    bool slow=Hotkeys_IsActive(HK_SLOW_MOTION);
    if(Net_Play) {Hotkeys_ClearActive();reverse=fast=slow=false;}
    bool unavailable=reverse&&!Rewind_CanUse();
    bool showUnavailable=unavailable&&!rewindUnavailable;
    if(unavailable)reverse=fast=slow=false;
    rewindUnavailable=unavailable;
    int mode=reverse?Reverse:fast?Fast:slow?Slow:Normal;
    int rate=Net_Play?100:GensPlayback::SelectedRate(reverse,fast,slow,
        s.normalPercent,s.ffPercent,s.slowPercent,s.rewindPercent);
    uint64_t now=Now();
    SetMode(mode,rate,CPU_Mode?50:60,now);
    if(showUnavailable) {
        const wchar_t*reason=Rewind_Reason();char text[640];
        if(reason&&WideCharToMultiByte(CP_UTF8,0,reason,-1,text,sizeof(text),NULL,NULL)>0)
            Overlay(text,2600);
    }
    /* A sound reinitialization from another UI command must not accidentally
       play at normal sample rate while accelerated transport is active. */
    if(mode==Reverse||rate!=100)MuteOutput(true,now);
    if(mode!=Reverse&&rate==100&&AudioFrames(window,now))return 1;
    int due=clockFrames.Due(now,frequency,activeFps,rate);
    if(mode==Reverse)ReverseFrames(window,due,now);
    else if(due)EmulateFrames(window,due,false,now);
    else Sleep(1);
    return 1;
}

int Playback_AdvanceOne(HWND window) {
    if(Net_Play)return 0;
    Playback_Suspend();
    const PlaybackSettings&s=Hotkeys_Settings();
    Rewind_Configure(s.rewindEnabled,s.rewindSeconds,s.rewindMemoryMiB);
    Update_Controllers();ClearMix();Update_Frame();++frameSerial;
    Rewind_Capture(frameSerial);Flip(window);return 1;
}
