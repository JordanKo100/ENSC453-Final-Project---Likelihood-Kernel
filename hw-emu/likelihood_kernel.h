#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#include <ap_int.h>

// --- TINY SCALE FOR HW_EMU ---
#define GLOBAL_NUM_PARTICLES 128
#define GLOBAL_ISZY 100
#define GLOBAL_MAX_SIZE 10000
#define GLOBAL_MASK_LENGTH 5
// 5x5 = 25. Divisible by 25.
#define PIX_SUM_LANES 25

#define MAX_COUNT_ONES (GLOBAL_MASK_LENGTH * GLOBAL_MASK_LENGTH)

#define AXI_BITS 512
#define DOUBLE_BITS 64
#define DOUBLES_PER_WORD (AXI_BITS / DOUBLE_BITS)

#define N_BUFFER_SIZE 64
#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)
#define OBJ_PER_TILE ((MAX_COUNT_ONES * 2) / DOUBLES_PER_WORD) 
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

static_assert(N_BUFFER_SIZE % WORDS_PER_TILE == 0, "N_BUFFER_SIZE must align with 512-bit ports!");
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
                       const double* objxy,
                       const int* I,
                       wide_t* likelihood);

#ifdef __cplusplus
}
#endif

#endif