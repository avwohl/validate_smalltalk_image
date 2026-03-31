#include "HeapWalker.hpp"
#include <cstring>

bool HeapWalker::walk(const SpurImage& image, ValidationContext& ctx) {
    const uint8_t* data = image.heapData();
    size_t size = image.heapSize();
    size_t pos = 0;
    int nonFreeIdx = 0;

    while (pos < size) {
        if (ctx.shouldStop()) break;

        // Read header word
        if (pos + 8 > size) {
            ctx.addWarning("heap", "Trailing bytes at end of heap (" +
                std::to_string(size - pos) + " bytes)", pos);
            break;
        }
        uint64_t word;
        memcpy(&word, data + pos, 8);

        // Skip zero words (padding / segment bridges)
        if (word == 0) {
            pos += 8;
            continue;
        }

        HeapObject obj{};
        obj.heapOffset = pos;

        // Check for overflow header
        uint8_t numSlots = headerNumSlots(word);
        size_t headerBytes = HeaderSize;

        if (numSlots == OverflowSlotMarker) {
            // Check if next word also has numSlots=255 → this word is the overflow count
            if (pos + 16 <= size) {
                uint64_t nextWord;
                memcpy(&nextWord, data + pos + 8, 8);
                if (headerNumSlots(nextWord) == OverflowSlotMarker) {
                    // Current = overflow count word, next = real header
                    obj.slotCount = static_cast<size_t>((word << 8) >> 8);
                    obj.header = nextWord;
                    obj.heapOffset = pos + 8;  // header is the next word
                    headerBytes = 16;
                    obj.hasOverflow = true;
                } else {
                    // This word IS the header with 255 slots (unlikely but possible)
                    obj.header = word;
                    obj.slotCount = 255;
                    obj.hasOverflow = false;
                }
            } else {
                obj.header = word;
                obj.slotCount = 255;
                obj.hasOverflow = false;
            }
        } else {
            obj.header = word;
            obj.slotCount = numSlots;
            obj.hasOverflow = false;
        }

        obj.format = headerFormat(obj.header);
        obj.classIndex = headerClassIndex(obj.header);
        obj.isFree = (obj.classIndex == 0);

        // Compute total bytes
        size_t bodyBytes;
        if (image.is64Bit()) {
            bodyBytes = obj.slotCount * 8;
        } else {
            // In Spur32, pointer slots are 4 bytes, but slot count in header
            // refers to 4-byte words for pointer objects and 4-byte words for others.
            // Actually in Spur32, each "slot" is 4 bytes (one 32-bit Oop).
            bodyBytes = obj.slotCount * 4;
        }
        obj.totalBytes = headerBytes + bodyBytes;
        size_t minSize = image.is64Bit() ? MinObjectSize64 : MinObjectSize32;
        if (obj.totalBytes < minSize) obj.totalBytes = minSize;
        obj.totalBytes = (obj.totalBytes + 7) & ~size_t(7);

        // Check fits in heap
        if (pos + obj.totalBytes > size) {
            ctx.addError("heap", "Object at offset 0x" +
                std::to_string(pos) + " extends past heap end (size=" +
                std::to_string(obj.totalBytes) + ", remaining=" +
                std::to_string(size - pos) + ")", pos);
            break;
        }

        // Track object starts (for pointer validation later)
        objectStarts_.insert(obj.heapOffset);

        // Track first 5 non-free objects
        if (!obj.isFree && nonFreeIdx < 5) {
            firstFive_[nonFreeIdx++] = static_cast<int>(objects_.size());
        }

        // Update stats
        auto& stats = ctx.stats();
        if (obj.isFree) {
            stats.freeChunks++;
            stats.freeBytes += obj.totalBytes;
        } else {
            stats.totalObjects++;
            stats.totalBytes += obj.totalBytes;
            if (obj.hasOverflow) stats.overflowObjects++;
            if (headerIsPinned(obj.header)) stats.pinnedObjects++;
            if (headerIsImmutable(obj.header)) stats.immutableObjects++;
            if (obj.slotCount > stats.maxSlotCount) stats.maxSlotCount = obj.slotCount;
            if (obj.totalBytes > stats.maxObjectBytes) stats.maxObjectBytes = obj.totalBytes;
            stats.classHistogram[obj.classIndex]++;

            if (isPointerFormat(obj.format)) stats.pointerObjects++;
            else if (isByteFormat(obj.format)) stats.byteObjects++;
            else if (isMethodFormat(obj.format)) stats.compiledMethods++;
            else if (isWordFormat(obj.format) || isShortFormat(obj.format) || is16bitFormat(obj.format))
                stats.wordObjects++;
            if (obj.format == Fmt_ZeroSized) stats.zeroSizedObjects++;
            if (obj.format == Fmt_Weak || obj.format == Fmt_WeakFixed) stats.weakObjects++;
        }

        offsetToIndex_[obj.heapOffset] = objects_.size();
        objects_.push_back(obj);
        pos += obj.totalBytes;
    }

    return true;
}

const HeapObject* HeapWalker::objectAtOffset(size_t offset) const {
    auto it = offsetToIndex_.find(offset);
    if (it != offsetToIndex_.end()) return &objects_[it->second];
    return nullptr;
}

const HeapObject* HeapWalker::nilObject() const {
    return firstFive_[0] >= 0 ? &objects_[firstFive_[0]] : nullptr;
}
const HeapObject* HeapWalker::falseObject() const {
    return firstFive_[1] >= 0 ? &objects_[firstFive_[1]] : nullptr;
}
const HeapObject* HeapWalker::trueObject() const {
    return firstFive_[2] >= 0 ? &objects_[firstFive_[2]] : nullptr;
}
const HeapObject* HeapWalker::freeListsObject() const {
    return firstFive_[3] >= 0 ? &objects_[firstFive_[3]] : nullptr;
}
const HeapObject* HeapWalker::hiddenRootsObject() const {
    return firstFive_[4] >= 0 ? &objects_[firstFive_[4]] : nullptr;
}
