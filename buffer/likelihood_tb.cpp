#include <iostream>
#include "likelihood_kernel.h"
#include <chrono>

#define TB_NPARTICLES 200
#define TB_COUNTONES 400
#define TB_ISZY 480
#define TB_NFR 3
#define TB_K 1
#define TB_MAX_SIZE 1000000

int main() {

	static double arrayX[TB_NPARTICLES];
	static double arrayY[TB_NPARTICLES];
	static double objxy[TB_COUNTONES * 2];
	static int I[TB_MAX_SIZE];
	static double likelihood[TB_NPARTICLES];

	for (int i = 0; i < TB_NPARTICLES; i++) {
		arrayX[i] = 100.0;
        	arrayY[i] = 100.0;
        	likelihood[i] = 0.0;
    	}
	
	for (int i = 0; i < TB_COUNTONES * 2; i++) {
        	objxy[i] = 5.0;
    	}

    	for (long i = 0; i < TB_MAX_SIZE; i++) {
        	I[i] = 120;

    	}

auto start = std::chrono::high_resolution_clock::now();

    	// Call kernel
	likelihood_kernel(TB_NPARTICLES,
                      	  TB_COUNTONES,
                      	  TB_ISZY,
                      	  TB_NFR,
                      	  TB_K,
                      	  TB_MAX_SIZE,
                      	  arrayX,
                          arrayY,
                      	  objxy,
                      	  I,
                      	  likelihood);

auto end = std::chrono::high_resolution_clock::now();

std::chrono::duration<double> elapsed = end - start;
std::cout << "Kernel execution time: " << elapsed.count() << " s" << std::endl;

    // prints some outputs
    for (int i = 0; i < 5; i++) {
        std::cout << "likelihood[" << i << "] = " << likelihood[i] << std::endl;
    }
}
