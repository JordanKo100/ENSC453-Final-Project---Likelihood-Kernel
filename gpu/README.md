# GPU Benchmark Workspace

This folder is a presentation and measurement helper for the final-project GPU section. It does not change the submission folders used for the FPGA deliverable.

Files:
- `likelihood_cpu_benchmark.cpp`: fixed-config CPU benchmark
- `likelihood_cuda_benchmark.cu`: fixed-config CUDA benchmark
- `likelihood_benchmark_common.h`: shared dataset generator and scalar reference
- `slide_notes.md`: slide-ready talking points and a comparison table template

## What the code measures

The kernel being benchmarked is the same likelihood update used by the FPGA work:

- one output likelihood per particle
- one loop over all mask points for each particle
- irregular gathers from `I[idx]`
- the same radius-5 mask generator used by the existing FPGA host

The optimized implementation uses the algebraic simplification already visible in the FPGA code:

`((pix - 100)^2 - (pix - 228)^2) / 50 = (256 * pix - 41984) / 50`

That turns the inner loop into:

- one precomputed particle base index
- one precomputed mask offset lookup
- one irregular image read
- one integer accumulation

## Build

CPU:

```sh
make cpu
```

GPU on the lab machine:

```sh
make gpu
```

Both benchmark files are intentionally compact now. Most settings live in the small constant block near the top of each file:

- [likelihood_cpu_benchmark.cpp](/Users/mukc/Documents/SFU COURSES/2026spring/ENSC453/proj_7/gpu/likelihood_cpu_benchmark.cpp)
- [likelihood_cuda_benchmark.cu](/Users/mukc/Documents/SFU COURSES/2026spring/ENSC453/proj_7/gpu/likelihood_cuda_benchmark.cu)

Common values to edit:
- `kParticles`
- `kMaxSize`
- `kWarmupIters`
- `kTimedIters`

CPU-only:
- `kThreads`

GPU-only:
- `kBlockSize`
- `kVerifyResult`

## Example runs

CPU:

```sh
./likelihood_cpu_benchmark
```

GPU:

```sh
./likelihood_cuda_benchmark
```

## Measurement rules for the slides

To match the rubric:

- use the same particle count and image size on CPU, GPU, and FPGA
- for the helpers here, change `kParticles` and `kMaxSize` in both
  [likelihood_cpu_benchmark.cpp](/Users/mukc/Documents/SFU COURSES/2026spring/ENSC453/proj_7/gpu/likelihood_cpu_benchmark.cpp) and
  [likelihood_cuda_benchmark.cu](/Users/mukc/Documents/SFU COURSES/2026spring/ENSC453/proj_7/gpu/likelihood_cuda_benchmark.cu)
- use the same logical dataset on FPGA
- report kernel execution time with the same scope on all three platforms
- keep one verification run in the appendix or backup slides

The current FPGA host in [`hw-run/host.cpp`](/Users/mukc/Documents/SFU COURSES/2026spring/ENSC453/proj_7/hw-run/host.cpp) still uses fixed constants, so either:

- collect CPU and GPU numbers with those defaults, or
- update the FPGA host to the same larger input before taking the final comparison table

## Suggested table

| Platform | Input size | Kernel time (ms) | Speedup vs CPU | Notes |
| --- | --- | ---: | ---: | --- |
| CPU (OpenMP) | `Nparticles = ...` |  | 1.00x | |
| GPU (CUDA) | `Nparticles = ...` |  |  | |
| FPGA (U50) | `Nparticles = ...` |  |  | |
