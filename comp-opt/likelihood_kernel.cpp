#include <cmath>
#include <cstdlib>
#include <stdint.h>

#include "likelihood_kernel.h"

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

const int PIX_SUM_LANES = 10;
const int PIX_CHUNKS = (MAX_COUNT_ONES / PIX_SUM_LANES);

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

inline int roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline long absLong(long value) {
    return (value < 0) ? -value : value;
}

void load_objxy(const double* objxy,
                int buffer_objxy[MAX_COUNT_ONES * 2],
                int countOnes) {
    #pragma HLS INLINE

    loadObjxy: for (int i = 0; i < countOnes * 2; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=1 max=160
        buffer_objxy[i] = roundDouble(objxy[i]);
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

void load_particles(const double* arrayX,
                    const double* arrayY,
                    long buffer_idx_1D[N_BUFFER_SIZE],
                    int base,
                    int tileSize,
                    int IszY,
                    int Nfr,
                    int k) {
    #pragma HLS INLINE

    loadParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < tileSize) {
            const int px = roundDouble(arrayX[base + x]);
            const int py = roundDouble(arrayY[base + x]);
            
            buffer_idx_1D[x] =
                (long)px * (long)IszY * (long)Nfr + (long)py * (long)Nfr + (long)k;
        }
    }
}

void load_pixels(const long buffer_idx_1D[N_BUFFER_SIZE],
                 const int buffer_objxy_offset[MAX_COUNT_ONES],
                 int countOnes,
                 long max_size,
                 const int* I,
                 int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                 int tileSize) {
    #pragma HLS INLINE

    int x = 0;
    int y = 0;

    loadPixels: for (int iter = 0; iter < tileSize * countOnes; iter++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=1 max=5120

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
                        int tileSize) {
    
    #pragma HLS INLINE
    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

    computeParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < tileSize){
            int partial_sum[PIX_SUM_LANES];
            #pragma HLS ARRAY_PARTITION variable=partial_sum complete dim=1

            initPartialSums: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
                #pragma HLS UNROLL
                partial_sum[lane] = 0;
            }

            accumPixels: for (int chunk = 0; chunk < PIX_CHUNKS; chunk++) {
                // #pragma HLS PIPELINE II=1
                const int chunk_idx = chunk * PIX_SUM_LANES;
                accumulateLanes: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
                    #pragma HLS UNROLL
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

void store_likelihood(double* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int tileSize) {
    #pragma HLS INLINE

    storeLikelihood: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < tileSize) {
            likelihood[base + x] = buffer_likelihood[x];
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
                       const double* arrayX,
                       const double* arrayY,
                       const double* objxy,
                       const int* I,
                       double* likelihood) {
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3 max_widen_bitwidth=32
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem4 max_widen_bitwidth=64

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
    int buffer_objxy_offset[MAX_COUNT_ONES];
    long buffer_idx_1D[N_BUFFER_SIZE];
    int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

// partition for likelihood computation
#pragma HLS ARRAY_PARTITION variable=buffer_pixels complete dim=2

    load_objxy(objxy, buffer_objxy, countOnes);
    build_obj_offsets(buffer_objxy, buffer_objxy_offset, countOnes, IszY, Nfr);

Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
        int tileSize = Nparticles - base;
        if (tileSize > N_BUFFER_SIZE) {
            tileSize = N_BUFFER_SIZE;
        }

        load_particles(arrayX, arrayY, buffer_idx_1D, base, tileSize, IszY, Nfr, k);
        load_pixels(buffer_idx_1D, buffer_objxy_offset, countOnes, max_size, I, buffer_pixels, tileSize);
        compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, tileSize);
        store_likelihood(likelihood, buffer_likelihood, base, tileSize);
    }
}
}