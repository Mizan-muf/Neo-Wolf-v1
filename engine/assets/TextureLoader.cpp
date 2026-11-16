#include "engine/assets/TextureLoader.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "engine/core/Color.h"
#include "engine/core/Paths.h"

namespace {
bool NextToken(std::istream& stream, std::string& outToken) {
    outToken.clear();
    while (stream >> outToken) {
        if (!outToken.empty() && outToken[0] == '#') {
            std::string ignored;
            std::getline(stream, ignored);
            continue;
        }
        return true;
    }
    return false;
}

// A PPM sample clamped to [0, maxValue] and scaled to 0-255.
std::uint8_t NormalizeChannel(int value, int maxValue) {
    if (value < 0) {
        value = 0;
    } else if (value > maxValue) {
        value = maxValue;
    }
    return static_cast<std::uint8_t>((static_cast<float>(value) / static_cast<float>(maxValue)) * 255.0f);
}

bool ParseIntToken(std::istream& stream, int& outValue) {
    std::string token;
    if (!NextToken(stream, token)) {
        return false;
    }

    std::istringstream parser(token);
    parser >> outValue;
    return !parser.fail();
}
}  // namespace

bool TextureLoader::LoadPPM(const std::string& filePath, Texture& outTexture) {
    std::ifstream file(PathFromUtf8(filePath));
    if (!file.is_open()) {
        return false;
    }

    std::string magic;
    if (!NextToken(file, magic) || magic != "P3") {
        return false;
    }

    int width = 0;
    int height = 0;
    int maxValue = 0;
    if (!ParseIntToken(file, width) || !ParseIntToken(file, height) || !ParseIntToken(file, maxValue)) {
        return false;
    }
    if (width <= 0 || height <= 0 || maxValue <= 0) {
        return false;
    }

    if (!outTexture.Resize(width, height)) {
        return false;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int r = 0;
            int g = 0;
            int b = 0;
            if (!ParseIntToken(file, r) || !ParseIntToken(file, g) || !ParseIntToken(file, b)) {
                return false;
            }
            outTexture.SetPixel(x, y,
                                PackARGB32(MakeColor(NormalizeChannel(r, maxValue), NormalizeChannel(g, maxValue),
                                                     NormalizeChannel(b, maxValue))));
        }
    }

    return true;
}

void TextureLoader::BuildFallbackChecker(Texture& outTexture, int width, int height) {
    if (!outTexture.Resize(width, height)) {
        return;
    }

    // Magenta and black 8x8 squares: impossible to mistake for real art.
    constexpr int kTileSize = 8;
    const std::uint32_t magenta = PackARGB32(MakeColor(255, 0, 255));
    const std::uint32_t black = PackARGB32(MakeColor(0, 0, 0));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int cx = x / kTileSize;
            const int cy = y / kTileSize;
            outTexture.SetPixel(x, y, ((cx + cy) % 2 == 0) ? magenta : black);
        }
    }
}
