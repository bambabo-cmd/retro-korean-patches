/* Gens KO playback controls, 2026-11-20. GPL-2.0; see LICENSE.txt. */
#ifndef GENS_KO_PLAYBACK_H
#define GENS_KO_PLAYBACK_H
#include <windows.h>

int Playback_Update(HWND window);
int Playback_AdvanceOne(HWND window);
/* Suspend preserves the rewind history, drops timing debt and releases all
   transport actions. Call at focus/pause/menu/modal UI transitions. The sound
   ring keeps playing silence so audio commands remain usable. Netplay/GYM
   invalidate local rewind history. */
void Playback_Suspend(void);
/* Reset also invalidates the history. Call after ROM/reset/load-state/core,
   region, sound format or cheat changes, before another frame is emulated. */
void Playback_ResetTimeline(void);
/* A fast rate must not be capped by the display's swap interval. */
bool Playback_UsesVSync(void);
#endif
