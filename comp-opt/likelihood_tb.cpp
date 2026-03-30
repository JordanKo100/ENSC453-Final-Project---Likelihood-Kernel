#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "likelihood_kernel.h"

// 1. Updated Data Constants
#define TB_MAX_NPARTICLES 1000000
#define TB_ISZY 4000
#define TB_NFR 1
#define TB_K 0
#define TB_MAX_SIZE 16000000

#define MASK_LENGTH 45
#define TB_MAX_COUNT_ONES (MASK_LENGTH * MASK_LENGTH) // 2025

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

// 2. Updated to use the 45x45 Square Mask Logic
int build_objxy_square(std::vector<double>& objxy) {
    const int length = MASK_LENGTH;
    const int center = (length - 1) / 2;
    int countOnes = length * length; 
    
    int current_point = 0;
    for (int x = 0; x < length; x++) {
        for (int y = 0; y < length; y++) {
            objxy[current_point * 2] = (double)(y - center);
            objxy[current_point * 2 + 1] = (double)(x - center);
            current_point++;
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

// 3. Updated Particle Initialization Math
void init_particles_interior(double* arrayX, double* arrayY, int Nparticles) {
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = 80.0 + (double)(i % 64) * 1.125;
        arrayY[i] = 120.0 + (double)(i % 32) * 0.875;
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
              const int* I) 
{
    // 4. Changed to std::vector to safely handle the 1,000,000 particle size
    std::vector<double> arrayX(Nparticles, 0.0);
    std::vector<double> arrayY(Nparticles, 0.0);
    std::vector<double> likelihood_hw(Nparticles, 0.0);
    std::vector<double> likelihood_ref(Nparticles, 0.0);

    if (boundary_case) {
        init_particles_boundary(arrayX.data(), arrayY.data(), Nparticles);
    } else {
        init_particles_interior(arrayX.data(), arrayY.data(), Nparticles);
    }

    compute_reference(
        Nparticles,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        max_size,
        arrayX.data(),
        arrayY.data(),
        objxy,
        I,
        likelihood_ref.data());

    auto start = std::chrono::high_resolution_clock::now();

    likelihood_kernel(
        Nparticles,
        countOnes,
        TB_ISZY,
        TB_NFR,
        TB_K,
        max_size,
        arrayX.data(),
        arrayY.data(),
        objxy,
        I,
        likelihood_hw.data());

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
    // 5. Changed to std::vector to safely handle the 16,000,000 max size
    std::vector<double> objxy(TB_MAX_COUNT_ONES * 2, 0.0);
    std::vector<int> I(TB_MAX_SIZE, 0);

    int countOnes = build_objxy_square(objxy);

    for (long i = 0; i < TB_MAX_SIZE; i++) {
        I[i] = 100 + (int)(i % 129);
    }

    bool pass = true;
    
    // Testing the massive 1,000,000 interior case
    pass &= run_case("massive_interior_case", TB_MAX_NPARTICLES, false, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    
    // Testing smaller corner cases to verify boundary logic
    pass &= run_case("small_boundary_case", 65, true, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case("clamp_case", 65, true, 37, objxy.data(), countOnes, I.data());

    if (pass) {
        std::cout << "TEST PASSED\n";
        return 0;
    }

    std::cout << "TEST FAILED\n";
    return 1;
}