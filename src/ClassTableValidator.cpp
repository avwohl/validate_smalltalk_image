#include "ClassTableValidator.hpp"
#include <cstring>

void ClassTableValidator::validate(ValidationContext& ctx) {
    const HeapObject* hiddenRoots = walker_.hiddenRootsObject();
    if (!hiddenRoots) {
        ctx.addError("classTable", "Cannot find hiddenRootsObj (5th non-free heap object)");
        return;
    }

    // hiddenRootsObj is typically format 2 (Array) in standard Pharo or format 9 (64-bit indexable)
    if (hiddenRoots->format != Fmt_Indexable && hiddenRoots->format != Fmt_Indexable64) {
        ctx.addWarning("classTable", "hiddenRootsObj has unexpected format: " +
            std::to_string(hiddenRoots->format) + " (expected 2 or 9)");
    }

    if (hiddenRoots->slotCount < MaxClassTablePages) {
        ctx.addError("classTable", "hiddenRootsObj has too few slots: " +
            std::to_string(hiddenRoots->slotCount) + " (expected >= " +
            std::to_string(MaxClassTablePages) + ")");
        return;
    }

    validateHiddenRoots(hiddenRoots, ctx);
    checkAllObjectsHaveClasses(ctx);

    ctx.stats().classTableEntries = classMap_.size();
    ctx.stats().uniqueClassIndices = classMap_.size();
}

void ClassTableValidator::validateHiddenRoots(const HeapObject* hiddenRoots, ValidationContext& ctx) {
    size_t dataStart = hiddenRoots->heapOffset + HeaderSize;
    size_t oopSize = image_.oopSize();
    uint64_t nilAddr = image_.startOfMemory();  // nil is at base

    size_t pageCount = 0;
    size_t rootOopSize = image_.oopSize();  // format 2 uses Oop-sized slots
    if (hiddenRoots->format == Fmt_Indexable64) rootOopSize = 8;  // format 9 uses 8-byte slots
    for (size_t i = 0; i < MaxClassTablePages && i < hiddenRoots->slotCount; i++) {
        if (ctx.shouldStop()) break;
        size_t slotOff = dataStart + i * rootOopSize;
        uint64_t pageOop = (rootOopSize == 8) ? image_.wordAt(slotOff) : image_.oopAt(slotOff);

        // nil page = empty
        if (pageOop == nilAddr || pageOop == 0) continue;

        pageCount++;
        validateClassTablePage(pageOop, i, ctx);
    }

    // Validate extra hidden root slots (4096..4103)
    for (size_t i = MaxClassTablePages; i < hiddenRoots->slotCount && i < MaxClassTablePages + 8; i++) {
        size_t slotOff = dataStart + i * rootOopSize;
        uint64_t rootOop = image_.wordAt(slotOff);
        if (rootOop != nilAddr && rootOop != 0) {
            oopVal_.validateOop(rootOop, "hiddenRoot[" + std::to_string(i) + "]",
                                image_.startOfMemory() + hiddenRoots->heapOffset, ctx);
        }
    }

    if (ctx.verbose()) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Class table: %zu pages, %zu classes",
                 pageCount, classMap_.size());
        ctx.addInfo("classTable", buf);
    }
}

void ClassTableValidator::validateClassTablePage(uint64_t pageOop, size_t pageIndex,
                                                  ValidationContext& ctx) {
    size_t pageOffset = image_.rawOopToOffset(pageOop);
    if (pageOffset == SIZE_MAX) {
        ctx.addError("classTable", "Class table page " + std::to_string(pageIndex) +
            " points outside heap: 0x" + std::to_string(pageOop));
        return;
    }

    const HeapObject* pageObj = walker_.objectAtOffset(pageOffset);
    if (!pageObj) {
        ctx.addError("classTable", "Class table page " + std::to_string(pageIndex) +
            " does not point to an object start");
        return;
    }

    // Page should be a pointer object or 64-bit indexable
    // In practice it's format 2 (Array) with up to 1024 pointer slots
    if (!isPointerFormat(pageObj->format) && pageObj->format != Fmt_Indexable64) {
        ctx.addWarning("classTable", "Class table page " + std::to_string(pageIndex) +
            " has unexpected format: " + std::to_string(pageObj->format));
    }

    uint64_t nilAddr = image_.startOfMemory();
    size_t oopSize = image_.oopSize();
    size_t dataStart = pageObj->heapOffset + HeaderSize;
    size_t slotsToCheck = pageObj->slotCount;
    if (slotsToCheck > ClassTablePageSize) slotsToCheck = ClassTablePageSize;

    for (size_t j = 0; j < slotsToCheck; j++) {
        size_t slotOff = dataStart + j * oopSize;
        uint64_t classOop = image_.oopAt(slotOff);

        if (classOop == nilAddr || classOop == 0) continue;

        uint32_t classIdx = static_cast<uint32_t>(pageIndex * ClassTablePageSize + j);

        // Validate the class Oop
        if (!image_.isInHeapRange(classOop)) {
            ctx.addError("classTable", "Class index " + std::to_string(classIdx) +
                " points outside heap: 0x" + std::to_string(classOop));
            continue;
        }

        size_t classOffset = image_.rawOopToOffset(classOop);
        if (!walker_.objectStarts().count(classOffset)) {
            ctx.addError("classTable", "Class index " + std::to_string(classIdx) +
                " does not point to an object start");
            continue;
        }

        classMap_[classIdx] = classOffset;
    }
}

void ClassTableValidator::checkAllObjectsHaveClasses(ValidationContext& ctx) {
    size_t missingCount = 0;
    for (auto& obj : walker_.objects()) {
        if (obj.isFree) continue;
        if (obj.classIndex == 0) continue;  // shouldn't happen for non-free, but skip
        // Class indices 1-31 are reserved for immediate/internal types in Spur.
        // These don't have entries in the class table pages — they're resolved
        // via the special objects array (SmallInteger=5, Character=19, SmallFloat=4, etc.)
        if (obj.classIndex < 32) continue;
        if (!classMap_.count(obj.classIndex)) {
            missingCount++;
            if (missingCount <= 10) {
                ctx.addError("classTable", "Object references class index " +
                    std::to_string(obj.classIndex) +
                    " which is not in the class table", obj.heapOffset);
            }
        }
    }
    if (missingCount > 10) {
        ctx.addError("classTable", "... and " + std::to_string(missingCount - 10) +
            " more objects referencing missing class indices");
    }
}
