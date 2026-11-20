/*
 * Gens KO cartridge support, written 2026-11-20.
 * Distributed under the GNU GPL version 2 terms in LICENSE.txt.
 *
 * Register/address facts were checked against MAME's BSD-3-Clause
 * src/devices/bus/megadrive/stm95.cpp (Fabio Priuli, MetalliC), ST's
 * M95320 datasheet, and Brandon's "Pier Solar and the Great Reverser".
 * This is an independent Gens implementation; no Genesis Plus GX or
 * PicoDrive source code is incorporated. See docs/CORE_PROVENANCE.md.
 */
#include "pier_mapper.h"
#include "cart_limits.h"
#include <string.h>

extern "C" {
extern unsigned char Rom_Data[GENS_ROM_BUFFER_SIZE];
extern unsigned int Rom_Size;
extern unsigned char SRAM[65536];
extern int SRAM_Start, SRAM_End, SRAM_ON, SRAM_Write;

int Cart_Active = 0;
unsigned char Cart_Visible[0x800002];
unsigned char *Cart_DMA_View = Rom_Data;
}

enum SpiPhase { SPI_COMMAND, SPI_ADDRESS, SPI_READ, SPI_WRITE,
                SPI_STATUS_READ, SPI_STATUS_WRITE, SPI_DONE, SPI_IGNORE };

struct CartState {
    uint8_t bank[8];
    uint8_t control, unlock_reads;
    uint8_t pins, held, phase, command, input, input_bits;
    uint8_t output, output_bits, output_shift, address_bytes;
    uint8_t write_enable, status, pending_status;
    uint16_t address, page_base;
    uint32_t page_mask;
    uint8_t page[32];
};

static CartState state;
static uint32_t source_hash;
static uint32_t source_size;

static uint32_t hash_bytes(const unsigned char *data, unsigned int size)
{
    uint32_t hash = 2166136261U;
    for (unsigned int i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619U;
    return hash;
}

static void map_bank(unsigned int window, unsigned int bank)
{
    const unsigned int destination = window * 0x80000U;
    const unsigned int source = bank * 0x80000U;
    if (window >= 16) return;
    memset(Cart_Visible + destination, 0xff, 0x80000);
    if (source < Rom_Size) {
        unsigned int size = ((Rom_Size + 1U) & ~1U) - source;
        if (size > 0x80000) size = 0x80000;
        memcpy(Cart_Visible + destination, Rom_Data + source, size);
    }
}

static void rebuild_view(void)
{
    Cart_DMA_View = Cart_Active ? Cart_Visible : Rom_Data;
    memset(Cart_Visible, 0xff, sizeof(Cart_Visible));
    if (!Cart_Active) return;
    if (Cart_Active == 2 && state.unlock_reads) {
        /* At power-on the cartridge exposes its 32 KiB bootstrap repeatedly.
         * The bootstrap unlocks the normal view with three byte accesses in
         * the 0x010000 page. This is cartridge behavior, not a ROM patch. */
        for (unsigned int i = 0; i < 0x400000; i += 0x8000)
            memcpy(Cart_Visible + i, Rom_Data, 0x8000);
        return;
    }
    unsigned int size = Rom_Size < 0x800000 ? ((Rom_Size + 1U) & ~1U) : 0x800000;
    memcpy(Cart_Visible, Rom_Data, size);
    if (Cart_Active == 2) {
        for (unsigned int i = 5; i < 8; ++i) map_bank(i, state.bank[i]);
    } else {
        for (unsigned int i = 0; i < 8; ++i) map_bank(i, state.bank[i]);
    }
}

void Cart_Configure(const unsigned char *source, unsigned int size)
{
    Cart_Active = 0;
    Cart_DMA_View = Rom_Data;
    source_size = size;
    source_hash = 0;
    memset(&state, 0, sizeof(state));
    if (!source || size < 512 || size > GENS_ROM_MAX_SIZE) return;
    for (unsigned int i = 0x180; i + 6 <= 0x18e; ++i) {
        if (memcmp(source + i, "T-5740", 6) == 0) {
            Cart_Active = 2;
            break;
        }
    }
    if (!Cart_Active && size > 0x400000) Cart_Active = 1;
    if (Cart_Active) source_hash = hash_bytes(source, size);
}

void Cart_Reset(void)
{
    if (!Cart_Active) return;
    const uint8_t nv_status = state.status;
    memset(&state, 0, sizeof(state));
    state.status = nv_status;
    state.pins = 0x0c; /* CS high, HOLD high, SCK low. */
    state.output = 1;
    state.control = 1;
    state.unlock_reads = Cart_Active == 2 ? 3 : 0;
    for (unsigned int i = 0; i < 8; ++i) state.bank[i] = (uint8_t)i;
    if (Cart_Active == 2) state.bank[5] = state.bank[6] = state.bank[7] = 0;
    rebuild_view();
}

void Cart_Unload(void)
{
    Cart_Active = 0;
    Cart_DMA_View = Rom_Data;
    memset(&state, 0, sizeof(state));
    source_size = source_hash = 0;
}

void Cart_RefreshView(void) { rebuild_view(); }

int Cart_IsPier(void) { return Cart_Active == 2; }
unsigned int Cart_GetNvStatus(void) { return state.status & 0x8c; }
void Cart_SetNvStatus(unsigned int value) { state.status = value & 0x8c; }

static bool page_protected(void)
{
    switch ((state.status >> 2) & 3) {
        case 1: return state.page_base >= 0x0c00;
        case 2: return state.page_base >= 0x0800;
        case 3: return true;
        default: return false;
    }
}

static void spi_finish(void)
{
    /* A command ending in the middle of a byte must not program the array. */
    if (state.input_bits) return;
    if (state.phase == SPI_DONE && state.command == 0x06) state.write_enable = 1;
    else if (state.phase == SPI_DONE && state.command == 0x04) state.write_enable = 0;
    else if (state.command == 0x02 && state.phase == SPI_WRITE &&
             state.write_enable && state.page_mask) {
        if (!page_protected()) {
            for (unsigned int i = 0; i < 32; ++i)
                if (state.page_mask & (1U << i)) SRAM[state.page_base + i] = state.page[i];
        }
        state.write_enable = 0;
    } else if (state.command == 0x01 && state.phase == SPI_DONE && state.write_enable) {
        /* The cartridge ties /WP high. Writes complete atomically on CS high;
         * write-cycle timing is intentionally not cycle-accurate. */
        state.status = state.pending_status & 0x8c;
        state.write_enable = 0;
    }
}

static void spi_receive(uint8_t value)
{
    switch (state.phase) {
        case SPI_COMMAND:
            state.command = value;
            state.address = 0;
            state.address_bytes = 0;
            switch (value) {
                case 0x02: case 0x03: state.phase = SPI_ADDRESS; break;
                case 0x05: state.phase = SPI_STATUS_READ; break;
                case 0x01: state.phase = SPI_STATUS_WRITE; break;
                case 0x04: case 0x06: state.phase = SPI_DONE; break;
                default: state.phase = SPI_IGNORE; break;
            }
            break;
        case SPI_ADDRESS:
            state.address = (uint16_t)((state.address << 8) | value);
            if (++state.address_bytes == 2) {
                state.address &= 0x0fff; /* M95320: 4096 x 8. */
                if (state.command == 3) state.phase = SPI_READ;
                else {
                    state.phase = SPI_WRITE;
                    state.page_base = state.address & 0x0fe0;
                    state.page_mask = 0;
                }
            }
            break;
        case SPI_WRITE:
            state.page[state.address & 31] = value;
            state.page_mask |= 1U << (state.address & 31);
            state.address = (uint16_t)(state.page_base | ((state.address + 1) & 31));
            break;
        case SPI_STATUS_WRITE:
            state.pending_status = value & 0x8c;
            state.phase = SPI_DONE;
            break;
        default: break;
    }
}

static void spi_pins(uint8_t value)
{
    value &= 15;
    const uint8_t previous = state.pins;
    state.pins = value;
    if ((previous ^ value) & 8) {
        if (value & 8) {
            spi_finish();
            state.output = 1;
        } else {
            state.phase = SPI_COMMAND;
            state.command = state.input = state.input_bits = 0;
            state.output_bits = 0;
            state.held = !(value & 4);
        }
        return;
    }
    if (value & 8) return;
    if (!(value & 2)) state.held = !(value & 4);
    if (state.held || !((previous ^ value) & 2)) return;
    if (value & 2) {
        if (state.phase == SPI_READ || state.phase == SPI_STATUS_READ || state.phase == SPI_IGNORE) return;
        if (state.phase == SPI_DONE) { state.phase = SPI_IGNORE; return; }
        state.input = (uint8_t)((state.input << 1) | (value & 1));
        if (++state.input_bits == 8) {
            state.input_bits = 0;
            spi_receive(state.input);
        }
    } else if (state.phase == SPI_READ || state.phase == SPI_STATUS_READ) {
        if (!state.output_bits) {
            state.output_shift = state.phase == SPI_READ ? SRAM[state.address] :
                (uint8_t)(state.status | (state.write_enable ? 2 : 0));
        }
        state.output = state.output_shift >> 7;
        state.output_shift <<= 1;
        if (++state.output_bits == 8) {
            state.output_bits = 0;
            if (state.phase == SPI_READ) state.address = (state.address + 1) & 0x0fff;
        }
    }
}

unsigned int Cart_ReadByte(unsigned int address)
{
    address &= 0xffffff;
    if (address < 0x800000) {
        if (Cart_Active == 1 && SRAM_ON && address >= (unsigned)SRAM_Start &&
            address <= (unsigned)SRAM_End && address - SRAM_Start < 65536)
            return SRAM[address - SRAM_Start];
        const unsigned int value = Cart_Visible[address ^ 1];
        if (Cart_Active == 2 && state.unlock_reads && address >= 0x10000 && address < 0x20000) {
            if (!--state.unlock_reads) rebuild_view();
        }
        return value;
    }
    if (Cart_Active == 2 && address == 0xa1300b)
        return ((state.pins & 8) || state.held) ? 1 : state.output;
    return 0xff;
}

unsigned int Cart_ReadWord(unsigned int address)
{
    address &= 0xfffffe;
    if (address < 0x800000) {
        if (Cart_Active == 1 && SRAM_ON && address >= (unsigned)SRAM_Start &&
            address < (unsigned)SRAM_End && address - SRAM_Start < 65535)
            return (SRAM[address - SRAM_Start] << 8) | SRAM[address + 1 - SRAM_Start];
        /* Word accesses do not clock the byte-read bootstrap unlock. */
        return Cart_Visible[address] | (Cart_Visible[address + 1] << 8);
    }
    if (Cart_Active == 2 && address == 0xa1300a) return Cart_ReadByte(address + 1);
    return 0xffff;
}

void Cart_WriteByte(unsigned int address, unsigned int value)
{
    address &= 0xffffff;
    value &= 0xff;
    if (address < 0x800000) {
        if (Cart_Active == 1 && SRAM_ON && SRAM_Write && address >= (unsigned)SRAM_Start &&
            address <= (unsigned)SRAM_End && address - SRAM_Start < 65536)
            SRAM[address - SRAM_Start] = (uint8_t)value;
        return;
    }
    if (Cart_Active == 2) {
        switch (address) {
            case 0xa13001: state.control = (uint8_t)value; break;
            case 0xa13003: case 0xa13005: case 0xa13007:
                if (state.control & 2) {
                    const unsigned int slot = 5 + (address - 0xa13003) / 2;
                    state.bank[slot] = value & 15;
                    if (!state.unlock_reads) map_bank(slot, state.bank[slot]);
                }
                break;
            case 0xa13009: spi_pins((uint8_t)value); break;
            default: break;
        }
    } else if (Cart_Active == 1) {
        if (address == 0xa130f1) {
            SRAM_ON = value & 1;
            SRAM_Write = !(value & 2);
        } else if (address >= 0xa130f3 && address <= 0xa130ff && (address & 1)) {
            const unsigned int slot = (address - 0xa130f1) / 2;
            state.bank[slot] = value & 63;
            map_bank(slot, state.bank[slot]);
        }
    }
}

void Cart_WriteWord(unsigned int address, unsigned int value)
{
    address &= 0xfffffe;
    if (address >= 0xa13000 && address <= 0xa130fe) Cart_WriteByte(address + 1, value);
    else if (address < 0x800000) {
        Cart_WriteByte(address, value >> 8);
        Cart_WriteByte(address + 1, value);
    }
}

/* Portable, fixed-layout state payload: do not dump a compiler-dependent C
 * structure. Validate all fields before applying a loaded extension. */
static void put32(uint8_t *p, uint32_t v)
{ p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t *p)
{ return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

static const unsigned int STATE_META_SIZE = 128;
static const unsigned int STATE_TOTAL_SIZE = STATE_META_SIZE + 65536;

unsigned int Cart_MemoryStateSize(void)
{ return Cart_Active ? STATE_TOTAL_SIZE : 0; }

int Cart_ExportMemory(unsigned char *buffer, unsigned int length)
{
    if (!Cart_Active) return length == 0;
    if (!buffer || length != STATE_TOTAL_SIZE) return 0;
    uint8_t *meta = buffer;
    memset(meta, 0, STATE_META_SIZE);
    memcpy(meta, "GKCMAP01", 8);
    put32(meta + 8, STATE_TOTAL_SIZE);
    put32(meta + 12, Cart_Active);
    put32(meta + 16, source_size);
    put32(meta + 20, source_hash);
    memcpy(meta + 24, state.bank, 8);
    meta[32] = state.control; meta[33] = state.unlock_reads;
    meta[34] = state.pins; meta[35] = state.held; meta[36] = state.phase;
    meta[37] = state.command; meta[38] = state.input; meta[39] = state.input_bits;
    meta[40] = state.output; meta[41] = state.output_bits; meta[42] = state.output_shift;
    meta[43] = state.address_bytes; meta[44] = state.write_enable;
    meta[45] = state.status; meta[46] = state.pending_status;
    put32(meta + 48, state.address); put32(meta + 52, state.page_base);
    put32(meta + 56, state.page_mask);
    memcpy(meta + 60, state.page, 32);
    put32(meta + 92, hash_bytes(SRAM, 65536));
    put32(meta + 96, SRAM_ON); put32(meta + 100, SRAM_Write);
    put32(meta + 104, hash_bytes(meta, 104));
    memcpy(buffer + STATE_META_SIZE, SRAM, 65536);
    return 1;
}

int Cart_ValidateMemory(const unsigned char *buffer, unsigned int length)
{
    if (!Cart_Active) return length == 0;
    if (!buffer || length != STATE_TOTAL_SIZE) return 0;
    const uint8_t *meta = buffer;
    const uint8_t *memory = buffer + STATE_META_SIZE;
    if (memcmp(meta, "GKCMAP01", 8) || get32(meta + 8) != STATE_TOTAL_SIZE ||
        get32(meta + 12) != (unsigned)Cart_Active || get32(meta + 16) != source_size ||
        get32(meta + 20) != source_hash || get32(meta + 104) != hash_bytes(meta, 104)) return 0;
    if (get32(meta + 92) != hash_bytes(memory, 65536)) return 0;
    if (meta[33] > 3 || meta[34] > 15 || meta[35] > 1 || meta[36] > SPI_IGNORE ||
        meta[39] > 7 || meta[40] > 1 || meta[41] > 7 || meta[43] > 2 ||
        meta[44] > 1 || (meta[45] & ~0x8c) || (meta[46] & ~0x8c) ||
        get32(meta + 48) > 0xffff || get32(meta + 52) > 0x0fe0 ||
        (get32(meta + 52) & 31) || get32(meta + 96) > 1 || get32(meta + 100) > 1) return 0;
    for (unsigned int i = 0; i < 8; ++i) if (meta[24 + i] > 63) return 0;
    if (Cart_Active == 2) {
        for (unsigned int i = 0; i < 5; ++i) if (meta[24 + i] != i) return 0;
        for (unsigned int i = 5; i < 8; ++i) if (meta[24 + i] > 15) return 0;
        if (meta[43] == 2 && get32(meta + 48) > 0x0fff) return 0;
    }
    return 1;
}

int Cart_ImportMemory(const unsigned char *buffer, unsigned int length)
{
    if (!Cart_ValidateMemory(buffer, length)) return 0;
    if (!Cart_Active) return 1;
    const uint8_t *meta = buffer;
    CartState loaded;
    memset(&loaded, 0, sizeof(loaded));
    memcpy(loaded.bank, meta + 24, 8);
    loaded.control = meta[32]; loaded.unlock_reads = meta[33];
    loaded.pins = meta[34]; loaded.held = meta[35]; loaded.phase = meta[36];
    loaded.command = meta[37]; loaded.input = meta[38]; loaded.input_bits = meta[39];
    loaded.output = meta[40]; loaded.output_bits = meta[41]; loaded.output_shift = meta[42];
    loaded.address_bytes = meta[43]; loaded.write_enable = meta[44];
    loaded.status = meta[45]; loaded.pending_status = meta[46];
    loaded.address = (uint16_t)get32(meta + 48); loaded.page_base = (uint16_t)get32(meta + 52);
    loaded.page_mask = get32(meta + 56);
    memcpy(loaded.page, meta + 60, 32);
    state = loaded;
    memcpy(SRAM, buffer + STATE_META_SIZE, 65536);
    SRAM_ON = get32(meta + 96); SRAM_Write = get32(meta + 100);
    rebuild_view();
    return 1;
}

int Cart_SaveState(FILE *file)
{
    if (!Cart_Active) return 1;
    uint8_t buffer[STATE_TOTAL_SIZE];
    return file && Cart_ExportMemory(buffer, sizeof(buffer)) &&
           fwrite(buffer, 1, sizeof(buffer), file) == sizeof(buffer);
}

int Cart_LoadState(FILE *file, long base_length)
{
    if (!Cart_Active) return 1;
    uint8_t buffer[STATE_TOTAL_SIZE];
    if (!file || base_length < 0 || fseek(file, base_length, SEEK_SET) ||
        fread(buffer, 1, sizeof(buffer), file) != sizeof(buffer)) return 0;
    return Cart_ImportMemory(buffer, sizeof(buffer));
}
