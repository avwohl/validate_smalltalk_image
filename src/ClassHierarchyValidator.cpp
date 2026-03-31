#include "ClassHierarchyValidator.hpp"
#include <unordered_set>

uint64_t ClassHierarchyValidator::readSlot(size_t objOffset, size_t slotIndex) const {
    size_t oopSize = image_.oopSize();
    size_t slotOff = objOffset + HeaderSize + slotIndex * oopSize;
    return image_.oopAt(slotOff);
}

void ClassHierarchyValidator::validate(ValidationContext& ctx) {
    for (auto& [classIdx, classOffset] : classTable_.classMap()) {
        if (ctx.shouldStop()) break;
        validateClass(classIdx, classOffset, ctx);
    }
}

void ClassHierarchyValidator::validateClass(uint32_t classIndex, size_t classOffset,
                                             ValidationContext& ctx) {
    const HeapObject* obj = walker_.objectAtOffset(classOffset);
    if (!obj) return;

    // Class should be a pointer object
    if (!isPointerFormat(obj->format)) {
        ctx.addError("classHierarchy", "Class index " + std::to_string(classIndex) +
            " has non-pointer format: " + std::to_string(obj->format), classOffset);
        return;
    }

    // Regular class: >= 7 slots. Metaclass: 6 slots.
    // We accept anything >= 3 slots (superclass, methodDict, instSpec minimum)
    if (obj->slotCount < 3) {
        ctx.addError("classHierarchy", "Class index " + std::to_string(classIndex) +
            " has too few slots: " + std::to_string(obj->slotCount), classOffset);
        return;
    }

    // Slot 0: superclass
    checkSuperclassChain(classIndex, classOffset, ctx);

    // Slot 1: method dictionary
    if (obj->slotCount >= 2) {
        checkMethodDict(classOffset, classIndex, ctx);
    }

    // Slot 2: instance specification (SmallInteger)
    if (obj->slotCount >= 3) {
        checkInstSpec(classOffset, classIndex, ctx);
    }

    // Slot 6: name (if present — regular class with >= 7 slots)
    if (obj->slotCount >= 7) {
        checkClassName(classOffset, classIndex, ctx);
    }
}

void ClassHierarchyValidator::checkSuperclassChain(uint32_t classIndex, size_t classOffset,
                                                    ValidationContext& ctx) {
    uint64_t nilAddr = image_.startOfMemory();
    std::unordered_set<uint64_t> visited;
    uint64_t currentOop = image_.startOfMemory() + classOffset;
    int depth = 0;
    static constexpr int MaxDepth = 200;

    while (depth < MaxDepth) {
        if (visited.count(currentOop)) {
            ctx.addError("classHierarchy", "Cycle in superclass chain for class index " +
                std::to_string(classIndex), classOffset);
            return;
        }
        visited.insert(currentOop);

        size_t offset = image_.rawOopToOffset(currentOop);
        if (offset == SIZE_MAX) break;

        const HeapObject* obj = walker_.objectAtOffset(offset);
        if (!obj || obj->slotCount < 1) break;

        uint64_t superOop = readSlot(offset, 0);

        // nil superclass = root (ProtoObject)
        if (superOop == nilAddr || superOop == 0) break;

        // Check superclass is a valid pointer
        OopKind kind = oopVal_.classify(superOop);
        if (kind != OopKind::ObjectPointer && kind != OopKind::Nil) {
            ctx.addError("classHierarchy", "Class index " + std::to_string(classIndex) +
                " has invalid superclass Oop: 0x" + std::to_string(superOop), classOffset);
            return;
        }

        currentOop = superOop;
        depth++;
    }

    if (depth >= MaxDepth) {
        ctx.addWarning("classHierarchy", "Superclass chain depth >= " +
            std::to_string(MaxDepth) + " for class index " +
            std::to_string(classIndex), classOffset);
    }
}

void ClassHierarchyValidator::checkMethodDict(size_t classOffset, uint32_t classIndex,
                                               ValidationContext& ctx) {
    uint64_t mdOop = readSlot(classOffset, 1);
    uint64_t nilAddr = image_.startOfMemory();

    // nil method dict is suspicious but allowed (some metaclasses)
    if (mdOop == nilAddr || mdOop == 0) return;

    OopKind kind = oopVal_.classify(mdOop);
    if (kind != OopKind::ObjectPointer) {
        ctx.addError("classHierarchy", "Class index " + std::to_string(classIndex) +
            " has non-pointer method dictionary", classOffset);
        return;
    }

    size_t mdOffset = image_.rawOopToOffset(mdOop);
    if (mdOffset == SIZE_MAX) return;

    const HeapObject* mdObj = walker_.objectAtOffset(mdOffset);
    if (!mdObj) {
        ctx.addError("classHierarchy", "Class index " + std::to_string(classIndex) +
            " method dict points to non-object", classOffset);
        return;
    }

    // Method dict should be a pointer object
    if (!isPointerFormat(mdObj->format)) {
        ctx.addWarning("classHierarchy", "Class index " + std::to_string(classIndex) +
            " method dict has non-pointer format: " + std::to_string(mdObj->format), classOffset);
    }

    // Should have at least 2 slots (tally + array)
    if (mdObj->slotCount < 2) {
        ctx.addWarning("classHierarchy", "Class index " + std::to_string(classIndex) +
            " method dict has fewer than 2 slots", classOffset);
    }
}

void ClassHierarchyValidator::checkInstSpec(size_t classOffset, uint32_t classIndex,
                                             ValidationContext& ctx) {
    uint64_t instSpec = readSlot(classOffset, 2);

    // Must be a SmallInteger
    if (image_.is64Bit()) {
        // Check tag bits
        bool isInt;
        if (true) {  // both standard and iOS use tag 001 for SmallInteger
            isInt = (instSpec & 0x7) == 1 || instSpec == 0;
        }
        if (!isInt && instSpec != image_.startOfMemory()) {
            ctx.addWarning("classHierarchy", "Class index " + std::to_string(classIndex) +
                " instSpec is not a SmallInteger: 0x" + std::to_string(instSpec), classOffset);
        }
    }
}

void ClassHierarchyValidator::checkClassName(size_t classOffset, uint32_t classIndex,
                                              ValidationContext& ctx) {
    uint64_t nameOop = readSlot(classOffset, 6);
    uint64_t nilAddr = image_.startOfMemory();

    if (nameOop == nilAddr || nameOop == 0) {
        // Metaclasses can have nil name
        return;
    }

    OopKind kind = oopVal_.classify(nameOop);
    if (kind != OopKind::ObjectPointer) return;  // immediate names don't make sense but skip

    size_t nameOffset = image_.rawOopToOffset(nameOop);
    if (nameOffset == SIZE_MAX) return;

    const HeapObject* nameObj = walker_.objectAtOffset(nameOffset);
    if (!nameObj) return;

    // Name should be a byte object (format 16-23) or a Symbol with fixed+indexable
    // layout (format 3, IndexableWithFixed). Pharo Symbols use format 3.
    if (!isByteFormat(nameObj->format) && nameObj->format != Fmt_IndexableFixed) {
        ctx.addWarning("classHierarchy", "Class index " + std::to_string(classIndex) +
            " name has unexpected format " + std::to_string(nameObj->format) +
            " (expected byte or IndexableFixed)", classOffset);
    }
}
