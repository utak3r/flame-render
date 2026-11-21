// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Piotr Borys ("utak3r") <piotr.borys@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
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
