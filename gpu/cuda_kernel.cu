#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <cuda_runtime.h>

#include "likelihood_benchmark_common.h"

namespace {

using likelihood_bench::BenchmarkConfig;
using likelihood_bench::Dataset;

// Fixed settings
constexpr int kParticleCount = 20000000;
constexpr long kImageElementCount = likelihood_bench::kDefaultMaxSize;
constexpr int kThreadsPerBlock = 256;
constexpr int kWarmupIterations = 10;
constexpr int kTimedIterations = 100;
constexpr bool kEnableVerification = true;
constexpr int kSpecializedMaskPointCount = 69;

__constant__ int g_maskOffsets[likelihood_bench::kMaxCountOnes];

__device__ __forceinline__ int load_image_cached(const int* ptr) {
#if __CUDA_ARCH__ >= 350
    return __ldg(ptr);
#else
    return *ptr;
#endif
}

__device__ __forceinline__ int round_to_int_device(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

// Baseline kernel with bounds handling.
__global__ void likelihood_kernel_checked(
    int particleCount, int maskPointCount, int imageHeight, int frameCount, int currentFrame,
    long imageElementCount, double likelihoodScale, double likelihoodBias,
    const double* __restrict__ particleX, const double* __restrict__ particleY,
    const int* __restrict__ imageData, double* __restrict__ particleLikelihood) {
    const int particleIndex = blockIdx.x * blockDim.x + threadIdx.x;
    if (particleIndex >= particleCount) {
        return;
    }

    const int roundedParticleX = round_to_int_device(particleX[particleIndex]);
    const int roundedParticleY = round_to_int_device(particleY[particleIndex]);
    const long particleBaseIndex = static_cast<long>(roundedParticleX) * imageHeight * frameCount +
                                   static_cast<long>(roundedParticleY) * frameCount +
                                   currentFrame;

    int neighborhoodPixelSum = 0;
#pragma unroll 4
    for (int maskIndex = 0; maskIndex < maskPointCount; maskIndex++) {
        long imageIndex = particleBaseIndex + static_cast<long>(g_maskOffsets[maskIndex]);
        imageIndex = (imageIndex < 0) ? -imageIndex : imageIndex;
        if (imageIndex >= imageElementCount) {
            imageIndex = 0;
        }
        neighborhoodPixelSum += imageData[imageIndex];
    }

    particleLikelihood[particleIndex] =
        likelihoodScale * static_cast<double>(neighborhoodPixelSum) - likelihoodBias;
}

// Specialized fixed-count accumulation used by the fast path.
template <int MaskPointCount>
__device__ __forceinline__ int accumulate_mask_pixels_fixed(
    int particleBaseIndex, const int* __restrict__ imageData) {
    int sum0 = 0;
    int sum1 = 0;
    int sum2 = 0;
    int sum3 = 0;
    const int unrolledEnd = (MaskPointCount / 4) * 4;

#pragma unroll
    for (int maskIndex = 0; maskIndex < unrolledEnd; maskIndex += 4) {
        sum0 += load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex + 0]);
        sum1 += load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex + 1]);
        sum2 += load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex + 2]);
        sum3 += load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex + 3]);
    }

    int neighborhoodPixelSum = sum0 + sum1 + sum2 + sum3;

#pragma unroll
    for (int maskIndex = unrolledEnd; maskIndex < MaskPointCount; maskIndex++) {
        neighborhoodPixelSum +=
            load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex]);
    }

    return neighborhoodPixelSum;
}

template <int MaskPointCount>
__global__ void likelihood_kernel_fast_fixed(
    int particleCount, int planeStride, int frameCount, int currentFrame, double likelihoodScale,
    double likelihoodBias, const double* __restrict__ particleX,
    const double* __restrict__ particleY, const int* __restrict__ imageData,
    double* __restrict__ particleLikelihood) {
    const int particleIndex = blockIdx.x * blockDim.x + threadIdx.x;
    if (particleIndex >= particleCount) {
        return;
    }

    const int roundedParticleX = round_to_int_device(particleX[particleIndex]);
    const int roundedParticleY = round_to_int_device(particleY[particleIndex]);
    const int particleBaseIndex = roundedParticleX * planeStride + roundedParticleY * frameCount +
                                  currentFrame;
    const int neighborhoodPixelSum =
        accumulate_mask_pixels_fixed<MaskPointCount>(particleBaseIndex, imageData);
    particleLikelihood[particleIndex] =
        likelihoodScale * static_cast<double>(neighborhoodPixelSum) - likelihoodBias;
}

// Runtime helpers.
void check_cuda(cudaError_t status, const char* what) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
    }
}

bool can_use_specialized_fast_kernel(const BenchmarkConfig& cfg, const Dataset& data) {
    if (data.countOnes != kSpecializedMaskPointCount) {
        return false;
    }
    if (cfg.maxSize > static_cast<long>(std::numeric_limits<int>::max())) {
        return false;
    }

    const int planeStride = cfg.iszY * cfg.nfr;
    int minParticleBaseIndex = std::numeric_limits<int>::max();
    int maxParticleBaseIndex = std::numeric_limits<int>::min();

    for (int particleIndex = 0; particleIndex < cfg.particles; particleIndex++) {
        const int roundedParticleX = likelihood_bench::round_double(
            data.arrayX[static_cast<std::size_t>(particleIndex)]);
        const int roundedParticleY = likelihood_bench::round_double(
            data.arrayY[static_cast<std::size_t>(particleIndex)]);
        const long long particleBaseIndex =
            static_cast<long long>(roundedParticleX) * planeStride +
            static_cast<long long>(roundedParticleY) * cfg.nfr +
            static_cast<long long>(cfg.frameIndex);
        if (particleBaseIndex < static_cast<long long>(std::numeric_limits<int>::min()) ||
            particleBaseIndex > static_cast<long long>(std::numeric_limits<int>::max())) {
            return false;
        }

        const int baseIndex = static_cast<int>(particleBaseIndex);
        minParticleBaseIndex = std::min(minParticleBaseIndex, baseIndex);
        maxParticleBaseIndex = std::max(maxParticleBaseIndex, baseIndex);
    }

    const auto maskOffsetRange = std::minmax_element(data.objOffsets.begin(), data.objOffsets.end());
    const long long minImageIndex = static_cast<long long>(minParticleBaseIndex) +
                                    static_cast<long long>(*maskOffsetRange.first);
    const long long maxImageIndex = static_cast<long long>(maxParticleBaseIndex) +
                                    static_cast<long long>(*maskOffsetRange.second);

    return minImageIndex >= 0 && maxImageIndex < cfg.maxSize;
}

void launch_selected_kernel(
    int gridSize, const BenchmarkConfig& cfg, const Dataset& data, int planeStride,
    bool useSpecializedFastKernel, double likelihoodScale, double likelihoodBias,
    const double* deviceParticleX, const double* deviceParticleY, const int* deviceImage,
    double* deviceLikelihood) {
    if (useSpecializedFastKernel) {
        likelihood_kernel_fast_fixed<kSpecializedMaskPointCount><<<gridSize, cfg.blockSize>>>(
            cfg.particles,
            planeStride,
            cfg.nfr,
            cfg.frameIndex,
            likelihoodScale,
            likelihoodBias,
            deviceParticleX,
            deviceParticleY,
            deviceImage,
            deviceLikelihood);
        return;
    }

    likelihood_kernel_checked<<<gridSize, cfg.blockSize>>>(
        cfg.particles,
        data.countOnes,
        cfg.iszY,
        cfg.nfr,
        cfg.frameIndex,
        cfg.maxSize,
        likelihoodScale,
        likelihoodBias,
        deviceParticleX,
        deviceParticleY,
        deviceImage,
        deviceLikelihood);
}

struct GpuRunResult {
    std::vector<double> likelihoodValues;
    double averageKernelMs;

    GpuRunResult(std::vector<double> likelihoodValuesIn, double averageKernelMsIn)
        : likelihoodValues(std::move(likelihoodValuesIn)), averageKernelMs(averageKernelMsIn) {}
};

GpuRunResult run_gpu_benchmark(const BenchmarkConfig& cfg, const Dataset& data) {
    const std::size_t particleArrayBytes =
        static_cast<std::size_t>(cfg.particles) * sizeof(double);
    const std::size_t imageArrayBytes = static_cast<std::size_t>(cfg.maxSize) * sizeof(int);
    const int gridSize = (cfg.particles + cfg.blockSize - 1) / cfg.blockSize;
    const int planeStride = cfg.iszY * cfg.nfr;
    const bool useSpecializedFastKernel = can_use_specialized_fast_kernel(cfg, data);
    const double likelihoodScale = likelihood_bench::pixel_scale(data.countOnes);
    const double likelihoodBias = likelihood_bench::pixel_bias();

    double* deviceParticleX = nullptr;
    double* deviceParticleY = nullptr;
    int* deviceImage = nullptr;
    double* deviceLikelihood = nullptr;
    cudaEvent_t startEvent = nullptr;
    cudaEvent_t stopEvent = nullptr;
    std::vector<double> hostLikelihood(static_cast<std::size_t>(cfg.particles), 0.0);

    check_cuda(cudaMalloc(&deviceParticleX, particleArrayBytes), "cudaMalloc arrayX");
    check_cuda(cudaMalloc(&deviceParticleY, particleArrayBytes), "cudaMalloc arrayY");
    check_cuda(cudaMalloc(&deviceImage, imageArrayBytes), "cudaMalloc image");
    check_cuda(cudaMalloc(&deviceLikelihood, particleArrayBytes), "cudaMalloc likelihood");

    check_cuda(cudaMemcpy(deviceParticleX,
                          data.arrayX.data(),
                          particleArrayBytes,
                          cudaMemcpyHostToDevice),
               "cudaMemcpy arrayX");
    check_cuda(cudaMemcpy(deviceParticleY,
                          data.arrayY.data(),
                          particleArrayBytes,
                          cudaMemcpyHostToDevice),
               "cudaMemcpy arrayY");
    check_cuda(cudaMemcpy(deviceImage,
                          data.image.data(),
                          imageArrayBytes,
                          cudaMemcpyHostToDevice),
               "cudaMemcpy image");
    check_cuda(cudaMemcpyToSymbol(
                   g_maskOffsets,
                   data.objOffsets.data(),
                   static_cast<std::size_t>(data.countOnes) * sizeof(int)),
               "cudaMemcpyToSymbol objOffsets");

    check_cuda(cudaEventCreate(&startEvent), "cudaEventCreate start");
    check_cuda(cudaEventCreate(&stopEvent), "cudaEventCreate stop");

    for (int iter = 0; iter < cfg.warmup; iter++) {
        launch_selected_kernel(gridSize,
                               cfg,
                               data,
                               planeStride,
                               useSpecializedFastKernel,
                               likelihoodScale,
                               likelihoodBias,
                               deviceParticleX,
                               deviceParticleY,
                               deviceImage,
                               deviceLikelihood);
    }
    check_cuda(cudaGetLastError(), "kernel warmup launch");
    check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize warmup");

    check_cuda(cudaEventRecord(startEvent), "cudaEventRecord start");
    for (int iter = 0; iter < cfg.iters; iter++) {
        launch_selected_kernel(gridSize,
                               cfg,
                               data,
                               planeStride,
                               useSpecializedFastKernel,
                               likelihoodScale,
                               likelihoodBias,
                               deviceParticleX,
                               deviceParticleY,
                               deviceImage,
                               deviceLikelihood);
    }
    check_cuda(cudaEventRecord(stopEvent), "cudaEventRecord stop");
    check_cuda(cudaGetLastError(), "kernel timed launch");
    check_cuda(cudaEventSynchronize(stopEvent), "cudaEventSynchronize");

    float totalMs = 0.0f;
    check_cuda(cudaEventElapsedTime(&totalMs, startEvent, stopEvent),
               "cudaEventElapsedTime");
    check_cuda(cudaMemcpy(hostLikelihood.data(),
                          deviceLikelihood,
                          particleArrayBytes,
                          cudaMemcpyDeviceToHost),
               "cudaMemcpy likelihood");

    check_cuda(cudaEventDestroy(startEvent), "cudaEventDestroy start");
    check_cuda(cudaEventDestroy(stopEvent), "cudaEventDestroy stop");
    check_cuda(cudaFree(deviceParticleX), "cudaFree arrayX");
    check_cuda(cudaFree(deviceParticleY), "cudaFree arrayY");
    check_cuda(cudaFree(deviceImage), "cudaFree image");
    check_cuda(cudaFree(deviceLikelihood), "cudaFree likelihood");

    return GpuRunResult(std::move(hostLikelihood), static_cast<double>(totalMs) / cfg.iters);
}

}  // namespace

int main() {
    try {
        BenchmarkConfig cfg;
        cfg.particles = kParticleCount;
        cfg.maxSize = kImageElementCount;
        cfg.blockSize = kThreadsPerBlock;
        cfg.warmup = kWarmupIterations;
        cfg.iters = kTimedIterations;
        cfg.verify = kEnableVerification;

        const Dataset data = likelihood_bench::build_dataset(cfg);
        const GpuRunResult result = run_gpu_benchmark(cfg, data);

        likelihood_bench::print_config("GPU", cfg, data.countOnes);
        std::cout << std::fixed << std::setprecision(6)
                  << "[GPU] grid_size="
                  << ((cfg.particles + cfg.blockSize - 1) / cfg.blockSize)
                  << ", block_size=" << cfg.blockSize << "\n"
                  << "[GPU] avg_kernel_ms=" << result.averageKernelMs << "\n"
                  << "[GPU] checksum=" << std::setprecision(15)
                  << likelihood_bench::checksum(result.likelihoodValues) << "\n";

        if (cfg.verify) {
            std::vector<double> reference;
            likelihood_bench::compute_reference(cfg, data, reference);

            double maxAbsErr = 0.0;
            for (int particleIndex = 0; particleIndex < cfg.particles; particleIndex++) {
                const double err =
                    std::abs(result.likelihoodValues[static_cast<std::size_t>(particleIndex)] -
                             reference[static_cast<std::size_t>(particleIndex)]);
                maxAbsErr = std::max(maxAbsErr, err);
            }

            std::cout << "[GPU] max_abs_err=" << std::setprecision(15) << maxAbsErr << "\n";
            if (maxAbsErr > 1.0e-9) {
                std::cerr << "[GPU] verification failed\n";
                return 1;
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
