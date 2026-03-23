// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xlikelihood_kernel.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XLikelihood_kernel_CfgInitialize(XLikelihood_kernel *InstancePtr, XLikelihood_kernel_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XLikelihood_kernel_Start(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL) & 0x80;
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XLikelihood_kernel_IsDone(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XLikelihood_kernel_IsIdle(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XLikelihood_kernel_IsReady(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XLikelihood_kernel_EnableAutoRestart(XLikelihood_kernel *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XLikelihood_kernel_DisableAutoRestart(XLikelihood_kernel *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_AP_CTRL, 0);
}

void XLikelihood_kernel_Set_Nparticles(XLikelihood_kernel *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_NPARTICLES_DATA, Data);
}

u32 XLikelihood_kernel_Get_Nparticles(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_NPARTICLES_DATA);
    return Data;
}

void XLikelihood_kernel_Set_countOnes(XLikelihood_kernel *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_COUNTONES_DATA, Data);
}

u32 XLikelihood_kernel_Get_countOnes(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_COUNTONES_DATA);
    return Data;
}

void XLikelihood_kernel_Set_IszY(XLikelihood_kernel *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISZY_DATA, Data);
}

u32 XLikelihood_kernel_Get_IszY(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISZY_DATA);
    return Data;
}

void XLikelihood_kernel_Set_Nfr(XLikelihood_kernel *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_NFR_DATA, Data);
}

u32 XLikelihood_kernel_Get_Nfr(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_NFR_DATA);
    return Data;
}

void XLikelihood_kernel_Set_k(XLikelihood_kernel *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_K_DATA, Data);
}

u32 XLikelihood_kernel_Get_k(XLikelihood_kernel *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_K_DATA);
    return Data;
}

void XLikelihood_kernel_Set_max_size(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_MAX_SIZE_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_MAX_SIZE_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_max_size(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_MAX_SIZE_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_MAX_SIZE_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_Set_arrayX(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYX_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYX_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_arrayX(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYX_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYX_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_Set_arrayY(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYY_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYY_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_arrayY(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYY_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ARRAYY_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_Set_objxy(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_OBJXY_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_OBJXY_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_objxy(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_OBJXY_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_OBJXY_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_Set_I(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_I_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_I_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_I(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_I_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_I_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_Set_likelihood(XLikelihood_kernel *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_LIKELIHOOD_DATA, (u32)(Data));
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_LIKELIHOOD_DATA + 4, (u32)(Data >> 32));
}

u64 XLikelihood_kernel_Get_likelihood(XLikelihood_kernel *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_LIKELIHOOD_DATA);
    Data += (u64)XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_LIKELIHOOD_DATA + 4) << 32;
    return Data;
}

void XLikelihood_kernel_InterruptGlobalEnable(XLikelihood_kernel *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_GIE, 1);
}

void XLikelihood_kernel_InterruptGlobalDisable(XLikelihood_kernel *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_GIE, 0);
}

void XLikelihood_kernel_InterruptEnable(XLikelihood_kernel *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER);
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER, Register | Mask);
}

void XLikelihood_kernel_InterruptDisable(XLikelihood_kernel *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER);
    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER, Register & (~Mask));
}

void XLikelihood_kernel_InterruptClear(XLikelihood_kernel *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLikelihood_kernel_WriteReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISR, Mask);
}

u32 XLikelihood_kernel_InterruptGetEnabled(XLikelihood_kernel *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_IER);
}

u32 XLikelihood_kernel_InterruptGetStatus(XLikelihood_kernel *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLikelihood_kernel_ReadReg(InstancePtr->Control_BaseAddress, XLIKELIHOOD_KERNEL_CONTROL_ADDR_ISR);
}

