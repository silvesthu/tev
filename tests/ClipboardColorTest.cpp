#include <tev/Common.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error{message};
    }
}

void testAllByteValues() {
    for (int value = 0; value <= 255; ++value) {
        const auto byte = static_cast<uint8_t>(value);
        require(
            tev::linearToSrgbByte(tev::srgbByteToLinear(byte)) == byte,
            "sRGB byte round-trip failed for " + std::to_string(value)
        );
        require(
            tev::linearToUnormByte(static_cast<float>(byte) / 255.0f) == byte,
            "alpha byte round-trip failed for " + std::to_string(value)
        );
    }

    require(tev::linearToSrgbByte(-1.0f) == 0, "negative RGB must clamp to zero");
    require(tev::linearToSrgbByte(2.0f) == 255, "HDR RGB must clamp to one");
    require(tev::linearToUnormByte(-1.0f) == 0, "negative alpha must clamp to zero");
    require(tev::linearToUnormByte(2.0f) == 255, "alpha above one must clamp to one");
    require(
        tev::linearToSrgbByte(std::numeric_limits<float>::quiet_NaN()) == 0,
        "NaN RGB must become zero"
    );
    require(
        tev::linearToUnormByte(std::numeric_limits<float>::quiet_NaN()) == 0,
        "NaN alpha must become zero"
    );
}

void testImage(const char* filename) {
    int width = 0;
    int height = 0;
    stbi_uc* pixels = stbi_load(filename, &width, &height, nullptr, 4);
    if (!pixels) {
        throw std::runtime_error{"Could not load test image: " + std::string{stbi_failure_reason()}};
    }

    size_t changedComponents = 0;
    const size_t componentCount = static_cast<size_t>(width) * height * 4;
    for (size_t i = 0; i < componentCount; ++i) {
        const uint8_t source = pixels[i];
        const uint8_t roundTripped = i % 4 == 3
            ? tev::linearToUnormByte(static_cast<float>(source) / 255.0f)
            : tev::linearToSrgbByte(tev::srgbByteToLinear(source));
        changedComponents += roundTripped != source;
    }

    stbi_image_free(pixels);

    std::cout
        << "Clipboard PNG formula: file=" << filename
        << ", width=" << width
        << ", height=" << height
        << ", changed components=" << changedComponents
        << "\n";
    require(changedComponents == 0, "PNG component round-trip changed values");
}

}

int main(int argc, char** argv) {
    try {
        testAllByteValues();
        if (argc > 1) {
            testImage(argv[1]);
        }

        std::cout
            << "Clipboard color conversion test passed for all 256 sRGB and alpha codes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "Clipboard color conversion test failed: " << error.what() << "\n";
        return 1;
    }
}
