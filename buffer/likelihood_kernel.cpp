#include <cmath>
#include <cstdlib>
#include "likelihood_kernel.h"

// countOnes = number of points in the object mask/disk
#define MAX_COUNT_ONES 400
#define N_BUFFER_SIZE 100

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
#pragma HLS LOOP_TRIPCOUNT min=800 max =800
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
#pragma HLS LOOP_TRIPCOUNT min=100 max=100
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
			int ind_buffer[N_BUFFER_SIZE * MAX_COUNT_ONES],
			int tileSize){

#pragma HLS INLINE

	computeLikelihood:
    		for (int x = 0; x < tileSize; x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=100 max=100
    		computeIndices:
        		for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=400 max=400
            			int indX = roundDouble(buffer_X[x]) + roundDouble(buffer_objxy[y * 2 + 1]);
            			int indY = roundDouble(buffer_Y[x]) + roundDouble(buffer_objxy[y * 2]);

            			ind_buffer[x * countOnes + y] = std::abs(indX * IszY * Nfr + indY * Nfr + k);

            			if (ind_buffer[x * countOnes + y] >= max_size) {
                			ind_buffer[x * countOnes + y] = 0;
            			}
        		}
        	buffer_likelihood[x] = 0.0;

    		accumuLikelihood:
        		for (int y = 0; y < countOnes; y++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=400 max=400
            			int idx = ind_buffer[x * countOnes + y];
				int a = I[idx] - 100;
				int b = I[idx] - 228;
            			buffer_likelihood[x] += ((double)(a*a)-(double)(b*b))/50.0;
        		}

        	buffer_likelihood[x] = buffer_likelihood[x] / (double)countOnes;
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
#pragma HLS LOOP_TRIPCOUNT min=100 max=100
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
	int ind_buffer[N_BUFFER_SIZE * MAX_COUNT_ONES];

	load_objxy(objxy, buffer_objxy, countOnes);
	

	Particle_loop:
		for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=20 max=20
			int tileSize = N_BUFFER_SIZE;

			if (base + N_BUFFER_SIZE > Nparticles) {
				tileSize = Nparticles - base;
			}

		load_particles(arrayX, arrayY, buffer_X, buffer_Y, base, tileSize);

		compute_likelihood(buffer_X, buffer_Y, buffer_objxy, countOnes, IszY, Nfr, k, max_size, I, buffer_likelihood, ind_buffer, tileSize);

		store_likelihood(likelihood, buffer_likelihood, base, tileSize);
		}
	}
}

