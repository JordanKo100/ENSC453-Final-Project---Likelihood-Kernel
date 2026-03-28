#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

// countOnes = number of points in the object mask/disk
#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

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
