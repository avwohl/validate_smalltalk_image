#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ValidationContext.hpp"

class HeapValidator {
public:
    HeapValidator(const SpurImage& image, const HeapWalker& walker,
                  const OopValidator& oopVal)
        : image_(image), walker_(walker), oopVal_(oopVal) {}

    void validate(ValidationContext& ctx);

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;

    void validateObject(const HeapObject& obj, ValidationContext& ctx);
    void validatePointerSlots(const HeapObject& obj, ValidationContext& ctx);
    void checkAlignment(const HeapObject& obj, ValidationContext& ctx);
    void checkFormat(const HeapObject& obj, ValidationContext& ctx);
    void checkForwarded(const HeapObject& obj, ValidationContext& ctx);
    void checkFreeChunk(const HeapObject& obj, ValidationContext& ctx);
    void checkOverflowHeader(const HeapObject& obj, ValidationContext& ctx);
};
