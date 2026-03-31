#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

// Statically fixed to 69 for a disk mask of radius 5
#define MAX_COUNT_ONES 69
#define N_BUFFER_SIZE 64

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