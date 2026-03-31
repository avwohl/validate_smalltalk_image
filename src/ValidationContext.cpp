#include "ValidationContext.hpp"

void ValidationContext::addError(const std::string& category, const std::string& msg, uint64_t offset) {
    findings_.push_back({Severity::Error, category, msg, offset});
    errorCount_++;
    if (verbose_) {
        if (offset)
            fprintf(stderr, "ERROR [%s] at 0x%llx: %s\n", category.c_str(),
                    (unsigned long long)offset, msg.c_str());
        else
            fprintf(stderr, "ERROR [%s]: %s\n", category.c_str(), msg.c_str());
    }
}

void ValidationContext::addWarning(const std::string& category, const std::string& msg, uint64_t offset) {
    findings_.push_back({Severity::Warning, category, msg, offset});
    warningCount_++;
    if (verbose_) {
        if (offset)
            fprintf(stderr, "WARN  [%s] at 0x%llx: %s\n", category.c_str(),
                    (unsigned long long)offset, msg.c_str());
        else
            fprintf(stderr, "WARN  [%s]: %s\n", category.c_str(), msg.c_str());
    }
}

void ValidationContext::addInfo(const std::string& category, const std::string& msg, uint64_t offset) {
    findings_.push_back({Severity::Info, category, msg, offset});
}
