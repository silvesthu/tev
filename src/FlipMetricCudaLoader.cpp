// This file is published under the BSD 3-Clause License within the LICENSE.txt file.

#include "FlipMetricCuda.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <stdexcept>
#include <string_view>

namespace tev::detail {

namespace {

using CuInitFunction = int (*)(unsigned int);
using CuDeviceGetCountFunction = int (*)(int*);

bool cudaWasExplicitlyHidden() {
    std::array<wchar_t, 64> value{};
    const DWORD length = GetEnvironmentVariableW(
        L"CUDA_VISIBLE_DEVICES",
        value.data(),
        static_cast<DWORD>(value.size())
    );
    if (length == 0 || length >= value.size()) {
        return false;
    }

    return std::wstring_view{value.data(), length} == L"-1";
}

bool detectCudaDevice() {
    if (cudaWasExplicitlyHidden()) {
        return false;
    }

    HMODULE driver = LoadLibraryW(L"nvcuda.dll");
    if (!driver) {
        return false;
    }

    const auto cuInit = reinterpret_cast<CuInitFunction>(GetProcAddress(driver, "cuInit"));
    const auto cuDeviceGetCount = reinterpret_cast<CuDeviceGetCountFunction>(
        GetProcAddress(driver, "cuDeviceGetCount")
    );

    int deviceCount = 0;
    const bool available = cuInit
        && cuDeviceGetCount
        && cuInit(0) == 0
        && cuDeviceGetCount(&deviceCount) == 0
        && deviceCount > 0;
    if (!available) {
        FreeLibrary(driver);
    } else {
        // Keep the driver loaded for the process lifetime after cuInit. The
        // statically linked CUDA runtime uses the driver state established here.
    }
    return available;
}

bool cudaRuntimeAvailable() {
    static const bool available = detectCudaDevice();
    return available;
}

}

bool isFlipCudaRuntimeAvailable() {
    return cudaRuntimeAvailable();
}

FlipMetricResult computeFlipMetricCuda(
    std::span<const float> referenceLinearRgb,
    std::span<const float> testLinearRgb,
    int width,
    int height,
    bool useHdr,
    float pixelsPerDegree
) {
    if (!cudaRuntimeAvailable()) {
        throw std::runtime_error{"The CUDA FLIP backend is unavailable."};
    }

    FlipMetricResult result;
    result.width = width;
    result.height = height;
    result.usesHdr = useHdr;
    result.backend = FlipMetricBackend::Cuda;
    result.tonemapper = "aces";
    result.errorMap.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    TevFlipCudaOutputs outputs{};
    std::array<char, 512> errorMessage{};
    const int status = tevFlipCudaEvaluate(
        referenceLinearRgb.data(),
        testLinearRgb.data(),
        width,
        height,
        useHdr ? 1 : 0,
        pixelsPerDegree,
        result.errorMap.data(),
        &outputs,
        errorMessage.data(),
        errorMessage.size()
    );
    if (status != 0) {
        throw std::runtime_error{
            errorMessage[0] ? errorMessage.data() : "The CUDA FLIP backend failed."
        };
    }

    result.pixelsPerDegree = outputs.pixelsPerDegree;
    result.meanError = outputs.meanError;
    result.startExposure = outputs.startExposure;
    result.stopExposure = outputs.stopExposure;
    result.numExposures = outputs.numExposures;
    return result;
}

}
