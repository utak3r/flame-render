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
#include "core/FlameXmlSerializer.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace flame {

namespace {

double parseDoubleAttr(const pugi::xml_attribute& attr,
                       double defaultValue) noexcept {
    const std::string_view sv = attr.value();
    const char* begin = sv.data();
    const char* end = begin + sv.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    if (begin < end && *begin == '+') {
        ++begin;
    }
    double val = defaultValue;
    const auto [ptr, ec] = std::from_chars(begin, end, val);
    return (ec == std::errc{}) ? val : defaultValue;
}

std::vector<double> parseDoubleList(std::string_view str) {
    std::vector<double> result;
    std::istringstream iss{std::string(str)};
    iss.imbue(std::locale::classic());
    double val = 0.0;
    while (iss >> val) {
        result.push_back(val);
    }
    return result;
}

FlameTransform parseXformNode(const pugi::xml_node& node, bool isFinal) {
    FlameTransform xf;
    xf.clear();
    xf.clearVariations();  // XML explicitly lists all non-zero variations

    if (!isFinal) {
        if (auto attr = node.attribute("weight")) {
            xf.weight = parseDoubleAttr(attr, 0.0);
        }
    }

    if (auto attr = node.attribute("color")) {
        xf.color = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("symmetry")) {
        xf.symmetry = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("color_speed")) {
        xf.symmetry = parseDoubleAttr(attr, 0.0);
    }

    if (auto attr = node.attribute("coefs")) {
        const auto vals = parseDoubleList(attr.value());
        if (vals.size() >= 6) {
            xf.coefs[0][0] = vals[0];
            xf.coefs[0][1] = vals[1];
            xf.coefs[1][0] = vals[2];
            xf.coefs[1][1] = vals[3];
            xf.coefs[2][0] = vals[4];
            xf.coefs[2][1] = vals[5];
        }
    }

    if (auto attr = node.attribute("post")) {
        const auto vals = parseDoubleList(attr.value());
        if (vals.size() >= 6) {
            xf.post[0][0] = vals[0];
            xf.post[0][1] = vals[1];
            xf.post[1][0] = vals[2];
            xf.post[1][1] = vals[3];
            xf.post[2][0] = vals[4];
            xf.post[2][1] = vals[5];
        }
    }

    if (auto attr = node.attribute("chaos")) {
        const auto vals = parseDoubleList(attr.value());
        for (size_t i = 0; i < vals.size() && i < xf.modWeights.size(); ++i) {
            xf.modWeights[i] = std::abs(vals[i]);
        }
    }

    if (auto attr = node.attribute("plotmode")) {
        if (std::string_view(attr.value()) == "off") {
            xf.opacity = 0.0;
        } else {
            xf.opacity = 1.0;
        }
    }
    if (auto attr = node.attribute("opacity")) {
        xf.opacity = parseDoubleAttr(attr, 1.0);
    }

    if (xf.opacity <= 0.0)
        xf.plotMode = -1;
    else if (xf.opacity >= 1.0)
        xf.plotMode = 1;
    else
        xf.plotMode = 0;

    // Parse variations by name in canonical order (0..39)
    for (size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        const auto& info = FlameVariation::getInfo(id);
        if (auto attr = node.attribute(std::string(info.name).c_str())) {
            const double w = parseDoubleAttr(attr, 0.0);
            if (w != 0.0) {
                xf.setVariation(id, w);
            }
        }
    }

    // Legacy flam3 'var1' or 'var' attributes
    if (auto attr = node.attribute("var1")) {
        xf.clearVariations();
        const int idx = attr.as_int(-1);
        if (idx >= 0 && static_cast<size_t>(idx) < kNumVariations) {
            xf.setVariation(static_cast<VariationId>(idx), 1.0);
        }
    }
    if (auto attr = node.attribute("var")) {
        xf.clearVariations();
        const auto vals = parseDoubleList(attr.value());
        for (size_t i = 0; i < vals.size() && i < kNumVariations; ++i) {
            if (vals[i] != 0.0) {
                xf.setVariation(static_cast<VariationId>(i), vals[i]);
            }
        }
    }

    // Parse variation parameters
    for (auto paramName : FlameVariation::allParameterNames()) {
        if (auto attr = node.attribute(std::string(paramName).c_str())) {
            xf.params.setParam(paramName, parseDoubleAttr(attr, 0.0));
        }
    }

    return xf;
}

FlameFractal parseFlameNode(const pugi::xml_node& node) {
    FlameFractal cp;
    cp.gammaThreshold =
        0.0;  // Apophysis sets gamma_threshold = 0 if omitted in XML

    if (auto attr = node.attribute("name")) {
        cp.name = attr.value();
    }
    if (auto attr = node.attribute("version")) {
        cp.version = attr.value();
    }
    if (auto attr = node.attribute("time")) {
        cp.time = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("hue")) {
        cp.hueRotation = parseDoubleAttr(attr, 1.0);
    }
    if (auto attr = node.attribute("brightness")) {
        cp.brightness = parseDoubleAttr(attr, 1.0);
    }
    if (auto attr = node.attribute("contrast")) {
        cp.contrast = parseDoubleAttr(attr, 1.0);
    }
    if (auto attr = node.attribute("gamma")) {
        cp.gamma = parseDoubleAttr(attr, 1.0);
    }
    if (auto attr = node.attribute("vibrancy")) {
        cp.vibrancy = parseDoubleAttr(attr, 1.0);
    }
    if (auto attr = node.attribute("gamma_threshold")) {
        cp.gammaThreshold = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("zoom")) {
        cp.zoom = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("scale")) {
        cp.scale = parseDoubleAttr(attr, 50.0);
    }
    if (auto attr = node.attribute("rotate")) {
        cp.setRotateDegrees(parseDoubleAttr(attr, 0.0));
    }
    if (auto attr = node.attribute("angle")) {
        cp.angle = parseDoubleAttr(attr, 0.0);
    }
    if (auto attr = node.attribute("oversample")) {
        cp.oversample = attr.as_int(1);
    }
    if (auto attr = node.attribute("filter")) {
        cp.filterRadius = parseDoubleAttr(attr, 0.5);
    }
    if (auto attr = node.attribute("quality")) {
        cp.quality = parseDoubleAttr(attr, 50.0);
    }
    if (auto attr = node.attribute("batches")) {
        cp.nbatches = attr.as_int(1);
    }
    if (auto attr = node.attribute("soloxform")) {
        cp.soloXform = attr.as_int(-1);
    }
    if (auto attr = node.attribute("nick")) {
        cp.nick = attr.value();
    }
    if (auto attr = node.attribute("url")) {
        cp.url = attr.value();
    }

    if (auto attr = node.attribute("center")) {
        const auto vals = parseDoubleList(attr.value());
        if (vals.size() >= 2) {
            cp.center[0] = vals[0];
            cp.center[1] = vals[1];
        }
    }

    if (auto attr = node.attribute("size")) {
        const auto vals = parseDoubleList(attr.value());
        if (vals.size() >= 2) {
            cp.width = static_cast<int>(vals[0]);
            cp.height = static_cast<int>(vals[1]);
        }
    }

    if (auto attr = node.attribute("background")) {
        const auto vals = parseDoubleList(attr.value());
        if (vals.size() >= 3) {
            cp.background[0] = std::clamp(
                static_cast<int>(std::floor(vals[0] * 255.0)), 0, 255);
            cp.background[1] = std::clamp(
                static_cast<int>(std::floor(vals[1] * 255.0)), 0, 255);
            cp.background[2] = std::clamp(
                static_cast<int>(std::floor(vals[2] * 255.0)), 0, 255);
        }
    }

    for (pugi::xml_node child : node.children()) {
        const std::string_view tagName = child.name();
        if (tagName == "xform") {
            cp.xforms.push_back(parseXformNode(child, false));
        } else if (tagName == "finalxform") {
            cp.finalXform = parseXformNode(child, true);
            if (auto attr = child.attribute("enabled")) {
                cp.finalXformEnabled = (attr.as_int(1) != 0);
            } else {
                cp.finalXformEnabled = true;
            }
        } else if (tagName == "palette") {
            const size_t count =
                static_cast<size_t>(child.attribute("count").as_int(256));
            const std::string_view format = child.attribute("format").value();
            const bool hasAlpha = (format == "RGBA");
            cp.gradient.parseHex(child.child_value(), hasAlpha, count);
        } else if (tagName == "colors") {
            const size_t count =
                static_cast<size_t>(child.attribute("count").as_int(256));
            cp.gradient.parseHex(child.attribute("data").value(), true, count);
        } else if (tagName == "color") {
            const int idx = child.attribute("index").as_int(-1);
            if (idx >= 0 && idx < 256) {
                const auto vals =
                    parseDoubleList(child.attribute("rgb").value());
                if (vals.size() >= 3) {
                    cp.gradient[static_cast<size_t>(idx)] = RGBColor{
                        static_cast<uint8_t>(std::clamp(
                            static_cast<int>(std::nearbyint(vals[0])), 0, 255)),
                        static_cast<uint8_t>(std::clamp(
                            static_cast<int>(std::nearbyint(vals[1])), 0, 255)),
                        static_cast<uint8_t>(std::clamp(
                            static_cast<int>(std::nearbyint(vals[2])), 0,
                            255))};
                }
            }
        } else if (tagName == "symmetry") {
            const int kind = child.attribute("kind").as_int(0);
            if (kind != 0) {
                cp.addSymmetry(kind);
            }
        }
    }

    return cp;
}

std::string formatDouble(double value) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(15) << value;
    return oss.str();
}

std::string formatDoubleList(std::initializer_list<double> values) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(15);
    bool first = true;
    for (double v : values) {
        if (!first) {
            oss << ' ';
        }
        oss << v;
        first = false;
    }
    return oss.str();
}

void appendXformNode(pugi::xml_node parent,
                     const FlameTransform& xf,
                     bool isFinal,
                     bool finalEnabled) {
    pugi::xml_node node = parent.append_child(isFinal ? "finalxform" : "xform");

    if (isFinal) {
        if (!finalEnabled) {
            node.append_attribute("enabled") = 0;
        }
    } else {
        node.append_attribute("weight") = formatDouble(xf.weight).c_str();
    }
    node.append_attribute("color") = formatDouble(xf.color).c_str();

    if (xf.symmetry != 0.0) {
        node.append_attribute("symmetry") = formatDouble(xf.symmetry).c_str();
    }
    if (!isFinal && xf.opacity != 1.0) {
        node.append_attribute("opacity") = formatDouble(xf.opacity).c_str();
    }

    for (size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        const double w = xf.variationWeight(id);
        if (w != 0.0) {
            node.append_attribute(
                std::string(FlameVariation::getInfo(id).name).c_str()) =
                formatDouble(w).c_str();
        }
    }

    node.append_attribute("coefs") =
        formatDoubleList({xf.coefs[0][0], xf.coefs[0][1], xf.coefs[1][0],
                          xf.coefs[1][1], xf.coefs[2][0], xf.coefs[2][1]})
            .c_str();

    if (xf.hasPostTransform()) {
        node.append_attribute("post") =
            formatDoubleList({xf.post[0][0], xf.post[0][1], xf.post[1][0],
                              xf.post[1][1], xf.post[2][0], xf.post[2][1]})
                .c_str();
    }

    for (size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        if (xf.variationWeight(id) != 0.0) {
            const auto& info = FlameVariation::getInfo(id);
            for (auto paramName : info.parameterNames) {
                if (auto val = xf.params.getParam(paramName)) {
                    node.append_attribute(std::string(paramName).c_str()) =
                        formatDouble(*val).c_str();
                }
            }
        }
    }

    if (!isFinal) {
        int lastNonOne = -1;
        for (int i = static_cast<int>(xf.modWeights.size()) - 1; i >= 0; --i) {
            if (xf.modWeights[static_cast<size_t>(i)] != 1.0) {
                lastNonOne = i;
                break;
            }
        }
        if (lastNonOne >= 0) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setprecision(15);
            for (int i = 0; i <= lastNonOne; ++i) {
                if (i > 0) {
                    oss << ' ';
                }
                oss << xf.modWeights[static_cast<size_t>(i)];
            }
            node.append_attribute("chaos") = oss.str().c_str();
        }
    }
}

void appendFlameNode(pugi::xml_node parent, const FlameFractal& cp) {
    pugi::xml_node node = parent.append_child("flame");
    node.append_attribute("name") = cp.name.c_str();
    node.append_attribute("version") = cp.version.c_str();

    if (!cp.nick.empty()) {
        node.append_attribute("nick") = cp.nick.c_str();
    }
    if (!cp.url.empty()) {
        node.append_attribute("url") = cp.url.c_str();
    }
    if (cp.time != 0.0) {
        node.append_attribute("time") = formatDouble(cp.time).c_str();
    }

    node.append_attribute("size") =
        (std::to_string(cp.width) + " " + std::to_string(cp.height)).c_str();
    node.append_attribute("center") =
        formatDoubleList({cp.center[0], cp.center[1]}).c_str();
    node.append_attribute("scale") = formatDouble(cp.scale).c_str();

    if (cp.angle != 0.0) {
        node.append_attribute("rotate") =
            formatDouble(cp.rotateDegrees()).c_str();
    }
    if (cp.zoom != 0.0) {
        node.append_attribute("zoom") = formatDouble(cp.zoom).c_str();
    }

    node.append_attribute("oversample") = cp.oversample;
    node.append_attribute("filter") = formatDouble(cp.filterRadius).c_str();
    node.append_attribute("quality") = formatDouble(cp.quality).c_str();

    if (cp.nbatches != 1) {
        node.append_attribute("batches") = cp.nbatches;
    }

    node.append_attribute("background") =
        formatDoubleList({cp.background[0] / 255.0, cp.background[1] / 255.0,
                          cp.background[2] / 255.0})
            .c_str();

    if (cp.hueRotation != 1.0) {
        node.append_attribute("hue") = formatDouble(cp.hueRotation).c_str();
    }
    node.append_attribute("brightness") = formatDouble(cp.brightness).c_str();
    if (cp.contrast != 1.0) {
        node.append_attribute("contrast") = formatDouble(cp.contrast).c_str();
    }
    node.append_attribute("gamma") = formatDouble(cp.gamma).c_str();

    if (cp.vibrancy != 1.0) {
        node.append_attribute("vibrancy") = formatDouble(cp.vibrancy).c_str();
    }
    if (cp.gammaThreshold != 0.0) {
        node.append_attribute("gamma_threshold") =
            formatDouble(cp.gammaThreshold).c_str();
    }
    if (cp.soloXform >= 0) {
        node.append_attribute("soloxform") = cp.soloXform;
    }

    for (const auto& xf : cp.xforms) {
        appendXformNode(node, xf, false, true);
    }
    if (cp.hasFinalXform()) {
        appendXformNode(node, *cp.finalXform, true, cp.finalXformEnabled);
    }

    pugi::xml_node palette = node.append_child("palette");
    palette.append_attribute("count") = 256;
    palette.append_attribute("format") = "RGB";
    const std::string paletteText = "\n" + cp.gradient.toHex(false) + "\n   ";
    palette.text().set(paletteText.c_str());
}

}  // namespace

std::vector<FlameFractal> FlameXmlSerializer::loadFromFile(
    const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Cannot open flame file: " + path.string());
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return loadFromString(oss.str());
}

std::vector<FlameFractal> FlameXmlSerializer::loadFromString(
    std::string_view xmlContent) {
    pugi::xml_document doc;
    pugi::xml_parse_result res =
        doc.load_buffer(xmlContent.data(), xmlContent.size());
    if (!res) {
        // Attempt wrapping in <Flames> if file contains multiple top-level
        // <flame> elements without a root
        std::string wrapped =
            "<Flames>\n" + std::string(xmlContent) + "\n</Flames>";
        res = doc.load_buffer(wrapped.data(), wrapped.size());
        if (!res) {
            throw std::runtime_error(std::string("XML parse error: ") +
                                     res.description());
        }
    }

    std::vector<FlameFractal> fractals;
    for (pugi::xml_node node : doc.children()) {
        const std::string_view name = node.name();
        if (name == "flame") {
            fractals.push_back(parseFlameNode(node));
        } else if (name == "Flames" || name == "flames") {
            for (pugi::xml_node flameNode : node.children("flame")) {
                fractals.push_back(parseFlameNode(flameNode));
            }
        }
    }
    return fractals;
}

std::string FlameXmlSerializer::toXmlString(const FlameFractal& cp) {
    pugi::xml_document doc;
    appendFlameNode(doc, cp);
    std::ostringstream oss;
    doc.save(oss, "   ", pugi::format_default | pugi::format_no_declaration);
    return oss.str();
}

std::string FlameXmlSerializer::toXmlString(
    const std::vector<FlameFractal>& fractals,
    std::string_view collectionName) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("Flames");
    root.append_attribute("name") = std::string(collectionName).c_str();
    for (const auto& f : fractals) {
        appendFlameNode(root, f);
    }
    std::ostringstream oss;
    doc.save(oss, "   ", pugi::format_default | pugi::format_no_declaration);
    return oss.str();
}

bool FlameXmlSerializer::saveToFile(const std::vector<FlameFractal>& fractals,
                                    const std::filesystem::path& path,
                                    std::string_view collectionName) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        return false;
    ofs << toXmlString(fractals, collectionName);
    return ofs.good();
}

}  // namespace flame
