#pragma once
#include "ValidationContext.hpp"
#include "SpurImage.hpp"
#include <string>

class Report {
public:
    static void printText(const SpurImage& image, const ValidationContext& ctx);
    static void printJson(const SpurImage& image, const ValidationContext& ctx);

private:
    static std::string formatBytes(size_t bytes);
};
