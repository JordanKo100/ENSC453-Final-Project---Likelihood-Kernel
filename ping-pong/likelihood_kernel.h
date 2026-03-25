#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#include <ap_int.h>

#define AXI_BITS 512
#define DOUBLE_BITS 64
#define DOUBLES_PER_WORD (AXI_BITS / DOUBLE_BITS)

typedef ap_uint<AXI_BITS> wide_t;

#ifdef __cplusplus
extern "C" {
#endif

void likelihood_kernel(int Nparticles,
                       int countOnes,
                       int IszY,
                       int Nfr,
                       int k,
                       long max_size,
                       const wide_t* arrayX,
                       const wide_t* arrayY,
                       const double* objxy,
                       const int* I,
                       wide_t* likelihood);

#ifdef __cplusplus
}
#endif

#endif
