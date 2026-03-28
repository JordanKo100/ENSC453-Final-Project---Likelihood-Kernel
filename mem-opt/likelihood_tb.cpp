#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "likelihood_kernel.h"

#define TB_MAX_NPARTICLES 130
#define TB_ISZY 480
#define TB_NFR 3
#define TB_K 1
#define TB_MAX_SIZE 1000000
#define TB_MAX_COUNTONES 80

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

// Bit manipulation helpers for packing doubles into 512-bit wide_t
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

void pack_doubles_to_wide(const double* in, wide_t* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = 0;
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const int idx = w * DOUBLES_PER_WORD + d;
            const double val = (idx < count) ? in[idx] : 0.0;
            const uint64_t bits = double_to_bits_tb(val);
            pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = bits;
        }
        out[w] = pack;
    }
}

void unpack_wide_to_doubles(const wide_t* in, double* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = in[w];
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const int idx = w * DOUBLES_PER_WORD + d;
            if (idx < count) {
                const uint64_t bits =
                    (uint64_t)pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                out[idx] = bits_to_double_tb(bits);
            }
        }
    }
}

int build_objxy_radius5(double* objxy) {
    const int radius = 5;
    const int diameter = radius * 2 - 1;
    const int center = radius - 1;

    int countOnes = 0;

    for (int x = 0; x < diameter; x++) {
        for (int y = 0; y < diameter; y++) {
            double distance = std::sqrt(
                std::pow((double)(x - center), 2.0) +
                std::pow((double)(y - center), 2.0));

            if (distance < radius) {
                objxy[countOnes * 2] = (double)(y - center);
                objxy[countOnes * 2 + 1] = (double)(x - center);
                countOnes++;
            }
        }
    }

    return countOnes;
}

void compute_reference(int Nparticles,
                       int countOnes,
                       int IszY,
                       int Nfr,
                       int k,
                       long max_size,
                       const double* arrayX,
                       const double* arrayY,
                       const double* objxy,
                       const int* I,
                       double* likelihood_ref) {
    for (int x = 0; x < Nparticles; x++) {
        int px = tb_roundDouble(arrayX[x]);
        int py = tb_roundDouble(arrayY[x]);
        double sum = 0.0;

        for (int y = 0; y < countOnes; y++) {
            int offY = tb_roundDouble(objxy[y * 2]);
            int offX = tb_roundDouble(objxy[y * 2 + 1]);
            int indX = px + offX;
            int indY = py + offY;
            long idx = std::labs((long)indX * (long)IszY * (long)Nfr +
                                 (long)indY * (long)Nfr + (long)k);

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

void init_particles_interior(double* arrayX, double* arrayY, int Nparticles) {
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = 80.0 + (i % 23) * 1.75;
        arrayY[i] = 120.0 + (i % 19) * 1.125;
    }
}

void init_particles_boundary(double* arrayX, double* arrayY, int Nparticles) {
    for (int i = 0; i < Nparticles; i++) {
        switch (i % 6) {
            case 0:
                arrayX[i] = -3.6;
                arrayY[i] = -1.8;
                break;
            case 1:
                arrayX[i] = 0.2;
                arrayY[i] = 479.7;
                break;
            case 2:
                arrayX[i] = 4800.0 + (double)i;
                arrayY[i] = 470.4;
                break;
            case 3:
                arrayX[i] = 1.2;
                arrayY[i] = -4.7;
                break;
            case 4:
                arrayX[i] = 100000.0 + (double)(i * 11);
                arrayY[i] = 100000.0 - (double)(i * 7);
                break;
            default:
                arrayX[i] = 82.0 + (i % 9) * 2.4;
                arrayY[i] = 118.0 + (i % 11) * 1.6;
                break;
        }
    }
}

bool run_case(const char* label,
              int Nparticles,
              bool boundary_case,
              long max_size,
              const double* objxy,
              int countOnes,
              const int* I) {
    static double arrayX[TB_MAX_NPARTICLES];
    static double arrayY[TB_MAX_NPARTICLES];
    static double likelihood_hw[TB_MAX_NPARTICLES];
    static double likelihood_ref[TB_MAX_NPARTICLES];

    // Wide array allocations
    static wide_t arrayX_wide[(TB_MAX_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];
    static wide_t arrayY_wide[(TB_MAX_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];
    static wide_t objxy_wide[(TB_MAX_COUNTONES * 2 + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];
    static wide_t likelihood_wide[(TB_MAX_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD];

    for (int i = 0; i < TB_MAX_NPARTICLES; i++) {
        arrayX[i] = 0.0;
        arrayY[i] = 0.0;
        likelihood_hw[i] = 0.0;
        likelihood_ref[i] = 0.0;
    }

    if (boundary_case) {
        init_particles_boundary(arrayX, arrayY, Nparticles);
    } else {
        init_particles_interior(arrayX, arrayY, Nparticles);
    }

    // Pack standard double arrays into 512-bit wide_t arrays
    pack_doubles_to_wide(arrayX, arrayX_wide, Nparticles);
    pack_doubles_to_wide(arrayY, arrayY_wide, Nparticles);
    // objxy stores (y, x) pairs, so the total count is countOnes * 2
    pack_doubles_to_wide(objxy, objxy_wide, countOnes * 2);

    // Initialize the output wide array
    for (int i = 0; i < (TB_MAX_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD; i++) {
        likelihood_wide[i] = 0;
    }

    compute_reference(
        Nparticles,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        max_size,
        arrayX,
        arrayY,
        objxy,
        I,
        likelihood_ref);

    auto start = std::chrono::high_resolution_clock::now();

    // Call the hardware kernel with the widened interfaces
    likelihood_kernel(
        Nparticles,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        max_size,
        arrayX_wide,
        arrayY_wide,
        objxy_wide,
        I,
        likelihood_wide);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Unpack the wide result back into standard doubles for comparison
    unpack_wide_to_doubles(likelihood_wide, likelihood_hw, Nparticles);

    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-9;

    for (int i = 0; i < Nparticles; i++) {
        double err = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err > max_abs_err) {
            max_abs_err = err;
        }
        if (err > tol) {
            pass = false;
            std::cout << label << " mismatch at particle " << i
                      << ": HW = " << std::setprecision(12) << likelihood_hw[i]
                      << ", REF = " << likelihood_ref[i]
                      << ", ABS_ERR = " << err << "\n";
            break;
        }
    }

    std::cout << label
              << "  Nparticles = " << Nparticles
              << "  elapsed = " << elapsed.count() << " s"
              << "  max_abs_err = " << std::setprecision(12) << max_abs_err
              << "\n";

    return pass;
}

int main() {
    static double objxy[TB_MAX_COUNTONES * 2];
    static int I[TB_MAX_SIZE];

    int countOnes = build_objxy_radius5(objxy);
    if (countOnes > TB_MAX_COUNTONES) {
        std::cerr << "ERROR: countOnes = " << countOnes
                  << " exceeds TB_MAX_COUNTONES = " << TB_MAX_COUNTONES << "\n";
        return 1;
    }

    for (long i = 0; i < TB_MAX_SIZE; i++) {
        I[i] = 100 + (int)(i % 129);
    }

    bool pass = true;
    pass &= run_case("single_tile_exact", 64, false, TB_MAX_SIZE, objxy, countOnes, I);
    pass &= run_case("multi_tile_exact", 128, false, TB_MAX_SIZE, objxy, countOnes, I);
    pass &= run_case("multi_tile_tail", 73, false, TB_MAX_SIZE, objxy, countOnes, I);
    pass &= run_case("small_case", 3, false, TB_MAX_SIZE, objxy, countOnes, I);
    pass &= run_case("boundary_case", 65, true, TB_MAX_SIZE, objxy, countOnes, I);
    pass &= run_case("clamp_case", 65, true, 37, objxy, countOnes, I);

    if (pass) {
        std::cout << "TEST PASSED\n";
        return 0;
    }

    std::cout << "TEST FAILED\n";
    return 1;
}