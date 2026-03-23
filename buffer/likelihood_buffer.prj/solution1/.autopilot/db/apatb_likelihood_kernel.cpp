#include <systemc>
#include <iostream>
#include <cstdlib>
#include <cstddef>
#include <stdint.h>
#include "SysCFileHandler.h"
#include "ap_int.h"
#include "ap_fixed.h"
#include <complex>
#include <stdbool.h>
#include "autopilot_cbe.h"
#include "hls_stream.h"
#include "hls_half.h"
#include "hls_signal_handler.h"

using namespace std;
using namespace sc_core;
using namespace sc_dt;

// wrapc file define:
#define AUTOTB_TVIN_gmem0 "../tv/cdatafile/c.likelihood_kernel.autotvin_gmem0.dat"
#define AUTOTB_TVOUT_gmem0 "../tv/cdatafile/c.likelihood_kernel.autotvout_gmem0.dat"
// wrapc file define:
#define AUTOTB_TVIN_gmem1 "../tv/cdatafile/c.likelihood_kernel.autotvin_gmem1.dat"
#define AUTOTB_TVOUT_gmem1 "../tv/cdatafile/c.likelihood_kernel.autotvout_gmem1.dat"
// wrapc file define:
#define AUTOTB_TVIN_gmem2 "../tv/cdatafile/c.likelihood_kernel.autotvin_gmem2.dat"
#define AUTOTB_TVOUT_gmem2 "../tv/cdatafile/c.likelihood_kernel.autotvout_gmem2.dat"
// wrapc file define:
#define AUTOTB_TVIN_gmem3 "../tv/cdatafile/c.likelihood_kernel.autotvin_gmem3.dat"
#define AUTOTB_TVOUT_gmem3 "../tv/cdatafile/c.likelihood_kernel.autotvout_gmem3.dat"
// wrapc file define:
#define AUTOTB_TVIN_gmem4 "../tv/cdatafile/c.likelihood_kernel.autotvin_gmem4.dat"
#define AUTOTB_TVOUT_gmem4 "../tv/cdatafile/c.likelihood_kernel.autotvout_gmem4.dat"
// wrapc file define:
#define AUTOTB_TVIN_Nparticles "../tv/cdatafile/c.likelihood_kernel.autotvin_Nparticles.dat"
#define AUTOTB_TVOUT_Nparticles "../tv/cdatafile/c.likelihood_kernel.autotvout_Nparticles.dat"
// wrapc file define:
#define AUTOTB_TVIN_countOnes "../tv/cdatafile/c.likelihood_kernel.autotvin_countOnes.dat"
#define AUTOTB_TVOUT_countOnes "../tv/cdatafile/c.likelihood_kernel.autotvout_countOnes.dat"
// wrapc file define:
#define AUTOTB_TVIN_IszY "../tv/cdatafile/c.likelihood_kernel.autotvin_IszY.dat"
#define AUTOTB_TVOUT_IszY "../tv/cdatafile/c.likelihood_kernel.autotvout_IszY.dat"
// wrapc file define:
#define AUTOTB_TVIN_Nfr "../tv/cdatafile/c.likelihood_kernel.autotvin_Nfr.dat"
#define AUTOTB_TVOUT_Nfr "../tv/cdatafile/c.likelihood_kernel.autotvout_Nfr.dat"
// wrapc file define:
#define AUTOTB_TVIN_k "../tv/cdatafile/c.likelihood_kernel.autotvin_k.dat"
#define AUTOTB_TVOUT_k "../tv/cdatafile/c.likelihood_kernel.autotvout_k.dat"
// wrapc file define:
#define AUTOTB_TVIN_max_size "../tv/cdatafile/c.likelihood_kernel.autotvin_max_size.dat"
#define AUTOTB_TVOUT_max_size "../tv/cdatafile/c.likelihood_kernel.autotvout_max_size.dat"
// wrapc file define:
#define AUTOTB_TVIN_arrayX "../tv/cdatafile/c.likelihood_kernel.autotvin_arrayX.dat"
#define AUTOTB_TVOUT_arrayX "../tv/cdatafile/c.likelihood_kernel.autotvout_arrayX.dat"
// wrapc file define:
#define AUTOTB_TVIN_arrayY "../tv/cdatafile/c.likelihood_kernel.autotvin_arrayY.dat"
#define AUTOTB_TVOUT_arrayY "../tv/cdatafile/c.likelihood_kernel.autotvout_arrayY.dat"
// wrapc file define:
#define AUTOTB_TVIN_objxy "../tv/cdatafile/c.likelihood_kernel.autotvin_objxy.dat"
#define AUTOTB_TVOUT_objxy "../tv/cdatafile/c.likelihood_kernel.autotvout_objxy.dat"
// wrapc file define:
#define AUTOTB_TVIN_I "../tv/cdatafile/c.likelihood_kernel.autotvin_I.dat"
#define AUTOTB_TVOUT_I "../tv/cdatafile/c.likelihood_kernel.autotvout_I.dat"
// wrapc file define:
#define AUTOTB_TVIN_likelihood "../tv/cdatafile/c.likelihood_kernel.autotvin_likelihood.dat"
#define AUTOTB_TVOUT_likelihood "../tv/cdatafile/c.likelihood_kernel.autotvout_likelihood.dat"

#define INTER_TCL "../tv/cdatafile/ref.tcl"

// tvout file define:
#define AUTOTB_TVOUT_PC_gmem0 "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_gmem0.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_gmem1 "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_gmem1.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_gmem2 "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_gmem2.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_gmem3 "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_gmem3.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_gmem4 "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_gmem4.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_Nparticles "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_Nparticles.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_countOnes "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_countOnes.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_IszY "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_IszY.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_Nfr "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_Nfr.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_k "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_k.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_max_size "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_max_size.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_arrayX "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_arrayX.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_arrayY "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_arrayY.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_objxy "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_objxy.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_I "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_I.dat"
// tvout file define:
#define AUTOTB_TVOUT_PC_likelihood "../tv/rtldatafile/rtl.likelihood_kernel.autotvout_likelihood.dat"

class INTER_TCL_FILE {
  public:
INTER_TCL_FILE(const char* name) {
  mName = name; 
  gmem0_depth = 0;
  gmem1_depth = 0;
  gmem2_depth = 0;
  gmem3_depth = 0;
  gmem4_depth = 0;
  Nparticles_depth = 0;
  countOnes_depth = 0;
  IszY_depth = 0;
  Nfr_depth = 0;
  k_depth = 0;
  max_size_depth = 0;
  arrayX_depth = 0;
  arrayY_depth = 0;
  objxy_depth = 0;
  I_depth = 0;
  likelihood_depth = 0;
  trans_num =0;
}
~INTER_TCL_FILE() {
  mFile.open(mName);
  if (!mFile.good()) {
    cout << "Failed to open file ref.tcl" << endl;
    exit (1); 
  }
  string total_list = get_depth_list();
  mFile << "set depth_list {\n";
  mFile << total_list;
  mFile << "}\n";
  mFile << "set trans_num "<<trans_num<<endl;
  mFile.close();
}
string get_depth_list () {
  stringstream total_list;
  total_list << "{gmem0 " << gmem0_depth << "}\n";
  total_list << "{gmem1 " << gmem1_depth << "}\n";
  total_list << "{gmem2 " << gmem2_depth << "}\n";
  total_list << "{gmem3 " << gmem3_depth << "}\n";
  total_list << "{gmem4 " << gmem4_depth << "}\n";
  total_list << "{Nparticles " << Nparticles_depth << "}\n";
  total_list << "{countOnes " << countOnes_depth << "}\n";
  total_list << "{IszY " << IszY_depth << "}\n";
  total_list << "{Nfr " << Nfr_depth << "}\n";
  total_list << "{k " << k_depth << "}\n";
  total_list << "{max_size " << max_size_depth << "}\n";
  total_list << "{arrayX " << arrayX_depth << "}\n";
  total_list << "{arrayY " << arrayY_depth << "}\n";
  total_list << "{objxy " << objxy_depth << "}\n";
  total_list << "{I " << I_depth << "}\n";
  total_list << "{likelihood " << likelihood_depth << "}\n";
  return total_list.str();
}
void set_num (int num , int* class_num) {
  (*class_num) = (*class_num) > num ? (*class_num) : num;
}
void set_string(std::string list, std::string* class_list) {
  (*class_list) = list;
}
  public:
    int gmem0_depth;
    int gmem1_depth;
    int gmem2_depth;
    int gmem3_depth;
    int gmem4_depth;
    int Nparticles_depth;
    int countOnes_depth;
    int IszY_depth;
    int Nfr_depth;
    int k_depth;
    int max_size_depth;
    int arrayX_depth;
    int arrayY_depth;
    int objxy_depth;
    int I_depth;
    int likelihood_depth;
    int trans_num;
  private:
    ofstream mFile;
    const char* mName;
};

static void RTLOutputCheckAndReplacement(std::string &AESL_token, std::string PortName) {
  bool no_x = false;
  bool err = false;

  no_x = false;
  // search and replace 'X' with '0' from the 3rd char of token
  while (!no_x) {
    size_t x_found = AESL_token.find('X', 0);
    if (x_found != string::npos) {
      if (!err) { 
        cerr << "WARNING: [SIM 212-201] RTL produces unknown value 'X' on port" 
             << PortName << ", possible cause: There are uninitialized variables in the C design."
             << endl; 
        err = true;
      }
      AESL_token.replace(x_found, 1, "0");
    } else
      no_x = true;
  }
  no_x = false;
  // search and replace 'x' with '0' from the 3rd char of token
  while (!no_x) {
    size_t x_found = AESL_token.find('x', 2);
    if (x_found != string::npos) {
      if (!err) { 
        cerr << "WARNING: [SIM 212-201] RTL produces unknown value 'x' on port" 
             << PortName << ", possible cause: There are uninitialized variables in the C design."
             << endl; 
        err = true;
      }
      AESL_token.replace(x_found, 1, "0");
    } else
      no_x = true;
  }
}
extern "C" void likelihood_kernel_hw_stub_wrapper(int, int, int, int, int, long long, volatile void *, volatile void *, volatile void *, volatile void *, volatile void *);

extern "C" void apatb_likelihood_kernel_hw(int __xlx_apatb_param_Nparticles, int __xlx_apatb_param_countOnes, int __xlx_apatb_param_IszY, int __xlx_apatb_param_Nfr, int __xlx_apatb_param_k, long long __xlx_apatb_param_max_size, volatile void * __xlx_apatb_param_arrayX, volatile void * __xlx_apatb_param_arrayY, volatile void * __xlx_apatb_param_objxy, volatile void * __xlx_apatb_param_I, volatile void * __xlx_apatb_param_likelihood) {
  refine_signal_handler();
  fstream wrapc_switch_file_token;
  wrapc_switch_file_token.open(".hls_cosim_wrapc_switch.log");
  int AESL_i;
  if (wrapc_switch_file_token.good())
  {

    CodeState = ENTER_WRAPC_PC;
    static unsigned AESL_transaction_pc = 0;
    string AESL_token;
    string AESL_num;{
      static ifstream rtl_tv_out_file;
      if (!rtl_tv_out_file.is_open()) {
        rtl_tv_out_file.open(AUTOTB_TVOUT_PC_gmem4);
        if (rtl_tv_out_file.good()) {
          rtl_tv_out_file >> AESL_token;
          if (AESL_token != "[[[runtime]]]")
            exit(1);
        }
      }
  
      if (rtl_tv_out_file.good()) {
        rtl_tv_out_file >> AESL_token; 
        rtl_tv_out_file >> AESL_num;  // transaction number
        if (AESL_token != "[[transaction]]") {
          cerr << "Unexpected token: " << AESL_token << endl;
          exit(1);
        }
        if (atoi(AESL_num.c_str()) == AESL_transaction_pc) {
          std::vector<sc_bv<64> > gmem4_pc_buffer(1);
          int i = 0;

          rtl_tv_out_file >> AESL_token; //data
          while (AESL_token != "[[/transaction]]"){

            RTLOutputCheckAndReplacement(AESL_token, "gmem4");
  
            // push token into output port buffer
            if (AESL_token != "") {
              gmem4_pc_buffer[i] = AESL_token.c_str();;
              i++;
            }
  
            rtl_tv_out_file >> AESL_token; //data or [[/transaction]]
            if (AESL_token == "[[[/runtime]]]" || rtl_tv_out_file.eof())
              exit(1);
          }
          if (i > 0) {{
            int i = 0;
            for (int j = 0, e = 1; j < e; j += 1, ++i) {
            ((long long*)__xlx_apatb_param_likelihood)[j] = gmem4_pc_buffer[i].to_int64();
          }}}
        } // end transaction
      } // end file is good
    } // end post check logic bolck
  
    AESL_transaction_pc++;
    return ;
  }
static unsigned AESL_transaction;
static AESL_FILE_HANDLER aesl_fh;
static INTER_TCL_FILE tcl_file(INTER_TCL);
std::vector<char> __xlx_sprintf_buffer(1024);
CodeState = ENTER_WRAPC;
//gmem0
aesl_fh.touch(AUTOTB_TVIN_gmem0);
aesl_fh.touch(AUTOTB_TVOUT_gmem0);
//gmem1
aesl_fh.touch(AUTOTB_TVIN_gmem1);
aesl_fh.touch(AUTOTB_TVOUT_gmem1);
//gmem2
aesl_fh.touch(AUTOTB_TVIN_gmem2);
aesl_fh.touch(AUTOTB_TVOUT_gmem2);
//gmem3
aesl_fh.touch(AUTOTB_TVIN_gmem3);
aesl_fh.touch(AUTOTB_TVOUT_gmem3);
//gmem4
aesl_fh.touch(AUTOTB_TVIN_gmem4);
aesl_fh.touch(AUTOTB_TVOUT_gmem4);
//Nparticles
aesl_fh.touch(AUTOTB_TVIN_Nparticles);
aesl_fh.touch(AUTOTB_TVOUT_Nparticles);
//countOnes
aesl_fh.touch(AUTOTB_TVIN_countOnes);
aesl_fh.touch(AUTOTB_TVOUT_countOnes);
//IszY
aesl_fh.touch(AUTOTB_TVIN_IszY);
aesl_fh.touch(AUTOTB_TVOUT_IszY);
//Nfr
aesl_fh.touch(AUTOTB_TVIN_Nfr);
aesl_fh.touch(AUTOTB_TVOUT_Nfr);
//k
aesl_fh.touch(AUTOTB_TVIN_k);
aesl_fh.touch(AUTOTB_TVOUT_k);
//max_size
aesl_fh.touch(AUTOTB_TVIN_max_size);
aesl_fh.touch(AUTOTB_TVOUT_max_size);
//arrayX
aesl_fh.touch(AUTOTB_TVIN_arrayX);
aesl_fh.touch(AUTOTB_TVOUT_arrayX);
//arrayY
aesl_fh.touch(AUTOTB_TVIN_arrayY);
aesl_fh.touch(AUTOTB_TVOUT_arrayY);
//objxy
aesl_fh.touch(AUTOTB_TVIN_objxy);
aesl_fh.touch(AUTOTB_TVOUT_objxy);
//I
aesl_fh.touch(AUTOTB_TVIN_I);
aesl_fh.touch(AUTOTB_TVOUT_I);
//likelihood
aesl_fh.touch(AUTOTB_TVIN_likelihood);
aesl_fh.touch(AUTOTB_TVOUT_likelihood);
CodeState = DUMP_INPUTS;
unsigned __xlx_offset_byte_param_arrayX = 0;
// print gmem0 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_gmem0, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_arrayX = 0*8;
  if (__xlx_apatb_param_arrayX) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<64> __xlx_tmp_lv = ((long long*)__xlx_apatb_param_arrayX)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_gmem0, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem0_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_gmem0, __xlx_sprintf_buffer.data());
}
unsigned __xlx_offset_byte_param_arrayY = 0;
// print gmem1 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_gmem1, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_arrayY = 0*8;
  if (__xlx_apatb_param_arrayY) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<64> __xlx_tmp_lv = ((long long*)__xlx_apatb_param_arrayY)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_gmem1, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem1_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_gmem1, __xlx_sprintf_buffer.data());
}
unsigned __xlx_offset_byte_param_objxy = 0;
// print gmem2 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_gmem2, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_objxy = 0*8;
  if (__xlx_apatb_param_objxy) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<64> __xlx_tmp_lv = ((long long*)__xlx_apatb_param_objxy)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_gmem2, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem2_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_gmem2, __xlx_sprintf_buffer.data());
}
unsigned __xlx_offset_byte_param_I = 0;
// print gmem3 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_gmem3, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_I = 0*4;
  if (__xlx_apatb_param_I) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<32> __xlx_tmp_lv = ((int*)__xlx_apatb_param_I)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_gmem3, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem3_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_gmem3, __xlx_sprintf_buffer.data());
}
unsigned __xlx_offset_byte_param_likelihood = 0;
// print gmem4 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_gmem4, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_likelihood = 0*8;
  if (__xlx_apatb_param_likelihood) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<64> __xlx_tmp_lv = ((long long*)__xlx_apatb_param_likelihood)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_gmem4, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem4_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_gmem4, __xlx_sprintf_buffer.data());
}
// print Nparticles Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_Nparticles, __xlx_sprintf_buffer.data());
  {
    sc_bv<32> __xlx_tmp_lv = *((int*)&__xlx_apatb_param_Nparticles);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_Nparticles, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.Nparticles_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_Nparticles, __xlx_sprintf_buffer.data());
}
// print countOnes Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_countOnes, __xlx_sprintf_buffer.data());
  {
    sc_bv<32> __xlx_tmp_lv = *((int*)&__xlx_apatb_param_countOnes);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_countOnes, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.countOnes_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_countOnes, __xlx_sprintf_buffer.data());
}
// print IszY Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_IszY, __xlx_sprintf_buffer.data());
  {
    sc_bv<32> __xlx_tmp_lv = *((int*)&__xlx_apatb_param_IszY);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_IszY, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.IszY_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_IszY, __xlx_sprintf_buffer.data());
}
// print Nfr Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_Nfr, __xlx_sprintf_buffer.data());
  {
    sc_bv<32> __xlx_tmp_lv = *((int*)&__xlx_apatb_param_Nfr);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_Nfr, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.Nfr_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_Nfr, __xlx_sprintf_buffer.data());
}
// print k Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_k, __xlx_sprintf_buffer.data());
  {
    sc_bv<32> __xlx_tmp_lv = *((int*)&__xlx_apatb_param_k);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_k, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.k_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_k, __xlx_sprintf_buffer.data());
}
// print max_size Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_max_size, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = *((long long*)&__xlx_apatb_param_max_size);

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_max_size, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.max_size_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_max_size, __xlx_sprintf_buffer.data());
}
// print arrayX Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_arrayX, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = __xlx_offset_byte_param_arrayX;

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_arrayX, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.arrayX_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_arrayX, __xlx_sprintf_buffer.data());
}
// print arrayY Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_arrayY, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = __xlx_offset_byte_param_arrayY;

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_arrayY, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.arrayY_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_arrayY, __xlx_sprintf_buffer.data());
}
// print objxy Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_objxy, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = __xlx_offset_byte_param_objxy;

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_objxy, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.objxy_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_objxy, __xlx_sprintf_buffer.data());
}
// print I Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_I, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = __xlx_offset_byte_param_I;

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_I, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.I_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_I, __xlx_sprintf_buffer.data());
}
// print likelihood Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVIN_likelihood, __xlx_sprintf_buffer.data());
  {
    sc_bv<64> __xlx_tmp_lv = __xlx_offset_byte_param_likelihood;

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVIN_likelihood, __xlx_sprintf_buffer.data()); 
  }
  tcl_file.set_num(1, &tcl_file.likelihood_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVIN_likelihood, __xlx_sprintf_buffer.data());
}
CodeState = CALL_C_DUT;
likelihood_kernel_hw_stub_wrapper(__xlx_apatb_param_Nparticles, __xlx_apatb_param_countOnes, __xlx_apatb_param_IszY, __xlx_apatb_param_Nfr, __xlx_apatb_param_k, __xlx_apatb_param_max_size, __xlx_apatb_param_arrayX, __xlx_apatb_param_arrayY, __xlx_apatb_param_objxy, __xlx_apatb_param_I, __xlx_apatb_param_likelihood);
CodeState = DUMP_OUTPUTS;
// print gmem4 Transactions
{
  sprintf(__xlx_sprintf_buffer.data(), "[[transaction]] %d\n", AESL_transaction);
  aesl_fh.write(AUTOTB_TVOUT_gmem4, __xlx_sprintf_buffer.data());
  {  __xlx_offset_byte_param_likelihood = 0*8;
  if (__xlx_apatb_param_likelihood) {
    for (int j = 0  - 0, e = 1 - 0; j != e; ++j) {
sc_bv<64> __xlx_tmp_lv = ((long long*)__xlx_apatb_param_likelihood)[j];

    sprintf(__xlx_sprintf_buffer.data(), "%s\n", __xlx_tmp_lv.to_string(SC_HEX).c_str());
    aesl_fh.write(AUTOTB_TVOUT_gmem4, __xlx_sprintf_buffer.data()); 
      }
  }
}
  tcl_file.set_num(1, &tcl_file.gmem4_depth);
  sprintf(__xlx_sprintf_buffer.data(), "[[/transaction]] \n");
  aesl_fh.write(AUTOTB_TVOUT_gmem4, __xlx_sprintf_buffer.data());
}
CodeState = DELETE_CHAR_BUFFERS;
AESL_transaction++;
tcl_file.set_num(AESL_transaction , &tcl_file.trans_num);
}
