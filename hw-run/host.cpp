#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include <fstream>

// Vitis OpenCL Headers
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#include <CL/cl2.hpp>

#include "likelihood_kernel.h"

#define TB_MAX_NPARTICLES 10000
#define TB_ISZY 1080
#define TB_NFR 1
#define TB_K 0
#define TB_MAX_SIZE 2073600 // 1920 * 1080 grayscale frame

// -------------------------------------------------------------------------
// Vitis Aligned Allocator (Required for Zero-Copy / CL_MEM_USE_HOST_PTR)
// -------------------------------------------------------------------------
template <typename T>
struct aligned_allocator {
    using value_type = T;
    T* allocate(std::size_t num) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, 4096, num * sizeof(T))) throw std::bad_alloc();
        return reinterpret_cast<T*>(ptr);
    }
    void deallocate(T* p, std::size_t num) { free(p); }
};

// -------------------------------------------------------------------------
// Original Testbench Helpers
// -------------------------------------------------------------------------
inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline double bits_to_double_tb(uint64_t bits) {
    union { uint64_t u; double d; } conv;
    conv.u = bits; return conv.d;
}

inline uint64_t double_to_bits_tb(double val) {
    union { uint64_t u; double d; } conv;
    conv.d = val; return conv.u;
}

void pack_doubles_to_wide(const double* in, wide_t* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = 0;
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const int idx = w * DOUBLES_PER_WORD + d;
            const double val = (idx < count) ? in[idx] : 0.0;
            const uint64_t bits = double_to_bits_tb(val);
            pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = bits;
        }
        out[w] = pack;
    }
}

void unpack_wide_to_doubles(const wide_t* in, double* out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = in[w];
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const int idx = w * DOUBLES_PER_WORD + d;
            if (idx < count) {
                const uint64_t bits = (uint64_t)pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS);
                out[idx] = bits_to_double_tb(bits);
            }
        }
    }
}

int build_objxy_radius5(double* objxy) {
    const int radius = 5;
    const int diameter = radius * 2 - 1;
    const int center = radius - 1;
    int countOnes = 0;
    for (int x = 0; x < diameter; x++) {
        for (int y = 0; y < diameter; y++) {
            double distance = std::sqrt(std::pow((double)(x - center), 2.0) + std::pow((double)(y - center), 2.0));
            if (distance < radius) {
                objxy[countOnes * 2] = (double)(y - center);
                objxy[countOnes * 2 + 1] = (double)(x - center);
                countOnes++;
            }
        }
    }
    return countOnes;
}

void compute_reference(int Nparticles, int countOnes, int IszY, int Nfr, int k, long max_size,
                       const double* arrayX, const double* arrayY, const double* objxy,
                       const int* I, double* likelihood_ref) {
    for (int x = 0; x < Nparticles; x++) {
        int px = tb_roundDouble(arrayX[x]);
        int py = tb_roundDouble(arrayY[x]);
        double sum = 0.0;
        for (int y = 0; y < countOnes; y++) {
            int offY = tb_roundDouble(objxy[y * 2]);
            int offX = tb_roundDouble(objxy[y * 2 + 1]);
            int indX = px + offX;
            int indY = py + offY;
            long idx = std::labs((long)indX * (long)IszY * (long)Nfr + (long)indY * (long)Nfr + (long)k);
            if (idx >= max_size) idx = 0;
            int pix = I[idx];
            int a = pix - 100;
            int b = pix - 228;
            sum += ((double)(a * a) - (double)(b * b)) / 50.0;
        }
        likelihood_ref[x] = sum / (double)countOnes;
    }
}

void init_particles_interior(double* arrayX, double* arrayY, int Nparticles) {
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = 80.0 + (i % 23) * 1.75;
        arrayY[i] = 120.0 + (i % 19) * 1.125;
    }
}

void init_particles_boundary(double* arrayX, double* arrayY, int Nparticles) {
    for (int i = 0; i < Nparticles; i++) {
        switch (i % 6) {
            case 0: arrayX[i] = -3.6; arrayY[i] = -1.8; break;
            case 1: arrayX[i] = 0.2; arrayY[i] = 479.7; break;
            case 2: arrayX[i] = 4800.0 + (double)i; arrayY[i] = 470.4; break;
            case 3: arrayX[i] = 1.2; arrayY[i] = -4.7; break;
            case 4: arrayX[i] = 100000.0 + (double)(i * 11); arrayY[i] = 100000.0 - (double)(i * 7); break;
            default: arrayX[i] = 82.0 + (i % 9) * 2.4; arrayY[i] = 118.0 + (i % 11) * 1.6; break;
        }
    }
}

// -------------------------------------------------------------------------
// Refactored OpenCL Run Case
// -------------------------------------------------------------------------
bool run_case(cl::Context& context, cl::CommandQueue& q, cl::Kernel& kernel,
              const char* label, int Nparticles, bool boundary_case, long max_size,
              const double* objxy, int countOnes, const int* I_src) 
{
    // Local host arrays for reference
    std::vector<double> arrayX(TB_MAX_NPARTICLES, 0.0);
    std::vector<double> arrayY(TB_MAX_NPARTICLES, 0.0);
    std::vector<double> likelihood_hw(TB_MAX_NPARTICLES, 0.0);
    std::vector<double> likelihood_ref(TB_MAX_NPARTICLES, 0.0);

    if (boundary_case) init_particles_boundary(arrayX.data(), arrayY.data(), Nparticles);
    else init_particles_interior(arrayX.data(), arrayY.data(), Nparticles);

    compute_reference(Nparticles, countOnes, TB_ISZY, TB_NFR, TB_K, max_size,
                      arrayX.data(), arrayY.data(), objxy, I_src, likelihood_ref.data());

    // Allocate 4K-aligned memory for OpenCL zero-copy buffers
    int wide_particles = (TB_MAX_NPARTICLES + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    int wide_objxy = (MAX_COUNT_ONES * 2 + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;

    std::vector<wide_t, aligned_allocator<wide_t>> arrayX_wide(wide_particles, 0);
    std::vector<wide_t, aligned_allocator<wide_t>> arrayY_wide(wide_particles, 0);
    std::vector<wide_t, aligned_allocator<wide_t>> objxy_wide(wide_objxy, 0);
    std::vector<wide_t, aligned_allocator<wide_t>> likelihood_wide(wide_particles, 0);
    std::vector<int, aligned_allocator<int>> I_wide(TB_MAX_SIZE);

    // Pack data into aligned buffers
    pack_doubles_to_wide(arrayX.data(), arrayX_wide.data(), Nparticles);
    pack_doubles_to_wide(arrayY.data(), arrayY_wide.data(), Nparticles);
    pack_doubles_to_wide(objxy, objxy_wide.data(), countOnes * 2);
    for (int i = 0; i < TB_MAX_SIZE; i++) I_wide[i] = I_src[i];

    // Create OpenCL Buffers mapping to the aligned host memory
    cl::Buffer buf_arrayX(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, wide_particles * sizeof(wide_t), arrayX_wide.data());
    cl::Buffer buf_arrayY(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, wide_particles * sizeof(wide_t), arrayY_wide.data());
    cl::Buffer buf_objxy(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, wide_objxy * sizeof(wide_t), objxy_wide.data());
    cl::Buffer buf_I(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, TB_MAX_SIZE * sizeof(int), I_wide.data());
    cl::Buffer buf_likelihood(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, wide_particles * sizeof(wide_t), likelihood_wide.data());

    // Set Kernel Arguments
    kernel.setArg(0, Nparticles);
    kernel.setArg(1, countOnes);
    kernel.setArg(2, TB_ISZY);
    kernel.setArg(3, TB_NFR);
    kernel.setArg(4, TB_K);
    kernel.setArg(5, max_size);
    kernel.setArg(6, buf_arrayX);
    kernel.setArg(7, buf_arrayY);
    kernel.setArg(8, buf_objxy);
    kernel.setArg(9, buf_I);
    kernel.setArg(10, buf_likelihood);

    auto start = std::chrono::high_resolution_clock::now();

    // Migrate Input Data to FPGA, Execute, and Migrate Output Data back
    q.enqueueMigrateMemObjects({buf_arrayX, buf_arrayY, buf_objxy, buf_I}, 0);
    q.enqueueTask(kernel);
    q.enqueueMigrateMemObjects({buf_likelihood}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Unpack results and verify
    unpack_wide_to_doubles(likelihood_wide.data(), likelihood_hw.data(), Nparticles);

    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-9;

    for (int i = 0; i < Nparticles; i++) {
        double err = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err > max_abs_err) max_abs_err = err;
        if (err > tol) {
            pass = false;
            std::cout << label << " mismatch at particle " << i
                      << ": HW = " << std::setprecision(12) << likelihood_hw[i]
                      << ", REF = " << likelihood_ref[i]
                      << ", ABS_ERR = " << err << "\n";
            break;
        }
    }

    std::cout << label << "  Nparticles = " << Nparticles << "  elapsed = " << elapsed.count() 
              << " s  max_abs_err = " << std::setprecision(12) << max_abs_err << "\n";

    return pass;
}

// -------------------------------------------------------------------------
// Main Host Application
// -------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <XCLBIN File>\n";
        return EXIT_FAILURE;
    }

    std::string xclbinFilename = argv[1];

    // OpenCL Setup
    std::vector<cl::Device> devices;
    cl::Platform::get(&devices);
    cl::Device device = devices.front();
    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);

    // Load XCLBIN
    std::cout << "Loading: '" << xclbinFilename << "'\n";
    std::ifstream bin_file(xclbinFilename, std::ifstream::binary);
    bin_file.seekg(0, bin_file.end);
    unsigned nb = bin_file.tellg();
    bin_file.seekg(0, bin_file.beg);
    char *buf = new char[nb];
    bin_file.read(buf, nb);
    
    cl::Program::Binaries bins{{buf, nb}};
    cl::Program program(context, {device}, bins);
    cl::Kernel kernel(program, "likelihood_kernel");

    // Static Setup Data
    std::vector<double> objxy(MAX_COUNT_ONES * 2);
    std::vector<int> I(TB_MAX_SIZE);

    int countOnes = build_objxy_radius5(objxy.data());
    if (countOnes > MAX_COUNT_ONES) {
        std::cerr << "ERROR: countOnes = " << countOnes << " exceeds " << MAX_COUNT_ONES << "\n";
        return EXIT_FAILURE;
    }

    for (long i = 0; i < TB_MAX_SIZE; i++) I[i] = 100 + (int)(i % 129);

    // Execute Test Cases
    bool pass = true;
    pass &= run_case(context, q, kernel, "single_tile_exact", 64, false, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case(context, q, kernel, "multi_tile_exact", 128, false, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case(context, q, kernel, "multi_tile_tail", 73, false, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case(context, q, kernel, "small_case", 3, false, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case(context, q, kernel, "boundary_case", 65, true, TB_MAX_SIZE, objxy.data(), countOnes, I.data());
    pass &= run_case(context, q, kernel, "clamp_case", 65, true, 37, objxy.data(), countOnes, I.data());

    delete[] buf;

    if (pass) {
        std::cout << "TEST PASSED\n";
        return EXIT_SUCCESS;
    }

    std::cout << "TEST FAILED\n";
    return EXIT_FAILURE;
}