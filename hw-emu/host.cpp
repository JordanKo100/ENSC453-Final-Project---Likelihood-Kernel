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

#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#include <CL/cl2.hpp>

#include "likelihood_kernel.h"
#include "my_timer.h"

// --- HW-EMU SCALED CONSTANTS ---
// Reduced to 100 to test both a full tile (64) and a residual tile (36)
#define GLOBAL_ISZX 512
#define GLOBAL_ISZY 512
#define GLOBAL_MAX_SIZE (GLOBAL_ISZX * GLOBAL_ISZY * 1) 

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

void pack_ints_to_wide(const std::vector<int>& in, 
                       std::vector<wide_t, aligned_allocator<wide_t>>& out) {
    for (std::size_t w = 0; w < out.size(); w++) {
        wide_t pack = 0;
        for (int i = 0; i < INTS_PER_WORD; i++) {
            const std::size_t idx = w * INTS_PER_WORD + static_cast<std::size_t>(i);
            const uint32_t val = (idx < in.size()) ? static_cast<uint32_t>(in[idx]) : 0;
            pack.range((i + 1) * INT_BITS - 1, i * INT_BITS) = val;
        }
        out[w] = pack;
    }
}

double randu(std::vector<int>& seed, int index) {
    long long num = (long long)PRNG_A * seed[index] + PRNG_C;
    seed[index] = static_cast<int>(num % PRNG_M);
    return std::fabs(seed[index] / static_cast<double>(PRNG_M));
}

double randn(std::vector<int>& seed, int index) {
    double u = randu(seed, index);
    double v = randu(seed, index);
    double cosine = std::cos(2.0 * M_PI * v);
    double rt = -2.0 * std::log(u);
    return std::sqrt(rt) * cosine;
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

void init_particles_interior(std::vector<double>& arrayX, std::vector<double>& arrayY) {
    std::vector<int> seed(arrayX.size());
    for (std::size_t i = 0; i < seed.size(); i++) {
        seed[i] = 1337 * (i + 1);
    }
    double center_x = GLOBAL_ISZY / 2.0;
    double center_y = GLOBAL_ISZX / 2.0;
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        arrayX[i] = center_x + 1.0 + 5.0 * randn(seed, i);
        arrayY[i] = center_y - 2.0 + 2.0 * randn(seed, i);
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

void pack_pixels_for_fpga(int Nparticles,
                          const std::vector<double>& arrayX,
                          const std::vector<double>& arrayY,
                          const std::vector<double>& objxy,
                          const std::vector<int>& I,
                          std::vector<int>& packed_I) {
    int write_idx = 0;
    for (int p = 0; p < Nparticles; p++) {
        int px = shared_roundDouble(arrayX[p]);
        int py = shared_roundDouble(arrayY[p]);

        for (int m = 0; m < ACTUAL_COUNT_ONES; m++) {
            int offY = shared_roundDouble(objxy[m * 2]);
            int offX = shared_roundDouble(objxy[m * 2 + 1]);
            long idx = std::labs(static_cast<long>(px + offX) * GLOBAL_ISZY * kNfr +
                                 static_cast<long>(py + offY) * kNfr + kFrameIndex);
            
            if (idx >= GLOBAL_MAX_SIZE) idx = 0; 
            packed_I[write_idx++] = I[idx];
        }

        for (int m = ACTUAL_COUNT_ONES; m < PADDED_COUNT_ONES; m++) {
            packed_I[write_idx++] = 0;
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

    std::cout << "\n======================================================\n";
    std::cout << "[INFO] FPGA Device Initialized: " << device.getInfo<CL_DEVICE_NAME>() << "\n";
    std::cout << "======================================================\n\n";
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
        init_particles_interior(arrayX, arrayY);
    }
    init_image(image);

    // Host Memory: Pack and Pad
    int total_pixels_padded = particle_count * PADDED_COUNT_ONES;
    std::vector<int> packed_I(total_pixels_padded, 0);
    
    pack_pixels_for_fpga(particle_count, arrayX, arrayY, objxy, image, packed_I);

    // Allocate FPGA Aligned Memory
    const std::size_t int_words = (total_pixels_padded + INTS_PER_WORD - 1) / INTS_PER_WORD;
    const std::size_t out_words = (particle_count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    
    std::vector<wide_t, aligned_allocator<wide_t>> packed_I_wide(int_words, 0);
    std::vector<wide_t, aligned_allocator<wide_t>> likelihood_wide(out_words, 0);

    pack_ints_to_wide(packed_I, packed_I_wide);

    // Compute CPU Reference
    std::vector<double> likelihood_ref;
    compute_reference(arrayX, arrayY, objxy, image, likelihood_ref);

    // OpenCL Setup
    cl_int err = CL_SUCCESS;
    cl::Buffer buf_packed_I(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, 
                            sizeof(wide_t) * int_words, packed_I_wide.data(), &err);
    cl::Buffer buf_likelihood(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 
                              sizeof(wide_t) * out_words, likelihood_wide.data(), &err);

    err = CL_SUCCESS;
    err |= session.kernel.setArg(0, particle_count);
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
    const double kernel_ms = static_cast<double>(kernel_elapsed.tv_sec) * 1.0e3 +
                             static_cast<double>(kernel_elapsed.tv_nsec) * 1.0e-6;

    err = session.queue.enqueueMigrateMemObjects({buf_likelihood}, CL_MIGRATE_MEM_OBJECT_HOST);
    err |= session.queue.finish();

    std::vector<double> likelihood_hw(static_cast<std::size_t>(particle_count), 0.0);
    unpack_wide_to_doubles(likelihood_wide, likelihood_hw);

    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-5;

    for (std::size_t i = 0; i < likelihood_hw.size(); i++) {
        const double err_abs = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err_abs > max_abs_err) max_abs_err = err_abs;
        if (err_abs > tol) {
            pass = false;
            std::cout << "[ERROR] " << label << " mismatch at particle " << i << "\n"
                      << "        HW = " << std::setprecision(15) << likelihood_hw[i] << "\n"
                      << "       REF = " << likelihood_ref[i] << "\n"
                      << "   ABS_ERR = " << err_abs << "\n";
            break;
        }
    }

    std::cout << ">>> TEST: " << label << " (" << particle_count << " particles)\n";
    std::cout << "    Compute Time  : " << std::fixed << std::setprecision(3) << kernel_ms << " ms\n";
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
        
        pass &= run_case(session, "Interior_Points", NUM_PARTICLES, false);
        pass &= run_case(session, "Boundary_Points", NUM_PARTICLES, true);

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