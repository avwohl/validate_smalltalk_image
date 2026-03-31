#include "ReachabilityAnalyzer.hpp"
#include <queue>

void ReachabilityAnalyzer::analyze(ValidationContext& ctx) {
    // BFS from all roots: special objects, class table entries, hidden roots
    std::queue<uint64_t> worklist;

    // Root 1: special objects array and its contents
    uint64_t specObjOop = image_.specialObjectsOop();
    worklist.push(specObjOop);

    // Root 2: all class table entries
    for (auto& [classIdx, classOffset] : classTable_.classMap()) {
        uint64_t classOop = image_.startOfMemory() + classOffset;
        worklist.push(classOop);
    }

    // Root 3: hidden roots object itself
    const HeapObject* hr = walker_.hiddenRootsObject();
    if (hr) {
        worklist.push(image_.startOfMemory() + hr->heapOffset);
    }

    // Root 4: first 3 objects (nil, false, true)
    const HeapObject* nilObj = walker_.nilObject();
    const HeapObject* falseObj = walker_.falseObject();
    const HeapObject* trueObj = walker_.trueObject();
    if (nilObj) worklist.push(image_.startOfMemory() + nilObj->heapOffset);
    if (falseObj) worklist.push(image_.startOfMemory() + falseObj->heapOffset);
    if (trueObj) worklist.push(image_.startOfMemory() + trueObj->heapOffset);

    // BFS
    while (!worklist.empty()) {
        uint64_t rawOop = worklist.front();
        worklist.pop();

        OopKind kind = oopVal_.classify(rawOop);
        if (kind != OopKind::ObjectPointer) continue;

        size_t offset = image_.rawOopToOffset(rawOop);
        if (offset == SIZE_MAX) continue;

        // Already visited?
        if (marked_.count(offset)) continue;
        marked_.insert(offset);

        const HeapObject* obj = walker_.objectAtOffset(offset);
        if (!obj || obj->isFree) continue;

        // Enqueue pointer slots
        if (isPointerFormat(obj->format) || isMethodFormat(obj->format)) {
            size_t oopSize = image_.oopSize();
            size_t dataStart = obj->heapOffset + HeaderSize;

            size_t slotsToScan = obj->slotCount;
            if (isMethodFormat(obj->format)) {
                // For methods, only scan the literal frame (slot 0 + numLiterals)
                uint64_t methodHeader = image_.oopAt(dataStart);
                bool isSmallInt = image_.is64Bit() ? ((methodHeader & 0x7) == 1) : ((methodHeader & 1) == 1);
                if (isSmallInt) {
                    int64_t hdrVal = image_.is64Bit()
                        ? (static_cast<int64_t>(methodHeader) >> 3)
                        : (static_cast<int32_t>(static_cast<uint32_t>(methodHeader)) >> 1);
                    size_t numLiterals = hdrVal & 0x7FFF;
                    slotsToScan = 1 + numLiterals;
                    if (slotsToScan > obj->slotCount) slotsToScan = obj->slotCount;
                }
            }

            for (size_t i = 0; i < slotsToScan; i++) {
                uint64_t slotVal = image_.oopAt(dataStart + i * oopSize);
                OopKind sk = oopVal_.classify(slotVal);
                if (sk == OopKind::ObjectPointer) {
                    size_t so = image_.rawOopToOffset(slotVal);
                    if (so != SIZE_MAX && !marked_.count(so)) {
                        worklist.push(slotVal);
                    }
                }
            }
        }

        // For format 9 (64-bit indexable, used by hiddenRoots), also scan slots as pointers
        if (obj->format == Fmt_Indexable64) {
            size_t dataStart = obj->heapOffset + HeaderSize;
            for (size_t i = 0; i < obj->slotCount; i++) {
                uint64_t val = image_.wordAt(dataStart + i * 8);
                if (val != 0 && image_.isInHeapRange(val) && val % 8 == 0) {
                    size_t so = image_.rawOopToOffset(val);
                    if (so != SIZE_MAX && !marked_.count(so)) {
                        worklist.push(val);
                    }
                }
            }
        }
    }

    // Count unreachable
    size_t unreachable = 0;
    size_t unreachableBytes = 0;
    for (auto& obj : walker_.objects()) {
        if (obj.isFree) continue;
        if (!marked_.count(obj.heapOffset)) {
            unreachable++;
            unreachableBytes += obj.totalBytes;
        }
    }

    if (unreachable > 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%zu unreachable objects (%zu bytes, %.1f%% of heap)",
                 unreachable, unreachableBytes,
                 100.0 * unreachableBytes / (ctx.stats().totalBytes + ctx.stats().freeBytes));
        ctx.addWarning("reachability", buf);
    }
}

size_t ReachabilityAnalyzer::unreachableCount() const {
    size_t count = 0;
    for (auto& obj : walker_.objects()) {
        if (obj.isFree) continue;
        if (!marked_.count(obj.heapOffset)) count++;
    }
    return count;
}
