#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ClassTableValidator.hpp"
#include "ValidationContext.hpp"
#include <unordered_set>

class ReachabilityAnalyzer {
public:
    ReachabilityAnalyzer(const SpurImage& image, const HeapWalker& walker,
                         const OopValidator& oopVal, const ClassTableValidator& classTable)
        : image_(image), walker_(walker), oopVal_(oopVal), classTable_(classTable) {}

    void analyze(ValidationContext& ctx);

    size_t reachableCount() const { return marked_.size(); }
    size_t unreachableCount() const;

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;
    const ClassTableValidator& classTable_;

    std::unordered_set<size_t> marked_;  // set of reachable heap offsets

    void mark(uint64_t rawOop);
    void markObjectSlots(size_t heapOffset);
};
