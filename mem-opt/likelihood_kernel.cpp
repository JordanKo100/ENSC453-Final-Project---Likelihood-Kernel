#include <cmath>
#include <cstdlib>
#include <stdint.h>

#include <ap_int.h>

#include "likelihood_kernel.h"

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64
#define POINT_PAR 4
#define POINT_GROUPS (MAX_COUNT_ONES / POINT_PAR)
#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)

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

void load_objxy(const double* objxy,
                int buffer_objxy[MAX_COUNT_ONES * 2],
                int countOnes) {
#pragma HLS INLINE

loadObjxy:
    for (int i = 0; i < countOnes * 2; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=160
        buffer_objxy[i] = roundDouble(objxy[i]);
    }
}

void build_obj_offsets(const int buffer_objxy[MAX_COUNT_ONES * 2],
                       int buffer_obj_offsets[MAX_COUNT_ONES],
                       int countOnes,
                       int IszY,
                       int Nfr) {
#pragma HLS INLINE

buildObjOffsets:
    for (int i = 0; i < countOnes; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=80
        const int offY = buffer_objxy[i * 2];
        const int offX = buffer_objxy[i * 2 + 1];
        buffer_obj_offsets[i] = offX * IszY * Nfr + offY * Nfr;
    }
}

void load_particles(const wide_t* arrayX,
                    const wide_t* arrayY,
                    long particle_base_buffer[N_BUFFER_SIZE],
                    int base,
                    int tileSize,
                    int IszY,
                    int Nfr,
                    int k) {
#pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;
    const int valid_words = (tileSize + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

loadParticlesWide:
    for (int w = 0; w < WORDS_PER_TILE; w++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=8 max=8
        wide_t xpack = 0;
        wide_t ypack = 0;

        if (w < valid_words) {
            xpack = arrayX[base_word + w];
            ypack = arrayY[base_word + w];
        }

    unpackWord:
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
#pragma HLS UNROLL
            const int idx = w * DOUBLES_PER_WORD + d;

            if (idx < tileSize) {
                const uint64_t xbits =
                    (uint64_t)xpack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                const uint64_t ybits =
                    (uint64_t)ypack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                const double xval = bits_to_double(xbits);
                const double yval = bits_to_double(ybits);
                const int px = roundDouble(xval);
                const int py = roundDouble(yval);

                particle_base_buffer[idx] =
                    (long)px * (long)IszY * (long)Nfr + (long)py * (long)Nfr + (long)k;
            } else {
                particle_base_buffer[idx] = 0;
            }
        }
    }
}

void load_pixels(const long particle_base_buffer[N_BUFFER_SIZE],
                 const int buffer_obj_offsets[MAX_COUNT_ONES],
                 int countOnes,
                 long max_size,
                 const int* I,
                 int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                 int tileSize) {
#pragma HLS INLINE

    int x = 0;
    int y = 0;

loadPixels:
    for (int iter = 0; iter < tileSize * countOnes; iter++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=5120
        long idx = absLong(particle_base_buffer[x] + (long)buffer_obj_offsets[y]);

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
                        int tileSize) {
#pragma HLS INLINE

    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

computeParticles:
    for (int x = 0; x < tileSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
#pragma HLS LOOP_FLATTEN off
        int partial_sum[POINT_PAR];
#pragma HLS ARRAY_PARTITION variable=partial_sum complete dim=1

    initPartialSums:
        for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
            partial_sum[lane] = 0;
        }

    accumuPoints:
        for (int group = 0; group < POINT_GROUPS; group++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=20 max=20
            const int base_idx = group * POINT_PAR;
        accumulateLanes:
            for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
                const int idx = base_idx + lane;
                if (idx < countOnes) {
                    partial_sum[lane] += buffer_pixels[x][idx];
                }
            }
        }

        int pixel_sum = 0;
    sumPartialSums:
        for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
            pixel_sum += partial_sum[lane];
        }

        buffer_likelihood[x] = scale * (double)pixel_sum - bias;
    }
}

void store_likelihood(wide_t* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int tileSize) {
#pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;
    const int full_words = tileSize / DOUBLES_PER_WORD;
    const int remainder = tileSize % DOUBLES_PER_WORD;

storeLikelihoodWide:
    for (int w = 0; w < WORDS_PER_TILE; w++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=8 max=8
        const bool write_word =
            (w < full_words) || ((w == full_words) && (remainder != 0));

        if (write_word) {
            wide_t out_pack = 0;

        packWord:
            for (int d = 0; d < DOUBLES_PER_WORD; d++) {
#pragma HLS UNROLL
                uint64_t bits = 0;
                const int idx = w * DOUBLES_PER_WORD + d;
                if (idx < tileSize) {
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
                       const double* objxy,
                       const int* I,
                       wide_t* likelihood) {
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0 max_widen_bitwidth=512
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1 max_widen_bitwidth=512
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3 max_widen_bitwidth=32 max_read_burst_length=1 num_read_outstanding=1
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

    int buffer_objxy[MAX_COUNT_ONES * 2];
    int buffer_obj_offsets[MAX_COUNT_ONES];
    long particle_base_buffer[N_BUFFER_SIZE];
    int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

#pragma HLS ARRAY_PARTITION variable=buffer_obj_offsets cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=particle_base_buffer cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_pixels cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1

    load_objxy(objxy, buffer_objxy, countOnes);
    build_obj_offsets(buffer_objxy, buffer_obj_offsets, countOnes, IszY, Nfr);

Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
        int tileSize = Nparticles - base;
        if (tileSize > N_BUFFER_SIZE) {
            tileSize = N_BUFFER_SIZE;
        }

        load_particles(arrayX, arrayY, particle_base_buffer, base, tileSize, IszY, Nfr, k);
        load_pixels(particle_base_buffer, buffer_obj_offsets, countOnes, max_size, I, buffer_pixels,
                    tileSize);
        compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, tileSize);
        store_likelihood(likelihood, buffer_likelihood, base, tileSize);
    }
}
}
