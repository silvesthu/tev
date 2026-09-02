// This file is published under the BSD 3-Clause License within the LICENSE.txt file.

#include <tev/FlipMetric.h>

#ifdef TEV_FLIP_HAS_CUDA
#include "FlipMetricCuda.h"
#endif

#include <FLIP.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>

namespace tev {

namespace {

void validateInputs(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument{"FLIP images must have positive dimensions."};
    }

    const size_t numValues = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    if (referenceLinearRgb.size() != numValues || testLinearRgb.size() != numValues) {
        throw std::invalid_argument{"FLIP inputs must contain exactly three interleaved RGB floats per pixel."};
    }
}

FlipMetricResult computeFlipMetricCpu(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height,
    bool useHdr,
    float pixelsPerDegree
) {
    FLIP::Parameters parameters;
    if (pixelsPerDegree > 0.0f) {
        parameters.PPD = pixelsPerDegree;
    }

    float meanError = -1.0f;
    float* rawErrorMap = nullptr;
    FLIP::evaluate(
        referenceLinearRgb.data(),
        testLinearRgb.data(),
        width,
        height,
        useHdr,
        parameters,
        false,
        true,
        meanError,
        &rawErrorMap
    );
    std::unique_ptr<float[]> errorMap{rawErrorMap};

    if (!errorMap || !std::isfinite(meanError)) {
        throw std::runtime_error{"CPU FLIP did not produce a finite error map."};
    }

    FlipMetricResult result;
    result.width = width;
    result.height = height;
    result.usesHdr = useHdr;
    result.pixelsPerDegree = parameters.PPD;
    result.meanError = meanError;
    result.startExposure = parameters.startExposure;
    result.stopExposure = parameters.stopExposure;
    result.numExposures = parameters.numExposures;
    result.tonemapper = parameters.tonemapper;
    result.backend = FlipMetricBackend::Cpu;

    const size_t numPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    result.errorMap.assign(errorMap.get(), errorMap.get() + numPixels);
    return result;
}

void normalizeIdenticalResult(bool inputsAreIdentical, FlipMetricResult& result) {
    if (!inputsAreIdentical) {
        return;
    }

    result.meanError = 0.0f;
    std::fill(result.errorMap.begin(), result.errorMap.end(), 0.0f);
}

void populateMagmaMap(FlipMetricResult& result) {
    const size_t numPixels = static_cast<size_t>(result.width) * static_cast<size_t>(result.height);
    FLIP::image<float> scalarErrorMap(result.width, result.height);
    scalarErrorMap.setPixels(result.errorMap.data(), result.width, result.height);
    FLIP::image<FLIP::color3> magmaErrorMap(result.width, result.height);
    magmaErrorMap.colorMap(scalarErrorMap, FLIP::magmaMap);

    const FLIP::color3* magmaPixels = magmaErrorMap.getHostData();
    result.magmaMapSrgb.resize(numPixels * 3);
    for (size_t pixel = 0; pixel < numPixels; ++pixel) {
        result.magmaMapSrgb[pixel * 3] = magmaPixels[pixel].r;
        result.magmaMapSrgb[pixel * 3 + 1] = magmaPixels[pixel].g;
        result.magmaMapSrgb[pixel * 3 + 2] = magmaPixels[pixel].b;
    }
}

}

float defaultFlipPixelsPerDegree() {
    return FLIP::calculatePPD(0.7f, 3840.0f, 0.7f);
}

bool isFlipCudaCompiled() {
#ifdef TEV_FLIP_HAS_CUDA
    return true;
#else
    return false;
#endif
}

bool isFlipCudaAvailable() {
#ifdef TEV_FLIP_HAS_CUDA
    return detail::isFlipCudaRuntimeAvailable();
#else
    return false;
#endif
}

const char* flipMetricBackendName(FlipMetricBackend backend) {
    switch (backend) {
        case FlipMetricBackend::Automatic:
            return "automatic";
        case FlipMetricBackend::Cpu:
            return "CPU";
        case FlipMetricBackend::Cuda:
            return "CUDA";
    }
    return "unknown";
}

FlipMetricResult computeFlipMetric(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height,
    bool useHdr,
    float pixelsPerDegree,
    FlipMetricBackend backend
) {
    validateInputs(referenceLinearRgb, testLinearRgb, width, height);
    const bool inputsAreIdentical = std::equal(
        referenceLinearRgb.begin(),
        referenceLinearRgb.end(),
        testLinearRgb.begin()
    );

    if (backend != FlipMetricBackend::Automatic
        && backend != FlipMetricBackend::Cpu
        && backend != FlipMetricBackend::Cuda) {
        throw std::invalid_argument{"Unknown FLIP backend."};
    }

#ifdef TEV_FLIP_HAS_CUDA
    const bool cudaAvailable = detail::isFlipCudaRuntimeAvailable();
    if (backend == FlipMetricBackend::Cuda && !cudaAvailable) {
        throw std::runtime_error{"CUDA FLIP was requested, but no CUDA device is available."};
    }

    if (backend != FlipMetricBackend::Cpu && cudaAvailable) {
        try {
            auto result = detail::computeFlipMetricCuda(
                referenceLinearRgb,
                testLinearRgb,
                width,
                height,
                useHdr,
                pixelsPerDegree
            );
            normalizeIdenticalResult(inputsAreIdentical, result);
            populateMagmaMap(result);
            return result;
        } catch (const std::exception&) {
            if (backend == FlipMetricBackend::Cuda) {
                throw;
            }
        }
    }
#else
    if (backend == FlipMetricBackend::Cuda) {
        throw std::runtime_error{"CUDA FLIP was requested, but tev was built without CUDA."};
    }
#endif

    auto result = computeFlipMetricCpu(
        referenceLinearRgb,
        testLinearRgb,
        width,
        height,
        useHdr,
        pixelsPerDegree
    );
    normalizeIdenticalResult(inputsAreIdentical, result);
    populateMagmaMap(result);
    return result;
}

}
