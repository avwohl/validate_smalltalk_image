#include "SpurImage.hpp"
#include <fstream>
#include <cstring>

bool SpurImage::load(const std::string& path, ValidationContext& ctx) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        ctx.addError("image", "Cannot open file: " + path);
        return false;
    }

    // Read first 4 bytes to get format
    uint32_t formatWord = 0;
    file.read(reinterpret_cast<char*>(&formatWord), 4);
    if (!file) {
        ctx.addError("image", "Cannot read image format");
        return false;
    }

    // Check if byte-swapped
    bool swapped = false;
    if (!isSpurFormat(formatWord)) {
        // Try byte-swapping
        uint32_t swappedFmt = __builtin_bswap32(formatWord);
        if (isSpurFormat(swappedFmt)) {
            ctx.addError("image", "Byte-swapped image detected (format " +
                std::to_string(swappedFmt) + "). Not supported — "
                "convert to native byte order first.");
            return false;
        }
        ctx.addError("image", "Unknown image format: " + std::to_string(formatWord));
        return false;
    }

    is64_ = isSpur64(formatWord);

    // Read the full header
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&header_), sizeof(SpurImageHeader));
    if (!file) {
        ctx.addError("image", "Cannot read image header (truncated file?)");
        return false;
    }

    // Validate header fields
    if (header_.headerSize < 64 || header_.headerSize > 1024) {
        ctx.addError("header", "Invalid header size: " + std::to_string(header_.headerSize));
        return false;
    }
    if (header_.imageBytes == 0) {
        ctx.addError("header", "Image reports 0 bytes of heap data");
        return false;
    }
    if (header_.imageBytes > 4ULL * 1024 * 1024 * 1024) {
        ctx.addWarning("header", "Very large image: " +
            std::to_string(header_.imageBytes / (1024*1024)) + " MB");
    }

    // Seek past header and read heap
    file.seekg(header_.headerSize);
    heap_.resize(header_.imageBytes);
    file.read(reinterpret_cast<char*>(heap_.data()), header_.imageBytes);
    size_t bytesRead = file.gcount();
    if (bytesRead < header_.imageBytes) {
        ctx.addWarning("image", "Truncated image: expected " +
            std::to_string(header_.imageBytes) + " bytes, got " +
            std::to_string(bytesRead));
        heap_.resize(bytesRead);
    }

    return true;
}

uint64_t SpurImage::wordAt(size_t offset) const {
    if (offset + 8 > heap_.size()) return 0;
    uint64_t val;
    memcpy(&val, heap_.data() + offset, 8);
    return val;
}

uint32_t SpurImage::word32At(size_t offset) const {
    if (offset + 4 > heap_.size()) return 0;
    uint32_t val;
    memcpy(&val, heap_.data() + offset, 4);
    return val;
}

uint64_t SpurImage::oopAt(size_t offset) const {
    if (is64_) return wordAt(offset);
    return word32At(offset);
}

bool SpurImage::isInHeapRange(uint64_t rawOop) const {
    uint64_t base = header_.startOfMemory;
    return rawOop >= base && rawOop < base + heap_.size();
}

size_t SpurImage::rawOopToOffset(uint64_t rawOop) const {
    if (!isInHeapRange(rawOop)) return SIZE_MAX;
    return static_cast<size_t>(rawOop - header_.startOfMemory);
}
