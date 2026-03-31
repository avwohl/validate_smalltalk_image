#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ClassTableValidator.hpp"
#include "ValidationContext.hpp"

class ClassHierarchyValidator {
public:
    ClassHierarchyValidator(const SpurImage& image, const HeapWalker& walker,
                            const OopValidator& oopVal, const ClassTableValidator& classTable)
        : image_(image), walker_(walker), oopVal_(oopVal), classTable_(classTable) {}

    void validate(ValidationContext& ctx);

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;
    const ClassTableValidator& classTable_;

    void validateClass(uint32_t classIndex, size_t classOffset, ValidationContext& ctx);
    void checkSuperclassChain(uint32_t classIndex, size_t classOffset, ValidationContext& ctx);
    void checkMethodDict(size_t classOffset, uint32_t classIndex, ValidationContext& ctx);
    void checkClassName(size_t classOffset, uint32_t classIndex, ValidationContext& ctx);
    void checkInstSpec(size_t classOffset, uint32_t classIndex, ValidationContext& ctx);

    // Read a slot from an object at the given heap offset
    uint64_t readSlot(size_t objOffset, size_t slotIndex) const;
};
