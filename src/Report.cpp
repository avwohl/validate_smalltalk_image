#include "Report.hpp"
#include <cstdio>
#include <algorithm>
#include <vector>

std::string Report::formatBytes(size_t bytes) {
    char buf[64];
    if (bytes >= 1024*1024*1024) {
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0*1024*1024));
    } else if (bytes >= 1024*1024) {
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0*1024));
    } else if (bytes >= 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%zu bytes", bytes);
    }
    return buf;
}

void Report::printText(const SpurImage& image, const ValidationContext& ctx) {
    auto& stats = ctx.stats();
    auto& header = image.header();

    printf("=== Smalltalk Image Validation Report ===\n\n");

    // Image info
    printf("Image format:      %u (%s)\n", header.imageFormat,
           image.is64Bit() ? "Spur 64-bit" : "Spur 32-bit");
    printf("Header size:       %u bytes\n", header.headerSize);
    printf("Heap size:         %s (%llu bytes)\n",
           formatBytes(header.imageBytes).c_str(),
           (unsigned long long)header.imageBytes);
    printf("Base address:      0x%llx\n", (unsigned long long)header.startOfMemory);
    printf("Special objects:   0x%llx\n", (unsigned long long)header.specialObjectsOop);
    printf("Flags:             0x%llx", (unsigned long long)header.imageHeaderFlags);
    if (header.imageHeaderFlags & Flag_FullBlockClosures) printf(" FullBlockClosures");
    if (header.imageHeaderFlags & Flag_SistaV1) printf(" SistaV1");
    if (header.imageHeaderFlags & Flag_PreemptionYields) printf(" PreemptionYields");
    printf("\n\n");

    // Statistics
    printf("=== Heap Statistics ===\n\n");
    printf("Total objects:     %zu\n", stats.totalObjects);
    printf("Total size:        %s\n", formatBytes(stats.totalBytes).c_str());
    printf("Free chunks:       %zu (%s)\n", stats.freeChunks, formatBytes(stats.freeBytes).c_str());
    printf("Heap utilization:  %.1f%%\n",
           100.0 * stats.totalBytes / (stats.totalBytes + stats.freeBytes));
    printf("\n");
    printf("Pointer objects:   %zu\n", stats.pointerObjects);
    printf("Byte objects:      %zu\n", stats.byteObjects);
    printf("Word objects:      %zu\n", stats.wordObjects);
    printf("Compiled methods:  %zu\n", stats.compiledMethods);
    printf("Weak objects:      %zu\n", stats.weakObjects);
    printf("Zero-sized:        %zu\n", stats.zeroSizedObjects);
    printf("Overflow headers:  %zu\n", stats.overflowObjects);
    printf("Pinned:            %zu\n", stats.pinnedObjects);
    printf("Immutable:         %zu\n", stats.immutableObjects);
    printf("Max slot count:    %zu\n", stats.maxSlotCount);
    printf("Max object size:   %s\n", formatBytes(stats.maxObjectBytes).c_str());
    printf("Class table:       %zu entries\n", stats.classTableEntries);
    printf("\n");

    // Top 20 classes by instance count
    if (!stats.classHistogram.empty()) {
        printf("=== Top 20 Classes by Instance Count ===\n\n");
        std::vector<std::pair<uint32_t, size_t>> sorted(
            stats.classHistogram.begin(), stats.classHistogram.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });
        size_t shown = 0;
        for (auto& [idx, count] : sorted) {
            if (shown >= 20) break;
            printf("  class %5u:  %8zu instances\n", idx, count);
            shown++;
        }
        printf("\n");
    }

    // Findings
    size_t errors = 0, warnings = 0;
    for (auto& f : ctx.findings()) {
        if (f.severity == Severity::Error) errors++;
        else if (f.severity == Severity::Warning) warnings++;
    }

    if (errors > 0) {
        printf("=== Errors (%zu) ===\n\n", errors);
        for (auto& f : ctx.findings()) {
            if (f.severity != Severity::Error) continue;
            if (f.offset)
                printf("  [%s] at 0x%llx: %s\n", f.category.c_str(),
                       (unsigned long long)f.offset, f.message.c_str());
            else
                printf("  [%s]: %s\n", f.category.c_str(), f.message.c_str());
        }
        printf("\n");
    }

    if (warnings > 0) {
        printf("=== Warnings (%zu) ===\n\n", warnings);
        for (auto& f : ctx.findings()) {
            if (f.severity != Severity::Warning) continue;
            if (f.offset)
                printf("  [%s] at 0x%llx: %s\n", f.category.c_str(),
                       (unsigned long long)f.offset, f.message.c_str());
            else
                printf("  [%s]: %s\n", f.category.c_str(), f.message.c_str());
        }
        printf("\n");
    }

    // Summary
    printf("=== Summary ===\n\n");
    if (errors == 0 && warnings == 0) {
        printf("PASS: No errors or warnings.\n");
    } else if (errors == 0) {
        printf("PASS: No errors, %zu warning(s).\n", warnings);
    } else {
        printf("FAIL: %zu error(s), %zu warning(s).\n", errors, warnings);
    }
}

void Report::printJson(const SpurImage& image, const ValidationContext& ctx) {
    auto& stats = ctx.stats();
    auto& header = image.header();

    printf("{\n");
    printf("  \"imageFormat\": %u,\n", header.imageFormat);
    printf("  \"is64Bit\": %s,\n", image.is64Bit() ? "true" : "false");
    printf("  \"heapBytes\": %llu,\n", (unsigned long long)header.imageBytes);
    printf("  \"baseAddress\": %llu,\n", (unsigned long long)header.startOfMemory);
    printf("  \"stats\": {\n");
    printf("    \"totalObjects\": %zu,\n", stats.totalObjects);
    printf("    \"totalBytes\": %zu,\n", stats.totalBytes);
    printf("    \"freeChunks\": %zu,\n", stats.freeChunks);
    printf("    \"freeBytes\": %zu,\n", stats.freeBytes);
    printf("    \"pointerObjects\": %zu,\n", stats.pointerObjects);
    printf("    \"byteObjects\": %zu,\n", stats.byteObjects);
    printf("    \"compiledMethods\": %zu,\n", stats.compiledMethods);
    printf("    \"weakObjects\": %zu,\n", stats.weakObjects);
    printf("    \"classTableEntries\": %zu,\n", stats.classTableEntries);
    printf("    \"maxSlotCount\": %zu,\n", stats.maxSlotCount);
    printf("    \"maxObjectBytes\": %zu\n", stats.maxObjectBytes);
    printf("  },\n");

    printf("  \"errors\": %zu,\n", ctx.errorCount());
    printf("  \"warnings\": %zu,\n", ctx.warningCount());

    printf("  \"findings\": [\n");
    bool first = true;
    for (auto& f : ctx.findings()) {
        if (f.severity == Severity::Info) continue;
        if (!first) printf(",\n");
        first = false;
        printf("    {\"severity\": \"%s\", \"category\": \"%s\", \"offset\": %llu, \"message\": \"",
               f.severity == Severity::Error ? "error" : "warning",
               f.category.c_str(), (unsigned long long)f.offset);
        // Escape JSON string
        for (char c : f.message) {
            if (c == '"') printf("\\\"");
            else if (c == '\\') printf("\\\\");
            else if (c == '\n') printf("\\n");
            else putchar(c);
        }
        printf("\"}");
    }
    printf("\n  ],\n");

    printf("  \"result\": \"%s\"\n", ctx.hasErrors() ? "FAIL" : "PASS");
    printf("}\n");
}
