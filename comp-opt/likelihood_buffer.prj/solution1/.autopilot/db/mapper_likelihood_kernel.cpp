#include <systemc>
#include <vector>
#include <iostream>
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_fixed.h"
using namespace std;
using namespace sc_dt;
class AESL_RUNTIME_BC {
  public:
    AESL_RUNTIME_BC(const char* name) {
      file_token.open( name);
      if (!file_token.good()) {
        cout << "Failed to open tv file " << name << endl;
        exit (1);
      }
      file_token >> mName;//[[[runtime]]]
    }
    ~AESL_RUNTIME_BC() {
      file_token.close();
    }
    int read_size () {
      int size = 0;
      file_token >> mName;//[[transaction]]
      file_token >> mName;//transaction number
      file_token >> mName;//pop_size
      size = atoi(mName.c_str());
      file_token >> mName;//[[/transaction]]
      return size;
    }
  public:
    fstream file_token;
    string mName;
};
extern "C" void likelihood_kernel(long long*, long long*, long long*, int*, long long*, int, int, int, int, int, long long, int, int, int, int, int);
extern "C" void apatb_likelihood_kernel_hw(int __xlx_apatb_param_Nparticles, int __xlx_apatb_param_countOnes, int __xlx_apatb_param_IszY, int __xlx_apatb_param_Nfr, int __xlx_apatb_param_k, long long __xlx_apatb_param_max_size, volatile void * __xlx_apatb_param_arrayX, volatile void * __xlx_apatb_param_arrayY, volatile void * __xlx_apatb_param_objxy, volatile void * __xlx_apatb_param_I, volatile void * __xlx_apatb_param_likelihood) {
  // Collect __xlx_arrayX__tmp_vec
  vector<sc_bv<64> >__xlx_arrayX__tmp_vec;
  for (int j = 0, e = 1; j != e; ++j) {
    __xlx_arrayX__tmp_vec.push_back(((long long*)__xlx_apatb_param_arrayX)[j]);
  }
  int __xlx_size_param_arrayX = 1;
  int __xlx_offset_param_arrayX = 0;
  int __xlx_offset_byte_param_arrayX = 0*8;
  long long* __xlx_arrayX__input_buffer= new long long[__xlx_arrayX__tmp_vec.size()];
  for (int i = 0; i < __xlx_arrayX__tmp_vec.size(); ++i) {
    __xlx_arrayX__input_buffer[i] = __xlx_arrayX__tmp_vec[i].range(63, 0).to_uint64();
  }
  // Collect __xlx_arrayY__tmp_vec
  vector<sc_bv<64> >__xlx_arrayY__tmp_vec;
  for (int j = 0, e = 1; j != e; ++j) {
    __xlx_arrayY__tmp_vec.push_back(((long long*)__xlx_apatb_param_arrayY)[j]);
  }
  int __xlx_size_param_arrayY = 1;
  int __xlx_offset_param_arrayY = 0;
  int __xlx_offset_byte_param_arrayY = 0*8;
  long long* __xlx_arrayY__input_buffer= new long long[__xlx_arrayY__tmp_vec.size()];
  for (int i = 0; i < __xlx_arrayY__tmp_vec.size(); ++i) {
    __xlx_arrayY__input_buffer[i] = __xlx_arrayY__tmp_vec[i].range(63, 0).to_uint64();
  }
  // Collect __xlx_objxy__tmp_vec
  vector<sc_bv<64> >__xlx_objxy__tmp_vec;
  for (int j = 0, e = 1; j != e; ++j) {
    __xlx_objxy__tmp_vec.push_back(((long long*)__xlx_apatb_param_objxy)[j]);
  }
  int __xlx_size_param_objxy = 1;
  int __xlx_offset_param_objxy = 0;
  int __xlx_offset_byte_param_objxy = 0*8;
  long long* __xlx_objxy__input_buffer= new long long[__xlx_objxy__tmp_vec.size()];
  for (int i = 0; i < __xlx_objxy__tmp_vec.size(); ++i) {
    __xlx_objxy__input_buffer[i] = __xlx_objxy__tmp_vec[i].range(63, 0).to_uint64();
  }
  // Collect __xlx_I__tmp_vec
  vector<sc_bv<32> >__xlx_I__tmp_vec;
  for (int j = 0, e = 1; j != e; ++j) {
    __xlx_I__tmp_vec.push_back(((int*)__xlx_apatb_param_I)[j]);
  }
  int __xlx_size_param_I = 1;
  int __xlx_offset_param_I = 0;
  int __xlx_offset_byte_param_I = 0*4;
  int* __xlx_I__input_buffer= new int[__xlx_I__tmp_vec.size()];
  for (int i = 0; i < __xlx_I__tmp_vec.size(); ++i) {
    __xlx_I__input_buffer[i] = __xlx_I__tmp_vec[i].range(31, 0).to_uint64();
  }
  // Collect __xlx_likelihood__tmp_vec
  vector<sc_bv<64> >__xlx_likelihood__tmp_vec;
  for (int j = 0, e = 1; j != e; ++j) {
    __xlx_likelihood__tmp_vec.push_back(((long long*)__xlx_apatb_param_likelihood)[j]);
  }
  int __xlx_size_param_likelihood = 1;
  int __xlx_offset_param_likelihood = 0;
  int __xlx_offset_byte_param_likelihood = 0*8;
  long long* __xlx_likelihood__input_buffer= new long long[__xlx_likelihood__tmp_vec.size()];
  for (int i = 0; i < __xlx_likelihood__tmp_vec.size(); ++i) {
    __xlx_likelihood__input_buffer[i] = __xlx_likelihood__tmp_vec[i].range(63, 0).to_uint64();
  }
  // DUT call
  likelihood_kernel(__xlx_arrayX__input_buffer, __xlx_arrayY__input_buffer, __xlx_objxy__input_buffer, __xlx_I__input_buffer, __xlx_likelihood__input_buffer, __xlx_apatb_param_Nparticles, __xlx_apatb_param_countOnes, __xlx_apatb_param_IszY, __xlx_apatb_param_Nfr, __xlx_apatb_param_k, __xlx_apatb_param_max_size, __xlx_offset_byte_param_arrayX, __xlx_offset_byte_param_arrayY, __xlx_offset_byte_param_objxy, __xlx_offset_byte_param_I, __xlx_offset_byte_param_likelihood);
// print __xlx_apatb_param_arrayX
  sc_bv<64>*__xlx_arrayX_output_buffer = new sc_bv<64>[__xlx_size_param_arrayX];
  for (int i = 0; i < __xlx_size_param_arrayX; ++i) {
    __xlx_arrayX_output_buffer[i] = __xlx_arrayX__input_buffer[i+__xlx_offset_param_arrayX];
  }
  for (int i = 0; i < __xlx_size_param_arrayX; ++i) {
    ((long long*)__xlx_apatb_param_arrayX)[i] = __xlx_arrayX_output_buffer[i].to_uint64();
  }
// print __xlx_apatb_param_arrayY
  sc_bv<64>*__xlx_arrayY_output_buffer = new sc_bv<64>[__xlx_size_param_arrayY];
  for (int i = 0; i < __xlx_size_param_arrayY; ++i) {
    __xlx_arrayY_output_buffer[i] = __xlx_arrayY__input_buffer[i+__xlx_offset_param_arrayY];
  }
  for (int i = 0; i < __xlx_size_param_arrayY; ++i) {
    ((long long*)__xlx_apatb_param_arrayY)[i] = __xlx_arrayY_output_buffer[i].to_uint64();
  }
// print __xlx_apatb_param_objxy
  sc_bv<64>*__xlx_objxy_output_buffer = new sc_bv<64>[__xlx_size_param_objxy];
  for (int i = 0; i < __xlx_size_param_objxy; ++i) {
    __xlx_objxy_output_buffer[i] = __xlx_objxy__input_buffer[i+__xlx_offset_param_objxy];
  }
  for (int i = 0; i < __xlx_size_param_objxy; ++i) {
    ((long long*)__xlx_apatb_param_objxy)[i] = __xlx_objxy_output_buffer[i].to_uint64();
  }
// print __xlx_apatb_param_I
  sc_bv<32>*__xlx_I_output_buffer = new sc_bv<32>[__xlx_size_param_I];
  for (int i = 0; i < __xlx_size_param_I; ++i) {
    __xlx_I_output_buffer[i] = __xlx_I__input_buffer[i+__xlx_offset_param_I];
  }
  for (int i = 0; i < __xlx_size_param_I; ++i) {
    ((int*)__xlx_apatb_param_I)[i] = __xlx_I_output_buffer[i].to_uint64();
  }
// print __xlx_apatb_param_likelihood
  sc_bv<64>*__xlx_likelihood_output_buffer = new sc_bv<64>[__xlx_size_param_likelihood];
  for (int i = 0; i < __xlx_size_param_likelihood; ++i) {
    __xlx_likelihood_output_buffer[i] = __xlx_likelihood__input_buffer[i+__xlx_offset_param_likelihood];
  }
  for (int i = 0; i < __xlx_size_param_likelihood; ++i) {
    ((long long*)__xlx_apatb_param_likelihood)[i] = __xlx_likelihood_output_buffer[i].to_uint64();
  }
}
