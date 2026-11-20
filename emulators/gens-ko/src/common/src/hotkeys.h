/* Gens KO transport settings, 2026-11-20. GPL v2; see LICENSE.txt. */
#ifndef GENS_HOTKEYS_H
#define GENS_HOTKEYS_H
#include <windows.h>
enum HotkeyAction { HK_FAST_FORWARD, HK_REWIND, HK_SLOW_MOTION, HK_COUNT };
struct PlaybackSettings {
    int normalPercent,ffPercent,rewindPercent,slowPercent;
    int rewindSeconds,rewindMemoryMiB;
    bool rewindEnabled;
};
void Hotkeys_LoadConfig(const char *file);
void Hotkeys_SaveConfig(const char *file);
void Hotkeys_ShowDialog(HWND owner);
bool Hotkeys_FilterMessage(MSG *message);
void Hotkeys_Poll(void);
void Hotkeys_ClearActive(void);
bool Hotkeys_IsActive(HotkeyAction action);
const PlaybackSettings &Hotkeys_Settings(void);
bool Hotkeys_IsCapturing(void);
#endif
