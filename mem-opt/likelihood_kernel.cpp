#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <cassert>
#include <ap_int.h>

#include "likelihood_kernel.h"

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

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

inline int roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline long absLong(long value) {
    return (value < 0) ? -value : value;
}

inline double bits_to_double(uint64_t bits) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = bits;
    return conv.d;
}

inline uint64_t double_to_bits(double val) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.d = val;
    return conv.u;
}

void load_objxy(const wide_t* objxy,
                int buffer_objxy[MAX_COUNT_ONES * 2],
                int countOnes) {
    #pragma HLS INLINE

    const int total_doubles = countOnes * 2;
    const int valid_words = (total_doubles + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    loadObjxyWide: for (int w = 0; w < OBJ_PER_TILE; w++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=17 max=20
        wide_t pack = 0;

        if (w < valid_words) {
            pack = objxy[w];
        }

        unpackWordObj: for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            #pragma HLS UNROLL
            const int idx = w * DOUBLES_PER_WORD + d;
            if (idx < total_doubles) {
                const uint64_t bits =
                    (uint64_t)pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                const double val = bits_to_double(bits);
                buffer_objxy[idx] = roundDouble(val);
            }
        }
    }
}

void build_obj_offsets(const int buffer_objxy[MAX_COUNT_ONES * 2],
                       int buffer_objxy_offset[MAX_COUNT_ONES],
                       int countOnes,
                       int IszY,
                       int Nfr) {
    #pragma HLS INLINE

    buildObjOffsets: for (int i = 0; i < countOnes; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=1 max=80
        const int offY = buffer_objxy[i * 2];
        const int offX = buffer_objxy[i * 2 + 1];
        buffer_objxy_offset[i] = offX * IszY * Nfr + offY * Nfr;
    }
}

void load_particles(const wide_t* arrayX,
                    const wide_t* arrayY,
                    long buffer_idx_1D[N_BUFFER_SIZE],
                    int base,
                    int activeParticles,
                    int IszY,
                    int Nfr,
                    int k) {
    #pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;
    const int valid_words = (activeParticles + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    loadParticlesWide: for (int w = 0; w < WORDS_PER_TILE; w++) {
        #pragma HLS PIPELINE II=1
        wide_t xpack = 0;
        wide_t ypack = 0;

        if (w < valid_words) {
            xpack = arrayX[base_word + w];
            ypack = arrayY[base_word + w];
        }

        unpackWord: for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            #pragma HLS UNROLL
            const int idx = w * DOUBLES_PER_WORD + d;
            if (idx < activeParticles) {
                const uint64_t xbits =
                    (uint64_t)xpack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                const uint64_t ybits =
                    (uint64_t)ypack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                const double xval = bits_to_double(xbits);
                const double yval = bits_to_double(ybits);
                const int px = roundDouble(xval);
                const int py = roundDouble(yval);
                
                buffer_idx_1D[idx] =
                    (long)px * (long)IszY * (long)Nfr + (long)py * (long)Nfr + (long)k;
            } else {
                buffer_idx_1D[idx] = 0;
            }
        }
    }
}

void load_pixels(const long buffer_idx_1D[N_BUFFER_SIZE],
                 const int buffer_objxy_offset[MAX_COUNT_ONES],
                 int countOnes,
                 long max_size,
                 const int* I,
                 int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                 int activeParticles) {
    #pragma HLS INLINE

    int x = 0;
    int y = 0;

    loadPixels: for (int iter = 0; iter < activeParticles * countOnes; iter++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=1 max=5120

        assert(x >= 0 && x < N_BUFFER_SIZE && "load_pixels: x index out of bounds!");
        assert(y >= 0 && y < MAX_COUNT_ONES && "load_pixels: y index out of bounds!");

        long idx = absLong(buffer_idx_1D[x] + (long)buffer_objxy_offset[y]);
        if (idx >= max_size) {
            idx = 0;
        }

        buffer_pixels[x][y] = I[idx];
        y++;
        if (y == countOnes) {
            y = 0;
            x++;
        }
    }
}

void compute_likelihood(const int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                        int countOnes,
                        double buffer_likelihood[N_BUFFER_SIZE],
                        int activeParticles) {
    
    #pragma HLS INLINE
    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

    computeParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < activeParticles){
            int partial_sum[PIX_SUM_LANES];
            #pragma HLS ARRAY_PARTITION variable=partial_sum complete dim=1

            initPartialSums: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
                #pragma HLS UNROLL
                partial_sum[lane] = 0;
            }

            accumPixels: for (int chunk = 0; chunk < PIX_CHUNKS; chunk++) {
                #pragma HLS UNROLL
                const int chunk_idx = chunk * PIX_SUM_LANES;
                accumulateLanes: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
                    const int idx = chunk_idx + lane;
                    if (idx < countOnes) {
                        partial_sum[lane] += buffer_pixels[x][idx];
                    }
                }
            }

            int pixel_sum = 0;
            sumPartialSums: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
                #pragma HLS UNROLL
                pixel_sum += partial_sum[lane];
            }

            buffer_likelihood[x] = scale * (double)pixel_sum - bias;
        }
    }
}

void store_likelihood(wide_t* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int activeParticles) {
    #pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;
    const int full_words = activeParticles / DOUBLES_PER_WORD;
    const int remainder = activeParticles % DOUBLES_PER_WORD;

    storeLikelihoodWide: for (int w = 0; w < WORDS_PER_TILE; w++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=8 max=8

        const bool write_word =
            (w < full_words) || ((w == full_words) && (remainder != 0));

        if (write_word) {
            wide_t out_pack = 0;
            packWord: for (int d = 0; d < DOUBLES_PER_WORD; d++) {
                #pragma HLS UNROLL
                uint64_t bits = 0;
                const int idx = w * DOUBLES_PER_WORD + d;
                if (idx < activeParticles) {
                    bits = double_to_bits(buffer_likelihood[idx]);
                }
                out_pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = bits;
            }

            likelihood[base_word + w] = out_pack;
        }
    }
}

extern "C" {
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
                       wide_t* likelihood) {
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0 max_widen_bitwidth=512
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1 max_widen_bitwidth=512
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2 max_widen_bitwidth=512
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3 max_widen_bitwidth=32
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem4 max_widen_bitwidth=512

#pragma HLS INTERFACE s_axilite port=Nparticles bundle=control
#pragma HLS INTERFACE s_axilite port=countOnes bundle=control
#pragma HLS INTERFACE s_axilite port=IszY bundle=control
#pragma HLS INTERFACE s_axilite port=Nfr bundle=control
#pragma HLS INTERFACE s_axilite port=k bundle=control
#pragma HLS INTERFACE s_axilite port=max_size bundle=control
#pragma HLS INTERFACE s_axilite port=arrayX bundle=control
#pragma HLS INTERFACE s_axilite port=arrayY bundle=control
#pragma HLS INTERFACE s_axilite port=objxy bundle=control
#pragma HLS INTERFACE s_axilite port=I bundle=control
#pragma HLS INTERFACE s_axilite port=likelihood bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    assert(countOnes > 0 && "countOnes must be greater than 0");
    assert(countOnes <= MAX_COUNT_ONES && "countOnes exceeds statically allocated MAX_COUNT_ONES!");
    assert(Nparticles > 0 && "Nparticles must be greater than 0");

    int buffer_objxy[MAX_COUNT_ONES * 2];
    int buffer_objxy_offset[MAX_COUNT_ONES];
    long buffer_idx_1D[N_BUFFER_SIZE];
    int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

// partition to accomodate AXI port widening
#pragma HLS ARRAY_PARTITION variable=buffer_objxy cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_idx_1D cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1

// partition for likelihood computation
#pragma HLS ARRAY_PARTITION variable=buffer_pixels complete dim=2

    load_objxy(objxy, buffer_objxy, countOnes);
    build_obj_offsets(buffer_objxy, buffer_objxy_offset, countOnes, IszY, Nfr);

Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32 // max is calculated as the maximum Nparticles / N_BUFFER_SIZE (i.e. max Nparticles = 64 * 32 = 2048 particles)
#pragma HLS LOOP_FLATTEN off

        int activeParticles = Nparticles - base;
        if (activeParticles > N_BUFFER_SIZE) {
            activeParticles = N_BUFFER_SIZE;
        }

        load_particles(arrayX, arrayY, buffer_idx_1D, base, activeParticles, IszY, Nfr, k);
        load_pixels(buffer_idx_1D, buffer_objxy_offset, countOnes, max_size, I, buffer_pixels, activeParticles);
        compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, activeParticles);
        store_likelihood(likelihood, buffer_likelihood, base, activeParticles);
    }
}
}