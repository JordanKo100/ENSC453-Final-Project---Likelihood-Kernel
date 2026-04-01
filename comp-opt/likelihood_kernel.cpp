#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <cassert>

#include "likelihood_kernel.h"

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

void load_pixels(const int* packed_I,
                 int buffer_pixels[N_BUFFER_SIZE][PADDED_COUNT_ONES],
                 int base, int activeParticles) {
    #pragma HLS INLINE off
    const int base_idx = base * PADDED_COUNT_ONES;

    loadParticles: for (int p = 0; p < activeParticles; p++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=N_BUFFER_SIZE
        
        loadWords: for (int i = 0; i < PADDED_COUNT_ONES; i++) {
            #pragma HLS PIPELINE II=1 
            buffer_pixels[p][i] = packed_I[base_idx + p * PADDED_COUNT_ONES + i];
        }
    }
}

void compute_likelihood(const int buffer_pixels[N_BUFFER_SIZE][PADDED_COUNT_ONES],
                        double buffer_likelihood[N_BUFFER_SIZE],
                        int activeParticles) {
    #pragma HLS INLINE off
    const double inv_count = 1.0 / (double)ACTUAL_COUNT_ONES;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

    int particle_sums[N_BUFFER_SIZE];
    #pragma HLS ARRAY_PARTITION variable=particle_sums complete dim=1

    init_sums: for (int i = 0; i < N_BUFFER_SIZE; i++) {
        #pragma HLS UNROLL
        particle_sums[i] = 0;
    }

    accumulate_offsets: for (int j = 0; j < ACTUAL_COUNT_ONES; j++) {
        #pragma HLS PIPELINE II=1
        
        parallel_particles: for (int i = 0; i < N_BUFFER_SIZE; i++) {
            #pragma HLS UNROLL
            if (i < activeParticles) {
                particle_sums[i] += buffer_pixels[i][j];
            }
        }
    }

    finalize: for (int i = 0; i < N_BUFFER_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        if (i < activeParticles) {
            buffer_likelihood[i] = scale * (double)particle_sums[i] - bias;
        } else {
            buffer_likelihood[i] = 0.0;
        }
    }
}

void store_likelihood(double* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base, int activeParticles) {
    #pragma HLS INLINE off
    
    storeLikelihood: for (int i = 0; i < activeParticles; i++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=N_BUFFER_SIZE
        #pragma HLS PIPELINE II=1
        likelihood[base + i] = buffer_likelihood[i];
    }
}

extern "C" {
void likelihood_kernel(int Nparticles,
                       const int* packed_I, 
                       double* likelihood) {
#pragma HLS INTERFACE m_axi port=packed_I offset=slave bundle=gmem0 max_read_burst_length=16 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem1 max_write_burst_length=16 num_write_outstanding=4

#pragma HLS INTERFACE s_axilite port=Nparticles bundle=control
#pragma HLS INTERFACE s_axilite port=packed_I bundle=control
#pragma HLS INTERFACE s_axilite port=likelihood bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    assert(Nparticles > 0 && "Nparticles must be greater than 0");

    int buffer_pixels[N_BUFFER_SIZE][PADDED_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

    #pragma HLS ARRAY_PARTITION variable=buffer_pixels complete dim=1 


Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=(NUM_NPARTICLES + N_BUFFER_SIZE - 1)/N_BUFFER_SIZE
        int activeParticles = (Nparticles - base > N_BUFFER_SIZE) ? N_BUFFER_SIZE : (Nparticles - base);

        load_pixels(packed_I, buffer_pixels, base, activeParticles);
        compute_likelihood(buffer_pixels, buffer_likelihood, activeParticles);
        store_likelihood(likelihood, buffer_likelihood, base, activeParticles);    
    }
}
}