#pragma once
#include <cstdint>
#include <cstddef>

// Spur image format magic numbers
enum class ImageFormat : uint32_t {
    Spur64       = 68021,  // Spur 64-bit
    Spur64Sista  = 68533,  // Spur 64-bit with Sista bytecodes
    Spur32       = 6521,   // Spur 32-bit
    Spur32Sista  = 6505,   // Spur 32-bit with Sista bytecodes
};

inline bool isSpur64(uint32_t fmt) {
    return fmt == 68021 || fmt == 68533;
}

inline bool isSpur32(uint32_t fmt) {
    return fmt == 6521 || fmt == 6505;
}

inline bool isSpurFormat(uint32_t fmt) {
    return isSpur64(fmt) || isSpur32(fmt);
}

// Raw image header as stored on disk (128 bytes padded)
struct SpurImageHeader {
    uint32_t imageFormat;
    uint32_t headerSize;
    uint64_t imageBytes;
    uint64_t startOfMemory;
    uint64_t specialObjectsOop;
    uint64_t lastHash;
    uint64_t screenSize;
    uint64_t imageHeaderFlags;
    uint32_t extraVMMemory;
    uint16_t numStackPages;
    uint16_t cogCodeSize;
    uint32_t edenBytes;
    uint16_t maxExtSemTabSize;
    uint16_t unused1;
    uint64_t firstSegmentBytes;
    uint64_t freeOldSpaceInImage;
};

// Image header flags
enum ImageFlags : uint64_t {
    Flag_FullBlockClosures  = 1 << 0,
    Flag_PreemptionYields   = 1 << 1,
    Flag_DisableVMDisplay   = 1 << 2,
    Flag_SistaV1            = 1 << 3,
    Flag_FloatsBigEndian    = 1 << 4,
    Flag_PosixFlock         = 1 << 5,
};

// Object header bit extraction (same for 32-bit and 64-bit Spur — header is always 64 bits)
inline uint32_t headerClassIndex(uint64_t h)  { return static_cast<uint32_t>(h & 0x3FFFFF); }
inline uint8_t  headerFormat(uint64_t h)      { return static_cast<uint8_t>((h >> 24) & 0x1F); }
inline uint32_t headerHash(uint64_t h)        { return static_cast<uint32_t>((h >> 32) & 0x3FFFFF); }
inline uint8_t  headerNumSlots(uint64_t h)    { return static_cast<uint8_t>((h >> 56) & 0xFF); }
inline bool     headerIsImmutable(uint64_t h) { return (h >> 23) & 1; }
inline bool     headerIsPinned(uint64_t h)    { return (h >> 30) & 1; }
inline bool     headerIsRemembered(uint64_t h){ return (h >> 29) & 1; }
inline bool     headerIsMarked(uint64_t h)    { return (h >> 55) & 1; }
inline bool     headerIsGrey(uint64_t h)      { return (h >> 31) & 1; }

// Object formats
enum ObjFormat : uint8_t {
    Fmt_ZeroSized        = 0,
    Fmt_FixedSize        = 1,
    Fmt_Indexable        = 2,
    Fmt_IndexableFixed   = 3,
    Fmt_Weak             = 4,
    Fmt_WeakFixed        = 5,
    Fmt_Reserved6        = 6,
    Fmt_Reserved7        = 7,
    Fmt_Reserved8        = 8,
    Fmt_Indexable64       = 9,
    Fmt_Indexable32       = 10,
    Fmt_Indexable32Odd    = 11,
    Fmt_Indexable16       = 12,
    Fmt_Indexable16_1     = 13,
    Fmt_Indexable16_2     = 14,
    Fmt_Indexable16_3     = 15,
    Fmt_Indexable8        = 16,
    Fmt_Indexable8_1      = 17,
    Fmt_Indexable8_2      = 18,
    Fmt_Indexable8_3      = 19,
    Fmt_Indexable8_4      = 20,
    Fmt_Indexable8_5      = 21,
    Fmt_Indexable8_6      = 22,
    Fmt_Indexable8_7      = 23,
    Fmt_CompiledMethod    = 24,
    Fmt_CompiledMethod_1  = 25,
    Fmt_CompiledMethod_2  = 26,
    Fmt_CompiledMethod_3  = 27,
    Fmt_CompiledMethod_4  = 28,
    Fmt_CompiledMethod_5  = 29,
    Fmt_CompiledMethod_6  = 30,
    Fmt_CompiledMethod_7  = 31,
};

inline bool isPointerFormat(uint8_t fmt) { return fmt <= 5; }
inline bool isByteFormat(uint8_t fmt)    { return fmt >= 16 && fmt <= 23; }
inline bool isMethodFormat(uint8_t fmt)  { return fmt >= 24; }
inline bool isWordFormat(uint8_t fmt)    { return fmt == 9; }
inline bool isShortFormat(uint8_t fmt)   { return fmt >= 10 && fmt <= 11; }
inline bool is16bitFormat(uint8_t fmt)   { return fmt >= 12 && fmt <= 15; }

// Special constants
static constexpr uint32_t ForwardedClassIndex = 8;
static constexpr uint8_t  OverflowSlotMarker = 255;
static constexpr size_t   MinObjectSize64 = 16;   // 64-bit Spur
static constexpr size_t   MinObjectSize32 = 16;   // 32-bit Spur also 16
static constexpr size_t   HeaderSize = 8;          // Always 64-bit header
static constexpr uint32_t MaxClassIndex = 0x3FFFFF;
static constexpr size_t   ClassTablePageSize = 1024;
static constexpr size_t   MaxClassTablePages = 4096;
