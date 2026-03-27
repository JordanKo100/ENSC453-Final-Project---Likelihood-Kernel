CXX ?= g++
NVCC ?= /usr/local/cuda-11.0/bin/nvcc
CUDA_ARCH ?= -gencode arch=compute_61,code=sm_61 -gencode arch=compute_70,code=sm_70 -gencode arch=compute_80,code=sm_80

BASE_CXXFLAGS ?= -O3 -std=c++11 -Wall -Wextra
OPENMP_FLAGS ?= $(shell printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -fopenmp -o /tmp/likelihood_openmp_check >/dev/null 2>&1 && echo -fopenmp)
CXXFLAGS ?= $(BASE_CXXFLAGS) $(OPENMP_FLAGS)
NVCCFLAGS ?= -ccbin g++ -m64 $(CUDA_ARCH) -O3 -std=c++11 -lineinfo

CPU_TARGET := likelihood_cpu_benchmark
GPU_TARGET := likelihood_cuda_benchmark

.PHONY: all cpu gpu run-cpu run-gpu clean

all: cpu

cpu: $(CPU_TARGET)

gpu: $(GPU_TARGET)

run-cpu: cpu
	./$(CPU_TARGET)

run-gpu: gpu
	./$(GPU_TARGET)

$(CPU_TARGET): likelihood_cpu_benchmark.cpp likelihood_benchmark_common.h
	$(CXX) $(CXXFLAGS) likelihood_cpu_benchmark.cpp -o $(CPU_TARGET)

$(GPU_TARGET): likelihood_cuda_benchmark.cu likelihood_benchmark_common.h
	$(NVCC) $(NVCCFLAGS) likelihood_cuda_benchmark.cu -o $(GPU_TARGET)

clean:
	rm -f $(CPU_TARGET) $(GPU_TARGET)
