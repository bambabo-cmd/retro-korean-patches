/* Gens KO 2026-11-20. GPL-2.0; see original LICENSE.txt. */
#ifndef GENS_REWIND_ENGINE_H
#define GENS_REWIND_ENGINE_H
#include <stdint.h>
#include <stddef.h>

void Rewind_Configure(bool enabled, int seconds, int memoryMiB);
void Rewind_Clear();
bool Rewind_Capture(uint64_t frame);
/* *restoredFrame is IN=current emulated frame, OUT=restored frame. A repeated
 * target that would not go backwards returns false without mutating state. */
bool Rewind_Step(uint64_t targetFrame, uint64_t *restoredFrame);
bool Rewind_CanUse();
bool Rewind_HasHistory();
const wchar_t *Rewind_Reason();
size_t Rewind_MemoryBytes();
uint64_t Rewind_OldestFrame();
uint64_t Rewind_NewestFrame();

#endif
