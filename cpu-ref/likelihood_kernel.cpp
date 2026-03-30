#include <iostream>
#include <vector>
#include <cmath>
#include <omp.h>
#include "my_timer.h" // Ensure this file is in the same directory

// Helper to simulate the roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

/**
 * The likelihood calculation core
 */
void run_likelihood(int Nparticles, int countOnes, int IszY, int Nfr, int k, 
                    long max_size, const double* arrayX, const double* arrayY, 
                    const double* objxy, const int* I, 
                    double* likelihood, bool use_parallel, int* ind_buffer, int threads) {
    
    if (use_parallel) {
        #pragma omp parallel for shared(likelihood, I, arrayX, arrayY, objxy, ind_buffer) num_threads(threads)
        for (int x = 0; x < Nparticles; x++) {
            for (int y = 0; y < countOnes; y++) {
                // Apply roundDouble to objxy since it is now a double, keeping indices as integers
                int indX = roundDouble(arrayX[x]) + roundDouble(objxy[y * 2 + 1]);
                int indY = roundDouble(arrayY[x]) + roundDouble(objxy[y * 2]);
                
                ind_buffer[x * countOnes + y] = std::abs(indX * IszY * Nfr + indY * Nfr + k);
                
                if (ind_buffer[x * countOnes + y] >= max_size)
                    ind_buffer[x * countOnes + y] = 0;
            }
            
            likelihood[x] = 0;
            for (int y = 0; y < countOnes; y++) {
                int idx = ind_buffer[x * countOnes + y];
                // Likelihood ratio calculation (I is now an int array)
                likelihood[x] += (std::pow((I[idx] - 100), 2) - std::pow((I[idx] - 228), 2)) / 50.0;
            }
            likelihood[x] = likelihood[x] / (double)countOnes;
        }
    } else {
        for (int x = 0; x < Nparticles; x++) {
            for (int y = 0; y < countOnes; y++) {
                int indX = roundDouble(arrayX[x]) + roundDouble(objxy[y * 2 + 1]);
                int indY = roundDouble(arrayY[x]) + roundDouble(objxy[y * 2]);
                
                ind_buffer[x * countOnes + y] = std::abs(indX * IszY * Nfr + indY * Nfr + k);
                
                if (ind_buffer[x * countOnes + y] >= max_size)
                    ind_buffer[x * countOnes + y] = 0;
            }
            
            likelihood[x] = 0;
            for (int y = 0; y < countOnes; y++) {
                int idx = ind_buffer[x * countOnes + y];
                likelihood[x] += (std::pow((I[idx] - 100), 2) - std::pow((I[idx] - 228), 2)) / 50.0;
            }
            likelihood[x] = likelihood[x] / (double)countOnes;
        }
    }
}

// --- PRNG Constants and Functions (Matches FPGA Testbench) ---
const long M = 2147483647; // INT_MAX
const int A = 1103515245;
const int C = 12345;

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
// -------------------------------------------------------------

int main() {
    // 1. Setup Data (Matched with HW testbench parameters)
    const int Nparticles = 1000000;
    const int IszX = 4000; 
    const int IszY = 4000;         
    const int Nfr = 1;
    const int k = 0;
    const long max_size = 16000000;
    const int threads = 6;

    printf("Thread count: %d\n", threads);

    // 2. Setup Data for a Circular Mask (Radius 5)
    const int radius = 5; 
    const int countOnes = 69; // Hardcoded to match the FPGA kernel's MAX_COUNT_ONES
    
    std::vector<double> objxy(countOnes * 2, 0.0);
    
    int current_point = 0;
    for (int x = -radius + 1; x < radius; x++) {
        for (int y = -radius + 1; y < radius; y++) {
            double distance = std::sqrt(x * x + y * y);
            if (distance < radius) {
                objxy[current_point * 2] = (double)y;
                objxy[current_point * 2 + 1] = (double)x;
                current_point++;
            }
        }
    }

    // 3. Initialize particles using identical random walk logic
    std::vector<double> arrayX(Nparticles, 0.0);
    std::vector<double> arrayY(Nparticles, 0.0);
    std::vector<int> seed(Nparticles);
    
    for (int i = 0; i < Nparticles; i++) {
        seed[i] = 1337 * (i + 1); // Deterministic seed for fair HW comparison
    }

    double center_x = IszY / 2.0;
    double center_y = IszX / 2.0;

    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = center_x + 1.0 + 5.0 * randn(seed, i);
        arrayY[i] = center_y - 2.0 + 2.0 * randn(seed, i);
    }
    
    // 4. Initialize Image array with the modulo pattern
    std::vector<int> I(max_size); 
    for (long i = 0; i < max_size; i++) {
        I[i] = 100 + (int)(i % 129);
    }
    
    std::vector<double> likelihood(Nparticles, 0.0);
    std::vector<int> ind_buffer((size_t)Nparticles * countOnes, 0);
    printf("Starting Speedup Test (N=%d, Points=%d)...\n", Nparticles, countOnes);

    // // Measure Serial Time (Uncomment if needed)
    // timespec start_serial = tic();
    // run_likelihood(Nparticles, countOnes, IszY, Nfr, k, max_size, 
    //                arrayX.data(), arrayY.data(), objxy.data(), I.data(), 
    //                likelihood.data(), false, ind_buffer.data(), threads);
    // timespec end_serial = tic();
    // timespec diff_serial = diff(start_serial, end_serial);
    // printTimeSpec(diff_serial, "SERIAL TIME  ");

    // Measure Parallel Time
    timespec start_parallel = tic();
    run_likelihood(Nparticles, countOnes, IszY, Nfr, k, max_size, 
                   arrayX.data(), arrayY.data(), objxy.data(), I.data(), 
                   likelihood.data(), true, ind_buffer.data(), threads);
    timespec end_parallel = tic();
    timespec diff_parallel = diff(start_parallel, end_parallel);
    printTimeSpec(diff_parallel, "PARALLEL TIME");

    double likelihood_sum = 0.0;
    for (double val : likelihood) {
        likelihood_sum += val;
    }
    printf("Sum of likelihood: %.f\n", likelihood_sum);

    // // Calculate Speedup (Uncomment if needed)
    // double s_sec = (double)diff_serial.tv_sec + (double)diff_serial.tv_nsec / 1000000000.0;
    // double p_sec = (double)diff_parallel.tv_sec + (double)diff_parallel.tv_nsec / 1000000000.0;
    // 
    // printf("------------------------------\n");
    // printf("Calculated Speedup: %.2fx\n", s_sec / p_sec);
    // printf("------------------------------\n");

    return 0;
}