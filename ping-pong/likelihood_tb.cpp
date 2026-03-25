#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <cstdint>
#include "likelihood_kernel.h"

#define TB_NPARTICLES 64
#define TB_ISZY 480
#define TB_NFR 3
#define TB_K 1
#define TB_MAX_SIZE 1000000
#define TB_MAX_COUNTONES 80

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + 0.5);
}

inline double bits_to_double_tb(uint64_t bits) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = bits;
    return conv.d;
}

inline uint64_t double_to_bits_tb(double val) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.d = val;
    return conv.u;
}

// Build realistic objxy for radius = 5
int build_objxy_radius5(double* objxy) {
    const int radius = 5;
    const int diameter = radius * 2 - 1;
    const int center = radius - 1;

    int countOnes = 0;

    for (int x = 0; x < diameter; x++) {
        for (int y = 0; y < diameter; y++) {
            double distance = std::sqrt(
                std::pow((double)(x - center), 2.0) +
                std::pow((double)(y - center), 2.0)
            );

            if (distance < radius) {
                objxy[countOnes * 2]     = (double)(y - center); // y offset
                objxy[countOnes * 2 + 1] = (double)(x - center); // x offset
                countOnes++;
            }
        }
    }

    return countOnes; // should be 69
}

void compute_reference(
    int Nparticles,
    int countOnes,
    int IszY,
    int Nfr,
    int k,
    long max_size,
    const double* arrayX,
    const double* arrayY,
    const double* objxy,
    const int* I,
    double* likelihood_ref
) {
    for (int x = 0; x < Nparticles; x++) {
        int px = tb_roundDouble(arrayX[x]);
        int py = tb_roundDouble(arrayY[x]);
        double sum = 0.0;

        for (int y = 0; y < countOnes; y++) {
            int offY = tb_roundDouble(objxy[y * 2]);
            int offX = tb_roundDouble(objxy[y * 2 + 1]);

            int indX = px + offX;
            int indY = py + offY;

            int idx = std::abs(indX * IszY * Nfr + indY * Nfr + k);
            if (idx >= max_size) {
                idx = 0;
            }

            int pix = I[idx];
            int a = pix - 100;
            int b = pix - 228;

            sum += ((double)(a * a) - (double)(b * b)) / 50.0;
        }

        likelihood_ref[x] = sum / (double)countOnes;
    }
}

// Pack scalar doubles into wide_t array
void pack_doubles_to_wide(const double* in, wide_t* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    for (int w = 0; w < words; w++) {
        wide_t pack = 0;

        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            int idx = w * DOUBLES_PER_WORD + d;
            double val = (idx < count) ? in[idx] : 0.0;
            uint64_t bits = double_to_bits_tb(val);
            pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = bits;
        }

        out[w] = pack;
    }
}

// Unpack wide_t array into scalar doubles
void unpack_wide_to_doubles(const wide_t* in, double* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    for (int w = 0; w < words; w++) {
        wide_t pack = in[w];

        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            int idx = w * DOUBLES_PER_WORD + d;
            if (idx < count) {
                uint64_t bits = (uint64_t)pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                out[idx] = bits_to_double_tb(bits);
            }
        }
    }
}

int main() {
    static double arrayX_scalar[TB_NPARTICLES];
    static double arrayY_scalar[TB_NPARTICLES];
    static double objxy[TB_MAX_COUNTONES * 2];
    static int I[TB_MAX_SIZE];

    static double likelihood_hw_scalar[TB_NPARTICLES];
    static double likelihood_ref[TB_NPARTICLES];

    static wide_t arrayX_wide[(TB_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];
    static wide_t arrayY_wide[(TB_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];
    static wide_t likelihood_wide[(TB_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];

    int countOnes = build_objxy_radius5(objxy);

    if (countOnes > TB_MAX_COUNTONES) {
        std::cerr << "ERROR: countOnes = " << countOnes
                  << " exceeds TB_MAX_COUNTONES = " << TB_MAX_COUNTONES << "\n";
        return 1;
    }

    std::cout << "countOnes = " << countOnes << std::endl;

    // Initialize particles with varied positions
    for (int i = 0; i < TB_NPARTICLES; i++) {
        arrayX_scalar[i] = 80.0 + (i % 16) * 3.25;
        arrayY_scalar[i] = 120.0 + (i % 8) * 2.75;
        likelihood_hw_scalar[i] = 0.0;
        likelihood_ref[i] = 0.0;
    }

    // Initialize image with varied contents
    for (long i = 0; i < TB_MAX_SIZE; i++) {
        I[i] = 100 + (i % 129); // cycles 100..228
    }

    // Pack X and Y into wide words
    pack_doubles_to_wide(arrayX_scalar, arrayX_wide, TB_NPARTICLES);
    pack_doubles_to_wide(arrayY_scalar, arrayY_wide, TB_NPARTICLES);

    // Optional init
    for (int i = 0; i < (TB_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD; i++) {
        likelihood_wide[i] = 0;
    }

    // Compute software reference
    compute_reference(
        TB_NPARTICLES,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        TB_MAX_SIZE,
        arrayX_scalar,
        arrayY_scalar,
        objxy,
        I,
        likelihood_ref
    );

    auto start = std::chrono::high_resolution_clock::now();

    likelihood_kernel(
        TB_NPARTICLES,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        TB_MAX_SIZE,
        arrayX_wide,
        arrayY_wide,
        objxy,
        I,
        likelihood_wide
    );

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Unpack likelihood output
    unpack_wide_to_doubles(likelihood_wide, likelihood_hw_scalar, TB_NPARTICLES);

    std::cout << "Kernel execution time: " << elapsed.count() << " s\n";

    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-9;

    for (int i = 0; i < TB_NPARTICLES; i++) {
        double err = std::fabs(likelihood_hw_scalar[i] - likelihood_ref[i]);

        if (err > max_abs_err) {
            max_abs_err = err;
        }

        if (err > tol) {
            pass = false;
            std::cout << "Mismatch at particle " << i
                      << ": HW = " << std::setprecision(12) << likelihood_hw_scalar[i]
                      << ", REF = " << likelihood_ref[i]
                      << ", ABS_ERR = " << err << "\n";
        }
    }

    std::cout << "Max absolute error: " << std::setprecision(12)
              << max_abs_err << "\n";

    for (int i = 0; i < 5; i++) {
        std::cout << "particle[" << i << "]  HW = "
                  << std::setprecision(12) << likelihood_hw_scalar[i]
                  << "   REF = " << likelihood_ref[i] << "\n";
    }

    if (pass) {
        std::cout << "TEST PASSED\n";
        return 0;
    } else {
        std::cout << "TEST FAILED\n";
        return 1;
    }
}
