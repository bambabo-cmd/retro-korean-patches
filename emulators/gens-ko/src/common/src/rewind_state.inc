/* Gens KO 2026-11-20. GPL-2.0; see original LICENSE.txt.
 * Included by save.cpp after the legacy core declarations.
 * PRIVATE IN-PROCESS snapshots, never a disk/network save-state format.
 * Unlike GST, this preserves CPU interrupt/STOP state, FM/PSG phases,
 * controller handshakes, pending DMA and the exact displayed framebuffer.
 * Native pointers are valid only within this running emulator instance.
 */
#include <stdint.h>
#include <zlib.h>

extern "C" {
extern unsigned int ScrA_Addr, ScrB_Addr, Win_Addr, Spr_Addr, H_Scroll_Addr;
extern unsigned int H_Cell, H_Win_Mul, H_Pix, H_Pix_Begin;
extern unsigned int H_Scroll_Mask, H_Scroll_CMul, H_Scroll_CMask;
extern unsigned int V_Scroll_CMask, V_Scroll_MMask, Win_X_Pos, Win_Y_Pos;
extern unsigned int DMAT_Tmp, DMAT_Type;
extern unsigned int Sprite_Visible[256], Data_Spr[2], Data_Misc[13];
extern unsigned int PSG_Noise_Step_Table[4];
extern int int_cnt, in0, in1, in2, in3, en0, en1, en2, en3;
extern unsigned int PWM_Cycle_Tmp, PWM_Int_Tmp, PWM_FIFO_L_Tmp, PWM_FIFO_R_Tmp;
}

namespace {
struct RewindBlock { void *address; unsigned int length; };
#define REWIND_BLOCK(v) { &(v), static_cast<unsigned int>(sizeof(v)) }
#define REWIND_PAD(p) \
 REWIND_BLOCK(Controller_##p##_Up), REWIND_BLOCK(Controller_##p##_Down), \
 REWIND_BLOCK(Controller_##p##_Left), REWIND_BLOCK(Controller_##p##_Right), \
 REWIND_BLOCK(Controller_##p##_Start), REWIND_BLOCK(Controller_##p##_Mode), \
 REWIND_BLOCK(Controller_##p##_A), REWIND_BLOCK(Controller_##p##_B), \
 REWIND_BLOCK(Controller_##p##_C), REWIND_BLOCK(Controller_##p##_X), \
 REWIND_BLOCK(Controller_##p##_Y), REWIND_BLOCK(Controller_##p##_Z)
static const RewindBlock mdRewindBlocks[] = {
    REWIND_BLOCK(Context_68K), REWIND_BLOCK(M_Z80),
    REWIND_BLOCK(Ram_68k), REWIND_BLOCK(Ram_Z80), REWIND_BLOCK(SRAM),
    REWIND_BLOCK(SRAM_Start), REWIND_BLOCK(SRAM_End), REWIND_BLOCK(SRAM_ON),
    REWIND_BLOCK(SRAM_Write), REWIND_BLOCK(SRAM_Custom),
    REWIND_BLOCK(Z80_State), REWIND_BLOCK(Bank_Z80),
    REWIND_BLOCK(Last_BUS_REQ_Cnt), REWIND_BLOCK(Last_BUS_REQ_St),
    REWIND_BLOCK(Cycles_M68K), REWIND_BLOCK(Cycles_Z80),
    REWIND_BLOCK(VDP_Reg), REWIND_BLOCK(Ctrl), REWIND_BLOCK(VRam),
    REWIND_BLOCK(CRam), REWIND_BLOCK(VSRam), REWIND_BLOCK(VDP_Status),
    REWIND_BLOCK(VDP_Int), REWIND_BLOCK(VDP_Current_Line),
    REWIND_BLOCK(VDP_Num_Lines), REWIND_BLOCK(VDP_Num_Vis_Lines),
    REWIND_BLOCK(DMAT_Lenght), REWIND_BLOCK(DMAT_Tmp), REWIND_BLOCK(DMAT_Type),
    REWIND_BLOCK(CRam_Flag), REWIND_BLOCK(VRam_Flag),
    REWIND_BLOCK(ScrA_Addr), REWIND_BLOCK(ScrB_Addr), REWIND_BLOCK(Win_Addr),
    REWIND_BLOCK(Spr_Addr), REWIND_BLOCK(H_Scroll_Addr), REWIND_BLOCK(H_Cell),
    REWIND_BLOCK(H_Win_Mul), REWIND_BLOCK(H_Pix), REWIND_BLOCK(H_Pix_Begin),
    REWIND_BLOCK(H_Scroll_Mask), REWIND_BLOCK(H_Scroll_CMul),
    REWIND_BLOCK(H_Scroll_CMask), REWIND_BLOCK(V_Scroll_CMask),
    REWIND_BLOCK(V_Scroll_MMask), REWIND_BLOCK(Win_X_Pos), REWIND_BLOCK(Win_Y_Pos),
    REWIND_BLOCK(MD_Screen), REWIND_BLOCK(MD_Palette),
    REWIND_BLOCK(Sprite_Struct), REWIND_BLOCK(Sprite_Visible),
    REWIND_BLOCK(Data_Spr), REWIND_BLOCK(Data_Misc), REWIND_BLOCK(Sprite_Over),
    REWIND_BLOCK(YM2612), REWIND_BLOCK(PSG), REWIND_BLOCK(PSG_Save),
    REWIND_BLOCK(PSG_Noise_Step_Table), REWIND_BLOCK(int_cnt),
    REWIND_BLOCK(in0), REWIND_BLOCK(in1), REWIND_BLOCK(in2), REWIND_BLOCK(in3),
    REWIND_BLOCK(en0), REWIND_BLOCK(en1), REWIND_BLOCK(en2), REWIND_BLOCK(en3),
    REWIND_BLOCK(YM_Buf), REWIND_BLOCK(PSG_Buf),
    REWIND_BLOCK(YM_Len), REWIND_BLOCK(PSG_Len),
    REWIND_BLOCK(Seg_L), REWIND_BLOCK(Seg_R),
    REWIND_BLOCK(Controller_1_State), REWIND_BLOCK(Controller_1_COM),
    REWIND_BLOCK(Controller_1_Counter), REWIND_BLOCK(Controller_1_Delay),
    REWIND_BLOCK(Controller_2_State), REWIND_BLOCK(Controller_2_COM),
    REWIND_BLOCK(Controller_2_Counter), REWIND_BLOCK(Controller_2_Delay),
    REWIND_PAD(1), REWIND_PAD(2), REWIND_PAD(1B), REWIND_PAD(1C),
    REWIND_PAD(1D), REWIND_PAD(2B), REWIND_PAD(2C), REWIND_PAD(2D),
    REWIND_BLOCK(M68K_Read_Byte_Table), REWIND_BLOCK(M68K_Read_Word_Table),
    REWIND_BLOCK(M68K_Write_Byte_Table), REWIND_BLOCK(M68K_Write_Word_Table)
};
static const RewindBlock x32RewindBlocks[] = {
    REWIND_BLOCK(M_SH2), REWIND_BLOCK(S_SH2), REWIND_BLOCK(_32X_Ram),
    REWIND_BLOCK(_MSH2_Reg), REWIND_BLOCK(_SSH2_Reg), REWIND_BLOCK(_SH2_VDP_Reg),
    REWIND_BLOCK(_32X_Comm), REWIND_BLOCK(_32X_ADEN), REWIND_BLOCK(_32X_RES),
    REWIND_BLOCK(_32X_FM), REWIND_BLOCK(_32X_RV),
    REWIND_BLOCK(_32X_DREQ_ST), REWIND_BLOCK(_32X_DREQ_SRC),
    REWIND_BLOCK(_32X_DREQ_DST), REWIND_BLOCK(_32X_DREQ_LEN),
    REWIND_BLOCK(_32X_FIFO_A), REWIND_BLOCK(_32X_FIFO_B),
    REWIND_BLOCK(_32X_FIFO_Block), REWIND_BLOCK(_32X_FIFO_Read), REWIND_BLOCK(_32X_FIFO_Write),
    /* Only the low byte carries the interrupt mask; NASM word padding is not
     * part of the register exposed by the core's C interface. */
    REWIND_BLOCK(_32X_MINT), REWIND_BLOCK(_32X_SINT), REWIND_BLOCK(_32X_HIC),
    REWIND_BLOCK(Bank_SH2), REWIND_BLOCK(Cycles_MSH2), REWIND_BLOCK(Cycles_SSH2),
    REWIND_BLOCK(_32X_VDP), REWIND_BLOCK(_32X_VDP_Ram),
    REWIND_BLOCK(_32X_VDP_CRam), REWIND_BLOCK(_32X_VDP_CRam_Ajusted),
    REWIND_BLOCK(PWM_FIFO_R), REWIND_BLOCK(PWM_FIFO_L),
    REWIND_BLOCK(PWM_RP_R), REWIND_BLOCK(PWM_WP_R), REWIND_BLOCK(PWM_RP_L), REWIND_BLOCK(PWM_WP_L),
    REWIND_BLOCK(PWM_Cycles), REWIND_BLOCK(PWM_Cycle), REWIND_BLOCK(PWM_Cycle_Cnt),
    REWIND_BLOCK(PWM_Int), REWIND_BLOCK(PWM_Int_Cnt), REWIND_BLOCK(PWM_Mode),
    REWIND_BLOCK(PWM_Out_R), REWIND_BLOCK(PWM_Out_L),
    REWIND_BLOCK(PWM_Cycle_Tmp), REWIND_BLOCK(PWM_Int_Tmp),
    REWIND_BLOCK(PWM_FIFO_L_Tmp), REWIND_BLOCK(PWM_FIFO_R_Tmp)
};
#undef REWIND_BLOCK
#undef REWIND_PAD
static unsigned int rewindBlockLength(const RewindBlock *blocks, size_t count)
{
    unsigned int length = 0;
    for (size_t i = 0; i < count; ++i) length += blocks[i].length;
    return length;
}
static unsigned int rewindTransfer(unsigned char *data, const RewindBlock *blocks,
                                   size_t count, bool restore)
{
    unsigned int offset = 0;
    for (size_t i = 0; i < count; ++i) {
        if (restore) memcpy(blocks[i].address, data + offset, blocks[i].length);
        else memcpy(data + offset, blocks[i].address, blocks[i].length);
        offset += blocks[i].length;
    }
    return offset;
}
static const size_t mdRewindCount = sizeof(mdRewindBlocks) / sizeof(mdRewindBlocks[0]);
static const size_t x32RewindCount = sizeof(x32RewindBlocks) / sizeof(x32RewindBlocks[0]);
static const unsigned int rewindHeaderSize = 32;
static void rewindPut32(unsigned char *p, uint32_t n) { memcpy(p, &n, 4); }
static uint32_t rewindGet32(const unsigned char *p) { uint32_t n; memcpy(&n, p, 4); return n; }
}

uint64_t State_RewindSignature(void)
{
    /* Any change that can rebuild synthesis tables, timings, colors or input
     * protocol starts a new history. Game loading/reset is also cleared by
     * the frontend, including loading a different ROM of the same size. */
    const unsigned int config[] = {
        Rom_Size, static_cast<unsigned int>(Cart_Active),
        static_cast<unsigned int>(_32X_Started), static_cast<unsigned int>(CPU_Mode),
        static_cast<unsigned int>(Game_Mode), static_cast<unsigned int>(Gen_Mode),
        static_cast<unsigned int>(Sound_Rate), static_cast<unsigned int>(Seg_Lenght),
        static_cast<unsigned int>(Sound_Stereo), static_cast<unsigned int>(Sound_Enable),
        static_cast<unsigned int>(Sprite_Over), static_cast<unsigned int>(YM2612_Improv),
        static_cast<unsigned int>(YM2612_Enable), static_cast<unsigned int>(PSG_Enable),
        static_cast<unsigned int>(PSG_Improv), static_cast<unsigned int>(DAC_Enable),
        static_cast<unsigned int>(DAC_Improv), static_cast<unsigned int>(YM2612_Enable_SSGEG),
        static_cast<unsigned int>(Mode_555), static_cast<unsigned int>(MSH2_Speed),
        static_cast<unsigned int>(SSH2_Speed), static_cast<unsigned int>(PWM_Enable),
        Controller_1_Type, Controller_2_Type, Controller_1B_Type, Controller_1C_Type,
        Controller_1D_Type, Controller_2B_Type, Controller_2C_Type, Controller_2D_Type,
        static_cast<unsigned int>(RMax_Level), static_cast<unsigned int>(GMax_Level),
        static_cast<unsigned int>(BMax_Level), static_cast<unsigned int>(Contrast_Level),
        static_cast<unsigned int>(Brightness_Level), static_cast<unsigned int>(Greyscale),
        static_cast<unsigned int>(Invert_Color)
    };
    return static_cast<uint64_t>(crc32(0, reinterpret_cast<const Bytef *>(config), sizeof(config))) |
           (static_cast<uint64_t>(State_RewindSize()) << 32);
}

unsigned int State_RewindSize(void)
{
    if (SegaCD_Started || (!Genesis_Started && !_32X_Started)) return 0;
    return rewindHeaderSize + rewindBlockLength(mdRewindBlocks, mdRewindCount) +
        (_32X_Started ? rewindBlockLength(x32RewindBlocks, x32RewindCount) : 0) +
        Cart_MemoryStateSize();
}

int State_CaptureRewind(unsigned char *data, unsigned int length)
{
    if (!data || !length || length != State_RewindSize()) return 0;
    memset(data, 0, rewindHeaderSize);
    memcpy(data, "GKREW002", 8);
    rewindPut32(data + 8, length);
    rewindPut32(data + 12, _32X_Started ? 32 : 16);
    uint64_t signature = State_RewindSignature();
    memcpy(data + 16, &signature, 8);
    main68k_GetContext(&Context_68K);
    unsigned int at = rewindHeaderSize;
    at += rewindTransfer(data + at, mdRewindBlocks, mdRewindCount, false);
    if (_32X_Started) at += rewindTransfer(data + at, x32RewindBlocks, x32RewindCount, false);
    const unsigned int cartLength = Cart_MemoryStateSize();
    rewindPut32(data + 28, cartLength);
    if (!Cart_ExportMemory(data + at, cartLength)) return 0;
    rewindPut32(data + 24, crc32(0, data + rewindHeaderSize, length - rewindHeaderSize));
    return 1;
}

int State_RestoreRewind(const unsigned char *data, unsigned int length)
{
    if (!data || length < rewindHeaderSize || length != State_RewindSize() ||
        memcmp(data, "GKREW002", 8) || rewindGet32(data + 8) != length ||
        rewindGet32(data + 12) != static_cast<unsigned int>(_32X_Started ? 32 : 16)) return 0;
    uint64_t signature; memcpy(&signature, data + 16, 8);
    if (signature != State_RewindSignature() ||
        rewindGet32(data + 24) != crc32(0, data + rewindHeaderSize, length - rewindHeaderSize)) return 0;
    const unsigned int cartLength = Cart_MemoryStateSize();
    if (rewindGet32(data + 28) != cartLength ||
        !Cart_ValidateMemory(data + length - cartLength, cartLength)) return 0;
    /* No core write occurs before every size/signature/checksum/mapper check. */
    unsigned int at = rewindHeaderSize;
    at += rewindTransfer(const_cast<unsigned char *>(data) + at, mdRewindBlocks, mdRewindCount, true);
    if (_32X_Started) rewindTransfer(const_cast<unsigned char *>(data) + at, x32RewindBlocks, x32RewindCount, true);
    Cart_ImportMemory(data + length - cartLength, cartLength);
    if (_32X_Started) {
        M68K_32X_Mode();
        M68K_Set_32X_Rom_Bank();
        _32X_Set_FB();
    }
    main68k_SetContext(&Context_68K);
    Flag_Clr_Scr = 1;
    return 1;
}
