#include <cmath>
#include <cstdlib>
#include "likelihood_kernel.h"

// countOnes = number of points in the object mask/disk
#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64

// Helper to simulate the roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + 0.5);
}

void load_objxy(const double* objxy,
		double buffer_objxy[MAX_COUNT_ONES * 2], 
		int countOnes){

#pragma HLS INLINE
	loadObjxy:
		for (int i = 0; i < countOnes * 2; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=140 max =160
			buffer_objxy[i] = objxy[i];
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
        double accum = 0.0;

    accumuPoints:
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

            accum += ((double)(a * a) - (double)(b * b)) / 50.0;
        }

        buffer_likelihood[x] = accum / (double)countOnes;
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

