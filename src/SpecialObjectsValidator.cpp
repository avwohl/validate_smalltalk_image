#include "SpecialObjectsValidator.hpp"

uint64_t SpecialObjectsValidator::readSlot(size_t objOffset, size_t slotIndex) const {
    size_t oopSize = image_.oopSize();
    return image_.oopAt(objOffset + HeaderSize + slotIndex * oopSize);
}

void SpecialObjectsValidator::validate(ValidationContext& ctx) {
    uint64_t specObjOop = image_.specialObjectsOop();

    // Validate the special objects Oop itself
    size_t specOffset = image_.rawOopToOffset(specObjOop);
    if (specOffset == SIZE_MAX) {
        ctx.addError("specialObjects", "Special objects array Oop 0x" +
            std::to_string(specObjOop) + " is outside heap range");
        return;
    }

    const HeapObject* specObj = walker_.objectAtOffset(specOffset);
    if (!specObj) {
        ctx.addError("specialObjects", "Special objects Oop does not point to an object start");
        return;
    }

    // Must be a pointer object (Array-like)
    if (!isPointerFormat(specObj->format)) {
        ctx.addError("specialObjects", "Special objects array has non-pointer format: " +
            std::to_string(specObj->format), specOffset);
        return;
    }

    // Should have at least 60 entries
    if (specObj->slotCount < SOI_MinSpecialObjects) {
        ctx.addError("specialObjects", "Special objects array has only " +
            std::to_string(specObj->slotCount) + " slots (expected >= " +
            std::to_string(SOI_MinSpecialObjects) + ")", specOffset);
    }

    uint64_t nilAddr = image_.startOfMemory();

    // Validate all slots as valid Oops
    for (size_t i = 0; i < specObj->slotCount; i++) {
        uint64_t slotVal = readSlot(specOffset, i);
        oopVal_.validateOop(slotVal, "specialObjects[" + std::to_string(i) + "]",
                            image_.startOfMemory() + specOffset, ctx);
    }

    // Check specific critical entries
    auto checkIsObject = [&](size_t idx, const char* name) {
        if (idx >= specObj->slotCount) return;
        uint64_t val = readSlot(specOffset, idx);
        OopKind kind = oopVal_.classify(val);
        if (kind != OopKind::ObjectPointer && kind != OopKind::Nil) {
            ctx.addError("specialObjects", std::string(name) +
                " (index " + std::to_string(idx) + ") is not a heap object: 0x" +
                std::to_string(val), specOffset);
        }
    };

    // nil should be the first object in the heap
    if (specObj->slotCount > SOI_Nil) {
        uint64_t nilOop = readSlot(specOffset, SOI_Nil);
        if (nilOop != nilAddr && nilOop != 0) {
            const HeapObject* nilObj = walker_.nilObject();
            uint64_t expectedNil = nilObj ? (image_.startOfMemory() + nilObj->heapOffset) : nilAddr;
            if (nilOop != expectedNil) {
                ctx.addWarning("specialObjects", "nil (index 0) does not point to first heap object");
            }
        }
    }

    // false and true should be the 2nd and 3rd heap objects
    if (specObj->slotCount > SOI_False) {
        uint64_t falseOop = readSlot(specOffset, SOI_False);
        const HeapObject* falseObj = walker_.falseObject();
        if (falseObj) {
            uint64_t expected = image_.startOfMemory() + falseObj->heapOffset;
            if (falseOop != expected) {
                ctx.addWarning("specialObjects", "false (index 1) does not point to 2nd heap object");
            }
        }
    }

    if (specObj->slotCount > SOI_True) {
        uint64_t trueOop = readSlot(specOffset, SOI_True);
        const HeapObject* trueObj = walker_.trueObject();
        if (trueObj) {
            uint64_t expected = image_.startOfMemory() + trueObj->heapOffset;
            if (trueOop != expected) {
                ctx.addWarning("specialObjects", "true (index 2) does not point to 3rd heap object");
            }
        }
    }

    // Check critical class entries exist and are objects
    checkIsObject(SOI_SchedulerAssociation, "SchedulerAssociation");
    checkIsObject(SOI_ClassSmallInteger, "ClassSmallInteger");
    checkIsObject(SOI_ClassByteString, "ClassByteString");
    checkIsObject(SOI_ClassArray, "ClassArray");
    checkIsObject(SOI_ClassFloat, "ClassFloat");
    checkIsObject(SOI_ClassMethodContext, "ClassMethodContext");
    checkIsObject(SOI_ClassBlockClosure, "ClassBlockClosure");
    checkIsObject(SOI_ClassPoint, "ClassPoint");
    checkIsObject(SOI_ClassLargePositiveInteger, "ClassLargePositiveInteger");
    checkIsObject(SOI_ClassMessage, "ClassMessage");
    checkIsObject(SOI_ClassCompiledMethod, "ClassCompiledMethod");
    checkIsObject(SOI_ClassSemaphore, "ClassSemaphore");
    checkIsObject(SOI_ClassCharacter, "ClassCharacter");
    checkIsObject(SOI_ClassByteArray, "ClassByteArray");
    checkIsObject(SOI_ClassProcess, "ClassProcess");

    // Special selectors array
    if (specObj->slotCount > SOI_SpecialSelectors) {
        uint64_t selArr = readSlot(specOffset, SOI_SpecialSelectors);
        OopKind kind = oopVal_.classify(selArr);
        if (kind == OopKind::ObjectPointer) {
            size_t selOffset = image_.rawOopToOffset(selArr);
            const HeapObject* selObj = walker_.objectAtOffset(selOffset);
            if (selObj && !isPointerFormat(selObj->format)) {
                ctx.addWarning("specialObjects", "SpecialSelectors array has non-pointer format");
            }
        }
    }
}
