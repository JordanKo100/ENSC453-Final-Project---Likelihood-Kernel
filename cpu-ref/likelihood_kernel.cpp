#include <iostream>
#include <vector>
#include <cmath>
#include <omp.h>
#include "my_timer.h" // Ensure this file is in the same directory

// Helper to simulate the roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + 0.5);
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

int main() {
    // 1. Setup Data (Matched with hw-run_host.txt)
    const int Nparticles = 1000000;
    const int IszY = 4000;         
    const int Nfr = 1;
    const int k = 0;
    const long max_size = 16000000;
    const int threads = 6;

    printf("Thread count: %d\n", threads);

    // Allocate max possible size for a 9x9 grid, then resize down to actual countOnes
    // 1. Setup Data for a 9x9 Square Mask
    const int length = 45; 
    const int center = (length - 1) / 2; // (length - 1) / 2
    int countOnes = length * length; 
    
    // Allocate exact size, no resizing needed
    std::vector<double> objxy(countOnes * 2, 0.0);
    
    int current_point = 0;
    for (int x = 0; x < length; x++) {
        for (int y = 0; y < length; y++) {
            // No distance calculation or if-statement needed!
            // Just store every point in the 9x9 grid.
            objxy[current_point * 2] = (double)(y - center);
            objxy[current_point * 2 + 1] = (double)(x - center);
            current_point++;
        }
    }

    // Initialize particles using the 'interior' logic from the testbench
    std::vector<double> arrayX(Nparticles);
    std::vector<double> arrayY(Nparticles);
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = 80.0 + (double)(i % 64) * 1.125; // Updated modulo and multiplier
        arrayY[i] = 120.0 + (double)(i % 32) * 0.875; // Updated modulo and multiplier
    }
    
    // Initialize I array with the modulo pattern from the testbench
    std::vector<int> I(max_size); 
    for (long i = 0; i < max_size; i++) {
        I[i] = 100 + (int)(i % 129);
    }
    
    std::vector<double> likelihood(Nparticles);
    std::vector<int> ind_buffer((size_t)Nparticles * countOnes);
    printf("Starting Speedup Test (N=%d, Points=%d)...\n", Nparticles, countOnes);

    // // 2. Measure Serial Time
    // timespec start_serial = tic();
    // run_likelihood(Nparticles, countOnes, IszY, Nfr, k, max_size, 
    //                arrayX.data(), arrayY.data(), objxy.data(), I.data(), 
    //                likelihood.data(), false, ind_buffer.data(), threads);
    // timespec end_serial = tic();
    // timespec diff_serial = diff(start_serial, end_serial);
    // printTimeSpec(diff_serial, "SERIAL TIME  ");

    // 3. Measure Parallel Time
    timespec start_parallel = tic();
    run_likelihood(Nparticles, countOnes, IszY, Nfr, k, max_size, 
                   arrayX.data(), arrayY.data(), objxy.data(), I.data(), 
                   likelihood.data(), true, ind_buffer.data(), threads);
    timespec end_parallel = tic();
    timespec diff_parallel = diff(start_parallel, end_parallel);
    printTimeSpec(diff_parallel, "PARALLEL TIME");

    // // 4. Calculate Speedup
    // double s_sec = (double)diff_serial.tv_sec + (double)diff_serial.tv_nsec / 1000000000.0;
    // double p_sec = (double)diff_parallel.tv_sec + (double)diff_parallel.tv_nsec / 1000000000.0;
    
    // printf("------------------------------\n");
    // printf("Calculated Speedup: %.2fx\n", s_sec / p_sec);
    // printf("------------------------------\n");

    return 0;
}