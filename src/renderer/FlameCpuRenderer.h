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

#include "renderer/FlameToneMapper.h"
#include "renderer/IFlameRenderer.h"

#include <atomic>

namespace flame {

class FlameCpuRenderer final : public IFlameRenderer {
   public:
    static constexpr int kMaxFilterWidth = 25;

    FlameCpuRenderer() = default;
    ~FlameCpuRenderer() override = default;

    void setFractal(const FlameFractal* fractal) override;
    FlameImage render(const RenderOptions& options = {}) override;
    void stop() override;
    RenderStats getLastStats() const override { return lastStats_; }

    // Apply RenderOptions overrides onto a copy of the fractal
    static FlameFractal resolveEffectiveFractal(const FlameFractal& base,
                                                const RenderOptions& options);

   private:
    const FlameFractal* fractal_ = nullptr;
    std::atomic<bool> stopRequested_{false};
    RenderStats lastStats_{};
};

}  // namespace flame
