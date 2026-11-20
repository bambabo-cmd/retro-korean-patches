/* Gens KO 2026-11-20. GPL-2.0; see original LICENSE.txt. */
#ifndef GENS_REWIND_HISTORY_H
#define GENS_REWIND_HISTORY_H

#include <stddef.h>
#include <stdint.h>
#include <list>
#include <memory>
#include <vector>
#include <cstring>
#include <zlib.h>

namespace GensRewind {

/* Independently compressed snapshots make eviction and timeline branching
 * deterministic: no snapshot depends on an already evicted delta/keyframe.
 * The caller reserves its raw snapshot and compression workspace separately. */
class History {
    struct Record {
        uint64_t frame;
        uint32_t crc;
        size_t rawSize, compressedSize;
        std::unique_ptr<unsigned char[]> compressed;
        size_t charge() const { return compressedSize + sizeof(Record) + 96; }
    };
    std::list<Record> records_;
    std::vector<unsigned char> compression_;
    size_t used_ = 0, limit_ = 0;
    uint64_t duration_ = 0;

    void popFront() { used_ -= records_.front().charge(); records_.pop_front(); }
    void popBack() { used_ -= records_.back().charge(); records_.pop_back(); }
public:
    void clear() { records_.clear(); used_ = 0; }
    void release() { clear(); std::vector<unsigned char>().swap(compression_); }
    void configure(uint64_t durationFrames, size_t bytes) {
        duration_ = durationFrames;
        limit_ = bytes;
        while (!records_.empty() && used_ > limit_) popFront();
        if (!duration_) clear();
    }
    size_t size() const { return records_.size(); }
    size_t bytes() const { return used_ + compression_.capacity(); }
    size_t storedBytes() const { return used_; }
    size_t workspaceBytes() const { return compression_.capacity(); }
    uint64_t oldest() const { return records_.empty() ? 0 : records_.front().frame; }
    uint64_t newest() const { return records_.empty() ? 0 : records_.back().frame; }

    bool capture(uint64_t frame, const unsigned char *raw, size_t length) {
        if (!raw || !length || !limit_ || !duration_ || length > 16 * 1024 * 1024 ||
            (!records_.empty() && frame <= records_.back().frame)) return false;
        uLongf capacity = compressBound(static_cast<uLong>(length));
        compression_.resize(capacity);
        if (compress2(compression_.data(), &capacity, raw, static_cast<uLong>(length),
                      Z_BEST_SPEED) != Z_OK) return false;
        Record entry;
        entry.frame = frame;
        entry.crc = static_cast<uint32_t>(crc32(0, raw, static_cast<uInt>(length)));
        entry.rawSize = length;
        entry.compressedSize = capacity;
        if (entry.charge() > limit_) return false;
        while (!records_.empty() && (used_ > limit_ - entry.charge() ||
               frame - records_.front().frame > duration_)) popFront();
        entry.compressed.reset(new unsigned char[capacity]);
        std::memcpy(entry.compressed.get(), compression_.data(), capacity);
        const size_t charge = entry.charge();
        records_.push_back(std::move(entry));
        used_ += charge;
        return true;
    }

    /* Reading is deliberately non-destructive. Validate/apply the decoded core
     * state first, then call commit() to discard its abandoned future. */
    bool read(uint64_t target, unsigned char *raw, size_t length, uint64_t *frame) const {
        if (!raw || !frame || records_.empty()) return false;
        auto selected = records_.cbegin();
        for (auto it = records_.cbegin(); it != records_.cend(); ++it) {
            if (it->frame > target) break;
            selected = it;
        }
        if (length != selected->rawSize) return false;
        uLongf actual = static_cast<uLongf>(length);
        if (uncompress(raw, &actual, selected->compressed.get(),
                       static_cast<uLong>(selected->compressedSize)) != Z_OK ||
            actual != length ||
            crc32(0, raw, static_cast<uInt>(length)) != selected->crc) return false;
        *frame = selected->frame;
        return true;
    }
    bool commit(uint64_t frame) {
        bool present = false;
        for (const auto &entry : records_) if (entry.frame == frame) { present = true; break; }
        if (!present) return false;
        while (!records_.empty() && records_.back().frame > frame) popBack();
        return true;
    }
};
}
#endif
