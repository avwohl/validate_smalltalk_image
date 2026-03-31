#pragma once
#include "SpurImage.hpp"
#include "OopTraits.hpp"
#include "HeapWalker.hpp"
#include "ValidationContext.hpp"
#include <cstdint>
#include <string>

enum class OopKind { Nil, Immediate, ObjectPointer, Invalid };

class OopValidator {
public:
    OopValidator(const SpurImage& image, const HeapWalker& walker, bool iosTags)
        : image_(image), walker_(walker), iosTags_(iosTags) {}

    // Classify a raw Oop value
    OopKind classify(uint64_t rawOop) const;

    // Validate a single Oop found at the given location.
    // Returns true if valid (immediate or valid pointer), false if invalid.
    bool validateOop(uint64_t rawOop, const std::string& context,
                     uint64_t locationOffset, ValidationContext& ctx) const;

    // Check if rawOop points to an actual object start
    bool pointsToObjectStart(uint64_t rawOop) const;

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    bool iosTags_;

    bool isImmediate64(uint64_t bits) const;
    bool isObjectPointer64(uint64_t bits) const;
};
