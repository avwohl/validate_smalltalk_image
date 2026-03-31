#include "HeapValidator.hpp"
#include <cstring>

void HeapValidator::validate(ValidationContext& ctx) {
    for (auto& obj : walker_.objects()) {
        if (ctx.shouldStop()) break;
        validateObject(obj, ctx);
    }
}

void HeapValidator::validateObject(const HeapObject& obj, ValidationContext& ctx) {
    checkAlignment(obj, ctx);
    checkFormat(obj, ctx);
    checkForwarded(obj, ctx);

    if (obj.isFree) {
        checkFreeChunk(obj, ctx);
        return;
    }

    if (obj.hasOverflow) {
        checkOverflowHeader(obj, ctx);
    }

    // Validate pointer slots
    if (isPointerFormat(obj.format)) {
        validatePointerSlots(obj, ctx);
    }
}

void HeapValidator::checkAlignment(const HeapObject& obj, ValidationContext& ctx) {
    if (obj.heapOffset % 8 != 0) {
        ctx.addError("alignment", "Object at offset 0x" +
            std::to_string(obj.heapOffset) + " is not 8-byte aligned",
            obj.heapOffset);
    }
}

void HeapValidator::checkFormat(const HeapObject& obj, ValidationContext& ctx) {
    uint8_t fmt = obj.format;
    if (fmt == Fmt_Reserved6 || fmt == Fmt_Reserved7 || fmt == Fmt_Reserved8) {
        ctx.addWarning("format", "Object uses reserved format " +
            std::to_string(fmt), obj.heapOffset);
    }

    // Class index range check (for non-free objects)
    if (!obj.isFree && obj.classIndex > MaxClassIndex) {
        ctx.addError("classIndex", "Class index " +
            std::to_string(obj.classIndex) + " exceeds maximum (" +
            std::to_string(MaxClassIndex) + ")", obj.heapOffset);
    }

    // Zero-sized object should have 0 slots
    if (fmt == Fmt_ZeroSized && obj.slotCount > 0 && !obj.isFree) {
        ctx.addWarning("format", "ZeroSized format but has " +
            std::to_string(obj.slotCount) + " slots", obj.heapOffset);
    }

    // Byte objects: check padding consistency
    if (isByteFormat(fmt)) {
        size_t unusedBytes = fmt - Fmt_Indexable8;
        if (obj.slotCount == 0 && unusedBytes > 0) {
            ctx.addWarning("format", "Byte object with 0 slots but format indicates " +
                std::to_string(unusedBytes) + " unused bytes", obj.heapOffset);
        }
    }

    // Methods: check padding consistency
    if (isMethodFormat(fmt)) {
        size_t unusedBytes = fmt - Fmt_CompiledMethod;
        if (obj.slotCount == 0 && unusedBytes > 0) {
            ctx.addWarning("format", "Method with 0 slots but format indicates " +
                std::to_string(unusedBytes) + " unused bytes", obj.heapOffset);
        }
    }
}

void HeapValidator::checkForwarded(const HeapObject& obj, ValidationContext& ctx) {
    if (obj.classIndex == ForwardedClassIndex && !obj.isFree) {
        ctx.addError("forwarded", "Forwarded object found in saved image "
            "(classIndex=8 should only exist during GC)", obj.heapOffset);
    }
}

void HeapValidator::checkFreeChunk(const HeapObject& obj, ValidationContext& ctx) {
    // Free chunks should have classIndex=0 and format=0
    if (obj.format != 0) {
        ctx.addWarning("freeChunk", "Free chunk (classIndex=0) has non-zero format: " +
            std::to_string(obj.format), obj.heapOffset);
    }
}

void HeapValidator::checkOverflowHeader(const HeapObject& obj, ValidationContext& ctx) {
    if (obj.slotCount < 255) {
        ctx.addWarning("overflow", "Overflow header with slot count < 255: " +
            std::to_string(obj.slotCount), obj.heapOffset);
    }
    // Warn on very large objects (>100MB)
    size_t bodyBytes = obj.slotCount * (image_.is64Bit() ? 8 : 4);
    if (bodyBytes > 100 * 1024 * 1024) {
        ctx.addWarning("overflow", "Very large object: " +
            std::to_string(bodyBytes / (1024*1024)) + " MB (" +
            std::to_string(obj.slotCount) + " slots)", obj.heapOffset);
    }
}

void HeapValidator::validatePointerSlots(const HeapObject& obj, ValidationContext& ctx) {
    // Read each slot and validate it as an Oop
    size_t oopSize = image_.oopSize();
    // Slot data starts after the header
    size_t dataStart = obj.heapOffset + HeaderSize;

    for (size_t i = 0; i < obj.slotCount; i++) {
        if (ctx.shouldStop()) break;
        size_t slotOffset = dataStart + i * oopSize;
        if (slotOffset + oopSize > image_.heapSize()) {
            ctx.addError("slot", "Slot " + std::to_string(i) +
                " extends past heap end", obj.heapOffset);
            break;
        }
        uint64_t slotValue = image_.oopAt(slotOffset);
        std::string context = "slot[" + std::to_string(i) + "] of object";
        uint64_t absOffset = image_.startOfMemory() + obj.heapOffset;
        oopVal_.validateOop(slotValue, context, absOffset, ctx);
    }
}
