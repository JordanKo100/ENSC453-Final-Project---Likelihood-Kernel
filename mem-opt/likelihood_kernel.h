#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#include <ap_int.h>

#define AXI_BITS 512
#define DOUBLE_BITS 64
#define DOUBLES_PER_WORD (AXI_BITS / DOUBLE_BITS)

#define MAX_COUNT_ONES 80 // MASK SIZE
#define N_BUFFER_SIZE 64

// Pre-calculate the number of wide words needed for loading particles and storing likelihood  
#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)

// Pre-calculate the maximum number of wide words needed for objxy
#define OBJ_PER_TILE ((MAX_COUNT_ONES * 2) / DOUBLES_PER_WORD) 

const int PIX_SUM_LANES = 10;
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

static_assert(N_BUFFER_SIZE % WORDS_PER_TILE == 0, "N_BUFFER_SIZE must be strictly divisible by 8 to align with 512-bit AXI ports!");
static_assert(MAX_COUNT_ONES % PIX_SUM_LANES == 0, "MAX_COUNT_ONES must be perfectly divisible by PIX_SUM_LANES!");

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
                       const wide_t* objxy,
                       const int* I,
                       wide_t* likelihood);

#ifdef __cplusplus
}
#endif

#endif
