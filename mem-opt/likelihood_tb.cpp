#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "likelihood_kernel.h"

#define TB_ISZX 4000
#define TB_ISZY 4000
#define TB_NFR 1
#define TB_K 0
#define TB_MAX_SIZE 16000000

const long M = 2147483647; 
const int A = 1103515245;
const int C = 12345;

inline int tb_roundDouble(double value) {
    return static_cast<int>(value + ((value >= 0.0) ? 0.5 : -0.5));
}

inline double bits_to_double_tb(uint64_t bits) {
    union { uint64_t u; double d; } conv;
    conv.u = bits;
    return conv.d;
}

inline uint64_t double_to_bits_tb(double val) {
    union { uint64_t u; double d; } conv;
    conv.d = val;
    return conv.u;
}

// Packs doubles into 512-bit words (8 doubles per word)
void pack_doubles_to_wide(const std::vector<double>& in, std::vector<wide_t>& out, int count) {
    const int words = (count + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = 0;
        for (int d = 0; d < DOUBLES_PER_WORD; d++) {
            const int idx = w * DOUBLES_PER_WORD + d;
            const double val = (idx < count) ? in[idx] : 0.0;
            pack.range((d + 1) * DOUBLE_BITS - 1, d * DOUBLE_BITS) = double_to_bits_tb(val);
        }
        out[w] = pack;
    }
}

// Unpacks 512-bit words into doubles
void unpack_wide_to_doubles(const std::vector<wide_t>& in, std::vector<double>& out, int count) {
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

// NEW: Packs integers into 512-bit words (16 ints per word)
void pack_ints_to_wide(const std::vector<int>& in, std::vector<wide_t>& out, int count) {
    const int words = (count + INTS_PER_WORD - 1) / INTS_PER_WORD;
    for (int w = 0; w < words; w++) {
        wide_t pack = 0;
        for (int i = 0; i < INTS_PER_WORD; i++) {
            const int idx = w * INTS_PER_WORD + i;
            const uint32_t val = (idx < count) ? (uint32_t)in[idx] : 0;
            pack.range((i + 1) * INT_BITS - 1, i * INT_BITS) = val;
        }
        out[w] = pack;
    }
}

double randu(std::vector<int>& seed, int index) {
    long long num = (long long)A * seed[index] + C;
    seed[index] = num % M;
    return std::fabs(seed[index] / ((double)M));
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
            if (std::sqrt(x * x + y * y) < radius) {
                objxy[current_point * 2] = (double)y;
                objxy[current_point * 2 + 1] = (double)x;
                current_point++;
            }
        }
    }
}

// CPU Gather: Looks up scattered pixels and saves them to a standard std::vector
void pack_pixels_for_fpga(int Nparticles,
                          int IszY, int Nfr, int k, long max_size,
                          const std::vector<double>& arrayX,
                          const std::vector<double>& arrayY,
                          const std::vector<double>& objxy,
                          const std::vector<int>& I,
                          std::vector<int>& packed_I) {
    int write_idx = 0;
    for (int p = 0; p < Nparticles; p++) {
        int px = tb_roundDouble(arrayX[p]);
        int py = tb_roundDouble(arrayY[p]);

        // 1. Pack the actual 69 valid pixels
        for (int m = 0; m < ACTUAL_COUNT_ONES; m++) {
            int offY = tb_roundDouble(objxy[m * 2]);
            int offX = tb_roundDouble(objxy[m * 2 + 1]);
            long idx = std::abs((long)(px + offX) * IszY * Nfr + (long)(py + offY) * Nfr + k);
            if (idx >= max_size) idx = 0; 
            
            packed_I[write_idx++] = I[idx];
        }

        // 2. Pad the remaining slots with zeros up to 80 (PADDED_COUNT_ONES)
        for (int m = ACTUAL_COUNT_ONES; m < PADDED_COUNT_ONES; m++) {
            packed_I[write_idx++] = 0;
        }
    }
}

// CPU Validation
void compute_reference(int Nparticles,
                       int IszY, int Nfr, int k, long max_size,
                       const double* arrayX, const double* arrayY,
                       const double* objxy, const int* I,
                       double* likelihood_ref) {
    for (int x = 0; x < Nparticles; x++) {
        int px = tb_roundDouble(arrayX[x]);
        int py = tb_roundDouble(arrayY[x]);
        double sum = 0.0;

        // FIX 1: Only loop for the 69 valid offsets
        for (int y = 0; y < ACTUAL_COUNT_ONES; y++) {
            int offY = tb_roundDouble(objxy[y * 2]);
            int offX = tb_roundDouble(objxy[y * 2 + 1]);
            long idx = std::abs((long)(px + offX) * IszY * Nfr + (long)(py + offY) * Nfr + k);
            if (idx >= max_size) idx = 0;

            int pix = I[idx];
            int a = pix - 100;
            int b = pix - 228;
            sum += ((double)(a * a) - (double)(b * b)) / 50.0;
        }
        
        // FIX 2: Divide by the actual count (69), not the padded count (80)
        likelihood_ref[x] = sum / (double)ACTUAL_COUNT_ONES;
    }
}

void init_particles(std::vector<double>& arrayX, std::vector<double>& arrayY, int Nparticles) {
    std::vector<int> seed(Nparticles);
    for (int i = 0; i < Nparticles; i++) {
        seed[i] = 1337 * (i + 1);
    }
    
    // SCATTERED WORKLOAD: Distribute uniformly across the entire 4000x4000 image
    for (int i = 0; i < Nparticles; i++) {
        arrayX[i] = randu(seed, i) * static_cast<double>(TB_ISZY);
        arrayY[i] = randu(seed, i) * static_cast<double>(TB_ISZX);
    }
}

bool run_case(const char* label,
              int Nparticles,
              long max_size,
              const double* objxy,
              const int* I) 
{
    std::vector<double> arrayX(Nparticles, 0.0);
    std::vector<double> arrayY(Nparticles, 0.0);
    std::vector<double> likelihood_hw(Nparticles, 0.0);
    std::vector<double> likelihood_ref(Nparticles, 0.0);

    // FIX: Use PADDED_COUNT_ONES for memory allocation sizing
    int total_pixels_padded = Nparticles * PADDED_COUNT_ONES;
    std::vector<int> packed_I(total_pixels_padded, 0);

    // Wide array allocations
    int num_wide_doubles = (Nparticles + DOUBLES_PER_WORD - 1) / DOUBLES_PER_WORD;
    // FIX: Use the padded total for calculating wide int words
    int num_wide_ints = (total_pixels_padded + INTS_PER_WORD - 1) / INTS_PER_WORD;
    std::vector<wide_t> likelihood_wide(num_wide_doubles, 0);
    std::vector<wide_t> packed_I_wide(num_wide_ints, 0);

    init_particles(arrayX, arrayY, Nparticles);

    // 1. Gather scattered reads (now includes zero padding)
    pack_pixels_for_fpga(Nparticles, TB_ISZY, TB_NFR, TB_K, max_size,
                         arrayX, arrayY, std::vector<double>(objxy, objxy + ACTUAL_COUNT_ONES * 2),
                         std::vector<int>(I, I + max_size), packed_I);

    // 2. Pack 32-bit ints into 512-bit words
    pack_ints_to_wide(packed_I, packed_I_wide, total_pixels_padded);

    // CPU Reference Calculation
    compute_reference(Nparticles, TB_ISZY, TB_NFR, TB_K, max_size,
                      arrayX.data(), arrayY.data(), objxy, I, likelihood_ref.data());

    auto start = std::chrono::high_resolution_clock::now();

    // The Ultimate Pipeline Call
    likelihood_kernel(Nparticles, packed_I_wide.data(), likelihood_wide.data());

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    unpack_wide_to_doubles(likelihood_wide, likelihood_hw, Nparticles);

    bool pass = true;
    double max_abs_err = 0.0;
    const double tol = 1e-9;

    for (int i = 0; i < Nparticles; i++) {
        double err = std::fabs(likelihood_hw[i] - likelihood_ref[i]);
        if (err > max_abs_err) { max_abs_err = err; }
        if (err > tol) {
            pass = false;
            std::cout << label << " mismatch at particle " << i
                      << ": HW = " << std::setprecision(12) << likelihood_hw[i]
                      << ", REF = " << likelihood_ref[i]
                      << ", ABS_ERR = " << err << "\n";
            break;
        }
    }

    std::cout << label << "  Nparticles = " << Nparticles
              << "  elapsed = " << elapsed.count() << " s"
              << "  max_abs