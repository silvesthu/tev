// Internal CUDA backend bridge for tev's FLIP wrapper.

#pragma once

#include <tev/FlipMetric.h>

#include <cstddef>
#include <span>

struct TevFlipCudaOutputs {
    float pixelsPerDegree;
    float meanError;
    float startExposure;
    float stopExposure;
    int numExposures;
};

extern "C" int tevFlipCudaEvaluate(
    const float* referenceLinearRgb,
    const float* testLinearRgb,
    int width,
    int height,
    int useHdr,
    float pixelsPerDegree,
    float* errorMap,
    TevFlipCudaOutputs* outputs,
    char* errorMessage,
    size_t errorMessageCapacity
) noexcept;

namespace tev::detail {

bool isFlipCudaRuntimeAvailable();

FlipMetricResult computeFlipMetricCuda(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height,
    bool useHdr,
    float pixelsPerDegree
);

}
