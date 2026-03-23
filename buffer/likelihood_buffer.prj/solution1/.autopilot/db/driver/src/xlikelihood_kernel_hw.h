// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - enable ap_done interrupt (Read/Write)
//        bit 1  - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - ap_done (COR/TOW)
//        bit 1  - ap_ready (COR/TOW)
//        others - reserved
// 0x10 : Data signal of Nparticles
//        bit 31~0 - Nparticles[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of countOnes
//        bit 31~0 - countOnes[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of IszY
//        bit 31~0 - IszY[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of Nfr
//        bit 31~0 - Nfr[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of k
//        bit 31~0 - k[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of max_size
//        bit 31~0 - max_size[31:0] (Read/Write)
// 0x3c : Data signal of max_size
//        bit 31~0 - max_size[63:32] (Read/Write)
// 0x40 : reserved
// 0x44 : Data signal of arrayX
//        bit 31~0 - arrayX[31:0] (Read/Write)
// 0x48 : Data signal of arrayX
//        bit 31~0 - arrayX[63:32] (Read/Write)
// 0x4c : reserved
// 0x50 : Data signal of arrayY
//        bit 31~0 - arrayY[31:0] (Read/Write)
// 0x54 : Data signal of arrayY
//        bit 31~0 - arrayY[63:32] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of objxy
//        bit 31~0 - objxy[31:0] (Read/Write)
// 0x60 : Data signal of objxy
//        bit 31~0 - objxy[63:32] (Read/Write)
// 0x64 : reserved
// 0x68 : Data signal of I
//        bit 31~0 - I[31:0] (Read/Write)
// 0x6c : Data signal of I
//        bit 31~0 - I[63:32] (Read/Write)
// 0x70 : reserved
// 0x74 : Data signal of likelihood
//        bit 31~0 - likelihood[31:0] (Read/Write)
// 0x78 : Data signal of likelihood
//        bit 31~0 - likelihood[63:32] (Read/Write)
// 0x7c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL         0x00
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_GIE             0x04
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER             0x08
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISR             0x0c
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_NPARTICLES_DATA 0x10
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_NPARTICLES_DATA 32
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_COUNTONES_DATA  0x18
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_COUNTONES_DATA  32
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISZY_DATA       0x20
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_ISZY_DATA       32
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_NFR_DATA        0x28
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_NFR_DATA        32
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_K_DATA          0x30
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_K_DATA          32
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_MAX_SIZE_DATA   0x38
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_MAX_SIZE_DATA   64
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYX_DATA     0x44
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_ARRAYX_DATA     64
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYY_DATA     0x50
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_ARRAYY_DATA     64
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_OBJXY_DATA      0x5c
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_OBJXY_DATA      64
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_I_DATA          0x68
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_I_DATA          64
#define XLIKELIHOOD_KERNEL_CONTROL_ADDR_LIKELIHOOD_DATA 0x74
#define XLIKELIHOOD_KERNEL_CONTROL_BITS_LIKELIHOOD_DATA 64

