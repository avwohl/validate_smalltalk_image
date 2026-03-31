#pragma once
#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "ClassTableValidator.hpp"
#include "ClassNameResolver.hpp"
#include "ObjectHasher.hpp"
#include <cstdio>
#include <string>

enum class ExportFormat { Text, Json, Csv };

class ImageExporter {
public:
    // --export-shasum: lightweight per-object hash manifest
    static void exportShasum(const SpurImage& image, const HeapWalker& walker,
                             const ClassNameResolver& names, const ObjectHasher& hasher,
                             const std::string& filterClass, FILE* out);

    // --export-catalog: full object metadata
    static void exportCatalog(const SpurImage& image, const HeapWalker& walker,
                              const ClassNameResolver& names, const ObjectHasher& hasher,
                              const std::string& filterClass, ExportFormat fmt, FILE* out);

    // --export-hierarchy: class inheritance tree
    static void exportHierarchy(const SpurImage& image, const HeapWalker& walker,
                                const OopValidator& oopVal,
                                const ClassTableValidator& classTable,
                                const ClassNameResolver& names, ExportFormat fmt, FILE* out);

    // --export-graph: object reference adjacency list
    static void exportGraph(const SpurImage& image, const HeapWalker& walker,
                            const OopValidator& oopVal, const ClassNameResolver& names,
                            size_t graphRoot, int graphDepth,
                            ExportFormat fmt, FILE* out);
};
