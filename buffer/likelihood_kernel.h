#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

const int MAX_NPARTICLES = 3000000;
const int N_BUFFER_SIZE = 128;

#define PADDED_COUNT_ONES 80
#define ACTUAL_COUNT_ONES 69

#ifdef __cplusplus
extern "C" {
#endif

void likelihood_kernel(int Nparticles,
                       const int* packed_I,
                       double* likelihood);

#ifdef __cplusplus
}
#endif

#endif