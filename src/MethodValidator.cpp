#include "MethodValidator.hpp"
#include <cstring>

void MethodValidator::validate(ValidationContext& ctx) {
    for (auto& obj : walker_.objects()) {
        if (ctx.shouldStop()) break;
        if (obj.isFree) continue;
        if (isMethodFormat(obj.format)) {
            validateMethod(obj, ctx);
        }
    }
}

void MethodValidator::validateMethod(const HeapObject& obj, ValidationContext& ctx) {
    if (obj.slotCount == 0) {
        ctx.addWarning("method", "CompiledMethod with 0 slots", obj.heapOffset);
        return;
    }

    size_t oopSize = image_.oopSize();
    size_t dataStart = obj.heapOffset + HeaderSize;

    // Slot 0 is the method header (must be a SmallInteger)
    uint64_t methodHeader = image_.oopAt(dataStart);

    // In both standard and iOS 64-bit Spur, SmallInteger tag is 001
    bool isSmallInt;
    if (image_.is64Bit()) {
        isSmallInt = (methodHeader & 0x7) == 1;
    } else {
        isSmallInt = (methodHeader & 0x1) == 1;
    }

    if (!isSmallInt) {
        ctx.addError("method", "Method header (slot 0) is not a SmallInteger: 0x" +
            std::to_string(methodHeader), obj.heapOffset);
        return;
    }

    // Decode the SmallInteger value (shift right by tag bits)
    int64_t headerValue;
    if (image_.is64Bit()) {
        headerValue = static_cast<int64_t>(methodHeader) >> 3;
    } else {
        headerValue = static_cast<int32_t>(static_cast<uint32_t>(methodHeader)) >> 1;
    }

    // Extract method header fields
    size_t numLiterals = headerValue & 0x7FFF;          // bits 0-14
    bool hasPrimitive = (headerValue >> 16) & 1;        // bit 16
    size_t numTemps = (headerValue >> 18) & 0x3F;       // bits 18-23
    size_t numArgs = (headerValue >> 24) & 0xF;         // bits 24-27

    // numLiterals must fit: slot 0 is the header, slots 1..numLiterals are literals
    if (numLiterals > obj.slotCount - 1) {
        ctx.addError("method", "numLiterals (" + std::to_string(numLiterals) +
            ") > available slots (" + std::to_string(obj.slotCount - 1) + ")",
            obj.heapOffset);
        return;
    }

    // numArgs must be <= numTemps
    if (numArgs > numTemps) {
        ctx.addWarning("method", "numArgs (" + std::to_string(numArgs) +
            ") > numTemps (" + std::to_string(numTemps) + ")", obj.heapOffset);
    }

    // Validate literal frame (slots 1..numLiterals): each must be a valid Oop
    for (size_t i = 1; i <= numLiterals; i++) {
        size_t slotOff = dataStart + i * oopSize;
        uint64_t litOop = image_.oopAt(slotOff);
        oopVal_.validateOop(litOop, "method literal[" + std::to_string(i-1) + "]",
                            image_.startOfMemory() + obj.heapOffset, ctx);
    }

    // Bytecodes start after the literal frame
    size_t bcStart = (1 + numLiterals) * oopSize;
    size_t unusedBytes = obj.format - Fmt_CompiledMethod;
    size_t totalBodyBytes = obj.slotCount * oopSize;

    if (bcStart > totalBodyBytes) {
        ctx.addError("method", "Literal frame extends past method body", obj.heapOffset);
        return;
    }

    size_t bcLength = totalBodyBytes - bcStart - unusedBytes;

    // If hasPrimitive, first bytecode should be 248 (callPrimitive)
    if (hasPrimitive && bcLength >= 3) {
        size_t bcOffset = dataStart + bcStart;
        if (bcOffset + 3 <= image_.heapSize()) {
            uint8_t firstByte = image_.heapData()[bcOffset];
            if (firstByte != 248) {
                ctx.addWarning("method", "hasPrimitive set but first bytecode is " +
                    std::to_string(firstByte) + " (expected 248 callPrimitive)",
                    obj.heapOffset);
            } else {
                // Decode primitive index
                uint8_t byte1 = image_.heapData()[bcOffset + 1];
                uint8_t byte2 = image_.heapData()[bcOffset + 2];
                uint32_t primIndex = byte1 | ((byte2 & 0x1F) << 8);
                if (primIndex > 8191) {
                    ctx.addWarning("method", "Primitive index " + std::to_string(primIndex) +
                        " out of range (max 8191)", obj.heapOffset);
                }
            }
        }
    }
}
