#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

// Statically fixed to 69 for the radius 5 disk
#define MAX_COUNT_ONES 69
#define N_BUFFER_SIZE 64

// 69 must be perfectly divisible by PIX_SUM_LANES for optimal hardware
// 23 is a prime factor of 69 (69 / 23 = 3 chunks)
const int PIX_SUM_LANES = 23;
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

extern "C" {
void likelihood_kernel(int Nparticles,
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