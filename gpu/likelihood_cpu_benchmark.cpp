#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#else
inline int omp_get_max_threads() {
    return 1;
}
#endif

#include "likelihood_benchmark_common.h"

namespace {

using likelihood_bench::BenchmarkConfig;
using likelihood_bench::Dataset;

// Edit only this block when you want a different benchmark setup.
constexpr int kParticles = likelihood_bench::kDefaultParticles;
constexpr long kMaxSize = likelihood_bench::kDefaultMaxSize;
constexpr int kThreads = 4;
constexpr int kWarmupIters = 3;
constexpr int kTimedIters = 20;
constexpr bool kVerifyResult = true;

void run_likelihood_cpu(const BenchmarkConfig& cfg,
                        const Dataset& data,
                        std::vector<double>& likelihood) {
    const double scale = likelihood_bench::pixel_scale(data.countOnes);
    const double bias = likelihood_bench::pixel_bias();
    likelihood.assign(static_cast<std::size_t>(cfg.particles), 0.0);

#if defined(_OPENMP)
#pragma omp parallel for schedule(static) num_threads(cfg.threads > 0 ? cfg.threads : omp_get_max_threads())
#endif
    for (int x = 0; x < cfg.particles; x++) {
        const int px = likelihood_bench::round_double(data.arrayX[static_cast<std::size_t>(x)]);
        const int py = likelihood_bench::round_double(data.arrayY[static_cast<std::size_t>(x)]);
        const long particleBase = static_cast<long>(px) * cfg.iszY * cfg.nfr +
                                  static_cast<long>(py) * cfg.nfr +
                                  cfg.frameIndex;

        int pixelSum = 0;
        for (int y = 0; y < data.countOnes; y++) {
            long idx = particleBase + static_cast<long>(data.objOffsets[static_cast<std::size_t>(y)]);
            idx = (idx < 0) ? -idx : idx;
            if (idx >= cfg.maxSize) {
                idx = 0;
            }
            pixelSum += data.image[static_cast<std::size_t>(idx)];
        }

        likelihood[static_cast<std::size_t>(x)] = scale * static_cast<double>(pixelSum) - bias;
    }
}

struct CpuRunResult {
    std::vector<double> likelihood;
    double avgKernelMs;
    double bestKernelMs;

    CpuRunResult(std::vector<double> likelihoodIn,
                 double avgKernelMsIn,
                 double bestKernelMsIn)
        : likelihood(std::move(likelihoodIn)),
          avgKernelMs(avgKernelMsIn),
          bestKernelMs(bestKernelMsIn) {}
};

CpuRunResult run_cpu_benchmark(const BenchmarkConfig& cfg, const Dataset& data) {
    std::vector<double> likelihood(static_cast<std::size_t>(cfg.particles), 0.0);

    for (int iter = 0; iter < cfg.warmup; iter++) {
        run_likelihood_cpu(cfg, data, likelihood);
    }

    double totalMs = 0.0;
    double bestMs = 0.0;
    for (int iter = 0; iter < cfg.iters; iter++) {
        const auto start = std::chrono::steady_clock::now();
        run_likelihood_cpu(cfg, data, likelihood);
        const auto end = std::chrono::steady_clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(end - start).count();
        totalMs += elapsedMs;
        bestMs = (iter == 0) ? elapsedMs : std::min(bestMs, elapsedMs);
    }

    return CpuRunResult(std::move(likelihood),
                        totalMs / static_cast<double>(cfg.iters),
                        bestMs);
}

}  // namespace

int main() {
    try {
        BenchmarkConfig cfg;
        cfg.particles = kParticles;
        cfg.maxSize = kMaxSize;
        cfg.threads = kThreads;
        cfg.warmup = kWarmupIters;
        cfg.iters = kTimedIters;
        cfg.verify = kVerifyResult;

        const Dataset data = likelihood_bench::build_dataset(cfg);
        const CpuRunResult result = run_cpu_benchmark(cfg, data);

        likelihood_bench::print_config("CPU", cfg, data.countOnes);
        std::cout << "[CPU] threads="
                  << (cfg.threads > 0 ? cfg.threads : omp_get_max_threads()) << "\n"
                  << std::fixed << std::setprecision(6)
                  << "[CPU] avg_kernel_ms=" << result.avgKernelMs << "\n"
                  << "[CPU] best_kernel_ms=" << result.bestKernelMs << "\n"
                  << "[CPU] checksum=" << std::setprecision(15)
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

            std::cout << "[CPU] max_abs_err=" << std::setprecision(15) << maxAbsErr << "\n";
            if (maxAbsErr > 1.0e-9) {
                std::cerr << "[CPU] verification failed\n";
                return 1;
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
