# GPU Section Notes

## Slide 1: What the GPU kernel computes

- This project accelerates the likelihood stage of a particle-filter style benchmark.
- Each particle produces one likelihood value.
- For each particle, the kernel visits every point in the radius-5 mask and gathers one pixel from the image array `I`.
- Computational complexity is `O(Nparticles * countOnes)`.
- In this project, `countOnes = 69` for the radius-5 mask, while `Nparticles` is the main scaling dimension.

Useful equation for the slide:

`likelihood[x] = (1 / countOnes) * sum_y (((I[idx] - 100)^2 - (I[idx] - 228)^2) / 50)`

## Slide 2: CUDA mapping and code understanding

- One CUDA thread handles one particle.
- The `arrayX` and `arrayY` loads are naturally coalesced because adjacent threads read adjacent particle positions.
- The mask offsets are tiny and reused by every thread, so they are placed in constant memory.
- The expensive repeated terms are hoisted out of the inner loop:
  - round particle position once
  - build `particleBase` once
  - precompute `objOffsets[y]`
- The main bottleneck is the irregular `I[idx]` gather, not the arithmetic.

Short code explanation:

- Host precomputes `objOffsets[y] = offX * IszY * Nfr + offY * Nfr`
- Thread computes `particleBase = round(x) * IszY * Nfr + round(y) * Nfr + k`
- Inner loop uses `idx = abs(particleBase + objOffsets[y])`
- Result is accumulated with the simplified formula `scale * pixelSum - bias`

## Slide 3: GPU finetuning and optimization

Present these as the main tuning decisions:

1. Algebraic simplification
- Replace two subtractions, two multiplies, one subtraction, and one divide per mask point with a linear expression in `pix`.
- This reduces arithmetic cost and exposes the kernel as memory-bound.

2. Precompute offsets
- Move mask rounding and address-offset construction out of the kernel.
- Reduces per-thread work and removes repeated integer math from the inner loop.

3. Constant memory for the mask
- `countOnes <= 80`, so the offsets fit easily in constant memory.
- This is better than rereading the same mask values from global memory in every thread.

4. Launch configuration
- Start with `blockSize = 256`.
- Sweep `128`, `256`, and `512` on the lab machine.
- Choose the best value by measured kernel time, not by guess.

5. Measurement discipline
- Warm up the kernel before timing.
- Use CUDA events for kernel-only timing.
- Keep the timing scope consistent with the CPU and FPGA measurements.

## Slide 4: Why CPU, GPU, and FPGA differ

Use this explanation framework:

- CPU:
  - Good control flow and cache hierarchy
  - Limited thread-level parallelism compared with GPU
  - Usually best baseline for small inputs or quick validation

- GPU:
  - Massive thread-level parallelism hides memory latency
  - Coalesced particle reads are efficient
  - Irregular image gathers still hurt, but the GPU can keep many warps in flight
  - Usually strongest for large `Nparticles`

- FPGA:
  - Very efficient pipelined execution with explicit buffering and overlap
  - Strong control over data movement and on-chip storage
  - Lower clock rate and less flexible random-access bandwidth than a high-end GPU
  - End-to-end results can also be affected by host-device transfer overhead

## Slide 5: Comparison table template

Use the same large input on all three platforms.

| Platform | `Nparticles` | `countOnes` | Kernel time (ms) | Speedup vs CPU | Main reason |
| --- | ---: | ---: | ---: | ---: | --- |
| CPU (OpenMP) |  |  |  | 1.00x | |
| GPU (CUDA) |  |  |  |  | |
| FPGA (U50) |  |  |  |  | |

## Slide 6: FPGA context you can reuse

These are already in your repo and help explain why the FPGA result improved before the final CPU/GPU/FPGA comparison:

| FPGA stage | Worst-case latency (cycles) | Improvement |
| --- | ---: | ---: |
| `buffer` | 382751 | baseline |
| `comp-opt` | 262113 | 1.46x vs `buffer` |
| `mem-opt` | 260730 | 1.005x vs `comp-opt` |
| `ping-pong` | 219549 | 1.19x vs `mem-opt` |

Talking point:

- The biggest FPGA gain came from compute-side parallelization.
- Memory widening alone helped only slightly because the dominant cost is still the irregular image gather.
- Ping-pong/dataflow helped again by overlapping load, compute, and store.

Important:

- Use the actual `hw-run` timing from the lab machine in the final comparison table.
- Do not use HLS cycle estimates as the final FPGA comparison number.

## Slide 7: Interpretation script

If GPU is fastest:
- The kernel is dominated by many independent particles and irregular memory reads.
- GPU wins because it hides latency with many resident warps and high memory bandwidth.

If FPGA is close to GPU:
- The FPGA pipeline, local buffering, and overlapped load-compute-store stages removed most control overhead.
- The remaining limitation is off-chip random image access and host transfer cost.

If FPGA beats CPU but not GPU:
- This is still a strong result for this access pattern.
- The design extracted spatial parallelism, but the GPU still has an advantage on random-memory-heavy workloads.

## Slide 8: What to say if asked about fairness

- Same algorithmic output on all three platforms
- Same mask generator and same image/particle initialization
- Same large input size
- Same timing scope across the table
- Verification run performed before reporting timing
