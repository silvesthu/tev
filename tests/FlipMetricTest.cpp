// FLIP wrapper regression and parity utility.

#include <tev/FlipMetric.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

float srgbToLinear(float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float maxAbsoluteDifference(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        return std::numeric_limits<float>::infinity();
    }

    float maximum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        maximum = std::max(maximum, std::abs(a[i] - b[i]));
    }
    return maximum;
}

std::vector<float> loadPng(const std::string& path, int& width, int& height) {
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);
    if (!pixels) {
        throw std::runtime_error{"Could not load PNG: " + path + ": " + stbi_failure_reason()};
    }

    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    std::vector<float> result(count);
    for (size_t i = 0; i < count; ++i) {
        result[i] = srgbToLinear(pixels[i] / 255.0f);
    }
    stbi_image_free(pixels);
    return result;
}

int runSynthetic() {
    constexpr int width = 16;
    constexpr int height = 12;
    std::vector<float> reference(static_cast<size_t>(width) * height * 3);
    std::vector<float> test(reference.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t base = static_cast<size_t>(x + y * width) * 3;
            reference[base] = float(x) / float(width - 1);
            reference[base + 1] = float(y) / float(height - 1);
            reference[base + 2] = 0.25f + 0.5f * float((x + y) % 5) / 4.0f;
            test[base] = std::clamp(reference[base] + (x % 3 == 0 ? 0.05f : -0.02f), 0.0f, 1.0f);
            test[base + 1] = std::clamp(reference[base + 1] + (y % 4 == 0 ? 0.04f : 0.0f), 0.0f, 1.0f);
            test[base + 2] = std::clamp(reference[base + 2] - 0.03f, 0.0f, 1.0f);
        }
    }

    const auto ldrCpu = tev::computeFlipMetric(
        reference,
        test,
        width,
        height,
        false,
        0.0f,
        tev::FlipMetricBackend::Cpu
    );
    const bool cudaAvailable = tev::isFlipCudaAvailable();
    std::cout << "CUDA compiled: " << tev::isFlipCudaCompiled() << '\n'
              << "CUDA available: " << cudaAvailable << '\n';
    const auto ldr = tev::computeFlipMetric(reference, test, width, height, false);
    const auto expectedBackend = cudaAvailable ? tev::FlipMetricBackend::Cuda : tev::FlipMetricBackend::Cpu;
    if (ldrCpu.backend != tev::FlipMetricBackend::Cpu
        || ldr.backend != expectedBackend
        || ldr.errorMap.size() != static_cast<size_t>(width) * height
        || ldr.magmaMapSrgb.size() != static_cast<size_t>(width) * height * 3
        || !(ldr.meanError > 0.0f && ldr.meanError < 1.0f)) {
        std::cerr << "Synthetic LDR-FLIP result or backend selection is invalid.\n";
        return EXIT_FAILURE;
    }

    const float meanDifference = std::abs(ldr.meanError - ldrCpu.meanError);
    const float mapDifference = maxAbsoluteDifference(ldr.errorMap, ldrCpu.errorMap);
    const float magmaDifference = maxAbsoluteDifference(ldr.magmaMapSrgb, ldrCpu.magmaMapSrgb);
    if (meanDifference > 0.0000005f || mapDifference > 0.00001f || magmaDifference > 0.02f) {
        std::cerr << "CUDA and CPU FLIP results differ beyond tolerance.\n";
        return EXIT_FAILURE;
    }

    for (float component : ldr.magmaMapSrgb) {
        if (!std::isfinite(component) || component < 0.0f || component > 1.0f) {
            std::cerr << "Synthetic FLIP magma map is invalid.\n";
            return EXIT_FAILURE;
        }
    }

    const auto identical = tev::computeFlipMetric(reference, reference, width, height, false);
    constexpr float magmaZero[] = {0.001462f, 0.000466f, 0.013866f};
    if (identical.meanError != 0.0f) {
        std::cerr << "Identical images did not produce exact zero FLIP error: "
                  << std::setprecision(9) << identical.meanError << "\n";
        return EXIT_FAILURE;
    }
    for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel) {
        for (size_t channel = 0; channel < 3; ++channel) {
            if (identical.magmaMapSrgb[pixel * 3 + channel] != magmaZero[channel]) {
                std::cerr << "Zero-error FLIP color does not match NVLabs magma.\n";
                return EXIT_FAILURE;
            }
        }
    }

    for (size_t i = 0; i < reference.size(); ++i) {
        reference[i] *= 8.0f;
        test[i] *= 8.0f;
    }
    const auto hdr = tev::computeFlipMetric(reference, test, width, height, true);
    if (hdr.errorMap.size() != static_cast<size_t>(width) * height
        || hdr.magmaMapSrgb.size() != static_cast<size_t>(width) * height * 3
        || !(hdr.meanError >= 0.0f && hdr.meanError <= 1.0f)
        || hdr.numExposures < 2
        || hdr.backend != expectedBackend) {
        std::cerr << "Synthetic HDR-FLIP result is invalid.\n";
        return EXIT_FAILURE;
    }

    std::cout << std::setprecision(9)
              << "Backend: " << tev::flipMetricBackendName(ldr.backend) << '\n'
              << "LDR mean: " << ldr.meanError << '\n'
              << "CPU mean difference: " << meanDifference << '\n'
              << "CPU map maximum difference: " << mapDifference << '\n'
              << "CPU magma maximum difference: " << magmaDifference << '\n'
              << "HDR mean: " << hdr.meanError << '\n';
    return EXIT_SUCCESS;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            return runSynthetic();
        }
        if (argc != 3 && argc != 4) {
            std::cerr << "Usage: tev_flip_metric_test [reference.png test.png [expected-mean]]\n";
            return EXIT_FAILURE;
        }

        int referenceWidth = 0;
        int referenceHeight = 0;
        int testWidth = 0;
        int testHeight = 0;
        const auto reference = loadPng(argv[1], referenceWidth, referenceHeight);
        const auto test = loadPng(argv[2], testWidth, testHeight);
        if (referenceWidth != testWidth || referenceHeight != testHeight) {
            throw std::runtime_error{"Reference and test PNG dimensions differ."};
        }

        const auto result = tev::computeFlipMetric(reference, test, referenceWidth, referenceHeight, false);
        std::cout << std::setprecision(9)
                  << "Backend: " << tev::flipMetricBackendName(result.backend) << '\n'
                  << "Mean: " << result.meanError << '\n';

        if (argc == 4) {
            const float expected = std::stof(argv[3]);
            const float difference = std::abs(result.meanError - expected);
            std::cout << "Expected: " << expected << "\nDifference: " << difference << '\n';
            if (difference > 0.0000005f) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
