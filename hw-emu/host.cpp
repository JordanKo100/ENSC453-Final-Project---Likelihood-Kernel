#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#include <CL/cl2.hpp>

// Your header drives the sizing dynamically!
#include "likelihood_kernel.h"
#include "my_timer.h"

// --- HW-EMU DYNAMIC CONSTANTS ---
#define GLOBAL_ISZX 512
#define GLOBAL_ISZY 512
#define GLOBAL_MAX_SIZE (GLOBAL_ISZX * GLOBAL_ISZY * 1) 

// The host chunk MUST be larger than N_BUFFER_SIZE to force the 
// hardware's Tile_loop to iterate and trigger Ping-Pong overlap.
#define EMULATION_CHUNK_SIZE (N_BUFFER_SIZE * 4) 

const long PRNG_M = 2147483647;
const int PRNG_A = 1103515245;
const int PRNG_C = 12345;

namespace {

constexpr int kNfr = 1;
constexpr int kFrameIndex = 0;

template <typename T>
struct aligned_allocator {
    using value_type = T;
    T* allocate(std::size_t num) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, 4096, num * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        return reinterpret_cast<T*>(ptr);
    }
    void deallocate(T* ptr, std::size_t) {
        free(ptr);
    }
};

struct OpenClSession {
    cl::Context context;
    cl::CommandQueue queue;
    cl::Kernel kernel;
};

inline double bits_to_double(uint64_t bits) {
    union { uint64_t u; double d; } conv;
    conv.u = bits;
    return conv.d;
}

inline int shared_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

void unpack_wide_to_doubles(const std::vector<wide_t, aligned_allocator<wide_t>>& in,
                            std::vector<double>& out) {
    for (std::size_t w = 0; w < in.size(); w++) {
        const wide_t pack = in[w];
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const std::size_t idx = w * DOUBLES_PER_WORD + static_cast<std::size_t>(d);
            if (idx < out.size()) {
                const uint64_t bits = static_cast<uint64_t>(
                    pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS));
                out[idx] = bits_to_double(bits);
            }
        }
    }
}

double randu(std::vector<int>& seed, int index) {
    long long num = (long long)PRNG_A * seed[index] + PRNG_C;
    seed[index] = static_cast<int>(num % PRNG_M);
    return std::fabs(seed[index] / static_cast<double>(PRNG_M));
}

void build_objxy_disk(std::vector<double>& objxy) {
    int radius = 5;
    int current_point = 0;
    for (int x = -radius + 1; x < radius; x++) {
        for (int y = -radius + 1; y < radius; y++) {
            double distance = std::sqrt(x * x + y * y);
            if (distance < radius) {
                if (current_point < ACTUAL_COUNT_ONES) {
                    objxy[current_point * 2] = static_cast<double>(y);
                    objxy[current_point * 2 + 1] = static_cast<double>(x);
                    current_point++;
                }
            }
        }
    }
}

void init_particles(std::vector<double>& arrayX, std::vector<double>& arrayY) {
    std::vector<int> seed(arrayX.size());
    for (std::size_t i = 0; i < seed.size(); i++) {
        seed[i] = 1337 * (i + 1);
    }

    for (std::size_t i = 0; i < arrayX.size(); i++) {
        arrayX[i] = randu(seed, static_cast<int>(i)) * static_cast<double>(GLOBAL_ISZY);
        arrayY[i] = randu(seed, static_cast<int>(i)) * static_cast<double>(GLOBAL_ISZX);
    }
}

void init_particles_boundary(std::vector<double>& arrayX, std::vector<double>& arrayY) {
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        switch (i % 5) {
        case 0: arrayX[i] = -15.0; arrayY[i] = -15.0; break;               
        case 1: arrayX[i] = GLOBAL_ISZX + 50.0; arrayY[i] = 256.0; break;  
        case 2: arrayX[i] = 256.0; arrayY[i] = GLOBAL_ISZY + 50.0; break;  
        case 3: arrayX[i] = GLOBAL_ISZX + 100.0; arrayY[i] = GLOBAL_ISZY + 100.0; break;
        case 4: arrayX[i] = 256.0; arrayY[i] = 256.0; break;
        }
    }
}

void init_image(std::vector<int>& image) {
    for (std::size_t i = 0; i < image.size(); i++) {
        image[i] = 100 + static_cast<int>(i % 129);
    }
}

void gather_and_pack_pixels_wide_chunk(int base_particle, 
                                       int chunk_particles,
                                       const std::vector<double>& arrayX,
                                       const std::vector<double>& arrayY,
                                       const std::vector<double>& objxy,
                                       const std::vector<int>& I,
                                       std::vector<wide_t, aligned_allocator<wide_t>>& packed_I_wide) {
    
    const int WORDS_PER_PARTICLE = PADDED_COUNT_ONES / INTS_PER_WORD; 

    for (int p = 0; p < chunk_particles; p++) {
        int global_p = base_particle + p; 
        int px = shared_roundDouble(arrayX[global_p]);
        int py = shared_roundDouble(arrayY[global_p]);

        for (int w = 0; w < WORDS_PER_PARTICLE; w++) {
            wide_t pack = 0;
            for (int i = 0; i < INTS_PER_WORD; i++) {
                int m = w * INTS_PER_WORD + i;
                uint32_t val = 0;

                if (m < ACTUAL_COUNT_ONES) { 
                    int offY = shared_roundDouble(objxy[m * 2]);
                    int offX = shared_roundDouble(objxy[m * 2 + 1]);
                    long idx = std::labs(static_cast<long>(px + offX) * GLOBAL_ISZY * kNfr +
                                         static_cast<long>(py + offY) * kNfr + kFrameIndex);
                    if (idx < GLOBAL_MAX_SIZE) {
                        val = static_cast<uint32_t>(I[idx]);
                    }
                }
                pack.range((i + 1) * INT_BITS - 1, i * INT_BITS) = static_cast<uint64_t>(val);
            }
            packed_I_wide[p * WORDS_PER_PARTICLE + w] = pack;
        }
    }
}

void compute_reference(const std::vector<double>& arrayX,
                       const std::vector<double>& arrayY,
                       const std::vector<double>& objxy,
                       const std::vector<int>& image,
                       std::vector<double>& likelihood_ref) {
    
    likelihood_ref.assign(arrayX.size(), 0.0);
    for (std::size_t x = 0; x < arrayX.size(); x++) {
        const int px = shared_roundDouble(arrayX[x]);
        const int py = shared_roundDouble(arrayY[x]);
        double sum = 0.0;

        for (int y = 0; y < ACTUAL_COUNT_ONES; y++) {
            const int offY = shared_roundDouble(objxy[static_cast<std::size_t>(y) * 2]);
            const int offX = shared_roundDouble(objxy[static_cast<std::size_t>(y) * 2 + 1]);
            const int indX = px + offX;
            const int indY = py + offY;
            long idx = std::labs(static_cast<long>(indX) * GLOBAL_ISZY * kNfr +
                                 static_cast<long>(indY) * kNfr + kFrameIndex);
            if (idx >= GLOBAL_MAX_SIZE) { idx = 0; }

            const int pix = image[static_cast<std::size_t>(idx)];
            const int a = pix - 100;
            const int b = pix - 228;
            sum += (static_cast<double>(a * a) - static_cast<double>(b * b)) / 50.0;
        }
        likelihood_ref[x] = sum / static_cast<double>(ACTUAL_COUNT_ONES);
    }
}

std::vector<unsigned char> read_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Failed to open xclbin: " + path);
    const std::streamsize size = file.tellg();
    if (size <= 0) throw std::runtime_error("xclbin is empty: " + path);
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        throw std::runtime_error("Failed to read xclbin: " + path);
    }
    return data;
}

cl::Device pick_device() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) throw std::runtime_error("No OpenCL platforms found");
    for (const cl::Platform& platform : platforms) {
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if (!devices.empty()) return devices.front();
    }
    throw std::runtime_error("No OpenCL devices found");
}

OpenClSession open_session(const std::string& binary_file) {
    const cl::Device device = pick_device();
    cl_int err = CL_SUCCESS;
    cl::Context context(device, nullptr, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create OpenCL context");
    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create command queue");
    const std::vector<unsigned char> file_buf = read_binary_file(binary_file);
    cl::Program::Binaries bins;
    bins.push_back({file_buf.data(), file_buf.size()});

    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to program device with xclbin");

    cl::Kernel kernel(program, "likelihood_kernel", &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create likelihood_kernel");

    return {context, queue, kernel};
}

bool run_case(OpenClSession& session, const char* label, int particle_count, bool boundary_case) {
    std::vector<double> arrayX(static_cast<std::size_t>(particle_count), 0.0);
    std::vector<double> arrayY(static_cast<std::size_t>(particle_count), 0.0);
    std::vector<double> objxy(ACTUAL_COUNT_ONES * 2, 0.0);
    std::vector<int> image(GLOBAL_MAX_SIZE);

    build_objxy_disk(objxy);
    if (boundary_case) {
        init_particles_boundary(arrayX, arrayY);
    } else {
        init_particles(arrayX, arrayY);
    }
    init_image(image);

    // Compute CPU Reference
    std::vector<double> likelihood_ref;
    compute_reference(arrayX, arrayY, objxy, image, likelihood_ref);
    
    std::vector<double> likelihood_hw_total(static_cast<std::size_t>(particle_count), 0.0);
    double total_kernel_ms = 0.0;

    std::cout << ">>> Running [" << label << "] Total Particles: " << particle_count << "\n";

    // Host loops using EMULATION_CHUNK_SIZE
    for (int base = 0; base < particle_count; base += EMULATION_CHUNK_SIZE) {
        const int chunk_particles = std::min(EMULATION_CHUNK_SIZE, particle_count - base);

        const std::size_t int_words = (chunk_particles * PADDED_COUNT_ONES) / INTS_PER_WORD;
        const std::size_t out_words = (chunk_particles + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

        std::vector<wide_t, aligned_allocator<wide_t>> packed_I_wide(int_words, 0);
        std::vector<wide_t, aligned_allocator<wide_t>> likelihood_wide(out_words, 0);
        std::vector<double> likelihood_hw_chunk(chunk_particles, 0.0);

        gather_and_pack_pixels_wide_chunk(base, chunk_particles, arrayX, arrayY, objxy, image, packed_I_wide);

        cl_int err = CL_SUCCESS;
        cl::Buffer buf_packed_I(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, 
                                sizeof(wide_t) * int_words, packed_I_wide.data(), &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create buf_packed_I chunk");

        cl::Buffer buf_likelihood(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 
                                  sizeof(wide_t) * out_words, likelihood_wide.data(), &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create buf_likelihood chunk");

        err = CL_SUCCESS;
        err |= session.kernel.setArg(0, chunk_particles);
        err |= session.kernel.setArg(1, buf_packed_I);
        err |= session.kernel.setArg(2, buf_likelihood);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to set kernel arguments");

        err = session.queue.enqueueMigrateMemObjects({buf_packed_I}, 0);
        err |= session.queue.finish();

        const timespec kernel_start = tic();
        err = session.queue.enqueueTask(session.kernel);
        err |= session.queue.finish();
        const timespec kernel_end = tic();

        if (err != CL_SUCCESS) throw std::runtime_error("Failed to execute kernel");

        const timespec kernel_elapsed = diff(kernel_start, kernel_end);
        total_kernel_ms += static_cast<double>(kernel_elapsed.tv_sec) * 1.0e3 +
                           static_cast<double>(kernel_elapsed.tv_nsec) * 1.0e-6;

        err = session.queue.enqueueMigrateMemObjects({buf_likelihood}, CL_MIGRATE_MEM_OBJECT_HOST);
        err |= session.queue.finish();

        unpack_wide_to_doubles(likelihood_wide, likelihood_hw_chunk);
        
        for (int p = 0; p < chunk_particles; p++) {
            likelihood_hw_total[base + p] = likelihood_hw_chunk[p];
        }
    }

    // Validation
    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-5;

    for (std::size_t i = 0; i < likelihood_hw_total.size(); i++) {
        const double err_abs = std::fabs(likelihood_hw_total[i] - likelihood_ref[i]);
        if (err_abs > max_abs_err) max_abs_err = err_abs;
        if (err_abs > tol) {
            pass = false;
            std::cout << "[ERROR] mismatch at particle " << i << "\n"
                      << "        HW = " << std::setprecision(15) << likelihood_hw_total[i] << "\n"
                      << "       REF = " << likelihood_ref[i] << "\n"
                      << "   ABS_ERR = " << err_abs << "\n";
            break;
        }
    }

    std::cout << "    Compute Time  : " << std::fixed << std::setprecision(3) << total_kernel_ms << " ms\n";
    std::cout << "    Max Abs Error : " << std::setprecision(15) << max_abs_err << "\n";
    std::cout << "    Status        : " << (pass ? "PASS" : "FAIL") << "\n\n";

    return pass;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin_path>\n";
        return EXIT_FAILURE;
    }

    try {
        OpenClSession session = open_session(argv[1]);
        bool pass = true;
        
        std::cout << "\n======================================================\n";
        std::cout << "  STARTING SCALABLE HARDWARE EMULATION TESTS\n";
        std::cout << "  Hardware N_BUFFER_SIZE  : " << N_BUFFER_SIZE << "\n";
        std::cout << "  Host Chunking Threshold : " << EMULATION_CHUNK_SIZE << "\n";
        std::cout << "======================================================\n\n";

        // TEST 1: Exact Alignment
        // Tests exactly 4 full tiles (e.g., 512 particles if buffer is 128). 
        // Proves standard dataflow loop overlap works without buffer overruns.
        pass &= run_case(session, "1. Exact_Aligned_Tiles", N_BUFFER_SIZE * 4, false);

        // TEST 2: Unaligned Tail + Boundaries
        // Tests 4 full tiles, plus 1 partial tile (e.g., 554 particles). 
        // Uses the boundary coordinates to test out-of-bounds zero padding.
        pass &= run_case(session, "2. Unaligned_Tail_with_Boundary", (N_BUFFER_SIZE * 4) + (N_BUFFER_SIZE / 3), true);

        // TEST 3: Multi-Chunk Stress Test
        // Tests a massive amount that exceeds the Host's EMULATION_CHUNK_SIZE.
        // Proves that multiple host transfers and kernel restarts work cleanly.
        pass &= run_case(session, "3. Multi_Chunk_Stress_Test", (EMULATION_CHUNK_SIZE * 2) + (N_BUFFER_SIZE / 2) + 17, false);

        if (pass) {
            std::cout << "======================================================\n";
            std::cout << "  ALL HARDWARE EMULATION TESTS PASSED \n";
            std::cout << "======================================================\n";
            return EXIT_SUCCESS;
        } else {
            std::cout << "======================================================\n";
            std::cout << "  HARDWARE EMULATION FAILED \n";
            std::cout << "======================================================\n";
            return EXIT_FAILURE;
        }

    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}