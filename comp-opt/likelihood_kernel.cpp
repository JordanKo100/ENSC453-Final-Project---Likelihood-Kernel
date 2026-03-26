#include <cmath>
#include <cstdlib>

#include "likelihood_kernel.h"

#define MAX_COUNT_ONES 80
#define N_BUFFER_SIZE 64
#define POINT_PAR 4
#define POINT_GROUPS (MAX_COUNT_ONES / POINT_PAR)

static const double kPixelScaleNum = 256.0;
static const double kPixelBiasNum = 41984.0;
static const double kPixelDen = 50.0;

inline int roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline long absLong(long value) {
    return (value < 0) ? -value : value;
}

void load_objxy(const double* objxy,
		int buffer_objxy[MAX_COUNT_ONES * 2], 
		int countOnes){

	#pragma HLS INLINE off
	loadObjxy: for (int i = 0; i < countOnes * 2; i++) {
		#pragma HLS PIPELINE II=1
		#pragma HLS LOOP_TRIPCOUNT min=140 max=160

		buffer_objxy[i] = roundDouble(objxy[i]);
	}
}

void load_particles(const double* arrayX, 
		    const double* arrayY, 
		    double buffer_X[N_BUFFER_SIZE], 
		    double buffer_Y[N_BUFFER_SIZE],
		    int base,
		    int tileSize){

	#pragma HLS INLINE off
	loadParticles: for (int x = 0; x < N_BUFFER_SIZE; x++) {
		#pragma HLS PIPELINE II=1

		if (x < tileSize){
			buffer_X[x] = arrayX[base + x];
			buffer_Y[x] = arrayY[base + x];
		}
	}
}

void load_pixels(const double buffer_X[N_BUFFER_SIZE],
                 const double buffer_Y[N_BUFFER_SIZE],
                 const int buffer_objxy[MAX_COUNT_ONES * 2],
                 int countOnes,
                 int IszY,
                 int Nfr,
                 int k,
                 long max_size,
                 const int* I,
                 int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                 int tileSize) {
	#pragma HLS INLINE off

	loadPixels: for (int x = 0; x < N_BUFFER_SIZE; x++) {
		if (x < tileSize){
			const int px = roundDouble(buffer_X[x]);
			const int py = roundDouble(buffer_Y[x]);

			loadParticlePixels: for (int y = 0; y < countOnes; y++) {
				#pragma HLS PIPELINE II=1
				#pragma HLS LOOP_TRIPCOUNT min=70 max=80
				const int offY = buffer_objxy[y * 2];
				const int offX = buffer_objxy[y * 2 + 1];
				const int indX = px + offX;
				const int indY = py + offY;
				long idx = absLong((long)indX * (long)IszY * (long)Nfr +
				(long)indY * (long)Nfr + (long)k);

            if (idx >= max_size) {
                idx = 0;
            }

            buffer_pixels[x][y] = I[idx];
        }
    }
}

void compute_likelihood(const int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES],
                        int countOnes,
                        double buffer_likelihood[N_BUFFER_SIZE],
                        int tileSize) {
#pragma HLS INLINE

    const double inv_count = 1.0 / (double)countOnes;
    const double scale = (kPixelScaleNum / kPixelDen) * inv_count;
    const double bias = kPixelBiasNum / kPixelDen;

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
        for (int group = 0; group < POINT_GROUPS; group++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=20 max=20
            const int base_idx = group * POINT_PAR;
        accumulateLanes:
            for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
                const int idx = base_idx + lane;
                if (idx < countOnes) {
                    partial_sum[lane] += buffer_pixels[x][idx];
                }
            }
        }

        int pixel_sum = 0;
    sumPartialSums:
        for (int lane = 0; lane < POINT_PAR; lane++) {
#pragma HLS UNROLL
            pixel_sum += partial_sum[lane];
        }

        buffer_likelihood[x] = scale * (double)pixel_sum - bias;
    }
}

void store_likelihood(double* likelihood,
		      const double buffer_likelihood[N_BUFFER_SIZE],
		      int base,
		      int tileSize) {

	#pragma HLS INLINE off
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
                       double* likelihood) {
#pragma HLS INTERFACE m_axi port=arrayX offset=slave bundle=gmem0 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=arrayY offset=slave bundle=gmem1 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=objxy offset=slave bundle=gmem2 max_widen_bitwidth=64
#pragma HLS INTERFACE m_axi port=I offset=slave bundle=gmem3 max_widen_bitwidth=32 max_read_burst_length=1 num_read_outstanding=1
#pragma HLS INTERFACE m_axi port=likelihood offset=slave bundle=gmem4 max_widen_bitwidth=64

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
    int buffer_pixels[N_BUFFER_SIZE][MAX_COUNT_ONES];
    double buffer_likelihood[N_BUFFER_SIZE];

#pragma HLS ARRAY_PARTITION variable=buffer_pixels cyclic factor=4 dim=2

    load_objxy(objxy, buffer_objxy, countOnes);

Particle_loop:
    for (int base = 0; base < Nparticles; base += N_BUFFER_SIZE) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=32
#pragma HLS LOOP_FLATTEN off
        int tileSize = Nparticles - base;
        if (tileSize > N_BUFFER_SIZE) {
            tileSize = N_BUFFER_SIZE;
        }

        load_particles(arrayX, arrayY, buffer_X, buffer_Y, base, tileSize);
        load_pixels(buffer_X, buffer_Y, buffer_objxy, countOnes, IszY, Nfr, k, max_size, I,
                    buffer_pixels, tileSize);
        compute_likelihood(buffer_pixels, countOnes, buffer_likelihood, tileSize);
        store_likelihood(likelihood, buffer_likelihood, base, tileSize);
    }
}
}
