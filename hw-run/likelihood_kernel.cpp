#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <cassert>
#include <ap_int.h>

#include "likelihood_kernel.h"

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

inline uint64_t double_to_bits(double val) {
    union { uint64_t u; double d; } conv;
    conv.d = val;
    return conv.u;
}

void load_packed_pixels_wide(const wide_t* packed_I,
                             int buffer_pixels[N_BUFFER_SIZE][PADDED_COUNT_ONES],
                             int base, int activeParticles) {
    #pragma HLS INLINE off
    
    // Each padded particle is exactly 5 words (80 ints / 16 lanes)
    const int WORDS_PER_PARTICLE = PADDED_COUNT_ONES / INTS_PER_WORD; 
    const int base_word = (base * PADDED_COUNT_ONES) / INTS_PER_WORD;

    // Outer loop: Iterate through active rows
    loadParticles: for (int p = 0; p < activeParticles; p++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=N_BUFFER_SIZE
        
        // Inner loop: Iterate through the 5 words of that specific row
        loadWords: for (int w = 0; w < WORDS_PER_PARTICLE; w++) {
            #pragma HLS PIPELINE II=1 
             
            int global_word = base_word + (p * WORDS_PER_PARTICLE) + w;
            wide_t pack = packed_I[global_word];

            unpackWord: for (int i = 0; i < INTS_PER_WORD; i++) {
                #pragma HLS UNROLL 
                // Because 'p' is constant for this unrolled loop, no MUX is generated across rows!
                // Because of cyclic=16 on dim=2, 'w * 16 + i' maps perfectly to the 16 banks.
                buffer_pixels[p][w * INTS_PER_WORD + i] = (int)pack.range((i + 1) * INT_BITS - 1, i * INT_BITS);
            }
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

    // Pipeline through ONLY the 69 valid offsets
    accumulate_offsets: for (int j = 0; j < ACTUAL_COUNT_ONES; j++) {
        #pragma HLS PIPELINE II=1
        
        parallel_particles: for (int i = 0; i < N_BUFFER_SIZE; i++) {
            #pragma HLS UNROLL
            if (i < activeParticles) {
                // Reads 1 element from all partitioned rows simultaneously
                particle_sums[i] += buffer_pixels[i][j];
            }
        }
    }

    finalize: for (int i = 0; i < N_BUFFER_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL factor=8
        if (i < activeParticles) {
            buffer_likelihood[i] = scale * (double)particle_sums[i] - bias;
        } else {
            buffer_likelihood[i] = 0.0;
        }
    }
}

void store_likelihood_wide(wide_t* likelihood,
                           const double buffer_likelihood[N_BUFFER_SIZE],
                           int base, int activeParticles) {
    #pragma HLS INLINE off
    const int base_word = base / DOUBLES_PER_WORD;
    const int valid_out_words = (activeParticles + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    storeLikelihoodWide: for (int w = 0; w < valid_out_words; w++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=8
        #pragma HLS PIPELINE II=1
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

// --- NEW DATAFLOW WRAPPER ---
void execute_tiles_dataflow(int total_tiles, int Nparticles, 
                            const wide_t* packed_I, wide_t* likelihood) {
    #pragma HLS INLINE off 
    
    Tile_loop:
    for (int tile = 0; tile < total_tiles; tile++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=(NUM_NPARTICLES + N_BUFFER_SIZE - 1)/N_BUFFER_SIZE
        
        // ONLY ONE DATAFLOW PRAGMA GOES HERE
        #pragma HLS DATAFLOW 
        
        // HLS will automatically convert these into Ping-Pong buffers (PIPO).
        int buffer_pixels[N_BUFFER_SIZE][PADDED_COUNT_ONES];
        double buffer_likelihood[N_BUFFER_SIZE];

        // Complete dim=1 gives us independent row BRAMs for the Compute phase.
        #pragma HLS ARRAY_PARTITION variable=buffer_pixels complete dim=1 
        // Cyclic 16 on dim=2 splits each row into 16 banks for the Load phase.
        #pragma HLS ARRAY_PARTITION variable=buffer_pixels cyclic factor=16 dim=2 
        // Cyclic 8 matches the 8-lane write in store_likelihood_wide
        #pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1
        
        int base = tile * N_BUFFER_SIZE;
        int activeParticles = (Nparticles - base > N_BUFFER_SIZE) ? N_BUFFER_SIZE : (Nparticles - base);

        load_packed_pixels_wide(packed_I, buffer_pixels, base, activeParticles);
        compute_likelihood(buffer_pixels, buffer_likelihood, activeParticles);
        store_likelihood_wide(likelihood, buffer_likelihood, base, activeParticles);    
    }
}

// --- MAIN KERNEL ---
extern "C" {
void likelihood_kernel(int Nparticles,
                       const wide_t* packed_I, 
                       wide_t* likelihood) {
#pragma HLS INTERFACE m_axi port=packed_I offset=slave bundle=gmem0 max_widen_bitwidth=512 max_read_burst_length=16 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem1 max_widen_bitwidth=512 max_write_burst_length=16 num_write_outstanding=4

#pragma HLS INTERFACE s_axilite port=Nparticles bundle=control
#pragma HLS INTERFACE s_axilite port=packed_I bundle=control
#pragma HLS INTERFACE s_axilite port=likelihood bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    assert(Nparticles > 0 && "Nparticles must be greater than 0");

    int total_tiles = (Nparticles + N_BUFFER_SIZE - 1) / N_BUFFER_SIZE;

    execute_tiles_dataflow(total_tiles, Nparticles, packed_I, likelihood);
}
}