/* Gens KO input extension, 2026-11-20. GPL v2; see LICENSE.txt. */
#ifndef GENS_MODERN_INPUT_H
#define GENS_MODERN_INPUT_H
#include <windows.h>
#include "modern_input_logic.h"

void ModernInput_LoadConfig(const char *file);
void ModernInput_SaveConfig(const char *file);
INT_PTR ModernInput_ShowDialog(HWND owner);
int ModernInput_IsCapturing(void);
int ModernInput_ActionPressed(int player, int action);
void ModernInput_Poll(int foreground);
int ModernInput_CheckXInput(unsigned code);
void ModernInput_ClearState(void);
int ModernInput_Deadzone(void);
int ModernInput_XConnected(int slot);
unsigned ModernInput_FirstXInput(int slot);
void ModernInput_DescribeBinding(unsigned code, wchar_t *out, unsigned capacity);
bool ModernInput_BindingAssigned(unsigned code);
#endif
