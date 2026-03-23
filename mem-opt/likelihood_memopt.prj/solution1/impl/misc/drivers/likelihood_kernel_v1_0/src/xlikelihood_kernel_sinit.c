// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#include "xparameters.h"
#include "xlikelihood_kernel.h"

extern XLikelihood_kernel_Config XLikelihood_kernel_ConfigTable[];

XLikelihood_kernel_Config *XLikelihood_kernel_LookupConfig(u16 DeviceId) {
	XLikelihood_kernel_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XLIKELIHOOD_KERNEL_NUM_INSTANCES; Index++) {
		if (XLikelihood_kernel_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XLikelihood_kernel_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLikelihood_kernel_Initialize(XLikelihood_kernel *InstancePtr, u16 DeviceId) {
	XLikelihood_kernel_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLikelihood_kernel_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLikelihood_kernel_CfgInitialize(InstancePtr, ConfigPtr);
}

#endif

