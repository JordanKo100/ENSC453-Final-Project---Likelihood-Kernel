#include <stdint.h>

#include <ap_int.h>
#include <hls_stream.h>

#include "likelihood_kernel.h"

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64
#define POINT_PAR 4
#define POINT_GROUPS (MAX_COUNT_ONES / POINT_PAR)
#define WORDS_PER_TILE (N_BUFFER_SIZE / DOUBLES_PER_WORD)

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

typedef ap_uint<64> tile_meta_t;
typedef ap_int<32> pixel_t;

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

inline tile_meta_t pack_tile_meta(int base, int tileSize) {
    tile_meta_t meta = 0;
    meta.range(31, 0) = (ap_uint<32>)base;
    meta.range(63, 32) = (ap_uint<32>)tileSize;
    return meta;
}

inline int unpack_tile_base(tile_meta_t meta) {
    return (int)(ap_uint<32>)meta.range(31, 0);
}

inline int unpack_tile_size(tile_meta_t meta) {
    return (int)(ap_uint<32>)meta.range(63, 32);
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

void load_pixel_groups(int Nparticles,
                       int countOnes,
                       int IszY,
                       int Nfr,
                       int k,
                       long max_size,
                       const wide_t* arrayX,
                       const wide_t* arrayY,
                       const int buffer_obj_offsets[MAX_COUNT_ONES],
                       const int* I,
                       hls::stream<tile_meta_t>& meta_stream,
                       hls::stream<pixel_t>& pixel_stream) {
#pragma HLS INLINE off

    long particle_base_buffer[N_BUFFER_SIZE];
#pragma HLS ARRAY_PARTITION variable=particle_base_buffer cyclic factor=8 dim=1

loadTiles:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
        int tileSize = Nparticles - base;
        if (tileSize > N_BUFFER_SIZE) {
            tileSize = N_BUFFER_SIZE;
        }

        load_particles(arrayX, arrayY, particle_base_buffer, base, tileSize, IszY, Nfr, k);
        meta_stream.write(pack_tile_meta(base, tileSize));

    loadTileParticles:
        for (int x = 0; x < tileSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
#pragma HLS LOOP_FLATTEN off
        loadTilePoints:
            for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=80
                long idx = absLong(particle_base_buffer[x] + (long)buffer_obj_offsets[y]);
                if (idx >= max_size) {
                    idx = 0;
                }
                pixel_stream.write((pixel_t)I[idx]);
            }
        }
    }
}

void compute_likelihood_stream(int total_tiles,
                               int countOnes,
                               hls::stream<tile_meta_t>& in_meta_stream,
                               hls::stream<tile_meta_t>& out_meta_stream,
                               hls::stream<pixel_t>& pixel_stream,
                               hls::stream<double>& likelihood_stream) {
#pragma HLS INLINE off

    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

computeTiles:
    for (int tile = 0; tile < total_tiles; tile++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
        const tile_meta_t meta = in_meta_stream.read();
        const int tileSize = unpack_tile_size(meta);
        out_meta_stream.write(meta);

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
            for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=80
                const int lane = y & (POINT_PAR - 1);
                partial_sum[lane] += (int)pixel_stream.read();
            }

            int pixel_sum = 0;
        sumPartialSums:
            for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
                pixel_sum += partial_sum[lane];
            }

            likelihood_stream.write(scale * (double)pixel_sum - bias);
        }
    }
}

void store_likelihood_stream(int total_tiles,
                             hls::stream<tile_meta_t>& meta_stream,
                             hls::stream<double>& likelihood_stream,
                             wide_t* likelihood) {
#pragma HLS INLINE off

    double buffer_likelihood[N_BUFFER_SIZE];
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=8 dim=1

storeTiles:
    for (int tile = 0; tile < total_tiles; tile++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
        const tile_meta_t meta = meta_stream.read();
        const int base = unpack_tile_base(meta);
        const int tileSize = unpack_tile_size(meta);
        const int base_word = base / DOUBLES_PER_WORD;
        const int valid_words = (tileSize + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    readLikelihoodTile:
        for (int x = 0; x < tileSize; x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
            buffer_likelihood[x] = likelihood_stream.read();
        }

    storeWords:
        for (int w = 0; w < valid_words; w++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=8
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

void process_tiles_dataflow(int Nparticles,
                            int countOnes,
                            int IszY,
                            int Nfr,
                            int k,
                            long max_size,
                            int total_tiles,
                            const wide_t* arrayX,
                            const wide_t* arrayY,
                            const int buffer_obj_offsets[MAX_COUNT_ONES],
                            const int* I,
                            wide_t* likelihood) {
#pragma HLS INLINE off

    hls::stream<tile_meta_t> tile_meta_load_to_compute("tile_meta_load_to_compute");
    hls::stream<tile_meta_t> tile_meta_compute_to_store("tile_meta_compute_to_store");
    hls::stream<pixel_t> pixel_stream("pixel_stream");
    hls::stream<double> likelihood_stream("likelihood_stream");

#pragma HLS STREAM variable=tile_meta_load_to_compute depth=4
#pragma HLS STREAM variable=tile_meta_compute_to_store depth=4
#pragma HLS STREAM variable=pixel_stream depth=128
#pragma HLS STREAM variable=likelihood_stream depth=128

#pragma HLS DATAFLOW
    load_pixel_groups(Nparticles, countOnes, IszY, Nfr, k, max_size, arrayX, arrayY,
                      buffer_obj_offsets, I, tile_meta_load_to_compute, pixel_stream);
    compute_likelihood_stream(total_tiles, countOnes, tile_meta_load_to_compute,
                              tile_meta_compute_to_store, pixel_stream, likelihood_stream);
    store_likelihood_stream(total_tiles, tile_meta_compute_to_store, likelihood_stream,
                            likelihood);
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
    const int total_tiles = (Nparticles + N_BUFFER_SIZE - 1) / N_BUFFER_SIZE;

#pragma HLS ARRAY_PARTITION variable=buffer_obj_offsets cyclic factor=4 dim=1

    load_objxy(objxy, buffer_objxy, countOnes);
    build_obj_offsets(buffer_objxy, buffer_obj_offsets, countOnes, IszY, Nfr);

    if (total_tiles <= 0) {
        return;
    }

    process_tiles_dataflow(Nparticles, countOnes, IszY, Nfr, k, max_size, total_tiles, arrayX,
                           arrayY, buffer_obj_offsets, I, likelihood);
}
}
