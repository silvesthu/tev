// This file is published under the BSD 3-Clause License within the LICENSE.txt file.

#pragma once

#include <span>
#include <string>
#include <vector>

namespace tev {

enum class FlipMetricBackend {
    Automatic,
    Cpu,
    Cuda,
};

struct FlipMetricResult {
    int width = 0;
    int height = 0;
    bool usesHdr = false;
    float pixelsPerDegree = 0.0f;
    float meanError = 0.0f;
    float startExposure = 0.0f;
    float stopExposure = 0.0f;
    int numExposures = 0;
    std::string tonemapper;
    std::vector<float> errorMap;
    std::vector<float> magmaMapSrgb;
    FlipMetricBackend backend = FlipMetricBackend::Cpu;
};

float defaultFlipPixelsPerDegree();

bool isFlipCudaCompiled();
bool isFlipCudaAvailable();
const char* flipMetricBackendName(FlipMetricBackend backend);

FlipMetricResult computeFlipMetric(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height,
    bool useHdr,
    float pixelsPerDegree = 0.0f,
    FlipMetricBackend backend = FlipMetricBackend::Automatic
);

}
