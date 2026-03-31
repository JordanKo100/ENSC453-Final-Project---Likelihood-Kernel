#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#include <ap_int.h>

#define AXI_BITS 512
#define DOUBLE_BITS 64
#define DOUBLES_PER_WORD (AXI_BITS / DOUBLE_BITS)

// Statically fixed to 69 for the Rodinia radius 5 disk
#define MAX_COUNT_ONES 69 
#define N_BUFFER_SIZE 64

#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)
#define OBJ_PER_TILE ((MAX_COUNT_ONES * 2) / DOUBLES_PER_WORD) 

// 69 must be perfectly divisible by PIX_SUM_LANES. We use 23 (69 / 23 = 3 chunks).
const int PIX_SUM_LANES = 23;
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

static_assert(N_BUFFER_SIZE % WORDS_PER_TILE == 0, "N_BUFFER_SIZE must align with 512-bit ports!");
static_assert(MAX_COUNT_ONES % PIX_SUM_LANES == 0, "MAX_COUNT_ONES must be perfectly divisible by PIX_SUM_LANES!");

typedef ap_uint<AXI_BITS> wide_t;

#ifdef __cplusplus
extern "C" {
#endif

void likelihood_kernel(int Nparticles,
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