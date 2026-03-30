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

namespace {

// Constant tracking values
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
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = bits;
    return conv.d;
}

inline uint64_t double_to_bits(double value) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.d = value;
    return conv.u;
}

void pack_doubles_to_wide(const std::vector<double>& in,
                          std::vector<wide_t, aligned_allocator<wide_t>>& out) {
    for (std::size_t w = 0; w < out.size(); w++) {
        wide_t pack = 0;
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const std::size_t idx = w * DOUBLES_PER_WORD + static_cast<std::size_t>(d);
            const double value = (idx < in.size()) ? in[idx] : 0.0;
            pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = double_to_bits(value);
        }
        out[w] = pack;
    }
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

int build_objxy_square(std::vector<double, aligned_allocator<double>>& objxy) {
    const int center = (GLOBAL_MASK_LENGTH - 1) / 2;
    int countOnes = GLOBAL_MASK_LENGTH * GLOBAL_MASK_LENGTH;
    int current_point = 0;
    for (int x = 0; x < GLOBAL_MASK_LENGTH; x++) {
        for (int y = 0; y < GLOBAL_MASK_LENGTH; y++) {
            objxy[current_point * 2] = static_cast<double>(y - center);
            objxy[current_point * 2 + 1] = static_cast<double>(x - center);
            current_point++;
        }
    }
    return countOnes;
}

void init_particles_interior(std::vector<double>& arrayX,
                             std::vector<double>& arrayY) {
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        arrayX[i] = 80.0 + static_cast<double>(i % 64) * 1.125;
        arrayY[i] = 120.0 + static_cast<double>(i % 32) * 0.875;
    }
}

void init_image(std::vector<int, aligned_allocator<int>>& image) {
    for (std::size_t i = 0; i < image.size(); i++) {
        image[i] = 100 + static_cast<int>(i % 129);
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

void run_hw_only(OpenClSession& session, int particle_count) {
    std::cout << ">>> Initializing data for Benchtest (" << particle_count << " particles)...\n";
    
    std::vector<double> arrayX(static_cast<std::size_t>(particle_count), 0.0);
    std::vector<double> arrayY(static_cast<std::size_t>(particle_count), 0.0);
    std::vector<double, aligned_allocator<double>> objxy(MAX_COUNT_ONES * 2, 0.0);
    std::vector<int, aligned_allocator<int>> image(GLOBAL_MAX_SIZE);

    const int countOnes = build_objxy_square(objxy);
    if (countOnes <= 0 || countOnes > MAX_COUNT_ONES) {
        throw std::runtime_error("Invalid countOnes generated for mask");
    }

    init_particles_interior(arrayX, arrayY);
    init_image(image);

    const std::size_t particle_words = (arrayX.size() + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    std::vector<wide_t, aligned_allocator<wide_t>> arrayX_wide(particle_words);
    std::vector<wide_t, aligned_allocator<wide_t>> arrayY_wide(particle_words);
    std::vector<wide_t, aligned_allocator<wide_t>> likelihood_wide(particle_words);

    pack_doubles_to_wide(arrayX, arrayX_wide);
    pack_doubles_to_wide(arrayY, arrayY_wide);

    cl_int err = CL_SUCCESS;
    cl::Buffer buf_arrayX(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, sizeof(wide_t) * particle_words, arrayX_wide.data(), &err);
    cl::Buffer buf_arrayY(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, sizeof(wide_t) * particle_words, arrayY_wide.data(), &err);
    cl::Buffer buf_objxy(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, sizeof(double) * objxy.size(), objxy.data(), &err);
    cl::Buffer buf_image(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, sizeof(int) * image.size(), image.data(), &err);
    cl::Buffer buf_likelihood(session.context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, sizeof(wide_t) * particle_words, likelihood_wide.data(), &err);

    err = CL_SUCCESS;
    err |= session.kernel.setArg(0, particle_count);
    err |= session.kernel.setArg(1, countOnes);
    err |= session.kernel.setArg(2, GLOBAL_ISZY);
    err |= session.kernel.setArg(3, kNfr);
    err |= session.kernel.setArg(4, kFrameIndex);
    err |= session.kernel.setArg(5, (cl_long)GLOBAL_MAX_SIZE);
    err |= session.kernel.setArg(6, buf_arrayX);
    err |= session.kernel.setArg(7, buf_arrayY);
    err |= session.kernel.setArg(8, buf_objxy);
    err |= session.kernel.setArg(9, buf_image);
    err |= session.kernel.setArg(10, buf_likelihood);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to set kernel arguments");

    err = session.queue.enqueueMigrateMemObjects({buf_arrayX, buf_arrayY, buf_objxy, buf_image}, 0);
    err |= session.queue.finish();
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to migrate inputs to FPGA");

    std::cout << ">>> Executing FPGA Kernel...\n";
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

    double total_likelihood = 0.0;
    for (double val : likelihood_hw) {
        total_likelihood += val;
    }

    std::cout << "------------------------------------------------------\n";
    std::cout << "    Compute Time     : " << std::fixed << std::setprecision(3) << kernel_ms << " ms\n";
    std::cout << "    Total Likelihood : " << std::setprecision(15) << total_likelihood << "\n";
    std::cout << "------------------------------------------------------\n\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin_path>\n";
        return EXIT_FAILURE;
    }

    try {
        OpenClSession session = open_session(argv[1]);
        
        run_hw_only(session, GLOBAL_NUM_PARTICLES);

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}