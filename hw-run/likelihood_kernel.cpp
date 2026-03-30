#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <cassert>
#include <ap_int.h>

#include "likelihood_kernel.h"

#define MAX_ROW_BUFFER_SPAN 512

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

static_assert(N_BUFFER_SIZE % WORDS_PER_TILE == 0, "N_BUFFER_SIZE must be strictly divisible by 8 to align with 512-bit AXI ports!");
static_assert(MAX_COUNT_ONES % PIX_SUM_LANES == 0, "MAX_COUNT_ONES must be perfectly divisible by PIX_SUM_LANES!");

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

    const int total_doubles = countOnes * 2;
    loadObjxy: for (int i = 0; i < total_doubles; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=2 max=4050
        buffer_objxy[i] = roundDouble(objxy[i]);
    }
}

void build_obj_offsets(const int buffer_objxy[MAX_COUNT_ONES * 2],
                       int buffer_obj_offsets[MAX_COUNT_ONES],
                       int countOnes,
                       int IszY,
                       int Nfr) {
    #pragma HLS INLINE

    buildObjOffsets: for (int i = 0; i < countOnes; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=1 max=2025
        const int offY = buffer_objxy[i * 2];
        const int offX = buffer_objxy[i * 2 + 1];
        buffer_obj_offsets[i] = offX * IszY * Nfr + offY * Nfr;
    }
}

void build_row_segments(const int buffer_objxy[MAX_COUNT_ONES * 2],
                        const int buffer_obj_offsets[MAX_COUNT_ONES],
                        int countOnes,
                        int row_start_idx[MAX_COUNT_ONES],
                        int row_len[MAX_COUNT_ONES],
                        int row_anchor_offset[MAX_COUNT_ONES],
                        int row_span[MAX_COUNT_ONES],
                        int point_local_offset[MAX_COUNT_ONES],
                        int& num_row_segments) {
    #pragma HLS INLINE

    int seg = 0;
    int idx = 0;

buildSegments:
    while (idx < countOnes) {
#pragma HLS LOOP_TRIPCOUNT min=45 max=45
        const int offX = buffer_objxy[idx * 2 + 1];
        int end = idx;

    findSegmentEnd:
        while ((end + 1) < countOnes && buffer_objxy[(end + 1) * 2 + 1] == offX) {
#pragma HLS LOOP_TRIPCOUNT min=44 max=44
            end++;
        }

        int min_offset = buffer_obj_offsets[idx];
        int max_offset = buffer_obj_offsets[idx];

    findSegmentMinMax:
        for (int i = idx + 1; i <= end; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=45
            const int curr = buffer_obj_offsets[i];
            if (curr < min_offset) {
                min_offset = curr;
            }
            if (curr > max_offset) {
                max_offset = curr;
            }
        }

        row_start_idx[seg] = idx;
        row_len[seg] = end - idx + 1;
        row_anchor_offset[seg] = min_offset;
        row_span[seg] = max_offset - min_offset + 1;

    buildLocalOffsets:
        for (int i = idx; i <= end; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=45
            point_local_offset[i] = buffer_obj_offsets[i] - min_offset;
        }

        seg++;
        idx = end + 1;
    }

    num_row_segments = seg;
}

void load_particles(const wide_t* arrayX,
                    const wide_t* arrayY,
                    long particle_base_buffer[N_BUFFER_SIZE],
                    int base,
                    int activeParticles,
                    int IszY,
                    int Nfr,
                    int k) {
    // INLINE OFF required for DATAFLOW process creation
    #pragma HLS INLINE off 

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
                particle_base_buffer[idx] =
                    (long)px * (long)IszY * (long)Nfr + (long)py * (long)Nfr + (long)k;
            } else {
                particle_base_buffer[idx] = 0;
            }
        }
    }
}

void load_pixels_buffered_burst(const long particle_base_buffer[N_BUFFER_SIZE],
                                const int buffer_obj_offsets[MAX_COUNT_ONES],
                                const int row_start_idx[MAX_COUNT_ONES],
                                const int row_len[MAX_COUNT_ONES],
                                const int row_anchor_offset[MAX_COUNT_ONES],
                                const int row_span[MAX_COUNT_ONES],
                                const int point_local_offset[MAX_COUNT_ONES],
                                int num_row_segments,
                                int countOnes,
                                long max_size,
                                const int* I,
                                int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                                int activeParticles) {
    // INLINE OFF required for DATAFLOW
    #pragma HLS INLINE off

    int row_buf[MAX_ROW_BUFFER_SPAN];

loadPixelsPerParticle:
    for (int x = 0; x < activeParticles; x++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
#pragma HLS LOOP_FLATTEN off
        bool use_buffered_path = true;

    checkBufferedPath:
        for (int seg = 0; seg < num_row_segments; seg++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=45 max=45
            const long start_addr = particle_base_buffer[x] + (long)row_anchor_offset[seg];
            const long end_addr = start_addr + (long)row_span[seg] - 1L;

            if (start_addr < 0 || end_addr >= max_size || row_span[seg] > MAX_ROW_BUFFER_SPAN) {
                use_buffered_path = false;
            }
        }

        if (use_buffered_path) {
        bufferedSegments:
            for (int seg = 0; seg < num_row_segments; seg++) {
#pragma HLS LOOP_TRIPCOUNT min=45 max=45
#pragma HLS LOOP_FLATTEN off
                const long start_addr = particle_base_buffer[x] + (long)row_anchor_offset[seg];
                const int span = row_span[seg];
                const int start_idx = row_start_idx[seg];
                const int len = row_len[seg];

            burstLoadRow:
                for (int t = 0; t < span; t++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=45 max=45
                    row_buf[t] = I[start_addr + (long)t];
                }

            scatterMaskPoints:
                for (int p = 0; p < len; p++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=45 max=45
                    const int y = start_idx + p;
                    buffer_pixels[x][y] = row_buf[point_local_offset[y]];
                }
            }
        } else {
        fallbackGather:
            for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=2025
                long idx = absLong(particle_base_buffer[x] + (long)buffer_obj_offsets[y]);

                if (idx >= max_size) {
                    idx = 0;
                }

                buffer_pixels[x][y] = I[idx];
            }
        }
    }
}

void compute_likelihood(const int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                        int countOnes,
                        double buffer_likelihood[N_BUFFER_SIZE],
                        int activeParticles) {
    // INLINE OFF required for DATAFLOW
    #pragma HLS INLINE off
    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

    computeParticles: for (int x = 0; x < activeParticles; x++) {
        #pragma HLS LOOP_TRIPCOUNT min=1 max=64
        int partial_sum[PIX_SUM_LANES];
        #pragma HLS ARRAY_PARTITION variable=partial_sum complete dim=1

        initPartialSums: for (int lane = 0; lane < PIX_SUM_LANES; lane++) {
            #pragma HLS UNROLL
            partial_sum[lane] = 0;
        }

        accumPixels: for (int chunk = 0; chunk < PIX_CHUNKS; chunk++) {
            #pragma HLS PIPELINE II=1
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

void store_likelihood(wide_t* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int activeParticles) {
    // INLINE OFF required for DATAFLOW
    #pragma HLS INLINE off

    const int base_word = base / DOUBLES_PER_WORD;
    const int full_words = activeParticles / DOUBLES_PER_WORD;
    const int remainder = activeParticles % DOUBLES_PER_WORD;

    storeLikelihoodWide: for (int w = 0; w < WORDS_PER_TILE; w++) {
        #pragma HLS PIPELINE II=1

        const bool write_word =
            (w < full_words) ||
            ((w == full_words) && (remainder != 0));

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

void process_tile(const wide_t* arrayX, const wide_t* arrayY, const int* I, 
                  const int buffer_obj_offsets[MAX_COUNT_ONES], 
                  const int row_start_idx[MAX_COUNT_ONES],
                  const int row_len[MAX_COUNT_ONES],
                  const int row_anchor_offset[MAX_COUNT_ONES],
                  const int row_span[MAX_COUNT_ONES],
                  const int point_local_offset[MAX_COUNT_ONES],
                  int num_row_segments,
                  wide_t* likelihood,
                  int base, int activeParticles, int countOnes, long max_size, 
                  int IszY, int Nfr, int k) {
    
    // The magic pragma: allows these 4 functions to run concurrently
    #pragma HLS DATAFLOW
    
    long particle_base_buffer[N_BUFFER_SIZE];
    int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

    // PIPO array memory partitions
    #pragma HLS ARRAY_PARTITION variable=particle_base_buffer cyclic factor=8 dim=1
    #pragma HLS ARRAY_PARTITION variable=buffer_pixels cyclic factor=45 dim=2
    #pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1

    load_particles(arrayX, arrayY, particle_base_buffer, base, activeParticles, IszY, Nfr, k);
    load_pixels_buffered_burst(particle_base_buffer, buffer_obj_offsets, row_start_idx, row_len, row_anchor_offset, row_span, point_local_offset, num_row_segments, countOnes, max_size, I, buffer_pixels, activeParticles);
    compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, activeParticles);
    store_likelihood(likelihood, buffer_likelihood, base, activeParticles);
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
// Wide bus and auto-burst pragmas
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0 max_widen_bitwidth=512 max_read_burst_length=16 num_read_outstanding=4
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1 max_widen_bitwidth=512 max_read_burst_length=16 num_read_outstanding=4
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2 max_widen_bitwidth=64 max_read_burst_length=16 num_read_outstanding=4
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3 max_widen_bitwidth=32 max_read_burst_length=64 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem4 max_widen_bitwidth=512 max_write_burst_length=16 num_write_outstanding=4

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
    int buffer_obj_offsets[MAX_COUNT_ONES];
    int row_start_idx[MAX_COUNT_ONES];
    int row_len[MAX_COUNT_ONES];
    int row_anchor_offset[MAX_COUNT_ONES];
    int row_span[MAX_COUNT_ONES];
    int point_local_offset[MAX_COUNT_ONES];
    int num_row_segments;

    #pragma HLS ARRAY_PARTITION variable=buffer_obj_offsets cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=point_local_offset cyclic factor=4 dim=1

    load_objxy(objxy, buffer_objxy, countOnes);
    build_obj_offsets(buffer_objxy, buffer_obj_offsets, countOnes, IszY, Nfr);
    build_row_segments(buffer_objxy,
                       buffer_obj_offsets,
                       countOnes,
                       row_start_idx,
                       row_len,
                       row_anchor_offset,
                       row_span,
                       point_local_offset,
                       num_row_segments);

Particle_loop: for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
    #pragma HLS LOOP_TRIPCOUNT min=1 max=15625
    #pragma HLS LOOP_FLATTEN off

        int activeParticles = Nparticles - base;
        if (activeParticles > N_BUFFER_SIZE) {
            activeParticles = N_BUFFER_SIZE;
        }

        process_tile(arrayX, arrayY, I, buffer_obj_offsets, 
                     row_start_idx, row_len, row_anchor_offset, row_span, point_local_offset, num_row_segments,
                     likelihood, base, activeParticles, countOnes, max_size, IszY, Nfr, k);
    }
}
}