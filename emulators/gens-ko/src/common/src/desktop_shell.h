/* Gens KO Windows shell integration, 2026-11-20. GPLv2. */
#ifndef GENS_DESKTOP_SHELL_H
#define GENS_DESKTOP_SHELL_H
#include <windows.h>
#define ID_KO_SHADER_SETTINGS 57001
#define ID_KO_MAKE_SHORTCUT 57002
#define ID_KO_TOGGLE_MENU 57003
#define ID_KO_QUICK_HELP 57004
#define ID_KO_HOTKEY_SETTINGS 57005
void Desktop_PreInit(void);
void Desktop_LoadConfig(const char *path);
void Desktop_SaveConfig(const char *path);
void Desktop_ApplyMode(HWND hwnd,int full,int force);
void Desktop_ApplyMenu(HWND hwnd,HMENU menu);
bool Desktop_FilterMessage(MSG *msg);
void Desktop_OnDeactivate(void);
void Desktop_CreateShortcut(HWND owner);
void Desktop_ShowHelp(HWND owner);
void Desktop_LaunchContent(HWND owner);
void Desktop_ToggleMenu(HWND owner);
void Desktop_OnDpiChanged(HWND hwnd,LPARAM param);
bool Desktop_IsMenuVisible(void);
bool Desktop_IsInitialized(void);
#endif
