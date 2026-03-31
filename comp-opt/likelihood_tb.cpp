#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "likelihood_kernel.h"

#define TB_MAX_NPARTICLES 1000000
#define TB_ISZY 4000
#define TB_ISZX 4000
#define TB_NFR 1
#define TB_K 0
#define TB_MAX_SIZE 16000000

// Constant for PRNG
const long M = 2147483647; // INT_MAX
const int A = 1103515245;
const int C = 12345;

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

// 1. Re-implemented PRNG for testbench verification
double randu(std::vector<int>& seed, int index) {
    long long num = (long long)A * seed[index] + C;
    seed[index] = num % M;
    return std::fabs(seed[index] / ((double)M));
}

double randn(std::vector<int>& seed, int index) {
    double u = randu(seed, index);
    double v = randu(seed, index);
    double cosine = std::cos(2.0 * M_PI * v);
    double rt = -2.0 * std::log(u);
    return std::sqrt(rt) * cosine;
}

// 2. Updated to generate exactly 69 points in a radius 5 disk
void build_objxy_disk(std::vector<double>& objxy) {
    int radius = 5;
    int current_point = 0;
    for (int x = -radius + 1; x < radius; x++) {
        for (int y = -radius + 1; y < radius; y++) {
            double distance = std::sqrt(x * x + y * y);
            if (distance < radius) {
                // Matches (y - center) and (x - center) logic
                objxy[current_point * 2] = (double)y;
                objxy[current_point * 2 + 1] = (double)x;
                current_point++;
            }
        }
    }
}

// 3. Removed countOnes parameter, relies on MAX_COUNT_ONES macro
void compute_reference(int Nparticles,
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

        for (int y = 0; y < MAX_COUNT_ONES; y++) {
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

        likelihood_ref[x] = sum / (double)MAX_COUNT_ONES;
    }
}

// 4. Initialize dynamically sized particles using randn
void init_particles(std::vector<double>& arrayX, std::vector<double>& arrayY, int Nparticles) {
    std::vector<int> seed(Nparticles);
    for (int i = 0; i < Nparticles; i++) {
        seed[i] = 1337 * (i + 1); // Deterministic seed for TB reproducibility
    }

    double center_x = TB_ISZY / 2.0;
    double center_y = TB_ISZX / 2.0;

    for (int i = 0; i < Nparticles; i++) {
        // Applying the random walk drift/noise from the OpenMP code
        arrayX[i] = center_x + 1.0 + 5.0 * randn(seed, i);
        arrayY[i] = center_y - 2.0 + 2.0 * randn(seed, i);
    }
}

bool run_case(const char* label,
              int Nparticles,
              long max_size,
              const double* objxy,
              const int* I) 
{
    std::vector<double> arrayX(Nparticles, 0.0);
    std::vector<double> arrayY(Nparticles, 0.0);
    std::vector<double> likelihood_hw(Nparticles, 0.0);
    std::vector<double> likelihood_ref(Nparticles, 0.0);

    init_particles(arrayX, arrayY, Nparticles);

    compute_reference(
        Nparticles,
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
    std::vector<double> objxy(MAX_COUNT_ONES * 2, 0.0);
    std::vector<int> I(TB_MAX_SIZE, 0);

    build_objxy_disk(objxy);

    for (long i = 0; i < TB_MAX_SIZE; i++) {
        I[i] = 100 + (int)(i % 129);
    }

    bool pass = true;
    
    // Testing specific particle bounds with the new random distribution
    pass &= run_case("massive_case", TB_MAX_NPARTICLES, TB_MAX_SIZE, objxy.data(), I.data());
    pass &= run_case("medium_case", 50000, TB_MAX_SIZE, objxy.data(), I.data());
    pass &= run_case("small_case", 65, TB_MAX_SIZE, objxy.data(), I.data());
    pass &= run_case("clamp_case", 65, 37, objxy.data(), I.data());

    if (pass) {
        std::cout << "TEST PASSED\n";
        return 0;
    }

    std::cout << "TEST FAILED\n";
    return 1;
}