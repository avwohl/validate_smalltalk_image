#include "ImageExporter.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include <cinttypes>

// ============================================================================
// Helpers
// ============================================================================

static std::string flagsString(const HeapObject& obj) {
    std::string f;
    if (headerIsImmutable(obj.header)) { if (!f.empty()) f += ','; f += "immutable"; }
    if (headerIsPinned(obj.header))    { if (!f.empty()) f += ','; f += "pinned"; }
    if (obj.hasOverflow)               { if (!f.empty()) f += ','; f += "overflow"; }
    if (f.empty()) f = "-";
    return f;
}

static void jsonEscape(const std::string& s, FILE* out) {
    fputc('"', out);
    for (char c : s) {
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out);  break;
            case '\r': fputs("\\r", out);  break;
            case '\t': fputs("\\t", out);  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    fprintf(out, "\\u%04x", (unsigned)c);
                else
                    fputc(c, out);
        }
    }
    fputc('"', out);
}

static uint64_t readSlot(const SpurImage& image, size_t objOffset, size_t slotIndex) {
    return image.oopAt(objOffset + HeaderSize + slotIndex * image.oopSize());
}

// ============================================================================
// exportShasum
// ============================================================================

void ImageExporter::exportShasum(const SpurImage& image, const HeapWalker& walker,
                                  const ClassNameResolver& names, const ObjectHasher& hasher,
                                  const std::string& filterClass, FILE* out) {
    for (auto& obj : walker.objects()) {
        if (obj.isFree) continue;

        const std::string& className = names.nameForClassIndex(obj.classIndex);
        if (!filterClass.empty() && className != filterClass) continue;

        std::string hash = hasher.contentHash(obj);
        uint32_t idHash = headerHash(obj.header);

        fprintf(out, "%016zx %06x %-40s %s\n",
                obj.heapOffset, idHash, className.c_str(), hash.c_str());
    }
}

// ============================================================================
// exportCatalog
// ============================================================================

void ImageExporter::exportCatalog(const SpurImage& image, const HeapWalker& walker,
                                   const ClassNameResolver& names, const ObjectHasher& hasher,
                                   const std::string& filterClass, ExportFormat fmt, FILE* out) {
    if (fmt == ExportFormat::Csv) {
        fprintf(out, "offset,classIndex,className,format,slots,bytes,identityHash,contentHash,flags\n");
    } else if (fmt == ExportFormat::Json) {
        fprintf(out, "[\n");
    }

    bool firstJson = true;

    for (auto& obj : walker.objects()) {
        if (obj.isFree) continue;

        const std::string& className = names.nameForClassIndex(obj.classIndex);
        if (!filterClass.empty() && className != filterClass) continue;

        std::string hash = hasher.contentHash(obj);
        uint32_t idHash = headerHash(obj.header);
        std::string flags = flagsString(obj);

        if (fmt == ExportFormat::Text) {
            fprintf(out, "%016zx  %6u  %-40s  fmt=%-2u  slots=%-8zu  bytes=%-10zu  hash=%06x  sha=%s  %s\n",
                    obj.heapOffset, obj.classIndex, className.c_str(),
                    obj.format, obj.slotCount, obj.totalBytes,
                    idHash, hash.c_str(), flags.c_str());
        } else if (fmt == ExportFormat::Csv) {
            fprintf(out, "%016zx,%u,", obj.heapOffset, obj.classIndex);
            // CSV-escape class name (quote if contains comma)
            if (className.find(',') != std::string::npos)
                fprintf(out, "\"%s\"", className.c_str());
            else
                fprintf(out, "%s", className.c_str());
            fprintf(out, ",%u,%zu,%zu,%06x,%s,%s\n",
                    obj.format, obj.slotCount, obj.totalBytes,
                    idHash, hash.c_str(), flags.c_str());
        } else {
            if (!firstJson) fprintf(out, ",\n");
            firstJson = false;
            fprintf(out, "  {\"offset\":\"%016zx\",\"classIndex\":%u,\"className\":",
                    obj.heapOffset, obj.classIndex);
            jsonEscape(className, out);
            fprintf(out, ",\"format\":%u,\"slots\":%zu,\"bytes\":%zu,"
                    "\"identityHash\":\"%06x\",\"contentHash\":\"%s\",\"flags\":\"%s\"}",
                    obj.format, obj.slotCount, obj.totalBytes,
                    idHash, hash.c_str(), flags.c_str());
        }
    }

    if (fmt == ExportFormat::Json) {
        fprintf(out, "\n]\n");
    }
}

// ============================================================================
// exportHierarchy
// ============================================================================

struct ClassInfo {
    uint32_t classIndex;
    size_t   classOffset;
    std::string name;
    uint32_t superclassIndex;  // 0 if root or unresolvable
    size_t   methodCount;
    size_t   instVarCount;
};

static size_t countMethods(const SpurImage& image, const HeapWalker& walker,
                           const OopValidator& oopVal, size_t classOffset) {
    const HeapObject* classObj = walker.objectAtOffset(classOffset);
    if (!classObj || classObj->slotCount < 2) return 0;

    uint64_t mdOop = readSlot(image, classOffset, 1);
    uint64_t nilAddr = image.startOfMemory();
    if (mdOop == nilAddr || mdOop == 0) return 0;

    OopKind kind = oopVal.classify(mdOop);
    if (kind != OopKind::ObjectPointer) return 0;

    size_t mdOffset = image.rawOopToOffset(mdOop);
    if (mdOffset == SIZE_MAX) return 0;

    const HeapObject* mdObj = walker.objectAtOffset(mdOffset);
    if (!mdObj || !isPointerFormat(mdObj->format) || mdObj->slotCount < 2) return 0;

    // Slot 1 of MethodDictionary is the values array
    uint64_t valuesOop = readSlot(image, mdOffset, 1);
    if (valuesOop == nilAddr || valuesOop == 0) return 0;

    kind = oopVal.classify(valuesOop);
    if (kind != OopKind::ObjectPointer) return 0;

    size_t valOffset = image.rawOopToOffset(valuesOop);
    if (valOffset == SIZE_MAX) return 0;

    const HeapObject* valObj = walker.objectAtOffset(valOffset);
    if (!valObj) return 0;

    // Count non-nil entries
    size_t count = 0;
    for (size_t i = 0; i < valObj->slotCount; i++) {
        uint64_t entry = readSlot(image, valOffset, i);
        if (entry != nilAddr && entry != 0)
            count++;
    }
    return count;
}

static size_t decodeInstVarCount(const SpurImage& image, size_t classOffset) {
    const uint8_t* heap = image.heapData();
    (void)heap;
    uint64_t instSpec = readSlot(image, classOffset, 2);

    // instSpec is a SmallInteger. Decode it.
    int64_t value;
    if (image.is64Bit()) {
        if ((instSpec & 7) != 1) return 0;  // not a SmallInteger
        value = static_cast<int64_t>(instSpec) >> 3;
    } else {
        if ((instSpec & 1) != 1) return 0;
        value = static_cast<int64_t>(static_cast<int32_t>(instSpec)) >> 1;
    }

    // instSpec layout: bits 0-15 = number of fixed fields (inst vars)
    return static_cast<size_t>(value & 0xFFFF);
}

void ImageExporter::exportHierarchy(const SpurImage& image, const HeapWalker& walker,
                                     const OopValidator& oopVal,
                                     const ClassTableValidator& classTable,
                                     const ClassNameResolver& names, ExportFormat fmt,
                                     FILE* out) {
    // Build class info and parent relationships
    // We need a reverse lookup: heapOffset -> classIndex
    std::unordered_map<size_t, uint32_t> offsetToClassIndex;
    for (auto& [idx, off] : classTable.classMap())
        offsetToClassIndex[off] = idx;

    std::unordered_map<uint32_t, ClassInfo> classes;

    for (auto& [classIdx, classOffset] : classTable.classMap()) {
        ClassInfo info;
        info.classIndex = classIdx;
        info.classOffset = classOffset;
        info.name = names.nameForClassIndex(classIdx);
        info.superclassIndex = 0;
        info.methodCount = countMethods(image, walker, oopVal, classOffset);

        const HeapObject* classObj = walker.objectAtOffset(classOffset);
        if (classObj && isPointerFormat(classObj->format) && classObj->slotCount >= 3) {
            info.instVarCount = decodeInstVarCount(image, classOffset);

            // Resolve superclass
            uint64_t superOop = readSlot(image, classOffset, 0);
            uint64_t nilAddr = image.startOfMemory();
            if (superOop != nilAddr && superOop != 0) {
                OopKind kind = oopVal.classify(superOop);
                if (kind == OopKind::ObjectPointer) {
                    size_t superOffset = image.rawOopToOffset(superOop);
                    if (superOffset != SIZE_MAX) {
                        auto it = offsetToClassIndex.find(superOffset);
                        if (it != offsetToClassIndex.end())
                            info.superclassIndex = it->second;
                    }
                }
            }
        } else {
            info.instVarCount = 0;
        }

        classes[classIdx] = info;
    }

    // Build children map
    std::unordered_map<uint32_t, std::vector<uint32_t>> children;
    std::vector<uint32_t> roots;

    for (auto& [idx, info] : classes) {
        if (info.superclassIndex == 0 || classes.find(info.superclassIndex) == classes.end())
            roots.push_back(idx);
        else
            children[info.superclassIndex].push_back(idx);
    }

    // Sort for deterministic output
    std::sort(roots.begin(), roots.end(), [&](uint32_t a, uint32_t b) {
        return classes[a].name < classes[b].name;
    });
    for (auto& [k, v] : children) {
        std::sort(v.begin(), v.end(), [&](uint32_t a, uint32_t b) {
            return classes[a].name < classes[b].name;
        });
    }

    if (fmt == ExportFormat::Json) {
        fprintf(out, "[\n");
        bool first = true;
        for (auto& [idx, info] : classes) {
            if (!first) fprintf(out, ",\n");
            first = false;
            fprintf(out, "  {\"classIndex\":%u,\"name\":", idx);
            jsonEscape(info.name, out);
            fprintf(out, ",\"superclassIndex\":%u,\"methods\":%zu,\"instVars\":%zu}",
                    info.superclassIndex, info.methodCount, info.instVarCount);
        }
        fprintf(out, "\n]\n");
        return;
    }

    // Text: indented tree
    std::function<void(uint32_t, int)> printTree = [&](uint32_t idx, int depth) {
        auto& info = classes[idx];
        for (int i = 0; i < depth; i++) fprintf(out, "  ");
        fprintf(out, "%s (classIndex=%u, methods=%zu, instVars=%zu)\n",
                info.name.c_str(), idx, info.methodCount, info.instVarCount);
        auto it = children.find(idx);
        if (it != children.end()) {
            for (uint32_t child : it->second)
                printTree(child, depth + 1);
        }
    };

    for (uint32_t root : roots)
        printTree(root, 0);
}

// ============================================================================
// exportGraph
// ============================================================================

void ImageExporter::exportGraph(const SpurImage& image, const HeapWalker& walker,
                                 const OopValidator& oopVal, const ClassNameResolver& names,
                                 size_t graphRoot, int graphDepth,
                                 ExportFormat fmt, FILE* out) {
    // If graphRoot is specified, do BFS from that root up to graphDepth.
    // Otherwise, export all pointer objects.
    bool useSubgraph = (graphRoot != SIZE_MAX);

    std::vector<const HeapObject*> toExport;

    if (useSubgraph) {
        // BFS from graphRoot
        std::unordered_set<size_t> visited;
        std::queue<std::pair<size_t, int>> queue;  // offset, depth
        queue.push({graphRoot, 0});
        visited.insert(graphRoot);

        while (!queue.empty()) {
            auto [offset, depth] = queue.front();
            queue.pop();

            const HeapObject* obj = walker.objectAtOffset(offset);
            if (!obj || obj->isFree) continue;

            toExport.push_back(obj);

            if (graphDepth >= 0 && depth >= graphDepth) continue;
            if (!isPointerFormat(obj->format)) continue;

            for (size_t i = 0; i < obj->slotCount; i++) {
                uint64_t slotOop = readSlot(image, offset, i);
                OopKind kind = oopVal.classify(slotOop);
                if (kind == OopKind::ObjectPointer) {
                    size_t targetOff = image.rawOopToOffset(slotOop);
                    if (targetOff != SIZE_MAX && !visited.count(targetOff)) {
                        visited.insert(targetOff);
                        queue.push({targetOff, depth + 1});
                    }
                }
            }
        }

        // Sort by offset for deterministic output
        std::sort(toExport.begin(), toExport.end(),
                  [](const HeapObject* a, const HeapObject* b) {
                      return a->heapOffset < b->heapOffset;
                  });
    }

    if (fmt == ExportFormat::Json) fprintf(out, "[\n");
    bool firstJson = true;

    auto exportOne = [&](const HeapObject& obj) {
        const std::string& className = names.nameForClassIndex(obj.classIndex);

        // Collect references
        std::vector<std::string> refs;
        if (isPointerFormat(obj.format)) {
            for (size_t i = 0; i < obj.slotCount; i++) {
                uint64_t slotOop = readSlot(image, obj.heapOffset, i);
                OopKind kind = oopVal.classify(slotOop);
                if (kind == OopKind::Nil) {
                    refs.push_back("nil");
                } else if (kind == OopKind::Immediate) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "imm:0x%" PRIx64, slotOop);
                    refs.push_back(buf);
                } else if (kind == OopKind::ObjectPointer) {
                    size_t targetOff = image.rawOopToOffset(slotOop);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%016zx", targetOff);
                    refs.push_back(buf);
                }
            }
        }

        if (fmt == ExportFormat::Text) {
            fprintf(out, "%016zx %-40s ->", obj.heapOffset, className.c_str());
            if (refs.empty()) {
                fprintf(out, " (no refs)");
            } else {
                for (auto& r : refs)
                    fprintf(out, " %s", r.c_str());
            }
            fprintf(out, "\n");
        } else {
            if (!firstJson) fprintf(out, ",\n");
            firstJson = false;
            fprintf(out, "  {\"offset\":\"%016zx\",\"className\":", obj.heapOffset);
            jsonEscape(className, out);
            fprintf(out, ",\"refs\":[");
            for (size_t i = 0; i < refs.size(); i++) {
                if (i > 0) fprintf(out, ",");
                jsonEscape(refs[i], out);
            }
            fprintf(out, "]}");
        }
    };

    if (useSubgraph) {
        for (auto* obj : toExport)
            exportOne(*obj);
    } else {
        for (auto& obj : walker.objects()) {
            if (obj.isFree) continue;
            if (!isPointerFormat(obj.format)) continue;
            exportOne(obj);
        }
    }

    if (fmt == ExportFormat::Json) fprintf(out, "\n]\n");
}
