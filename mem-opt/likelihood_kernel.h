#ifndef LIKELIHOOD_KERNEL_H
#define LIKELIHOOD_KERNEL_H

#include <ap_int.h>

#define AXI_BITS 512
#define DOUBLE_BITS 64
#define INT_BITS 32
#define DOUBLES_PER_WORD (AXI_BITS / DOUBLE_BITS) // 8 doubles
#define INTS_PER_WORD (AXI_BITS / INT_BITS)       // 16 ints

const int MAX_NPARTICLES = 100000;
const int N_BUFFER_SIZE = 256;

#define PADDED_COUNT_ONES 80
#define ACTUAL_COUNT_ONES 69

#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)


static_assert(N_BUFFER_SIZE % WORDS_PER_TILE == 0, "N_BUFFER_SIZE must align with 512-bit ports!");

typedef ap_uint<AXI_BITS> wide_t;

#ifdef __cplusplus
extern "C" {
#endif

void likelihood_kernel(int Nparticles,
                       const wide_t* packed_I,
                       wide_t* likelihood);

#ifdef __cplusplus
}
#endif

#endif