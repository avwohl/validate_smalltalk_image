#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ClassTableValidator.hpp"
#include <string>
#include <unordered_map>

class ClassNameResolver {
public:
    ClassNameResolver(const SpurImage& image, const HeapWalker& walker,
                      const OopValidator& oopVal, const ClassTableValidator& classTable)
        : image_(image), walker_(walker), oopVal_(oopVal), classTable_(classTable) {}

    // Build the classIndex -> name map. Call once after validation.
    void resolve();

    // Look up by class index. Returns "<unknown:N>" if not found.
    const std::string& nameForClassIndex(uint32_t classIndex) const;

    const std::unordered_map<uint32_t, std::string>& classNames() const { return names_; }

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;
    const ClassTableValidator& classTable_;
    std::unordered_map<uint32_t, std::string> names_;
    mutable std::string fallback_;

    uint64_t readSlot(size_t objOffset, size_t slotIndex) const;
    std::string extractByteString(size_t objOffset, const HeapObject& obj) const;
    std::string resolveClassName(size_t classOffset) const;
};
