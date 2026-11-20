/* Gens KO modification, 2026-11-20. GPL-2.0 terms; see LICENSE.txt. */
#ifndef GENS_PIER_MAPPER_H
#define GENS_PIER_MAPPER_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0: ordinary cartridge, 1: large/SSF2 cartridge, 2: Pier Solar. */
extern int Cart_Active;
extern unsigned char Cart_Visible[0x800002];
/* VDP DMA uses the same byte-swapped image as 68000 fetch/data reads. */
extern unsigned char *Cart_DMA_View;

/* Configure is called on the unswapped source ROM; Reset on the byte-swapped
 * ROM used by Gens' 68000 core. The stable visible buffer is also the opcode
 * fetch buffer, so a bank switch cannot leave instruction fetch stale. */
void Cart_Configure(const unsigned char *source, unsigned int size);
void Cart_Reset(void);
void Cart_RefreshView(void);
void Cart_Unload(void);
int Cart_IsPier(void);
unsigned int Cart_ReadByte(unsigned int address);
unsigned int Cart_ReadWord(unsigned int address);
void Cart_WriteByte(unsigned int address, unsigned int value);
void Cart_WriteWord(unsigned int address, unsigned int value);

/* Raw SRAM images remain compatible with other emulators. The nonvolatile
 * EEPROM status byte is stored separately by the frontend. */
unsigned int Cart_GetNvStatus(void);
void Cart_SetNvStatus(unsigned int value);

/* A tagged extension follows an otherwise unmodified Genesis save-state.
 * Loading validates the extension before changing any mapper/SRAM state. */
/* The same fixed-layout payload is shared by file saves and rewind. */
unsigned int Cart_MemoryStateSize(void);
int Cart_ExportMemory(unsigned char *buffer, unsigned int length);
int Cart_ValidateMemory(const unsigned char *buffer, unsigned int length);
int Cart_ImportMemory(const unsigned char *buffer, unsigned int length);
int Cart_SaveState(FILE *file);
int Cart_LoadState(FILE *file, long base_length);

#ifdef __cplusplus
}
#endif
#endif
