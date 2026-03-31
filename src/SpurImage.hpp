#pragma once
#include "SpurHeader.hpp"
#include "ValidationContext.hpp"
#include <string>
#include <vector>
#include <cstdint>

class SpurImage {
public:
    bool load(const std::string& path, ValidationContext& ctx);

    const SpurImageHeader& header() const { return header_; }
    const uint8_t* heapData() const { return heap_.data(); }
    size_t heapSize() const { return heap_.size(); }
    uint64_t startOfMemory() const { return header_.startOfMemory; }
    uint64_t specialObjectsOop() const { return header_.specialObjectsOop; }

    bool is64Bit() const { return is64_; }
    bool is32Bit() const { return !is64_; }
    size_t oopSize() const { return is64_ ? 8 : 4; }

    // Read a 64-bit word at a heap byte offset
    uint64_t wordAt(size_t offset) const;

    // Read a 32-bit word at a heap byte offset
    uint32_t word32At(size_t offset) const;

    // Read an Oop-sized value at a heap byte offset (64-bit in Spur64, 32-bit in Spur32)
    uint64_t oopAt(size_t offset) const;

    // Check if a raw oop value (as stored in the image) falls within the heap address range
    bool isInHeapRange(uint64_t rawOop) const;

    // Convert a raw oop to a heap byte offset, or return SIZE_MAX if out of range
    size_t rawOopToOffset(uint64_t rawOop) const;

private:
    SpurImageHeader header_{};
    std::vector<uint8_t> heap_;
    bool is64_ = true;
};
