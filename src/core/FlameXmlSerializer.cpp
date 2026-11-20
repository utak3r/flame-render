#include "core/FlameXmlSerializer.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace flame {

namespace {

std::vector<double> parseDoubleList(std::string_view str) {
    std::vector<double> result;
    std::istringstream iss{std::string(str)};
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
            xf.weight = attr.as_double(0.0);
        }
    }

    if (auto attr = node.attribute("color")) {
        xf.color = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("symmetry")) {
        xf.symmetry = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("color_speed")) {
        xf.symmetry = attr.as_double(0.0);
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
        xf.opacity = attr.as_double(1.0);
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
            const double w = attr.as_double(0.0);
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
            xf.params.setParam(paramName, attr.as_double(0.0));
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
        cp.time = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("hue")) {
        cp.hueRotation = attr.as_double(1.0);
    }
    if (auto attr = node.attribute("brightness")) {
        cp.brightness = attr.as_double(1.0);
    }
    if (auto attr = node.attribute("contrast")) {
        cp.contrast = attr.as_double(1.0);
    }
    if (auto attr = node.attribute("gamma")) {
        cp.gamma = attr.as_double(1.0);
    }
    if (auto attr = node.attribute("vibrancy")) {
        cp.vibrancy = attr.as_double(1.0);
    }
    if (auto attr = node.attribute("gamma_threshold")) {
        cp.gammaThreshold = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("zoom")) {
        cp.zoom = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("scale")) {
        cp.scale = attr.as_double(50.0);
    }
    if (auto attr = node.attribute("rotate")) {
        cp.setRotateDegrees(attr.as_double(0.0));
    }
    if (auto attr = node.attribute("angle")) {
        cp.angle = attr.as_double(0.0);
    }
    if (auto attr = node.attribute("oversample")) {
        cp.oversample = attr.as_int(1);
    }
    if (auto attr = node.attribute("filter")) {
        cp.filterRadius = attr.as_double(0.5);
    }
    if (auto attr = node.attribute("quality")) {
        cp.quality = attr.as_double(50.0);
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

void writeXformXml(std::ostringstream& oss,
                   const FlameTransform& xf,
                   bool isFinal) {
    if (isFinal) {
        oss << "   <finalxform color=\"" << xf.color << "\" ";
    } else {
        oss << "   <xform weight=\"" << xf.weight << "\" color=\"" << xf.color
            << "\" ";
    }
    if (xf.symmetry != 0.0) {
        oss << "symmetry=\"" << xf.symmetry << "\" ";
    }
    if (!isFinal && xf.opacity != 1.0) {
        oss << "opacity=\"" << xf.opacity << "\" ";
    }

    for (size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        const double w = xf.variationWeight(id);
        if (w != 0.0) {
            oss << FlameVariation::getInfo(id).name << "=\"" << w << "\" ";
        }
    }

    oss << "coefs=\"" << xf.coefs[0][0] << " " << xf.coefs[0][1] << " "
        << xf.coefs[1][0] << " " << xf.coefs[1][1] << " " << xf.coefs[2][0]
        << " " << xf.coefs[2][1] << "\" ";

    if (xf.hasPostTransform()) {
        oss << "post=\"" << xf.post[0][0] << " " << xf.post[0][1] << " "
            << xf.post[1][0] << " " << xf.post[1][1] << " " << xf.post[2][0]
            << " " << xf.post[2][1] << "\" ";
    }

    for (size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        if (xf.variationWeight(id) != 0.0) {
            const auto& info = FlameVariation::getInfo(id);
            for (auto paramName : info.parameterNames) {
                if (auto val = xf.params.getParam(paramName)) {
                    oss << paramName << "=\"" << *val << "\" ";
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
            oss << "chaos=\"";
            for (int i = 0; i <= lastNonOne; ++i) {
                oss << xf.modWeights[static_cast<size_t>(i)] << " ";
            }
            oss << "\" ";
        }
    }

    oss << "/>\n";
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
    std::ostringstream oss;
    oss << std::setprecision(15);
    oss << "<flame name=\"" << cp.name << "\" version=\"" << cp.version << "\" "
        << "size=\"" << cp.width << " " << cp.height << "\" "
        << "center=\"" << cp.center[0] << " " << cp.center[1] << "\" "
        << "scale=\"" << cp.scale << "\" ";

    if (cp.angle != 0.0) {
        oss << "rotate=\"" << cp.rotateDegrees() << "\" ";
    }
    if (cp.zoom != 0.0) {
        oss << "zoom=\"" << cp.zoom << "\" ";
    }

    oss << "oversample=\"" << cp.oversample << "\" "
        << "filter=\"" << cp.filterRadius << "\" "
        << "quality=\"" << cp.quality << "\" ";

    if (cp.nbatches != 1) {
        oss << "batches=\"" << cp.nbatches << "\" ";
    }

    oss << "background=\"" << (cp.background[0] / 255.0) << " "
        << (cp.background[1] / 255.0) << " " << (cp.background[2] / 255.0)
        << "\" "
        << "brightness=\"" << cp.brightness << "\" "
        << "gamma=\"" << cp.gamma << "\" ";

    if (cp.vibrancy != 1.0) {
        oss << "vibrancy=\"" << cp.vibrancy << "\" ";
    }
    if (cp.gammaThreshold != 0.0) {
        oss << "gamma_threshold=\"" << cp.gammaThreshold << "\" ";
    }
    if (cp.soloXform >= 0) {
        oss << "soloxform=\"" << cp.soloXform << "\" ";
    }
    oss << ">\n";

    for (const auto& xf : cp.xforms) {
        writeXformXml(oss, xf, false);
    }
    if (cp.hasFinalXform()) {
        writeXformXml(oss, *cp.finalXform, true);
    }

    oss << "   <palette count=\"256\" format=\"RGB\">\n"
        << cp.gradient.toHex(false) << "\n"
        << "   </palette>\n"
        << "</flame>\n";
    return oss.str();
}

std::string FlameXmlSerializer::toXmlString(
    const std::vector<FlameFractal>& fractals,
    std::string_view collectionName) {
    std::ostringstream oss;
    oss << "<Flames name=\"" << collectionName << "\">\n";
    for (const auto& f : fractals) {
        oss << toXmlString(f);
    }
    oss << "</Flames>\n";
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
