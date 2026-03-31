#include "ClassNameResolver.hpp"

uint64_t ClassNameResolver::readSlot(size_t objOffset, size_t slotIndex) const {
    size_t oopSz = image_.oopSize();
    return image_.oopAt(objOffset + HeaderSize + slotIndex * oopSz);
}

std::string ClassNameResolver::extractByteString(size_t objOffset, const HeapObject& obj) const {
    if (!isByteFormat(obj.format) && obj.format != Fmt_IndexableFixed)
        return {};

    size_t oopSz = image_.oopSize();
    size_t byteCount;

    if (isByteFormat(obj.format)) {
        // Byte objects: actual bytes = slotCount * wordSize - padding
        // padding = format - 16 (for 64-bit) or format - 16 (same formula)
        size_t padding = obj.format - Fmt_Indexable8;
        byteCount = obj.slotCount * oopSz - padding;
    } else {
        // IndexableFixed (format 3): used by Pharo Symbols
        // These have fixed fields + indexable bytes
        // For Symbols, the fixed part is typically 0 extra inst vars beyond what
        // the indexable portion provides. The bytes are stored after the fixed fields.
        // In Pharo, Symbol is format 16+ (byte), but in some dialects it can be format 3.
        // For format 3, treat all slots as containing the string data packed as oops.
        // This is a heuristic - extract bytes from the raw slot data.
        byteCount = obj.slotCount * oopSz;
    }

    if (byteCount == 0 || byteCount > 1024)
        return {};

    const uint8_t* heap = image_.heapData();
    size_t dataStart = objOffset + HeaderSize;

    // For overflow objects, header is preceded by the overflow word
    // but heapOffset already points to the actual header, and data follows it

    if (dataStart + byteCount > image_.heapSize())
        return {};

    // Build string, rejecting non-printable ASCII (class names should be ASCII identifiers)
    std::string result;
    result.reserve(byteCount);
    for (size_t i = 0; i < byteCount; i++) {
        uint8_t ch = heap[dataStart + i];
        if (ch >= 0x20 && ch < 0x7F)
            result.push_back(static_cast<char>(ch));
        else
            return {};  // not a valid class name
    }
    return result;
}

std::string ClassNameResolver::resolveClassName(size_t classOffset) const {
    const HeapObject* classObj = walker_.objectAtOffset(classOffset);
    if (!classObj || !isPointerFormat(classObj->format))
        return {};

    // Regular class has >= 7 slots, with name at slot 6
    if (classObj->slotCount >= 7) {
        uint64_t nameOop = readSlot(classOffset, 6);
        uint64_t nilAddr = image_.startOfMemory();

        if (nameOop != nilAddr && nameOop != 0) {
            OopKind kind = oopVal_.classify(nameOop);
            if (kind == OopKind::ObjectPointer) {
                size_t nameOffset = image_.rawOopToOffset(nameOop);
                if (nameOffset != SIZE_MAX) {
                    const HeapObject* nameObj = walker_.objectAtOffset(nameOffset);
                    if (nameObj) {
                        std::string name = extractByteString(nameOffset, *nameObj);
                        if (!name.empty())
                            return name;
                    }
                }
            }
        }
    }

    // Metaclass: typically has 6 slots, with thisClass at slot 5
    // Try to derive "Foo class" from the instance side
    if (classObj->slotCount >= 6) {
        uint64_t thisClassOop = readSlot(classOffset, 5);
        OopKind kind = oopVal_.classify(thisClassOop);
        if (kind == OopKind::ObjectPointer) {
            size_t thisClassOffset = image_.rawOopToOffset(thisClassOop);
            if (thisClassOffset != SIZE_MAX && thisClassOffset != classOffset) {
                std::string baseName = resolveClassName(thisClassOffset);
                if (!baseName.empty())
                    return baseName + " class";
            }
        }
    }

    return {};
}

void ClassNameResolver::resolve() {
    // Well-known immediate class indices
    names_[1] = "SmallInteger";
    names_[19] = "Character";
    if (image_.is64Bit())
        names_[4] = "SmallFloat64";

    for (auto& [classIdx, classOffset] : classTable_.classMap()) {
        if (names_.count(classIdx))
            continue;
        std::string name = resolveClassName(classOffset);
        if (!name.empty())
            names_[classIdx] = name;
        else
            names_[classIdx] = "<unknown:" + std::to_string(classIdx) + ">";
    }
}

const std::string& ClassNameResolver::nameForClassIndex(uint32_t classIndex) const {
    auto it = names_.find(classIndex);
    if (it != names_.end())
        return it->second;
    fallback_ = "<unknown:" + std::to_string(classIndex) + ">";
    return fallback_;
}
