#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <cuda_runtime.h>

#include "likelihood_benchmark_common.h"

namespace {

using likelihood_bench::BenchmarkConfig;
using likelihood_bench::Dataset;
// SECTION: Fixed settings
// Edit only this block when you want a different benchmark setup.
constexpr int kParticles = likelihood_bench::kDefaultParticles;
constexpr long kMaxSize = likelihood_bench::kDefaultMaxSize;
constexpr int kBlockSize = 256;
constexpr int kWarmupIters = 10;
constexpr int kTimedIters = 100;
constexpr bool kVerifyResult = true;

__constant__ int g_objOffsets[likelihood_bench::kMaxCountOnes];

__device__ __forceinline__ int round_double_device(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}
// SECTION: kernel GPU algorithm
__global__ void likelihood_kernel_cuda(int particles,
                                       int countOnes,
                                       int iszY,
                                       int nfr,
                                       int frameIndex,
                                       long maxSize,
                                       double scale,
                                       double bias,
                                       const double* __restrict__ arrayX,
                                       const double* __restrict__ arrayY,
                                       const int* __restrict__ image,
                                       double* __restrict__ likelihood) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    if (x >= particles) {
        return;
    }

    const int px = round_double_device(arrayX[x]);
    const int py = round_double_device(arrayY[x]);
    const long particleBase =
        static_cast<long>(px) * iszY * nfr + static_cast<long>(py) * nfr + frameIndex;

    int pixelSum = 0;
#pragma unroll 4
    for (int y = 0; y < countOnes; y++) {
        long idx = particleBase + static_cast<long>(g_objOffsets[y]);
        idx = (idx < 0) ? -idx : idx;
        if (idx >= maxSize) {
            idx = 0;
        }
        pixelSum += image[idx];
    }

    likelihood[x] = scale * static_cast<double>(pixelSum) - bias;
}
// SECTION: GPU execution wrapper
void check_cuda(cudaError_t status, const char* what) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
    }
}

struct GpuRunResult {
    std::vector<double> likelihood;
    double avgKernelMs = 0.0;
};

GpuRunResult run_gpu_benchmark(const BenchmarkConfig& cfg, const Dataset& data) {
    const std::size_t particleBytes =
        static_cast<std::size_t>(cfg.particles) * sizeof(double);
    const std::size_t imageBytes =
        static_cast<std::size_t>(cfg.maxSize) * sizeof(int);
    const int gridSize = (cfg.particles + cfg.blockSize - 1) / cfg.blockSize;
    const double scale = likelihood_bench::pixel_scale(data.countOnes);
    const double bias = likelihood_bench::pixel_bias();

    double* dArrayX = nullptr;
    double* dArrayY = nullptr;
    int* dImage = nullptr;
    double* dLikelihood = nullptr;
    cudaEvent_t startEvent = nullptr;
    cudaEvent_t stopEvent = nullptr;
    std::vector<double> likelihoodHost(static_cast<std::size_t>(cfg.particles), 0.0);

    check_cuda(cudaMalloc(&dArrayX, particleBytes), "cudaMalloc arrayX");
    check_cuda(cudaMalloc(&dArrayY, particleBytes), "cudaMalloc arrayY");
    check_cuda(cudaMalloc(&dImage, imageBytes), "cudaMalloc image");
    check_cuda(cudaMalloc(&dLikelihood, particleBytes), "cudaMalloc likelihood");

    check_cuda(cudaMemcpy(dArrayX, data.arrayX.data(), particleBytes, cudaMemcpyHostToDevice),
               "cudaMemcpy arrayX");
    check_cuda(cudaMemcpy(dArrayY, data.arrayY.data(), particleBytes, cudaMemcpyHostToDevice),
               "cudaMemcpy arrayY");
    check_cuda(cudaMemcpy(dImage, data.image.data(), imageBytes, cudaMemcpyHostToDevice),
               "cudaMemcpy image");
    check_cuda(cudaMemcpyToSymbol(g_objOffsets,
                                  data.objOffsets.data(),
                                  static_cast<std::size_t>(data.countOnes) * sizeof(int)),
               "cudaMemcpyToSymbol objOffsets");

    check_cuda(cudaEventCreate(&startEvent), "cudaEventCreate start");
    check_cuda(cudaEventCreate(&stopEvent), "cudaEventCreate stop");

    for (int iter = 0; iter < cfg.warmup; iter++) {
        likelihood_kernel_cuda<<<gridSize, cfg.blockSize>>>(
            cfg.particles,
            data.countOnes,
            cfg.iszY,
            cfg.nfr,
            cfg.frameIndex,
            cfg.maxSize,
            scale,
            bias,
            dArrayX,
            dArrayY,
            dImage,
            dLikelihood);
    }
    check_cuda(cudaGetLastError(), "kernel warmup launch");
    check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize warmup");

    check_cuda(cudaEventRecord(startEvent), "cudaEventRecord start");
    for (int iter = 0; iter < cfg.iters; iter++) {
        likelihood_kernel_cuda<<<gridSize, cfg.blockSize>>>(
            cfg.particles,
            data.countOnes,
            cfg.iszY,
            cfg.nfr,
            cfg.frameIndex,
            cfg.maxSize,
            scale,
            bias,
            dArrayX,
            dArrayY,
            dImage,
            dLikelihood);
    }
    check_cuda(cudaEventRecord(stopEvent), "cudaEventRecord stop");
    check_cuda(cudaGetLastError(), "kernel timed launch");
    check_cuda(cudaEventSynchronize(stopEvent), "cudaEventSynchronize");

    float totalMs = 0.0f;
    check_cuda(cudaEventElapsedTime(&totalMs, startEvent, stopEvent),
               "cudaEventElapsedTime");
    check_cuda(cudaMemcpy(likelihoodHost.data(),
                          dLikelihood,
                          particleBytes,
                          cudaMemcpyDeviceToHost),
               "cudaMemcpy likelihood");

    check_cuda(cudaEventDestroy(startEvent), "cudaEventDestroy start");
    check_cuda(cudaEventDestroy(stopEvent), "cudaEventDestroy stop");
    check_cuda(cudaFree(dArrayX), "cudaFree arrayX");
    check_cuda(cudaFree(dArrayY), "cudaFree arrayY");
    check_cuda(cudaFree(dImage), "cudaFree image");
    check_cuda(cudaFree(dLikelihood), "cudaFree likelihood");

    return {std::move(likelihoodHost),
            static_cast<double>(totalMs) / static_cast<double>(cfg.iters)};
}

}  // namespace
// SECTION: main
int main() {
    try {
        BenchmarkConfig cfg;
        cfg.particles = kParticles;
        cfg.maxSize = kMaxSize;
        cfg.blockSize = kBlockSize;
        cfg.warmup = kWarmupIters;
        cfg.iters = kTimedIters;
        cfg.verify = kVerifyResult;

        const Dataset data = likelihood_bench::build_dataset(cfg);
        const GpuRunResult result = run_gpu_benchmark(cfg, data);

        likelihood_bench::print_config("GPU", cfg, data.countOnes);
        std::cout << std::fixed << std::setprecision(6)
                  << "[GPU] grid_size="
                  << ((cfg.particles + cfg.blockSize - 1) / cfg.blockSize)
                  << ", block_size=" << cfg.blockSize << "\n"
                  << "[GPU] avg_kernel_ms=" << result.avgKernelMs << "\n"
                  << "[GPU] checksum=" << std::setprecision(15)
                  << likelihood_bench::checksum(result.likelihood) << "\n";

        if (cfg.verify) {
            std::vector<double> reference;
            likelihood_bench::compute_reference(cfg, data, reference);

            double maxAbsErr = 0.0;
            for (int i = 0; i < cfg.particles; i++) {
                const double err = std::abs(result.likelihood[static_cast<std::size_t>(i)] -
                                            reference[static_cast<std::size_t>(i)]);
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
