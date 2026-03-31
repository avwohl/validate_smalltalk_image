#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

enum class Severity { Error, Warning, Info };

struct Finding {
    Severity severity;
    std::string category;
    std::string message;
    uint64_t offset;  // heap offset where issue was found (0 if N/A)
};

struct ImageStats {
    size_t totalObjects = 0;
    size_t totalBytes = 0;
    size_t freeChunks = 0;
    size_t freeBytes = 0;
    size_t pointerObjects = 0;
    size_t byteObjects = 0;
    size_t wordObjects = 0;
    size_t compiledMethods = 0;
    size_t weakObjects = 0;
    size_t zeroSizedObjects = 0;
    size_t overflowObjects = 0;  // objects with >254 slots
    size_t pinnedObjects = 0;
    size_t immutableObjects = 0;
    size_t maxSlotCount = 0;
    size_t maxObjectBytes = 0;
    size_t classTableEntries = 0;
    size_t uniqueClassIndices = 0;

    // class index histogram: classIndex -> instance count
    std::unordered_map<uint32_t, size_t> classHistogram;
};

class ValidationContext {
public:
    void addError(const std::string& category, const std::string& msg, uint64_t offset = 0);
    void addWarning(const std::string& category, const std::string& msg, uint64_t offset = 0);
    void addInfo(const std::string& category, const std::string& msg, uint64_t offset = 0);

    bool hasErrors() const { return errorCount_ > 0; }
    size_t errorCount() const { return errorCount_; }
    size_t warningCount() const { return warningCount_; }

    const std::vector<Finding>& findings() const { return findings_; }
    ImageStats& stats() { return stats_; }
    const ImageStats& stats() const { return stats_; }

    void setMaxErrors(size_t max) { maxErrors_ = max; }
    bool shouldStop() const { return maxErrors_ > 0 && errorCount_ >= maxErrors_; }

    void setVerbose(bool v) { verbose_ = v; }
    bool verbose() const { return verbose_; }

private:
    std::vector<Finding> findings_;
    ImageStats stats_;
    size_t errorCount_ = 0;
    size_t warningCount_ = 0;
    size_t maxErrors_ = 0;  // 0 = unlimited
    bool verbose_ = false;
};
