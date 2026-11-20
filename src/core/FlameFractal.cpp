#include "core/FlameFractal.h"

#include <cmath>
#include <numbers>
#include <random>

#include "core/FlameXmlSerializer.h"

namespace flame {

FlameFractal::FlameFractal() = default;

double FlameFractal::ppux() const noexcept {
    return scale * std::pow(2.0, zoom);
}

double FlameFractal::ppuy() const noexcept {
    return scale * std::pow(2.0, zoom);
}

bool FlameFractal::hasFinalXform() const noexcept {
    return finalXform.has_value() && !finalXform->isIdentityFinal();
}

bool FlameFractal::useFinalXform() const noexcept {
    return finalXformEnabled && hasFinalXform();
}

void FlameFractal::adjustScale(int newWidth, int newHeight) noexcept {
    if (width > 0 && newWidth > 0) {
        scale =
            scale * static_cast<double>(newWidth) / static_cast<double>(width);
    }
    if (newWidth > 0)
        width = newWidth;
    if (newHeight > 0)
        height = newHeight;
}

int FlameFractal::addSymmetry(int sym) {
    if (sym == 0 || sym == 1)
        return 0;

    int added = 0;
    if (sym < 0) {
        FlameTransform xf;
        xf.weight = 1.0;
        xf.symmetry = 1.0;
        xf.color = 1.0;
        xf.clearVariations();
        xf.setVariation(VariationId::Linear, 1.0);
        xf.coefs = {{{-1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};
        xforms.push_back(std::move(xf));
        ++added;
        sym = -sym;
    }

    const double a = 2.0 * std::numbers::pi / static_cast<double>(sym);
    for (int k = 1; k < sym; ++k) {
        FlameTransform xf;
        xf.weight = 1.0;
        xf.symmetry = 1.0;
        xf.clearVariations();
        xf.setVariation(VariationId::Linear, 1.0);
        xf.color = (sym < 3) ? 0.0
                             : static_cast<double>(k - 1) /
                                   static_cast<double>(sym - 2);
        while (xf.color > 1.0)
            xf.color -= 1.0;

        const double ca = std::cos(k * a);
        const double sa = std::sin(k * a);
        xf.coefs = {{{ca, sa}, {-sa, ca}, {0.0, 0.0}}};
        xforms.push_back(std::move(xf));
        ++added;
    }
    return added;
}

void FlameFractal::equalizeWeights() noexcept {
    for (auto& xf : xforms) {
        xf.weight = 0.5;
    }
}

void FlameFractal::normalizeWeights() noexcept {
    if (xforms.empty()) {
        return;
    }
    double total = 0.0;
    for (const auto& xf : xforms) {
        total += xf.weight;
    }
    if (total < 0.001) {
        const double w = 1.0 / static_cast<double>(xforms.size());
        for (auto& xf : xforms) {
            xf.weight = w;
        }
    } else {
        for (auto& xf : xforms) {
            xf.weight /= total;
        }
    }
}

FlameFractal FlameFractal::createRandom(int minXforms,
                                        int maxXforms,
                                        uint64_t seed) {
    std::mt19937_64 rng(seed ? seed : std::random_device{}());
    if (minXforms < 1)
        minXforms = 1;
    if (maxXforms < minXforms)
        maxXforms = minXforms;

    std::uniform_int_distribution<int> countDist(minXforms, maxXforms);
    std::uniform_real_distribution<double> coefDist(-1.0, 1.0);
    std::uniform_real_distribution<double> transDist(-1.5, 1.5);
    std::uniform_int_distribution<int> varDist(0, 27);

    FlameFractal flame;
    flame.name = "RandomFlame";
    flame.width = 520;
    flame.height = 390;
    flame.scale = 130.0;
    flame.oversample = 2;
    flame.filterRadius = 0.4;
    flame.quality = 500.0;
    flame.brightness = 4.0;
    flame.gamma = 3.0;
    flame.gammaThreshold = 0.0;
    flame.gradient = FlameGradient::createRandom(rng());

    const int n = countDist(rng);
    flame.xforms.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        auto& xf = flame.xforms[static_cast<size_t>(i)];
        xf.weight = 1.0 / static_cast<double>(n);
        xf.color =
            (n > 1) ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.0;
        xf.coefs[0][0] = coefDist(rng);
        xf.coefs[0][1] = coefDist(rng);
        xf.coefs[1][0] = coefDist(rng);
        xf.coefs[1][1] = coefDist(rng);
        xf.coefs[2][0] = transDist(rng);
        xf.coefs[2][1] = transDist(rng);

        xf.clearVariations();
        xf.setVariation(VariationId::Linear, 0.5);
        xf.setVariation(static_cast<VariationId>(varDist(rng)), 0.5);
    }
    return flame;
}

std::vector<FlameFractal> FlameFractal::loadFromXmlFile(
    const std::filesystem::path& path) {
    return FlameXmlSerializer::loadFromFile(path);
}

std::vector<FlameFractal> FlameFractal::loadFromXmlString(
    std::string_view xmlContent) {
    return FlameXmlSerializer::loadFromString(xmlContent);
}

std::string FlameFractal::toXmlString() const {
    return FlameXmlSerializer::toXmlString(*this);
}

bool FlameFractal::saveToXmlFile(const std::vector<FlameFractal>& fractals,
                                 const std::filesystem::path& path,
                                 std::string_view collectionName) {
    return FlameXmlSerializer::saveToFile(fractals, path, collectionName);
}

}  // namespace flame
