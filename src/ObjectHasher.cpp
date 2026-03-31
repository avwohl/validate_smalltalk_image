#include "ObjectHasher.hpp"

std::string ObjectHasher::contentHash(const HeapObject& obj) const {
    // Hash the stable parts of the header (classIndex + format + hash + slotCount)
    // plus all body bytes.
    // We exclude mutable GC bits: marked (bit 55), remembered (bit 29), grey (bit 31).

    Sha256 sha;

    // Hash a canonicalized header: zero out mutable GC bits
    // Mutable bits: remembered (29), pinned (30) is debatable but keep it,
    //               grey (31), marked (55)
    uint64_t stableHeader = obj.header;
    stableHeader &= ~(uint64_t(1) << 29);  // remembered
    stableHeader &= ~(uint64_t(1) << 31);  // grey
    stableHeader &= ~(uint64_t(1) << 55);  // marked

    uint8_t hdrBytes[8];
    for (int i = 0; i < 8; i++)
        hdrBytes[i] = uint8_t(stableHeader >> (i * 8));
    sha.update(hdrBytes, 8);

    // Hash the body (everything after the 8-byte header)
    size_t bodyStart = obj.heapOffset + HeaderSize;
    size_t bodyBytes = obj.totalBytes - HeaderSize;
    if (obj.hasOverflow)
        bodyBytes = obj.totalBytes - HeaderSize - 8;  // overflow word precedes header

    if (bodyBytes > 0 && bodyStart + bodyBytes <= image_.heapSize()) {
        sha.update(image_.heapData() + bodyStart, bodyBytes);
    }

    return sha.hexFinalize();
}
