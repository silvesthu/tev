// This file is published under the BSD 3-Clause License within the LICENSE.txt file.

#include "FlipMetricCuda.h"

#include <stdexcept>

#define FLIP_ENABLE_CUDA
#define FLIP FLIP_CUDA
#define FLIP_FATAL_EXIT(status) throw std::runtime_error{"NVLabs FLIP CUDA operation failed."}
#include <FLIP.h>
#undef FLIP_FATAL_EXIT
#undef FLIP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>

namespace {

void copyErrorMessage(char* destination, size_t capacity, const char* message) noexcept {
    if (!destination || capacity == 0) {
        return;
    }

    const size_t length = std::min(capacity - 1, std::strlen(message));
    std::memcpy(destination, message, length);
    destination[length] = '\0';
}

}

extern "C" int tevFlipCudaEvaluate(
    const float* referenceLinearRgb,
    const float* testLinearRgb,
    int width,
    int height,
    int useHdr,
    float pixelsPerDegree,
    float* outputErrorMap,
    TevFlipCudaOutputs* outputs,
    char* errorMessage,
    size_t errorMessageCapacity
) noexcept {
    try {
        if (!referenceLinearRgb || !testLinearRgb || !outputErrorMap || !outputs || width <= 0 || height <= 0) {
            throw std::invalid_argument{"Invalid CUDA FLIP input."};
        }

        FLIP_CUDA::Parameters parameters;
        if (pixelsPerDegree > 0.0f) {
            parameters.PPD = pixelsPerDegree;
        }

        float meanError = -1.0f;
        float* rawErrorMap = nullptr;
        FLIP_CUDA::evaluate(
            referenceLinearRgb,
            testLinearRgb,
            width,
            height,
            useHdr != 0,
            parameters,
            false,
            true,
            meanError,
            &rawErrorMap
        );
        std::unique_ptr<float[]> errorMap{rawErrorMap};

        if (!errorMap || !std::isfinite(meanError)) {
            throw std::runtime_error{"CUDA FLIP did not produce a finite error map."};
        }

        const size_t numPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        std::copy_n(errorMap.get(), numPixels, outputErrorMap);
        outputs->pixelsPerDegree = parameters.PPD;
        outputs->meanError = meanError;
        outputs->startExposure = parameters.startExposure;
        outputs->stopExposure = parameters.stopExposure;
        outputs->numExposures = parameters.numExposures;
        copyErrorMessage(errorMessage, errorMessageCapacity, "");
        return 0;
    } catch (const std::exception& error) {
        copyErrorMessage(errorMessage, errorMessageCapacity, error.what());
        return 1;
    } catch (...) {
        copyErrorMessage(errorMessage, errorMessageCapacity, "Unknown CUDA FLIP failure.");
        return 1;
    }
}
