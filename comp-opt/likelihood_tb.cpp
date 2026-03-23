#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <vector>
#include <cstdlib>
#include "likelihood_kernel.h"

// -----------------------------------------------------------------------------
// Better testbench for likelihood_kernel
// - Uses a realistic disk-style objxy for radius = 5
// - Uses varied particle positions
// - Uses varied image contents
// - Computes a software reference and compares against kernel output
// -----------------------------------------------------------------------------

#define TB_NPARTICLES 64
#define TB_ISZY       480
#define TB_NFR        3
#define TB_K          1
#define TB_MAX_SIZE   1000000

// Match your kernel capacity
#define TB_MAX_COUNTONES 80

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + 0.5);
}

// Build realistic objxy from a disk of radius 5, matching the original code style.
// neighbors[2*i]     = y offset
// neighbors[2*i + 1] = x offset
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

    return countOnes; // should be 69 for radius 5
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

int main() {
    static double arrayX[TB_NPARTICLES];
    static double arrayY[TB_NPARTICLES];
    static double objxy[TB_MAX_COUNTONES * 2];
    static int    I[TB_MAX_SIZE];
    static double likelihood_hw[TB_NPARTICLES];
    static double likelihood_ref[TB_NPARTICLES];

    // -------------------------------------------------------------------------
    // Build realistic objxy for radius=5
    // -------------------------------------------------------------------------
    int countOnes = build_objxy_radius5(objxy);

    if (countOnes > TB_MAX_COUNTONES) {
        std::cerr << "ERROR: countOnes = " << countOnes
                  << " exceeds TB_MAX_COUNTONES = " << TB_MAX_COUNTONES << "\n";
        return 1;
    }

    std::cout << "countOnes = " << countOnes << std::endl;

    // -------------------------------------------------------------------------
    // Initialize particles with varied positions
    // Keep them away from extreme edges to reduce accidental clamping
    // -------------------------------------------------------------------------
    for (int i = 0; i < TB_NPARTICLES; i++) {
        arrayX[i] = 80.0 + (i % 16) * 3.25;   // varying x positions
        arrayY[i] = 120.0 + (i % 8) * 2.75;   // varying y positions
        likelihood_hw[i] = 0.0;
        likelihood_ref[i] = 0.0;
    }

    // -------------------------------------------------------------------------
    // Initialize image with varied contents
    // This is still synthetic, but much better than filling everything with 120
    // -------------------------------------------------------------------------
    for (long i = 0; i < TB_MAX_SIZE; i++) {
        I[i] = 100 + (i % 129); // values cycle from 100 to 228
    }

    // -------------------------------------------------------------------------
    // Compute software reference
    // -------------------------------------------------------------------------
    compute_reference(
        TB_NPARTICLES,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        TB_MAX_SIZE,
        arrayX,
        arrayY,
        objxy,
        I,
        likelihood_ref
    );

    // -------------------------------------------------------------------------
    // Call kernel
    // -------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();

    likelihood_kernel(
        TB_NPARTICLES,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        TB_MAX_SIZE,
        arrayX,
        arrayY,
        objxy,
        I,
        likelihood_hw
    );

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Kernel execution time: " << elapsed.count() << " s\n";

    // -------------------------------------------------------------------------
    // Compare results
    // -------------------------------------------------------------------------
    bool pass = true;
    const double tol = 1e-9;
    double max_abs_err = 0.0;

    for (int i = 0; i < TB_NPARTICLES; i++) {
        double err = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err > max_abs_err) {
            max_abs_err = err;
        }

        if (err > tol) {
            pass = false;
            std::cout << "Mismatch at particle " << i
                      << ": HW = " << std::setprecision(12) << likelihood_hw[i]
                      << ", REF = " << likelihood_ref[i]
                      << ", ABS_ERR = " << err << "\n";
        }
    }

    std::cout << "Max absolute error: " << std::setprecision(12)
              << max_abs_err << "\n";

    for (int i = 0; i < 5; i++) {
        std::cout << "particle[" << i << "]  HW = "
                  << std::setprecision(12) << likelihood_hw[i]
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
