#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

const int PIX_SUM_LANES = 10;
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
