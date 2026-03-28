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

constexpr int kEmuParticles = 130;
constexpr int kIszY = 480;
constexpr int kNfr = 3;
constexpr int kFrameIndex = 1;
constexpr long kMaxSize = 1000000;

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

double checksum(const std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum;
}

int build_objxy_radius5(std::vector<double>& objxy) {
    const int radius = 5;
    const int diameter = radius * 2 - 1;
    const int center = radius - 1;

    int countOnes = 0;
    for (int x = 0; x < diameter; x++) {
        for (int y = 0; y < diameter; y++) {
            const double dx = static_cast<double>(x - center);
            const double dy = static_cast<double>(y - center);
            const double distance = std::sqrt(dx * dx + dy * dy);

            if (distance < static_cast<double>(radius)) {
                objxy[static_cast<std::size_t>(countOnes) * 2] = static_cast<double>(y - center);
                objxy[static_cast<std::size_t>(countOnes) * 2 + 1] = static_cast<double>(x - center);
                countOnes++;
            }
        }
    }

    return countOnes;
}

void init_particles_interior(std::vector<double>& arrayX,
                             std::vector<double>& arrayY) {
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        arrayX[i] = 80.0 + static_cast<double>(i % 23) * 1.75;
        arrayY[i] = 120.0 + static_cast<double>(i % 19) * 1.125;
    }
}

void init_particles_boundary(std::vector<double>& arrayX,
                             std::vector<double>& arrayY) {
    for (std::size_t i = 0; i < arrayX.size(); i++) {
        switch (i % 6) {
        case 0:
            arrayX[i] = -3.6;
            arrayY[i] = -1.8;
            break;
        case 1:
            arrayX[i] = 0.2;
            arrayY[i] = 479.7;
            break;
        case 2:
            arrayX[i] = 4800.0 + static_cast<double>(i);
            arrayY[i] = 470.4;
            break;
        case 3:
            arrayX[i] = 1.2;
            arrayY[i] = -4.7;
            break;
        case 4:
            arrayX[i] = 100000.0 + static_cast<double>(i * 11);
            arrayY[i] = 100000.0 - static_cast<double>(i * 7);
            break;
        default:
            arrayX[i] = 82.0 + static_cast<double>(i % 9) * 2.4;
            arrayY[i] = 118.0 + static_cast<double>(i % 11) * 1.6;
            break;
        }
    }
}

void init_image(std::vector<int, aligned_allocator<int>>& image) {
    for (std::size_t i = 0; i < image.size(); i++) {
        image[i] = 100 + static_cast<int>(i % 129);
    }
}

void compute_reference(const std::vector<double>& arrayX,
                       const std::vector<double>& arrayY,
                       const std::vector<double>& objxy,
                       const std::vector<int, aligned_allocator<int>>& image,
                       int countOnes,
                       std::vector<double>& likelihood_ref) {
    likelihood_ref.assign(arrayX.size(), 0.0);

    for (std::size_t x = 0; x < arrayX.size(); x++) {
        const int px = shared_roundDouble(arrayX[x]);
        const int py = shared_roundDouble(arrayY[x]);
        double sum = 0.0;

        for (int y = 0; y < countOnes; y++) {
            const int offY = shared_roundDouble(objxy[static_cast<std::size_t>(y) * 2]);
            const int offX = shared_roundDouble(objxy[static_cast<std::size_t>(y) * 2 + 1]);
            const int indX = px + offX;
            const int indY = py + offY;
            long idx = std::labs(static_cast<long>(indX) * kIszY * kNfr +
                                 static_cast<long>(indY) * kNfr + kFrameIndex);

            if (idx >= kMaxSize) {
                idx = 0;
            }

            const int pix = image[static_cast<std::size_t>(idx)];
            const int a = pix - static_cast<int>(PIXEL_A_OFFSET);
            const int b = pix - static_cast<int>(PIXEL_B_OFFSET);
            sum += (static_cast<double>(a * a) - static_cast<double>(b * b)) / PIXEL_DENOM;
        }

        likelihood_ref[x] = sum / static_cast<double>(countOnes);
    }
}

std::vector<unsigned char> read_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open xclbin: " + path);
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("xclbin is empty: " + path);
    }

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
    if (platforms.empty()) {
        throw std::runtime_error("No OpenCL platforms found");
    }

    for (const cl::Platform& platform : platforms) {
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if (!devices.empty()) {
            return devices.front();
        }
    }

    throw std::runtime_error("No OpenCL devices found");
}

OpenClSession open_session(const std::string& binary_file) {
    const cl::Device device = pick_device();

    cl_int err = CL_SUCCESS;
    cl::Context context(device, nullptr, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create OpenCL context");
    }

    cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create command queue");
    }

    const std::vector<unsigned char> file_buf = read_binary_file(binary_file);
    cl::Program::Binaries bins;
    bins.push_back({file_buf.data(), file_buf.size()});

    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to program device with xclbin");
    }

    cl::Kernel kernel(program, "likelihood_kernel", &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create likelihood_kernel");
    }

    std::cout << "[INFO] Using device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";
    return {context, queue, kernel};
}

bool run_case(OpenClSession& session,
              const char* label,
              bool boundary_case) {
    std::vector<double> arrayX(kEmuParticles, 0.0);
    std::vector<double> arrayY(kEmuParticles, 0.0);
    std::vector<double> objxy(MAX_COUNT_ONES * 2, 0.0);
    std::vector<double> likelihood_hw(kEmuParticles, 0.0);
    std::vector<double> likelihood_ref;
    std::vector<int, aligned_allocator<int>> image(kMaxSize);

    const int countOnes = build_objxy_radius5(objxy);
    if (countOnes <= 0 || countOnes > MAX_COUNT_ONES) {
        throw std::runtime_error("Invalid countOnes generated for radius-5 mask");
    }

    if (boundary_case) {
        init_particles_boundary(arrayX, arrayY);
    } else {
        init_particles_interior(arrayX, arrayY);
    }
    init_image(image);
    compute_reference(arrayX, arrayY, objxy, image, countOnes, likelihood_ref);

    const std::size_t particle_words =
        (arrayX.size() + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    const std::size_t objxy_words =
        (static_cast<std::size_t>(countOnes) * 2 + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    std::vector<wide_t, aligned_allocator<wide_t>> arrayX_wide(particle_words);
    std::vector<wide_t, aligned_allocator<wide_t>> arrayY_wide(particle_words);
    std::vector<wide_t, aligned_allocator<wide_t>> objxy_wide(objxy_words);
    std::vector<wide_t, aligned_allocator<wide_t>> likelihood_wide(particle_words);

    pack_doubles_to_wide(arrayX, arrayX_wide);
    pack_doubles_to_wide(arrayY, arrayY_wide);
    objxy.resize(static_cast<std::size_t>(countOnes) * 2);
    pack_doubles_to_wide(objxy, objxy_wide);

    cl_int err = CL_SUCCESS;
    cl::Buffer buf_arrayX(session.context,
                          CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                          sizeof(wide_t) * particle_words,
                          arrayX_wide.data(),
                          &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to allocate arrayX buffer");
    }

    cl::Buffer buf_arrayY(session.context,
                          CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                          sizeof(wide_t) * particle_words,
                          arrayY_wide.data(),
                          &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to allocate arrayY buffer");
    }

    cl::Buffer buf_objxy(session.context,
                         CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                         sizeof(wide_t) * objxy_words,
                         objxy_wide.data(),
                         &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to allocate objxy buffer");
    }

    cl::Buffer buf_image(session.context,
                         CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                         sizeof(int) * image.size(),
                         image.data(),
                         &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to allocate image buffer");
    }

    cl::Buffer buf_likelihood(session.context,
                              CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                              sizeof(wide_t) * particle_words,
                              likelihood_wide.data(),
                              &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to allocate likelihood buffer");
    }

    err = CL_SUCCESS;
    err |= session.kernel.setArg(0, kEmuParticles);
    err |= session.kernel.setArg(1, countOnes);
    err |= session.kernel.setArg(2, kIszY);
    err |= session.kernel.setArg(3, kNfr);
    err |= session.kernel.setArg(4, kFrameIndex);
    err |= session.kernel.setArg(5, kMaxSize);
    err |= session.kernel.setArg(6, buf_arrayX);
    err |= session.kernel.setArg(7, buf_arrayY);
    err |= session.kernel.setArg(8, buf_objxy);
    err |= session.kernel.setArg(9, buf_image);
    err |= session.kernel.setArg(10, buf_likelihood);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to set kernel arguments");
    }

    err = session.queue.enqueueMigrateMemObjects({buf_arrayX, buf_arrayY, buf_objxy, buf_image}, 0);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to migrate inputs");
    }
    err = session.queue.finish();
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to finish input migration");
    }

    const timespec kernel_start = tic();
    err = session.queue.enqueueTask(session.kernel);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to launch kernel");
    }
    err = session.queue.finish();
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to finish kernel execution");
    }
    const timespec kernel_end = tic();

    const timespec kernel_elapsed = diff(kernel_start, kernel_end);
    const double kernel_ms = static_cast<double>(kernel_elapsed.tv_sec) * 1.0e3 +
                             static_cast<double>(kernel_elapsed.tv_nsec) * 1.0e-6;

    err = session.queue.enqueueMigrateMemObjects({buf_likelihood}, CL_MIGRATE_MEM_OBJECT_HOST);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to migrate output");
    }
    err = session.queue.finish();
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to finish output migration");
    }

    unpack_wide_to_doubles(likelihood_wide, likelihood_hw);

    bool pass = true;
    double max_abs_err = 0.0;
    for (std::size_t i = 0; i < likelihood_hw.size(); i++) {
        const double err_abs = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err_abs > max_abs_err) {
            max_abs_err = err_abs;
        }
        if (err_abs > 1e-9) {
            pass = false;
            std::cout << "[" << label << "] mismatch at particle " << i
                      << ": hw=" << std::setprecision(15) << likelihood_hw[i]
                      << ", ref=" << likelihood_ref[i]
                      << ", abs_err=" << err_abs << "\n";
            break;
        }
    }

    std::cout << "[" << label << "] kernel time = " << std::fixed << std::setprecision(3)
              << kernel_ms << " ms, checksum = " << std::setprecision(15)
              << checksum(likelihood_hw)
              << ", max abs error = " << max_abs_err << "\n";

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
        pass &= run_case(session, "emu_interior", false);
        pass &= run_case(session, "emu_boundary", true);

        std::cout << (pass ? "TEST PASSED\n" : "TEST FAILED\n");
        return pass ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
