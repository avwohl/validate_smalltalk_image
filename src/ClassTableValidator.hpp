#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ValidationContext.hpp"
#include <unordered_map>

class ClassTableValidator {
public:
    ClassTableValidator(const SpurImage& image, const HeapWalker& walker,
                        const OopValidator& oopVal)
        : image_(image), walker_(walker), oopVal_(oopVal) {}

    void validate(ValidationContext& ctx);

    // After validation, maps class index -> heap offset of class object
    const std::unordered_map<uint32_t, size_t>& classMap() const { return classMap_; }

    // Check if a class index has a valid class object
    bool hasClass(uint32_t classIndex) const {
        return classMap_.count(classIndex) > 0;
    }

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;
    std::unordered_map<uint32_t, size_t> classMap_;  // classIndex -> heapOffset

    void validateHiddenRoots(const HeapObject* hiddenRoots, ValidationContext& ctx);
    void validateClassTablePage(uint64_t pageOop, size_t pageIndex, ValidationContext& ctx);
    void checkAllObjectsHaveClasses(ValidationContext& ctx);
};
