#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <cuda_runtime.h>

// ============================================================================
// 1. CONFIGURATION CONSTANTS (Matched to FPGA & CPU)
// ============================================================================
constexpr int kParticleCount = 3000000;
constexpr long kImageElementCount = 16000000;
constexpr int kIszX = 4000;
constexpr int kIszY = 4000;
constexpr int kNfr = 1;
constexpr int kFrameIndex = 0;

constexpr int kThreadsPerBlock = 256;
constexpr int kWarmupIterations = 10;
constexpr int kTimedIterations = 100;
constexpr bool kEnableVerification = true;
constexpr int kSpecializedMaskPointCount = 69;

// PRNG Constants (Rodinia LCG)
const long PRNG_M = 2147483647;
const int PRNG_A = 1103515245;
const int PRNG_C = 12345;

// ============================================================================
// 2. DATA STRUCTURES & CPU GENERATION
// ============================================================================
struct BenchmarkConfig {
    int particles = kParticleCount;
    long maxSize = kImageElementCount;
    int blockSize = kThreadsPerBlock;
    int warmup = kWarmupIterations;
    int iters = kTimedIterations;
    bool verify = kEnableVerification;
    int iszX = kIszX;
    int iszY = kIszY;
    int nfr = kNfr;
    int frameIndex = kFrameIndex;
};

struct Dataset {
    int countOnes;
    std::vector<double> arrayX;
    std::vector<double> arrayY;
    std::vector<int> objOffsets;
    std::vector<int> image;
};

// Rodinia PRNG Logic
double randu(std::vector<int>& seed, int index) {
    long long num = (long long)PRNG_A * seed[index] + PRNG_C;
    seed[index] = static_cast<int>(num % PRNG_M);
    return std::fabs(seed[index] / static_cast<double>(PRNG_M));
}

inline int host_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

Dataset build_dataset(const BenchmarkConfig& cfg) {
    Dataset data;
    data.countOnes = kSpecializedMaskPointCount;
    data.arrayX.resize(cfg.particles);
    data.arrayY.resize(cfg.particles);
    data.objOffsets.resize(data.countOnes);
    data.image.resize(cfg.maxSize);

    // 1. Build 69-point Circular Disk offsets
    int radius = 5;
    int current_point = 0;
    for (int x = -radius + 1; x < radius; x++) {
        for (int y = -radius + 1; y < radius; y++) {
            if (std::sqrt(x * x + y * y) < radius) {
                if (current_point < data.countOnes) {
                    data.objOffsets[current_point] = x * cfg.iszY * cfg.nfr + y * cfg.nfr;
                    current_point++;
                }
            }
        }
    }

    // 2. Initialize Particles (Scattered Workload)
    std::vector<int> seed(cfg.particles);
    for (int i = 0; i < cfg.particles; i++) {
        seed[i] = 1337 * (i + 1);
    }
    
    for (int i = 0; i < cfg.particles; i++) {
        data.arrayX[i] = randu(seed, i) * static_cast<double>(cfg.iszY);
        data.arrayY[i] = randu(seed, i) * static_cast<double>(cfg.iszX);
    }

    // 3. Initialize Image Array
    for (long i = 0; i < cfg.maxSize; i++) {
        data.image[i] = 100 + (int)(i % 129);
    }

    return data;
}

void compute_reference(const BenchmarkConfig& cfg, const Dataset& data, std::vector<double>& ref) {
    ref.resize(cfg.particles, 0.0);
    const double likelihoodScale = (256.0 / 50.0) * (1.0 / static_cast<double>(data.countOnes));
    const double likelihoodBias = 41984.0 / 50.0;

    for (int i = 0; i < cfg.particles; i++) {
        int px = host_roundDouble(data.arrayX[i]);
        int py = host_roundDouble(data.arrayY[i]);
        int sum = 0;

        for (int m = 0; m < data.countOnes; m++) {
            long idx = std::abs((long)px * cfg.iszY * cfg.nfr + (long)py * cfg.nfr + cfg.frameIndex + data.objOffsets[m]);
            int pix = (idx < cfg.maxSize) ? data.image[idx] : 0;
            sum += pix;
        }
        ref[i] = likelihoodScale * static_cast<double>(sum) - likelihoodBias;
    }
}

// ============================================================================
// 3. CUDA DEVICE CODE (Split into 2 Phases)
// ============================================================================
__constant__ int g_maskOffsets[kSpecializedMaskPointCount];

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

// ----------------------------------------------------------------------------
// PHASE 1: Memory Extraction Kernels
// ----------------------------------------------------------------------------
__global__ void extract_pixels_baseline(
    int particleCount, int maskPointCount, int imageHeight, int frameCount, int currentFrame,
    long imageElementCount, const double* __restrict__ particleX, const double* __restrict__ particleY,
    const int* __restrict__ imageData, int* __restrict__ pixelBuffer) {
    
    const int particleIndex = blockIdx.x * blockDim.x + threadIdx.x;
    if (particleIndex >= particleCount) return;

    const int roundedParticleX = round_to_int_device(particleX[particleIndex]);
    const int roundedParticleY = round_to_int_device(particleY[particleIndex]);
    const long particleBaseIndex = static_cast<long>(roundedParticleX) * imageHeight * frameCount +
                                   static_cast<long>(roundedParticleY) * frameCount +
                                   currentFrame;
                                   
    for (int maskIndex = 0; maskIndex < maskPointCount; maskIndex++) {
        long imageIndex = particleBaseIndex + static_cast<long>(g_maskOffsets[maskIndex]);
        imageIndex = (imageIndex < 0) ? -imageIndex : imageIndex;
        
        int pix = (imageIndex < imageElementCount) ? imageData[imageIndex] : 0;
        pixelBuffer[particleIndex * maskPointCount + maskIndex] = pix;
    }
}

__global__ void extract_pixels_fast_69(
    int particleCount, int planeStride, int frameCount, int currentFrame, 
    const double* __restrict__ particleX, const double* __restrict__ particleY, 
    const int* __restrict__ imageData, int* __restrict__ pixelBuffer) {
    
    const int particleIndex = blockIdx.x * blockDim.x + threadIdx.x;
    if (particleIndex >= particleCount) return;

    const int roundedParticleX = round_to_int_device(particleX[particleIndex]);
    const int roundedParticleY = round_to_int_device(particleY[particleIndex]);
    const int particleBaseIndex = roundedParticleX * planeStride + roundedParticleY * frameCount + currentFrame;

    // Use __ldg caching to rapidly extract and store in the contiguous buffer
#pragma unroll
    for (int maskIndex = 0; maskIndex < kSpecializedMaskPointCount; maskIndex++) {
        pixelBuffer[particleIndex * kSpecializedMaskPointCount + maskIndex] = 
            load_image_cached(imageData + particleBaseIndex + g_maskOffsets[maskIndex]);
    }
}

// ----------------------------------------------------------------------------
// PHASE 2: Pure Likelihood Math Kernel
// ----------------------------------------------------------------------------
__global__ void compute_math_kernel(
    int particleCount, int maskPointCount, double likelihoodScale, double likelihoodBias,
    const int* __restrict__ pixelBuffer, double* __restrict__ particleLikelihood) {
    
    const int particleIndex = blockIdx.x * blockDim.x + threadIdx.x;
    if (particleIndex >= particleCount) return;

    int neighborhoodPixelSum = 0;
    const int baseOffset = particleIndex * maskPointCount;
    
    // Pure contiguous memory read (L1 cache hit guaranteed) + integer summation
#pragma unroll 4
    for (int maskIndex = 0; maskIndex < maskPointCount; maskIndex++) {
        neighborhoodPixelSum += pixelBuffer[baseOffset + maskIndex];
    }

    // Floating point scale and bias
    particleLikelihood[particleIndex] =
        likelihoodScale * static_cast<double>(neighborhoodPixelSum) - likelihoodBias;
}

// ============================================================================
// 4. CUDA EXECUTION WRAPPERS
// ============================================================================
void check_cuda(cudaError_t status, const char* what) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
    }
}

bool can_use_fast_69(const BenchmarkConfig& cfg, const Dataset& data) {
    if (data.countOnes != kSpecializedMaskPointCount) return false;
    if (cfg.maxSize > static_cast<long>(std::numeric_limits<int>::max())) return false;

    const int planeStride = cfg.iszY * cfg.nfr;
    int minParticleBaseIndex = std::numeric_limits<int>::max();
    int maxParticleBaseIndex = std::numeric_limits<int>::min();
    
    for (int particleIndex = 0; particleIndex < cfg.particles; particleIndex++) {
        const int roundedParticleX = host_roundDouble(data.arrayX[particleIndex]);
        const int roundedParticleY = host_roundDouble(data.arrayY[particleIndex]);
        
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
    const long long minImageIndex = static_cast<long long>(minParticleBaseIndex) + static_cast<long long>(*maskOffsetRange.first);
    const long long maxImageIndex = static_cast<long long>(maxParticleBaseIndex) + static_cast<long long>(*maskOffsetRange.second);
    return minImageIndex >= 0 && maxImageIndex < cfg.maxSize;
}

struct GpuRunResult {
    std::vector<double> likelihoodValues;
    double extractKernelMs;
    double mathKernelMs;
    double totalEndToEndMs;
    GpuRunResult(std::vector<double> likelihoodValuesIn, double extractKernelMsIn, double mathKernelMsIn, double totalEndToEndMsIn)
        : likelihoodValues(std::move(likelihoodValuesIn)), extractKernelMs(extractKernelMsIn), mathKernelMs(mathKernelMsIn), totalEndToEndMs(totalEndToEndMsIn) {}
};

GpuRunResult run_gpu_benchmark(const BenchmarkConfig& cfg, const Dataset& data) {
    const std::size_t particleArrayBytes = static_cast<std::size_t>(cfg.particles) * sizeof(double);
    const std::size_t imageArrayBytes = static_cast<std::size_t>(cfg.maxSize) * sizeof(int);
    const std::size_t maskBytes = static_cast<std::size_t>(data.countOnes) * sizeof(int);
    
    // Intermediate buffer to hold 69 extracted pixels per particle (Requires ~828 MB)
    const std::size_t pixelBufferBytes = static_cast<std::size_t>(cfg.particles) * data.countOnes * sizeof(int);
    
    const int gridSize = (cfg.particles + cfg.blockSize - 1) / cfg.blockSize;
    const int planeStride = cfg.iszY * cfg.nfr;
    const bool useFast69 = can_use_fast_69(cfg, data);
    
    const double likelihoodScale = (256.0 / 50.0) * (1.0 / static_cast<double>(data.countOnes));
    const double likelihoodBias = 41984.0 / 50.0;

    double* deviceParticleX = nullptr;
    double* deviceParticleY = nullptr;
    int* deviceImage = nullptr;
    int* devicePixelBuffer = nullptr;
    double* deviceLikelihood = nullptr;

    double* pinnedParticleX = nullptr;
    double* pinnedParticleY = nullptr;
    int* pinnedImage = nullptr;
    double* pinnedLikelihood = nullptr;
    
    cudaStream_t stream = nullptr;
    cudaEvent_t extractStartEvent = nullptr, extractStopEvent = nullptr;
    cudaEvent_t mathStartEvent = nullptr, mathStopEvent = nullptr;
    cudaEvent_t totalStartEvent = nullptr, totalStopEvent = nullptr;
    std::vector<double> hostLikelihood(static_cast<std::size_t>(cfg.particles), 0.0);

    check_cuda(cudaMallocHost(&pinnedParticleX, particleArrayBytes), "cudaMallocHost arrayX");
    check_cuda(cudaMallocHost(&pinnedParticleY, particleArrayBytes), "cudaMallocHost arrayY");
    check_cuda(cudaMallocHost(&pinnedImage, imageArrayBytes), "cudaMallocHost image");
    check_cuda(cudaMallocHost(&pinnedLikelihood, particleArrayBytes), "cudaMallocHost likelihood");
    std::memcpy(pinnedParticleX, data.arrayX.data(), particleArrayBytes);
    std::memcpy(pinnedParticleY, data.arrayY.data(), particleArrayBytes);
    std::memcpy(pinnedImage, data.image.data(), imageArrayBytes);

    check_cuda(cudaMalloc(&deviceParticleX, particleArrayBytes), "cudaMalloc arrayX");
    check_cuda(cudaMalloc(&deviceParticleY, particleArrayBytes), "cudaMalloc arrayY");
    check_cuda(cudaMalloc(&deviceImage, imageArrayBytes), "cudaMalloc image");
    check_cuda(cudaMalloc(&devicePixelBuffer, pixelBufferBytes), "cudaMalloc pixelBuffer");
    check_cuda(cudaMalloc(&deviceLikelihood, particleArrayBytes), "cudaMalloc likelihood");

    check_cuda(cudaStreamCreate(&stream), "cudaStreamCreate");
    check_cuda(cudaEventCreate(&extractStartEvent), "cudaEventCreate extract start");
    check_cuda(cudaEventCreate(&extractStopEvent), "cudaEventCreate extract stop");
    check_cuda(cudaEventCreate(&mathStartEvent), "cudaEventCreate math start");
    check_cuda(cudaEventCreate(&mathStopEvent), "cudaEventCreate math stop");
    check_cuda(cudaEventCreate(&totalStartEvent), "cudaEventCreate total start");
    check_cuda(cudaEventCreate(&totalStopEvent), "cudaEventCreate total stop");

    check_cuda(cudaEventRecord(totalStartEvent, stream), "cudaEventRecord total start");
    check_cuda(cudaMemcpyAsync(deviceParticleX, pinnedParticleX, particleArrayBytes, cudaMemcpyHostToDevice, stream), "cpy arrayX");
    check_cuda(cudaMemcpyAsync(deviceParticleY, pinnedParticleY, particleArrayBytes, cudaMemcpyHostToDevice, stream), "cpy arrayY");
    check_cuda(cudaMemcpyAsync(deviceImage, pinnedImage, imageArrayBytes, cudaMemcpyHostToDevice, stream), "cpy image");
    check_cuda(cudaMemcpyToSymbol(g_maskOffsets, data.objOffsets.data(), maskBytes), "cudaMemcpyToSymbol objOffsets");
    
    // Warmup
    for (int iter = 0; iter < cfg.warmup; iter++) {
        if (useFast69) {
            extract_pixels_fast_69<<<gridSize, cfg.blockSize, 0, stream>>>(
                cfg.particles, planeStride, cfg.nfr, cfg.frameIndex, 
                deviceParticleX, deviceParticleY, deviceImage, devicePixelBuffer);
        } else {
            extract_pixels_baseline<<<gridSize, cfg.blockSize, 0, stream>>>(
                cfg.particles, data.countOnes, cfg.iszY, cfg.nfr, cfg.frameIndex, cfg.maxSize, 
                deviceParticleX, deviceParticleY, deviceImage, devicePixelBuffer);
        }
            
        compute_math_kernel<<<gridSize, cfg.blockSize, 0, stream>>>(
            cfg.particles, data.countOnes, likelihoodScale, likelihoodBias,
            devicePixelBuffer, deviceLikelihood);
    }
    check_cuda(cudaStreamSynchronize(stream), "cudaStreamSynchronize warmup");

    // Timed Execution: Extraction Phase (Memory Bounds)
    check_cuda(cudaEventRecord(extractStartEvent, stream), "cudaEventRecord extract start");
    for (int iter = 0; iter < cfg.iters; iter++) {
        if (useFast69) {
            extract_pixels_fast_69<<<gridSize, cfg.blockSize, 0, stream>>>(
                cfg.particles, planeStride, cfg.nfr, cfg.frameIndex, 
                deviceParticleX, deviceParticleY, deviceImage, devicePixelBuffer);
        } else {
            extract_pixels_baseline<<<gridSize, cfg.blockSize, 0, stream>>>(
                cfg.particles, data.countOnes, cfg.iszY, cfg.nfr, cfg.frameIndex, cfg.maxSize, 
                deviceParticleX, deviceParticleY, deviceImage, devicePixelBuffer);
        }
    }
    check_cuda(cudaEventRecord(extractStopEvent, stream), "cudaEventRecord extract stop");

    // Timed Execution: Math Phase (Pure Computation)
    check_cuda(cudaEventRecord(mathStartEvent, stream), "cudaEventRecord math start");
    for (int iter = 0; iter < cfg.iters; iter++) {
        compute_math_kernel<<<gridSize, cfg.blockSize, 0, stream>>>(
            cfg.particles, data.countOnes, likelihoodScale, likelihoodBias,
            devicePixelBuffer, deviceLikelihood);
    }
    check_cuda(cudaEventRecord(mathStopEvent, stream), "cudaEventRecord math stop");

    check_cuda(cudaMemcpyAsync(pinnedLikelihood, deviceLikelihood, particleArrayBytes, cudaMemcpyDeviceToHost, stream), "cpy likelihood");
    check_cuda(cudaEventRecord(totalStopEvent, stream), "cudaEventRecord total stop");
    check_cuda(cudaEventSynchronize(totalStopEvent), "cudaEventSynchronize total stop");

    float extractTotalMs = 0.0f;
    float mathTotalMs = 0.0f;
    float totalEndToEndMs = 0.0f;
    check_cuda(cudaEventElapsedTime(&extractTotalMs, extractStartEvent, extractStopEvent), "cudaEventElapsedTime extract");
    check_cuda(cudaEventElapsedTime(&mathTotalMs, mathStartEvent, mathStopEvent), "cudaEventElapsedTime math");
    check_cuda(cudaEventElapsedTime(&totalEndToEndMs, totalStartEvent, totalStopEvent), "cudaEventElapsedTime total");

    std::memcpy(hostLikelihood.data(), pinnedLikelihood, particleArrayBytes);
    
    // Cleanup
    cudaEventDestroy(extractStartEvent); cudaEventDestroy(extractStopEvent);
    cudaEventDestroy(mathStartEvent); cudaEventDestroy(mathStopEvent);
    cudaEventDestroy(totalStartEvent); cudaEventDestroy(totalStopEvent);
    cudaStreamDestroy(stream);
    cudaFree(deviceParticleX); cudaFree(deviceParticleY); cudaFree(deviceImage); cudaFree(deviceLikelihood); cudaFree(devicePixelBuffer);
    cudaFreeHost(pinnedParticleX); cudaFreeHost(pinnedParticleY); cudaFreeHost(pinnedImage); cudaFreeHost(pinnedLikelihood);
    
    return GpuRunResult(std::move(hostLikelihood), static_cast<double>(extractTotalMs) / cfg.iters, static_cast<double>(mathTotalMs) / cfg.iters, static_cast<double>(totalEndToEndMs));
}

// ============================================================================
// 5. MAIN EXECUTION
// ============================================================================
int main() {
    try {
        BenchmarkConfig cfg;
        const Dataset data = build_dataset(cfg);
        
        std::cout << "Starting GPU Benchmark (Particles=" << cfg.particles << ", Points=" << data.countOnes << ")...\n";
        const GpuRunResult result = run_gpu_benchmark(cfg, data);

        double checksum = 0.0;
        for (double val : result.likelihoodValues) checksum += val;
        
        std::cout << std::fixed << std::setprecision(6)
                  << "[GPU] grid_size=" << ((cfg.particles + cfg.blockSize - 1) / cfg.blockSize) << "\n"
                  << "[GPU] block_size=" << cfg.blockSize << "\n"
                  << "[GPU] avg_EXTRACT_ms=" << result.extractKernelMs << "\n"
                  << "[GPU] avg_MATH_ms=" << result.mathKernelMs << "\n"
                  << "[GPU] total_end_to_end_ms=" << result.totalEndToEndMs << "\n"
                  << "[GPU] checksum=" << std::setprecision(15) << checksum << "\n";
                  
        if (cfg.verify) {
            std::vector<double> reference;
            compute_reference(cfg, data, reference);
            
            double maxAbsErr = 0.0;
            for (int i = 0; i < cfg.particles; i++) {
                double err = std::abs(result.likelihoodValues[i] - reference[i]);
                maxAbsErr = std::max(maxAbsErr, err);
            }

            std::cout << "[GPU] max_abs_err=" << std::setprecision(15) << maxAbsErr << "\n";
            
            if (maxAbsErr > 1.0e-5) {
                std::cerr << "[GPU] Verification FAILED\n";
                return 1;
            } else {
                std::cout << "[GPU] Verification PASSED\n";
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}