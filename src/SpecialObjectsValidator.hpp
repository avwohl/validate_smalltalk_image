#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ValidationContext.hpp"

class SpecialObjectsValidator {
public:
    SpecialObjectsValidator(const SpurImage& image, const HeapWalker& walker,
                            const OopValidator& oopVal)
        : image_(image), walker_(walker), oopVal_(oopVal) {}

    void validate(ValidationContext& ctx);

private:
    const SpurImage& image_;
    const HeapWalker& walker_;
    const OopValidator& oopVal_;

    uint64_t readSlot(size_t objOffset, size_t slotIndex) const;
};

// Special object indices (from Pharo's SpecialObjectIndex)
enum SpecObjIndex {
    SOI_Nil = 0,
    SOI_False = 1,
    SOI_True = 2,
    SOI_SchedulerAssociation = 3,
    SOI_ClassBitmap = 4,
    SOI_ClassSmallInteger = 5,
    SOI_ClassByteString = 6,
    SOI_ClassArray = 7,
    SOI_SmalltalkDictionary = 8,
    SOI_ClassFloat = 9,
    SOI_ClassMethodContext = 10,
    SOI_ClassBlockClosure = 11,
    SOI_ClassPoint = 12,
    SOI_ClassLargePositiveInteger = 13,
    SOI_ClassMessage = 14,
    SOI_ClassCompiledMethod = 15,
    SOI_ClassSemaphore = 18,
    SOI_ClassCharacter = 19,
    SOI_SelectorDoesNotUnderstand = 20,
    SOI_SelectorCannotReturn = 21,
    SOI_SpecialSelectors = 23,
    SOI_ClassByteArray = 26,
    SOI_ClassProcess = 27,
    SOI_ClassBlockContext = 28,  // or FullBlockClosure
    SOI_ClassLargeNegativeInteger = 42,
    SOI_MinSpecialObjects = 60,  // minimum expected size
};
