#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace likelihood_bench {

static constexpr int kDefaultParticles = 10000;
static constexpr int kDefaultIszY = 480;
static constexpr int kDefaultNfr = 3;
static constexpr int kDefaultFrameIndex = 1;
static constexpr long kDefaultMaxSize = 1000000;
static constexpr int kMaxCountOnes = 80;
static constexpr double kPixelScaleNum = 256.0;
static constexpr double kPixelBiasNum = 41984.0;
static constexpr double kPixelDen = 50.0;

struct BenchmarkConfig {
    int particles = kDefaultParticles;
    int iszY = kDefaultIszY;
    int nfr = kDefaultNfr;
    int frameIndex = kDefaultFrameIndex;
    long maxSize = kDefaultMaxSize;
    int threads = 0;
    int iters = 20;
    int warmup = 3;
    int blockSize = 256;
    bool verify = false;
};

struct Dataset {
    std::vector<double> arrayX;
    std::vector<double> arrayY;
    std::vector<double> objxy;
    std::vector<int> objOffsets;
    std::vector<int> image;
    int countOnes = 0;
};

inline int round_double(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline double pixel_scale(int countOnes) {
    return (kPixelScaleNum / kPixelDen) / static_cast<double>(countOnes);
}

inline double pixel_bias() {
    return kPixelBiasNum / kPixelDen;
}

inline int build_objxy_radius5(std::vector<double>& objxy) {
    const int radius = 5;
    const int diameter = radius * 2 - 1;
    const int center = radius - 1;

    int countOnes = 0;
    for (int x = 0; x < diameter; x++) {
        for (int y = 0; y < diameter; y++) {
            const double dx = static_cast<double>(x - center);
            const double dy = static_cast<double>(y - center);
            const double distance = std::sqrt(dx * dx + dy * dy);

            if (distance < static_cast<double>(radius)) {
                objxy[countOnes * 2] = static_cast<double>(y - center);
                objxy[countOnes * 2 + 1] = static_cast<double>(x - center);
                countOnes++;
            }
        }
    }

    return countOnes;
}

inline void build_obj_offsets(const BenchmarkConfig& cfg,
                              const std::vector<double>& objxy,
                              int countOnes,
                              std::vector<int>& objOffsets) {
    objOffsets.resize(static_cast<std::size_t>(countOnes));
    for (int i = 0; i < countOnes; i++) {
        const int offY = round_double(objxy[static_cast<std::size_t>(i) * 2]);
        const int offX = round_double(objxy[static_cast<std::size_t>(i) * 2 + 1]);
        objOffsets[static_cast<std::size_t>(i)] = offX * cfg.iszY * cfg.nfr + offY * cfg.nfr;
    }
}

inline void init_particles(std::vector<double>& arrayX, std::vector<double>& arrayY) {
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        arrayX[i] = 80.0 + static_cast<double>(i % 64) * 1.125;
        arrayY[i] = 120.0 + static_cast<double>(i % 32) * 0.875;
    }
}

inline void init_image(std::vector<int>& image) {
    for (std::size_t i = 0; i < image.size(); i++) {
        image[i] = 100 + static_cast<int>(i % 129);
    }
}

inline Dataset build_dataset(const BenchmarkConfig& cfg) {
    Dataset data;
    data.arrayX.resize(static_cast<std::size_t>(cfg.particles));
    data.arrayY.resize(static_cast<std::size_t>(cfg.particles));
    data.objxy.assign(static_cast<std::size_t>(kMaxCountOnes) * 2, 0.0);
    data.image.resize(static_cast<std::size_t>(cfg.maxSize));

    init_particles(data.arrayX, data.arrayY);
    init_image(data.image);
    data.countOnes = build_objxy_radius5(data.objxy);

    if (data.countOnes > kMaxCountOnes) {
        throw std::runtime_error("countOnes exceeds kMaxCountOnes");
    }

    build_obj_offsets(cfg, data.objxy, data.countOnes, data.objOffsets);
    return data;
}

inline void compute_reference(const BenchmarkConfig& cfg,
                              const Dataset& data,
                              std::vector<double>& likelihood) {
    likelihood.assign(static_cast<std::size_t>(cfg.particles), 0.0);

    for (int x = 0; x < cfg.particles; x++) {
        const int px = round_double(data.arrayX[static_cast<std::size_t>(x)]);
        const int py = round_double(data.arrayY[static_cast<std::size_t>(x)]);
        double sum = 0.0;

        for (int y = 0; y < data.countOnes; y++) {
            const int offY = round_double(data.objxy[static_cast<std::size_t>(y) * 2]);
            const int offX = round_double(data.objxy[static_cast<std::size_t>(y) * 2 + 1]);
            const int indX = px + offX;
            const int indY = py + offY;
            long idx = std::labs(static_cast<long>(indX) * cfg.iszY * cfg.nfr +
                                 static_cast<long>(indY) * cfg.nfr + cfg.frameIndex);

            if (idx >= cfg.maxSize) {
                idx = 0;
            }

            const int pix = data.image[static_cast<std::size_t>(idx)];
            const int a = pix - 100;
            const int b = pix - 228;
            sum += (static_cast<double>(a * a) - static_cast<double>(b * b)) / kPixelDen;
        }

        likelihood[static_cast<std::size_t>(x)] = sum / static_cast<double>(data.countOnes);
    }
}

inline double checksum(const std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum;
}

inline void print_config(const std::string& label,
                         const BenchmarkConfig& cfg,
                         int countOnes) {
    std::cout << "[" << label << "] particles=" << cfg.particles
              << ", countOnes=" << countOnes
              << ", iszY=" << cfg.iszY
              << ", nfr=" << cfg.nfr
              << ", frame=" << cfg.frameIndex
              << ", maxSize=" << cfg.maxSize
              << ", warmup=" << cfg.warmup
              << ", iters=" << cfg.iters
              << "\n";
}

}  // namespace likelihood_bench
