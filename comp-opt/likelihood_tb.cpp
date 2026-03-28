#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "likelihood_kernel.h"

#define TB_MAX_NPARTICLES 130
#define TB_ISZY 480
#define TB_NFR 3
#define TB_K 1
#define TB_MAX_SIZE 1000000

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
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

    likelihood_kernel(
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
        likelihood_hw);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

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
    static double objxy[MAX_COUNT_ONES * 2];
    static int I[TB_MAX_SIZE];

    int countOnes = build_objxy_radius5(objxy);
    if (countOnes > MAX_COUNT_ONES) {
        std::cerr << "ERROR: countOnes = " << countOnes
                  << " exceeds MAX_COUNT_ONES = " << MAX_COUNT_ONES << "\n";
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
