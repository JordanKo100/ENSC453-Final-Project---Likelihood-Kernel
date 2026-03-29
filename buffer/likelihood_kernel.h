#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

// countOnes = number of points in the object mask/disk
// Updated to 2025 to support up to a 45x45 square mask
#define MAX_COUNT_ONES 2025
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