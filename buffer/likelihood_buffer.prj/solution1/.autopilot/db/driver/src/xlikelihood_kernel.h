// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef XLIKELIHOOD_KERNEL_H
#define XLIKELIHOOD_KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xlikelihood_kernel_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
    u16 DeviceId;
    u32 Control_BaseAddress;
} XLikelihood_kernel_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XLikelihood_kernel;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XLikelihood_kernel_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XLikelihood_kernel_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XLikelihood_kernel_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XLikelihood_kernel_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
int XLikelihood_kernel_Initialize(XLikelihood_kernel *InstancePtr, u16 DeviceId);
XLikelihood_kernel_Config* XLikelihood_kernel_LookupConfig(u16 DeviceId);
int XLikelihood_kernel_CfgInitialize(XLikelihood_kernel *InstancePtr, XLikelihood_kernel_Config *ConfigPtr);
#else
int XLikelihood_kernel_Initialize(XLikelihood_kernel *InstancePtr, const char* InstanceName);
int XLikelihood_kernel_Release(XLikelihood_kernel *InstancePtr);
#endif

void XLikelihood_kernel_Start(XLikelihood_kernel *InstancePtr);
u32 XLikelihood_kernel_IsDone(XLikelihood_kernel *InstancePtr);
u32 XLikelihood_kernel_IsIdle(XLikelihood_kernel *InstancePtr);
u32 XLikelihood_kernel_IsReady(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_EnableAutoRestart(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_DisableAutoRestart(XLikelihood_kernel *InstancePtr);

void XLikelihood_kernel_Set_Nparticles(XLikelihood_kernel *InstancePtr, u32 Data);
u32 XLikelihood_kernel_Get_Nparticles(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_countOnes(XLikelihood_kernel *InstancePtr, u32 Data);
u32 XLikelihood_kernel_Get_countOnes(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_IszY(XLikelihood_kernel *InstancePtr, u32 Data);
u32 XLikelihood_kernel_Get_IszY(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_Nfr(XLikelihood_kernel *InstancePtr, u32 Data);
u32 XLikelihood_kernel_Get_Nfr(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_k(XLikelihood_kernel *InstancePtr, u32 Data);
u32 XLikelihood_kernel_Get_k(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_max_size(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_max_size(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_arrayX(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_arrayX(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_arrayY(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_arrayY(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_objxy(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_objxy(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_I(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_I(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_Set_likelihood(XLikelihood_kernel *InstancePtr, u64 Data);
u64 XLikelihood_kernel_Get_likelihood(XLikelihood_kernel *InstancePtr);

void XLikelihood_kernel_InterruptGlobalEnable(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_InterruptGlobalDisable(XLikelihood_kernel *InstancePtr);
void XLikelihood_kernel_InterruptEnable(XLikelihood_kernel *InstancePtr, u32 Mask);
void XLikelihood_kernel_InterruptDisable(XLikelihood_kernel *InstancePtr, u32 Mask);
void XLikelihood_kernel_InterruptClear(XLikelihood_kernel *InstancePtr, u32 Mask);
u32 XLikelihood_kernel_InterruptGetEnabled(XLikelihood_kernel *InstancePtr);
u32 XLikelihood_kernel_InterruptGetStatus(XLikelihood_kernel *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
