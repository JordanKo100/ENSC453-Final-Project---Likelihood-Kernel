#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

// Updated to 2025 to support up to a 45x45 square mask
#define MAX_COUNT_ONES 2025
#define N_BUFFER_SIZE 64

// 2025 must be perfectly divisible by PIX_SUM_LANES. 
// Factors of 2025 include 15, 25, 45. We use 45 for wide accumulation.
const int PIX_SUM_LANES = 45;
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

static_assert(MAX_COUNT_ONES % PIX_SUM_LANES == 0, "MAX_COUNT_ONES must be perfectly divisible by PIX_SUM_LANES!");

extern "C" {
void likelihood_kernel(int Nparticles,
                       int countOnes,
                       int IszY,
                       int Nfr,
                       int k,
                       long max_size,
                       const double* arrayX,
                       const double* arrayY,
                       const double* objxy,
                       const int* I,
                       double* likelihood);
}

#endif