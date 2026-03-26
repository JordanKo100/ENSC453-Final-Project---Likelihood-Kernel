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
		int buffer_objxy[MAX_COUNT_ONES * 2], 
		int countOnes){

#pragma HLS INLINE
	loadObjxy:
			for (int i = 0; i < countOnes * 2; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=140 max =160
				buffer_objxy[i] = roundDouble(objxy[i]);
			}
}


void load_particles(const double* arrayX, 
		    const double* arrayY, 
		    double buffer_X[N_BUFFER_SIZE], 
		    double buffer_Y[N_BUFFER_SIZE],
		    int base,
		    int tileSize){

	#pragma HLS INLINE
	loadParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
		#pragma HLS PIPELINE II=1

		if (x < tileSize){
			buffer_X[x] = arrayX[base + x];
			buffer_Y[x] = arrayY[base + x];
		}
	}
}


void compute_likelihood(const double buffer_X[N_BUFFER_SIZE], 
			const double buffer_Y[N_BUFFER_SIZE], 
			const int buffer_objxy[MAX_COUNT_ONES * 2],
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
	int ind_buffer[MAX_COUNT_ONES];

		computeLikelihood:
	    		for (int x = 0; x < tileSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			int px = roundDouble(buffer_X[x]);
			int py = roundDouble(buffer_Y[x]);
			int pixel_sum = 0;
		computeIndices:
			for (int y = 0; y < countOnes; y++) {
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
				int offY = buffer_objxy[y * 2];
				int offX = buffer_objxy[y * 2 + 1];
				int indX = px + offX;
				int indY = py + offY;
				int idx = std::abs(indX * IszY * Nfr + indY * Nfr + k);

				if (idx >= max_size) {
					idx = 0;
				}

				ind_buffer[y] = idx;
			}
	    		accumuLikelihood:
	        		for (int y = 0; y < countOnes; y++) {
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
				pixel_sum += I[ind_buffer[y]];
        		}

        	buffer_likelihood[x] = scale * (double)pixel_sum - bias;
    		}
}


void store_likelihood(double* likelihood,
		      const double buffer_likelihood[N_BUFFER_SIZE],
		      int base,
		      int tileSize) {

	#pragma HLS INLINE
	storeLikelihood: for (int x = 0; x < N_BUFFER_SIZE; x++) {
		#pragma HLS PIPELINE II=1
		
		if (x < tileSize){
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

	int buffer_objxy[MAX_COUNT_ONES * 2];
	double buffer_X[N_BUFFER_SIZE];
	double buffer_Y[N_BUFFER_SIZE];
	double buffer_likelihood[N_BUFFER_SIZE];

	load_objxy(objxy, buffer_objxy, countOnes);
	
		Particle_loop:
			for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
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
