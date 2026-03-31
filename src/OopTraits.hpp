#pragma once
#include <cstdint>

// Tag scheme variants
enum class TagScheme { Standard, iOS };

// Standard Spur 64-bit tags:
//   000 = object pointer
//   001 = SmallInteger
//   010 = Character
//   100 = SmallFloat
//
// iOS variant (iospharo):
//   000 = object pointer
//   001 = SmallInteger
//   011 = Character
//   101 = SmallFloat

struct StandardTags64 {
    static constexpr uint64_t TagMask      = 0x7;
    static constexpr uint64_t SmallIntTag  = 0x1;  // 001
    static constexpr uint64_t CharacterTag = 0x2;  // 010
    static constexpr uint64_t SmallFloatTag= 0x4;  // 100

    static bool isImmediate(uint64_t bits) {
        return (bits & TagMask) != 0;
    }
    static bool isSmallInteger(uint64_t bits) {
        return (bits & TagMask) == SmallIntTag;
    }
    static bool isCharacter(uint64_t bits) {
        return (bits & TagMask) == CharacterTag;
    }
    static bool isSmallFloat(uint64_t bits) {
        return (bits & TagMask) == SmallFloatTag;
    }
    static bool isObjectPointer(uint64_t bits) {
        return bits != 0 && (bits & TagMask) == 0;
    }
    static bool isNil(uint64_t bits, uint64_t nilAddr) {
        return bits == nilAddr;
    }
};

struct iOSTags64 {
    static constexpr uint64_t TagMask      = 0x7;
    static constexpr uint64_t SmallIntTag  = 0x1;  // 001
    static constexpr uint64_t CharacterTag = 0x3;  // 011
    static constexpr uint64_t SmallFloatTag= 0x5;  // 101

    static bool isImmediate(uint64_t bits) {
        return (bits & 1) != 0;
    }
    static bool isSmallInteger(uint64_t bits) {
        return (bits & TagMask) == SmallIntTag;
    }
    static bool isCharacter(uint64_t bits) {
        return (bits & TagMask) == CharacterTag;
    }
    static bool isSmallFloat(uint64_t bits) {
        return (bits & TagMask) == SmallFloatTag;
    }
    static bool isObjectPointer(uint64_t bits) {
        return bits != 0 && (bits & 1) == 0;
    }
    static bool isNil(uint64_t bits, uint64_t nilAddr) {
        return bits == nilAddr;
    }
};

// Standard Spur 32-bit tags:
//   bit 0 = 1: SmallInteger (31-bit signed)
//   bit 0 = 0: object pointer (no Character/SmallFloat immediates in 32-bit)
struct StandardTags32 {
    static bool isImmediate(uint32_t bits) {
        return (bits & 1) != 0;
    }
    static bool isSmallInteger(uint32_t bits) {
        return (bits & 1) != 0;
    }
    static bool isCharacter(uint32_t) { return false; }
    static bool isSmallFloat(uint32_t) { return false; }
    static bool isObjectPointer(uint32_t bits) {
        return bits != 0 && (bits & 1) == 0;
    }
};
