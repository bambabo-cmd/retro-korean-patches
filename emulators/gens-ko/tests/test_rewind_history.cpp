/* Gens KO 2026-11-20. Exercises production compressed history, no game ROM. */
#include "rewind_history.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <limits>

static unsigned checks;
static void check(bool ok, const char *why) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
static void pattern(std::vector<unsigned char>& data, unsigned frame) {
    unsigned n = 0x9e3779b9U ^ frame;
    for (auto &byte : data) { n ^= n << 13; n ^= n >> 17; n ^= n << 5; byte = n; }
}
int main() {
    GensRewind::History history;
    std::vector<unsigned char> state(192 * 1024), decoded(state.size());
    history.configure(600, 2 * 1024 * 1024);
    check(!history.capture(0, nullptr, state.size()), "null state rejected");
    uint64_t restored = 99;
    check(!history.read(0, decoded.data(), decoded.size(), &restored) && restored == 99,
          "empty history leaves output unchanged");
    for (unsigned frame = 0; frame <= 200; ++frame) {
        pattern(state, frame);
        check(history.capture(frame, state.data(), state.size()), "incompressible frame captured");
        check(history.storedBytes() <= 2 * 1024 * 1024, "strict memory budget under eviction");
        check(history.read(frame, decoded.data(), decoded.size(), &restored) &&
              restored == frame && state == decoded, "latest exact roundtrip after eviction");
    }
    check(history.size() < 12 && history.oldest() > 0, "oldest frames evicted under memory cap");
    const uint64_t oldest = history.oldest(), newest = history.newest();
    check(history.read(0, decoded.data(), decoded.size(), &restored) && restored == oldest,
          "rewind beyond retained time clamps to oldest");
    check(history.newest() == newest, "reading a candidate does not drop its future");
    check(!history.read(10, decoded.data(), decoded.size() - 1, &restored), "size mismatch rejected");
    check(!history.commit(newest + 1) && history.newest() == newest, "unknown commit cannot prune");
    check(history.commit(oldest) && history.size() == 1, "successful branch prunes entire future");
    pattern(state, 98765);
    check(history.capture(oldest + 1, state.data(), state.size()), "resumed branch accepts new frame");
    check(history.read(oldest + 1, decoded.data(), decoded.size(), &restored) && decoded == state,
          "resumed branch reads new state, not abandoned future");
    check(!history.capture(oldest + 1, state.data(), state.size()), "duplicate serial rejected");
    check(!history.capture(oldest, state.data(), state.size()), "out-of-order serial rejected");
    history.clear();
    history.configure(30, 8 * 1024 * 1024);
    std::fill(state.begin(), state.end(), 0);
    for (uint64_t frame = 0; frame <= 300; frame += 5) {
        state[0] = static_cast<unsigned char>(frame);
        check(history.capture(frame, state.data(), state.size()), "sparse frame captured");
        check(history.newest() - history.oldest() <= 30, "duration measured in emulated frames");
    }
    check(history.read(297, decoded.data(), decoded.size(), &restored) && restored == 295,
          "sparse fast-forward rewind selects nearest prior frame");
    check(decoded[0] == static_cast<unsigned char>(295), "sparse frame restored contents");
    history.clear();
    history.configure(60, 128);
    check(!history.capture(1, state.data(), state.size()) && history.size() == 0,
          "single record larger than memory cap is rejected");
    history.configure(0, 1024 * 1024);
    check(!history.capture(1, state.data(), state.size()), "zero duration disables capture");
    history.configure(600, 1024 * 1024);
    const uint64_t high = std::numeric_limits<uint64_t>::max() - 1;
    check(history.capture(high, state.data(), state.size()), "64-bit frame serial accepted");
    check(history.capture(high + 1, state.data(), state.size()), "64-bit final serial accepted");
    check(history.read(high, decoded.data(), decoded.size(), &restored) && restored == high,
          "64-bit selection does not truncate frame serial");
    history.release();
    check(history.size() == 0 && history.bytes() == 0, "release frees history and workspace");
    std::printf("PASS: %u rewind history checks\n", checks);
}
