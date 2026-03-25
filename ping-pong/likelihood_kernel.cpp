#include <cmath>
#include <cstdlib>
#include <stdint.h>

#include <ap_int.h>

#include "likelihood_kernel.h"

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64
#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

inline int roundDouble(double value) {
    return static_cast<int>(value + 0.5);
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
                int buffer_obj_offsets[MAX_COUNT_ONES],
                int countOnes,
                int IszY,
                int Nfr) {
#pragma HLS INLINE
loadObjxy:
    for (int i = 0; i < MAX_COUNT_ONES; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
        if (i < countOnes) {
            int offY = roundDouble(objxy[i * 2]);
            int offX = roundDouble(objxy[i * 2 + 1]);
            buffer_obj_offsets[i] = offX * IszY * Nfr + offY * Nfr;
        }
    }
}

void load_particles(const wide_t* arrayX,
                    const wide_t* arrayY,
                    double buffer_X[N_BUFFER_SIZE],
                    double buffer_Y[N_BUFFER_SIZE],
                    int base,
                    int tileSize) {
#pragma HLS INLINE off

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
                uint64_t xbits =
                    (uint64_t)xpack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                uint64_t ybits =
                    (uint64_t)ypack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                buffer_X[idx] = bits_to_double(xbits);
                buffer_Y[idx] = bits_to_double(ybits);
            } else {
                buffer_X[idx] = 0.0;
                buffer_Y[idx] = 0.0;
            }
        }
    }
}

void compute_likelihood(const double buffer_X[N_BUFFER_SIZE],
                        const double buffer_Y[N_BUFFER_SIZE],
                        const int buffer_obj_offsets[MAX_COUNT_ONES],
                        int countOnes,
                        int IszY,
                        int Nfr,
                        int k,
                        long max_size,
                        const int* I,
                        double buffer_likelihood[N_BUFFER_SIZE],
                        int tileSize) {
#pragma HLS INLINE off

    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

computeParticles:
    for (int x = 0; x < tileSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=64
        int px = roundDouble(buffer_X[x]);
        int py = roundDouble(buffer_Y[x]);
        const int particle_base = px * IszY * Nfr + py * Nfr + k;
        int pixel_sum = 0;

    accumulatePoints:
        for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
            int idx = std::abs(particle_base + buffer_obj_offsets[y]);
            if (idx >= max_size) {
                idx = 0;
            }
            pixel_sum += I[idx];
        }

        buffer_likelihood[x] = scale * (double)pixel_sum - bias;
    }
}

void store_likelihood(wide_t* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int tileSize) {
#pragma HLS INLINE off

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
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem4

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

    int buffer_obj_offsets[MAX_COUNT_ONES];
    double buffer_X[N_BUFFER_SIZE];
    double buffer_Y[N_BUFFER_SIZE];
    double buffer_likelihood[N_BUFFER_SIZE];

#pragma HLS BIND_STORAGE variable=buffer_obj_offsets type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_X type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_Y type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_likelihood type=ram_2p impl=bram

#pragma HLS ARRAY_PARTITION variable=buffer_obj_offsets complete dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_X cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_Y cyclic factor=8 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1

    load_objxy(objxy, buffer_obj_offsets, countOnes, IszY, Nfr);

Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
        int tileSize = N_BUFFER_SIZE;

        if (base + N_BUFFER_SIZE > Nparticles) {
            tileSize = Nparticles - base;
        }

        load_particles(arrayX, arrayY, buffer_X, buffer_Y, base, tileSize);
        compute_likelihood(buffer_X, buffer_Y, buffer_obj_offsets, countOnes, IszY, Nfr, k,
                           max_size, I, buffer_likelihood, tileSize);
        store_likelihood(likelihood, buffer_likelihood, base, tileSize);
    }
}
}
