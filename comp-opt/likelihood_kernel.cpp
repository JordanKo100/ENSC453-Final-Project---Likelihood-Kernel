#include <cmath>
#include <cstdlib>
#include "likelihood_kernel.h"

// countOnes = number of points in the object mask/disk
#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

// Helper to simulate the roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + 0.5);
}

void load_objxy(const double* objxy,
		int buffer_obj_offsets[MAX_COUNT_ONES],
		int countOnes,
		int IszY,
		int Nfr){

#pragma HLS INLINE
	loadObjxy:
		for (int i = 0; i < countOnes; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
			int offY = roundDouble(objxy[i * 2]);
			int offX = roundDouble(objxy[i * 2 + 1]);
			buffer_obj_offsets[i] = offX * IszY * Nfr + offY * Nfr;
		}
}


void load_particles(const double* arrayX, 
		    const double* arrayY, 
		    double buffer_X[N_BUFFER_SIZE], 
		    double buffer_Y[N_BUFFER_SIZE],
		    int base,
		    int tileSize){

#pragma HLS INLINE
	loadParticles:
		for (int x = 0; x < tileSize; x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=64 max=64
        		buffer_X[x] = arrayX[base + x];
        		buffer_Y[x] = arrayY[base + x];
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
			int tileSize){

#pragma HLS INLINE
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

    accumuPoints:
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

void store_likelihood(double* likelihood,
		      const double buffer_likelihood[N_BUFFER_SIZE],
		      int base,
		      int tileSize) {
#pragma HLS INLINE
	storeLikelihood:
    		for (int x = 0; x < tileSize; x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			likelihood[base + x] = buffer_likelihood[x];
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
		       double* likelihood){

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
#pragma HLS ARRAY_PARTITION variable=buffer_X cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_Y cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=buffer_likelihood cyclic factor=4 dim=1

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

		compute_likelihood(buffer_X, buffer_Y, buffer_obj_offsets, countOnes, IszY, Nfr, k, max_size, I, buffer_likelihood, tileSize);

		store_likelihood(likelihood, buffer_likelihood, base, tileSize);
		}
	}
}
