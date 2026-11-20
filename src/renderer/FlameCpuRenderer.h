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
