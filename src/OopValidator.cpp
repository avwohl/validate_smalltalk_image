#include "OopValidator.hpp"

OopKind OopValidator::classify(uint64_t rawOop) const {
    if (image_.is64Bit()) {
        uint64_t nilAddr = image_.startOfMemory();  // nil is the first object
        if (rawOop == nilAddr) return OopKind::Nil;
        if (isImmediate64(rawOop)) return OopKind::Immediate;
        if (isObjectPointer64(rawOop)) {
            if (image_.isInHeapRange(rawOop)) return OopKind::ObjectPointer;
            return OopKind::Invalid;
        }
        // rawOop == 0 with non-zero nilAddr is suspicious but could be pre-init
        if (rawOop == 0) return OopKind::Nil;  // treat raw 0 as nil for tolerance
        return OopKind::Invalid;
    } else {
        // 32-bit Spur
        uint32_t bits = static_cast<uint32_t>(rawOop);
        if (bits == 0) return OopKind::Nil;
        if (StandardTags32::isSmallInteger(bits)) return OopKind::Immediate;
        if (StandardTags32::isObjectPointer(bits)) {
            if (image_.isInHeapRange(bits)) return OopKind::ObjectPointer;
            return OopKind::Invalid;
        }
        return OopKind::Invalid;
    }
}

bool OopValidator::validateOop(uint64_t rawOop, const std::string& context,
                                uint64_t locationOffset, ValidationContext& ctx) const {
    OopKind kind = classify(rawOop);

    if (kind == OopKind::Invalid) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: invalid Oop 0x%llx (not immediate, not in heap range)",
                 context.c_str(), (unsigned long long)rawOop);
        ctx.addError("oop", buf, locationOffset);
        return false;
    }

    if (kind == OopKind::ObjectPointer) {
        // Check alignment
        size_t alignment = 8;  // Spur objects are 8-byte aligned in both 32 and 64 bit
        if (rawOop % alignment != 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: misaligned pointer 0x%llx",
                     context.c_str(), (unsigned long long)rawOop);
            ctx.addError("oop", buf, locationOffset);
            return false;
        }

        // Check points to actual object start
        if (!pointsToObjectStart(rawOop)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: pointer 0x%llx does not point to an object start",
                     context.c_str(), (unsigned long long)rawOop);
            ctx.addError("oop", buf, locationOffset);
            return false;
        }
    }

    return true;
}

bool OopValidator::pointsToObjectStart(uint64_t rawOop) const {
    size_t offset = image_.rawOopToOffset(rawOop);
    if (offset == SIZE_MAX) return false;
    return walker_.objectStarts().count(offset) > 0;
}

bool OopValidator::isImmediate64(uint64_t bits) const {
    if (iosTags_) return iOSTags64::isImmediate(bits);
    return StandardTags64::isImmediate(bits);
}

bool OopValidator::isObjectPointer64(uint64_t bits) const {
    if (bits == 0) return false;
    if (iosTags_) return !iOSTags64::isImmediate(bits);
    return !StandardTags64::isImmediate(bits);
}
