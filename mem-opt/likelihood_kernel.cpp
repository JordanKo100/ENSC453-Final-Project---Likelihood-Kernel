#include <cmath>
#include <cstdlib>
#include "likelihood_kernel.h"

#include <ap_int.h>
#include <stdint.h>

// countOnes = number of points in the object mask/disk
#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

// Helper to simulate roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + 0.5);
}

// Helpers for packing/unpacking double to and from 64-bit bits
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
		double buffer_objxy[MAX_COUNT_ONES * 2], 
		int countOnes){

#pragma HLS INLINE
	loadObjxy:
		for (int i = 0; i < MAX_COUNT_ONES * 2; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=140 max =160
			if (i < countOnes *2) {
				buffer_objxy[i] = objxy[i];
			}
		}
}


void load_particles(const wide_t* arrayX, 
		    const wide_t* arrayY, 
		    double buffer_X[N_BUFFER_SIZE], 
		    double buffer_Y[N_BUFFER_SIZE],
		    int base,
		    int tileSize){

#pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;
    const int num_words = (tileSize + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

loadParticlesWide:
    for (int w = 0; w < N_BUFFER_SIZE / DOUBLES_PER_WORD; w++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=8 max=8

        if (w < num_words) {
            wide_t xpack = arrayX[base_word + w];
            wide_t ypack = arrayY[base_word + w];

        unpackWord:
            for (int d = 0; d < DOUBLES_PER_WORD; d++) {
#pragma HLS UNROLL
                int idx = w * DOUBLES_PER_WORD + d;

                if (idx < tileSize) {
                    uint64_t xbits = (uint64_t)xpack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                    uint64_t ybits = (uint64_t)ypack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);

                    buffer_X[idx] = bits_to_double(xbits);
                    buffer_Y[idx] = bits_to_double(ybits);
                }
            }
        }
    }
}


void compute_likelihood(const double buffer_X[N_BUFFER_SIZE], 
			const double buffer_Y[N_BUFFER_SIZE], 
			const double buffer_objxy[MAX_COUNT_ONES * 2],
			int countOnes,
			int IszY,
			int Nfr,
			int k,
			long max_size,
			const int* I, 
			double buffer_likelihood[N_BUFFER_SIZE],
			int tileSize){

#pragma HLS INLINE

computeParticles:
    for (int x = 0; x < tileSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=64

        int px = roundDouble(buffer_X[x]);
        int py = roundDouble(buffer_Y[x]);
        double sum = 0.0;

    accumulatePoints:
        for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
#pragma HLS UNROLL factor=2

            int offY = roundDouble(buffer_objxy[y * 2]);
            int offX = roundDouble(buffer_objxy[y * 2 + 1]);

            int indX = px + offX;
            int indY = py + offY;

            int idx = std::abs(indX * IszY * Nfr + indY * Nfr + k);

            if (idx >= max_size) {
                idx = 0;
            }

            int pix = I[idx];
            int a = pix - 100;
            int b = pix - 228;

            sum += ((double)(a * a) - (double)(b * b)) / 50.0;
        }

        buffer_likelihood[x] = sum / (double)countOnes;
    }
}

void store_likelihood(wide_t* likelihood,
		      const double buffer_likelihood[N_BUFFER_SIZE],
		      int base,
		      int tileSize) {
#pragma HLS INLINE

    const int base_word = base / DOUBLES_PER_WORD;

storeLikelihoodWide:
    for (int w = 0; w < tileSize / DOUBLES_PER_WORD; w++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=8 max=8

        wide_t out_pack = 0;

    packWord:
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
#pragma HLS UNROLL
            uint64_t bits = double_to_bits(buffer_likelihood[w * DOUBLES_PER_WORD + d]);
            out_pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = bits;
        }

        likelihood[base_word + w] = out_pack;
    }

storeLikelihoodRemainder:
    for (int x = (tileSize / DOUBLES_PER_WORD) * DOUBLES_PER_WORD; x < tileSize; x++) {
#pragma HLS PIPELINE II=1
        double* likelihood_scalar = reinterpret_cast<double*>(likelihood);
        likelihood_scalar[base + x] = buffer_likelihood[x];
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
		       wide_t* likelihood){

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

	double buffer_objxy[MAX_COUNT_ONES * 2];
	double buffer_X[N_BUFFER_SIZE];
	double buffer_Y[N_BUFFER_SIZE];
	double buffer_likelihood[N_BUFFER_SIZE];

#pragma HLS BIND_STORAGE variable=buffer_objxy type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_X type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_Y type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=buffer_likelihood type=ram_2p impl=bram

#pragma HLS ARRAY_PARTITION variable=buffer_objxy cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_X cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_Y cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=4 dim=1

	load_objxy(objxy, buffer_objxy, countOnes);
	

	Particle_loop:
		for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=32 max=32
			int tileSize = N_BUFFER_SIZE;

			if (base + N_BUFFER_SIZE > Nparticles) {
				tileSize = Nparticles - base;
			}

		load_particles(arrayX, arrayY, buffer_X, buffer_Y, base, tileSize);

		compute_likelihood(buffer_X, buffer_Y, buffer_objxy, countOnes, IszY, Nfr, k, max_size, I, buffer_likelihood, tileSize);

		store_likelihood(likelihood, buffer_likelihood, base, tileSize);
		}
	}
}

