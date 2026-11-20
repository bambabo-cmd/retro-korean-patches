/* Gens KO 2026-11-20. GPL-2.0; see original LICENSE.txt. */
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <new>
#include <vector>
#include "rewind_engine.h"
#include "rewind_history.h"
#include "save.h"
#include "g_main.h"
#include "g_dsound.h"
#include "vdp_io.h"
#include "mem_m68k.h"
#include "wave.h"

namespace {
GensRewind::History history;
std::vector<unsigned char> raw;
bool enabled = true, allocationFailed = false, invalidSnapshot = false;
int seconds = 30, memoryMiB = 128;
uint64_t signature = 0;
/* zlib's deflate workspace and STL node/allocator overhead are included in a
 * conservative fixed reserve. Each history node also charges 96 extra bytes. */
const size_t workspaceReserve = 1024 * 1024;
int bounded(int n, int low, int high) { return std::max(low, std::min(high, n)); }

bool prepare()
{
    const unsigned int length = State_RewindSize();
    const uint64_t current = State_RewindSignature();
    if (!length) return false;
    if (signature != current || raw.size() != length) {
        history.release();
        std::vector<unsigned char>(length).swap(raw);
        signature = current;
    }
    const size_t total = static_cast<size_t>(memoryMiB) * 1024 * 1024;
    const size_t reserve = raw.capacity() + compressBound(length) + workspaceReserve;
    if (reserve >= total) return false;
    history.configure(static_cast<uint64_t>(seconds) * (CPU_Mode ? 50 : 60), total - reserve);
    return true;
}
}

void Rewind_Configure(bool use, int duration, int memory)
{
    duration = bounded(duration, 1, 120);
    memory = bounded(memory, 16, 512);
    if (use == enabled && duration == seconds && memory == memoryMiB) return;
    enabled = use; seconds = duration; memoryMiB = memory;
    Rewind_Clear();
}

void Rewind_Clear()
{
    history.release();
    std::vector<unsigned char>().swap(raw);
    signature = 0;
    allocationFailed = invalidSnapshot = false;
}

bool Rewind_CanUse()
{
    return enabled && !allocationFailed && !invalidSnapshot && !Net_Play &&
        !SegaCD_Started && (Genesis_Started || _32X_Started) &&
        !WAV_Dumping && !GYM_Dumping && !GYM_Playing;
}

const wchar_t *Rewind_Reason()
{
    if (!enabled) return L"되감기가 설정에서 꺼져 있습니다.";
    if (Net_Play) return L"넷플레이에서는 동기화를 위해 되감기를 사용할 수 없습니다.";
    if (SegaCD_Started) return L"메가 CD 되감기는 아직 지원하지 않습니다. FF와 슬로우모션은 사용할 수 있습니다.";
    if (WAV_Dumping || GYM_Dumping || GYM_Playing) return L"오디오 기록 또는 GYM 재생 중에는 되감기를 사용할 수 없습니다.";
    if (allocationFailed) return L"되감기 메모리를 확보하지 못했습니다. 되감기를 껐다 켜거나 게임을 다시 실행해 주세요.";
    if (invalidSnapshot) return L"되감기 기록 검증에 실패했습니다. 게임 상태는 변경하지 않았습니다.";
    if (!Genesis_Started && !_32X_Started) return L"먼저 메가 드라이브 또는 32X 게임을 실행해 주세요.";
    return L"되감을 기록이 아직 없습니다.";
}

bool Rewind_Capture(uint64_t frame)
{
    if (!Rewind_CanUse()) {
        if (!history.size()) return false;
        history.release(); std::vector<unsigned char>().swap(raw); signature = 0;
        return false;
    }
    try {
        if (!prepare() || !State_CaptureRewind(raw.data(), static_cast<unsigned int>(raw.size()))) return false;
        return history.capture(frame, raw.data(), raw.size());
    } catch (const std::bad_alloc &) {
        Rewind_Clear(); allocationFailed = true; return false;
    }
}

bool Rewind_Step(uint64_t targetFrame, uint64_t *restoredFrame)
{
    if (!restoredFrame || !Rewind_CanUse() || !history.size()) return false;
    if (signature != State_RewindSignature()) { Rewind_Clear(); return false; }
    uint64_t candidate = 0;
    if (!history.read(targetFrame, raw.data(), raw.size(), &candidate)) {
        Rewind_Clear(); invalidSnapshot = true; return false;
    }
    if (candidate >= *restoredFrame) return false;
    if (!State_RestoreRewind(raw.data(), static_cast<unsigned int>(raw.size()))) {
        Rewind_Clear(); invalidSnapshot = true; return false;
    }
    /* Validation and restoration have succeeded; this is now the active
     * timeline. Capturing resumed play cannot accidentally retain its future. */
    history.commit(candidate);
    *restoredFrame = candidate;
    return true;
}

bool Rewind_HasHistory() { return Rewind_CanUse() && history.size() != 0; }
size_t Rewind_MemoryBytes() { return history.bytes() + raw.capacity(); }
uint64_t Rewind_OldestFrame() { return history.oldest(); }
uint64_t Rewind_NewestFrame() { return history.newest(); }
