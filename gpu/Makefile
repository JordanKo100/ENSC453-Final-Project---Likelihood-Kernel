CXX ?= /usr/bin/clang++
ifeq ($(origin CXX), default)
CXX := /usr/bin/clang++
endif
NVCC ?= nvcc
BASE_CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
OPENMP_FLAGS ?= $(shell printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -fopenmp -o /tmp/likelihood_openmp_check >/dev/null 2>&1 && echo -fopenmp)
CXXFLAGS ?= $(BASE_CXXFLAGS) $(OPENMP_FLAGS)
NVCCFLAGS ?= -O3 -std=c++17 -lineinfo

CPU_TARGET := likelihood_cpu_benchmark
GPU_TARGET := likelihood_cuda_benchmark

.PHONY: all cpu gpu clean

all: cpu

cpu: $(CPU_TARGET)

gpu: $(GPU_TARGET)

$(CPU_TARGET): likelihood_cpu_benchmark.cpp likelihood_benchmark_common.h
	$(CXX) $(CXXFLAGS) likelihood_cpu_benchmark.cpp -o $(CPU_TARGET)

$(GPU_TARGET): likelihood_cuda_benchmark.cu likelihood_benchmark_common.h
	$(NVCC) $(NVCCFLAGS) likelihood_cuda_benchmark.cu -o $(GPU_TARGET)

clean:
	rm -f $(CPU_TARGET) $(GPU_TARGET)
