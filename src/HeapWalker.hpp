#pragma once
#include "SpurImage.hpp"
#include "SpurHeader.hpp"
#include "ValidationContext.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

// Represents a single heap object found during the walk
struct HeapObject {
    size_t   heapOffset;      // byte offset from start of heap to this object's header
    uint64_t header;          // the 64-bit header word
    size_t   slotCount;       // resolved slot count (handles overflow)
    uint8_t  format;
    uint32_t classIndex;
    size_t   totalBytes;      // total size including header(s), aligned
    bool     hasOverflow;     // uses overflow slot count
    bool     isFree;          // classIndex == 0 (free chunk)
};

class HeapWalker {
public:
    // Walk the entire heap, populating the objects list.
    // Returns false if the heap is too corrupted to walk.
    bool walk(const SpurImage& image, ValidationContext& ctx);

    const std::vector<HeapObject>& objects() const { return objects_; }
    const std::unordered_set<size_t>& objectStarts() const { return objectStarts_; }

    // Find an object by its heap offset, or nullptr if not found
    const HeapObject* objectAtOffset(size_t offset) const;

    // The first 5 objects in a Spur heap are: nil, false, true, freeListsObj, hiddenRootsObj
    const HeapObject* nilObject() const;
    const HeapObject* falseObject() const;
    const HeapObject* trueObject() const;
    const HeapObject* freeListsObject() const;
    const HeapObject* hiddenRootsObject() const;

private:
    std::vector<HeapObject> objects_;
    std::unordered_set<size_t> objectStarts_;  // set of all valid object start offsets
    std::unordered_map<size_t, size_t> offsetToIndex_;  // heapOffset -> index in objects_
    // indices into objects_ for the first 5 objects (non-free)
    int firstFive_[5] = {-1, -1, -1, -1, -1};
};
