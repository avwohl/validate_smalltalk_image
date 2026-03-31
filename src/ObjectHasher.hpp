#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "Sha256.hpp"
#include <string>

class ObjectHasher {
public:
    ObjectHasher(const SpurImage& image) : image_(image) {}

    // SHA-256 of the object's content bytes (body after header).
    // Excludes mutable GC flags from the header.
    std::string contentHash(const HeapObject& obj) const;

private:
    const SpurImage& image_;
};
