#pragma once

#include "core/FlameFractal.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace flame {

class FlameXmlSerializer {
   public:
    static std::vector<FlameFractal> loadFromFile(
        const std::filesystem::path& path);
    static std::vector<FlameFractal> loadFromString(
        std::string_view xmlContent);

    static std::string toXmlString(const FlameFractal& fractal);
    static std::string toXmlString(const std::vector<FlameFractal>& fractals,
                                   std::string_view collectionName = "renders");
    static bool saveToFile(const std::vector<FlameFractal>& fractals,
                           const std::filesystem::path& path,
                           std::string_view collectionName = "renders");
};

}  // namespace flame
