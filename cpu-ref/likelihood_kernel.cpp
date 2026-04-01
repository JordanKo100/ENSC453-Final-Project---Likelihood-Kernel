#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <omp.h>
#include "my_timer.h" // Ensure this file is in the same directory

// Matches the FPGA host rounding exactly
inline int shared_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

// --- PHASE 1: PRECOMPUTE INDICES (Offsets) ---
void precompute_indices(int Nparticles, int countOnes, int IszY, int Nfr, int k, 
                        long max_size, const double* arrayX, const double* arrayY, 
                        const double* objxy, int* ind_buffer, int threads) {
    #pragma omp parallel for shared(arrayX, arrayY, objxy, ind_buffer) num_threads(threads)
    for (int x = 0; x < Nparticles; x++) {
        for (int y = 0; y < countOnes; y++) {
            int indX = shared_roundDouble(arrayX[x]) + shared_roundDouble(objxy[y * 2 + 1]);
            int indY = shared_roundDouble(arrayY[x]) + shared_roundDouble(objxy[y * 2]);
            
            long idx = std::abs(indX * IszY * Nfr + indY * Nfr + k);
            
            // If out of bounds, store -1 so the math phase knows to substitute a 0
            if (idx >= max_size) {
                ind_buffer[x * countOnes + y] = -1;
            } else {
                ind_buffer[x * countOnes + y] = static_cast<int>(idx);
            }
        }
    }
}

// --- PHASE 2: COMPUTE LIKELIHOOD (Math Only) ---
void compute_likelihood_math(int Nparticles, int countOnes, const int* I, 
                             double* likelihood, bool use_parallel, const int* ind_buffer, int threads) {
    
    // HW Match Constants
    const double kPixelScaleNum = 256.0;
    const double kPixelBiasNum = 41984.0;
    const double kPixelDen = 50.0;
    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

    if (use_parallel) {
        #pragma omp parallel for shared(likelihood, I, ind_buffer) num_threads(threads)
        for (int x = 0; x < Nparticles; x++) {
            int particle_sum = 0;
            for (int y = 0; y < countOnes; y++) {
                int idx = ind_buffer[x * countOnes + y];
                
                // Fetch pixel, or use 0 if out of bounds (matches FPGA pack logic)
                int pixel_val = (idx == -1) ? 0 : I[idx]; 
                particle_sum += pixel_val;
            }
            // Apply scale and bias at the very end to prevent floating-point drift
            likelihood[x] = scale * (double)particle_sum - bias;
        }
    } else {
        for (int x = 0; x < Nparticles; x++) {
            int particle_sum = 0;
            for (int y = 0; y < countOnes; y++) {
                int idx = ind_buffer[x * countOnes + y];
                int pixel_val = (idx == -1) ? 0 : I[idx];
                particle_sum += pixel_val;
            }
            likelihood[x] = scale * (double)particle_sum - bias;
        }
    }
}

// --- PRNG Constants and Functions ---
const long PRNG_M = 2147483647; 
const int PRNG_A = 1103515245;
const int PRNG_C = 12345;

double randu(std::vector<int>& seed, int index) {
    long long num = (long long)PRNG_A * seed[index] + PRNG_C;
    seed[index] = static_cast<int>(num % PRNG_M);
    return std::fabs(seed[index] / static_cast<double>(PRNG_M));
}

double randn(std::vector<int>& seed, int index) {
    double u = randu(seed, index);
    double v = randu(seed, index);
    double cosine = std::cos(2.0 * M_PI * v);
    double rt = -2.0 * std::log(u);
    return std::sqrt(rt) * cosine;
}
// -------------------------------------------------------------

int main() {
    // 1. Setup Data
    const int Nparticles = 3000000;
    const int IszX = 4000; 
    const int IszY = 4000;         
    const int Nfr = 1;
    const int k = 0;
    const long max_size = 16000000;
    const int threads = 6;

    printf("Thread count: %d\n", threads);

    // 2. Setup Data for a Circular Mask (Radius 5)
    const int radius = 5;
    const int countOnes = 69; 
    
    std::vector<double> objxy(countOnes * 2, 0.0);
    int current_point = 0;
    for (int x = -radius + 1; x < radius; x++) {
        for (int y = -radius + 1; y < radius; y++) {
            double distance = std::sqrt(x * x + y * y);
            if (distance < radius) {
                if (current_point < countOnes) { // Bound check to match host
                    objxy[current_point * 2] = static_cast<double>(y);
                    objxy[current_point * 2 + 1] = static_cast<double>(x);
                    current_point++;
                }
            }
        }
    }

    // 3. Initialize SCATTERED particles
    std::vector<double> arrayX(Nparticles, 0.0);
    std::vector<double> arrayY(Nparticles, 0.0);
    std::vector<int> seed(Nparticles);
    
    for (int i = 0; i < Nparticles; i++) {
        seed[i] = 1337 * (i + 1); // Deterministic seed
    }

    // Distribute uniformly across the 4000x4000 image
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = randu(seed, i) * static_cast<double>(IszY);
        arrayY[i] = randu(seed, i) * static_cast<double>(IszX);
    }
    
    // 4. Initialize Image array
    std::vector<int> I(max_size);
    for (long i = 0; i < max_size; i++) {
        I[i] = 100 + static_cast<int>(i % 129);
    }
    
    std::vector<double> likelihood(Nparticles, 0.0);
    std::vector<int> ind_buffer((size_t)Nparticles * countOnes, 0);

    printf("Starting Speedup Test (N=%d, Points=%d)...\n", Nparticles, countOnes);

    // PRECOMPUTE: Isolate the mask offset extraction from the timer
    printf("Precomputing mask indices...\n");
    precompute_indices(Nparticles, countOnes, IszY, Nfr, k, max_size, 
                       arrayX.data(), arrayY.data(), objxy.data(), ind_buffer.data(), threads);

    printf("Executing math computation...\n");
    timespec start_serial = tic();
    compute_likelihood_math(Nparticles, countOnes, I.data(), 
                            likelihood.data(), false, ind_buffer.data(), threads);
    timespec end_serial = tic();
    
    timespec diff_serial = diff(start_serial, end_serial);
    printTimeSpec(diff_serial, "SERIAL MATH TIME");

    // COMPUTE: Time ONLY the math execution
    printf("Executing math computation...\n");
    timespec start_parallel = tic();
    compute_likelihood_math(Nparticles, countOnes, I.data(), 
                            likelihood.data(), true, ind_buffer.data(), threads);
    timespec end_parallel = tic();
    
    timespec diff_parallel = diff(start_parallel, end_parallel);
    printTimeSpec(diff_parallel, "PARALLEL MATH TIME");

    double likelihood_sum = 0.0;
    for (double val : likelihood) {
        likelihood_sum += val;
    }
    
    std::cout << "Sum of likelihood: " << std::setprecision(15) << likelihood_sum << std::endl;

    return 0;
}