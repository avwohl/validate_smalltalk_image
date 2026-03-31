#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ValidationContext.hpp"

class MethodValidator {
public:
    MethodValidator(const SpurImage& image, const HeapWalker& walker,
                    const OopValidator& oopVal)
        : image_(image), walker_(walker), oopVal_(oopVal) {}

    void validate(ValidationContext& ctx);

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;

    void validateMethod(const HeapObject& obj, ValidationContext& ctx);
};
