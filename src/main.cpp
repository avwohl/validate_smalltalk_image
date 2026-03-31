#include "SpurImage.hpp"
#include "HeapWalker.hpp"
#include "OopValidator.hpp"
#include "HeapValidator.hpp"
#include "ClassTableValidator.hpp"
#include "ClassHierarchyValidator.hpp"
#include "SpecialObjectsValidator.hpp"
#include "MethodValidator.hpp"
#include "ReachabilityAnalyzer.hpp"
#include "Report.hpp"
#include "ClassNameResolver.hpp"
#include "ObjectHasher.hpp"
#include "ImageExporter.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] <image-file>\n\n", prog);
    fprintf(stderr, "Validate a Spur-format Smalltalk image (Pharo, Squeak, Cuis).\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --ios-tags       Use iOS tag variant (iospharo)\n");
    fprintf(stderr, "  --reachability   Run reachability analysis (slower)\n");
    fprintf(stderr, "  --json           Output JSON instead of text\n");
    fprintf(stderr, "  --verbose        Print findings as they are discovered\n");
    fprintf(stderr, "  --max-errors N   Stop after N errors (default: unlimited)\n");
    fprintf(stderr, "  --help           Show this help\n");
    fprintf(stderr, "\nExport options (mutually exclusive):\n");
    fprintf(stderr, "  --export-shasum      Per-object SHA-256 manifest (for diff)\n");
    fprintf(stderr, "  --export-catalog     Full object metadata catalog\n");
    fprintf(stderr, "  --export-hierarchy   Class inheritance tree\n");
    fprintf(stderr, "  --export-graph       Object reference graph\n");
    fprintf(stderr, "\nExport modifiers:\n");
    fprintf(stderr, "  --csv                CSV output (with --export-catalog)\n");
    fprintf(stderr, "  --filter-class NAME  Restrict to instances of named class\n");
    fprintf(stderr, "  --graph-root OFFSET  Hex offset of subgraph root\n");
    fprintf(stderr, "  --graph-depth N      Max traversal depth (with --export-graph)\n");
    fprintf(stderr, "\nSupports Spur 64-bit (format 68021, 68533) and Spur 32-bit\n");
    fprintf(stderr, "(format 6521, 6505). Compatible with Pharo 5+, Squeak 5+, Cuis 6+.\n");
}

int main(int argc, char* argv[]) {
    std::string imagePath;
    bool iosTags = false;
    bool reachability = false;
    bool json = false;
    bool csv = false;
    bool verbose = false;
    size_t maxErrors = 0;

    // Export modes
    bool exportShasum = false;
    bool exportCatalog = false;
    bool exportHierarchy = false;
    bool exportGraph = false;
    std::string filterClass;
    size_t graphRoot = SIZE_MAX;
    int graphDepth = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ios-tags") == 0) {
            iosTags = true;
        } else if (strcmp(argv[i], "--reachability") == 0) {
            reachability = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--max-errors") == 0 && i + 1 < argc) {
            maxErrors = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--export-shasum") == 0) {
            exportShasum = true;
        } else if (strcmp(argv[i], "--export-catalog") == 0) {
            exportCatalog = true;
        } else if (strcmp(argv[i], "--export-hierarchy") == 0) {
            exportHierarchy = true;
        } else if (strcmp(argv[i], "--export-graph") == 0) {
            exportGraph = true;
        } else if (strcmp(argv[i], "--filter-class") == 0 && i + 1 < argc) {
            filterClass = argv[++i];
        } else if (strcmp(argv[i], "--graph-root") == 0 && i + 1 < argc) {
            graphRoot = strtoull(argv[++i], nullptr, 16);
        } else if (strcmp(argv[i], "--graph-depth") == 0 && i + 1 < argc) {
            graphDepth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        } else {
            imagePath = argv[i];
        }
    }

    bool anyExport = exportShasum || exportCatalog || exportHierarchy || exportGraph;
    int exportCount = (exportShasum ? 1 : 0) + (exportCatalog ? 1 : 0) +
                      (exportHierarchy ? 1 : 0) + (exportGraph ? 1 : 0);
    if (exportCount > 1) {
        fprintf(stderr, "Error: only one --export-* flag at a time.\n");
        return 2;
    }

    if (imagePath.empty()) {
        usage(argv[0]);
        return 2;
    }

    ValidationContext ctx;
    ctx.setVerbose(verbose);
    if (maxErrors > 0) ctx.setMaxErrors(maxErrors);

    // 1. Load image
    SpurImage image;
    if (!image.load(imagePath, ctx)) {
        if (json) Report::printJson(image, ctx);
        else Report::printText(image, ctx);
        return 1;
    }

    if (!json || anyExport) {
        fprintf(stderr, "Loaded %s (%s, %s)\n",
                imagePath.c_str(),
                image.is64Bit() ? "64-bit" : "32-bit",
                iosTags ? "iOS tags" : "standard tags");
    }

    // 2. Walk heap
    HeapWalker walker;
    if (!walker.walk(image, ctx)) {
        if (json) Report::printJson(image, ctx);
        else Report::printText(image, ctx);
        return 1;
    }

    if (!json || anyExport) {
        fprintf(stderr, "Heap walk complete: %zu objects, %zu free chunks\n",
                ctx.stats().totalObjects, ctx.stats().freeChunks);
    }

    // 3. Validate Oops and heap structure
    OopValidator oopVal(image, walker, iosTags);
    HeapValidator heapVal(image, walker, oopVal);
    heapVal.validate(ctx);

    // 4. Class table
    ClassTableValidator classTableVal(image, walker, oopVal);
    classTableVal.validate(ctx);

    // 5. Special objects
    SpecialObjectsValidator specVal(image, walker, oopVal);
    specVal.validate(ctx);

    // 6. Class hierarchy
    ClassHierarchyValidator hierVal(image, walker, oopVal, classTableVal);
    hierVal.validate(ctx);

    // 7. Compiled methods
    MethodValidator methodVal(image, walker, oopVal);
    methodVal.validate(ctx);

    // 8. Reachability (optional)
    if (reachability) {
        if (!json || anyExport) fprintf(stderr, "Running reachability analysis...\n");
        ReachabilityAnalyzer reach(image, walker, oopVal, classTableVal);
        reach.analyze(ctx);
        if (!json || anyExport) {
            fprintf(stderr, "Reachable: %zu, unreachable: %zu\n",
                    reach.reachableCount(), reach.unreachableCount());
        }
    }

    // 9. Export or Report
    if (anyExport) {
        // Resolve class names for export
        ClassNameResolver nameResolver(image, walker, oopVal, classTableVal);
        nameResolver.resolve();
        fprintf(stderr, "Resolved %zu class names\n", nameResolver.classNames().size());

        ExportFormat efmt = ExportFormat::Text;
        if (json) efmt = ExportFormat::Json;
        else if (csv) efmt = ExportFormat::Csv;

        if (exportShasum) {
            ImageExporter::exportShasum(image, walker, nameResolver,
                                        ObjectHasher(image), filterClass, stdout);
        } else if (exportCatalog) {
            ImageExporter::exportCatalog(image, walker, nameResolver,
                                          ObjectHasher(image), filterClass, efmt, stdout);
        } else if (exportHierarchy) {
            ImageExporter::exportHierarchy(image, walker, oopVal, classTableVal,
                                            nameResolver, efmt, stdout);
        } else if (exportGraph) {
            ImageExporter::exportGraph(image, walker, oopVal, nameResolver,
                                        graphRoot, graphDepth, efmt, stdout);
        }

        return 0;
    }

    // Standard validation report
    if (json) Report::printJson(image, ctx);
    else Report::printText(image, ctx);

    return ctx.hasErrors() ? 1 : 0;
}
