#include <cmath>
#include <cstdlib>
#include <cassert>

#include "likelihood_kernel.h"

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

// Helper to simulate the roundDouble functionality
inline int roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline long absLong(long value) {
    return (value < 0) ? -value : value;
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

void build_obj_offsets(const int buffer_objxy[MAX_COUNT_ONES * 2],
		       int buffer_objxy_offset[MAX_COUNT_ONES],
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
			buffer_objxy_offset[i] = offX * IszY * Nfr + offY * Nfr;
		}
}

void load_particles(const double* arrayX,
                    const double* arrayY,
                    long buffer_idx_1D[N_BUFFER_SIZE],
                    int base,
                    int activeParticles,
                    int IszY,
                    int Nfr,
                    int k) {
    #pragma HLS INLINE

    loadParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < activeParticles) {
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
		 int activeParticles){

#pragma HLS INLINE
	int x = 0;
	int y = 0;

	loadPixels:
		for (int iter = 0; iter < activeParticles * countOnes; iter++) {
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
			int activeParticles){

#pragma HLS INLINE
	const double inv_count = 1.0 / (double)countOnes;
	const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
	const double bias = kPixelBiasNum / kPixelDen;

		computeLikelihood:
	    		for (int x = 0; x < activeParticles; x++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			int pixel_sum = 0;
	    		accumuLikelihood:
	        		for (int y = 0; y < countOnes; y++) {
#pragma HLS LOOP_TRIPCOUNT min=70 max=80
				pixel_sum += buffer_pixels[x][y];
        		}

        	buffer_likelihood[x] = scale * (double)pixel_sum - bias;
    		}
}


void store_likelihood(double* likelihood,
                      const double buffer_likelihood[N_BUFFER_SIZE],
                      int base,
                      int activeParticles) {
    #pragma HLS INLINE

    storeLikelihood: for (int x = 0; x < N_BUFFER_SIZE; x++) {
        #pragma HLS PIPELINE II=1
        if (x < activeParticles) {
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

    assert(countOnes > 0 && "countOnes must be greater than 0");
	    assert(countOnes <= MAX_COUNT_ONES && "countOnes exceeds statically allocated MAX_COUNT_ONES!");
	    assert(Nparticles > 0 && "Nparticles must be greater than 0");

		int buffer_objxy[MAX_COUNT_ONES * 2];
		int buffer_objxy_offset[MAX_COUNT_ONES];
		long buffer_idx_1D[N_BUFFER_SIZE];
		int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
		double buffer_likelihood[N_BUFFER_SIZE];

		load_objxy(objxy, buffer_objxy, countOnes);
		build_obj_offsets(buffer_objxy, buffer_objxy_offset, countOnes, IszY, Nfr);
		
			Particle_loop:
				for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off

        int activeParticles = Nparticles - base;
	        if (activeParticles > N_BUFFER_SIZE) {
	            activeParticles = N_BUFFER_SIZE;
	        }

			load_particles(arrayX, arrayY, buffer_idx_1D, base, activeParticles, IszY, Nfr, k);

			load_pixels(buffer_idx_1D, buffer_objxy_offset, countOnes, max_size, I, buffer_pixels, activeParticles);

			compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, activeParticles);

			store_likelihood(likelihood, buffer_likelihood, base, activeParticles);
			}
	}
}
