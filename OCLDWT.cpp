/*  Copyright 2014 Aaron Boxer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#pragma once

#include "OCLDWT.h"
#include "OCLMemoryManager.h"
#include <stdint.h>


template<typename T> OCLDWT<T>::OCLDWT(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : 
	                initInfo(initInfo),
					memoryManager(memMgr),
					forward53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53.cl", "run") ))
				
											
{
}


template<typename T> OCLDWT<T>::~OCLDWT(void)
{
	if (forward53)
		delete forward53;
}

template<typename T> void OCLDWT<T>::encode(bool lossy, std::vector<int*> components,	int w,	int h) {

	if (components.size() == 0)
		return;
	OCLKernel* targetKernel = forward53;
	memoryManager->init(components,w,h,lossy);
	setKernelArgs(targetKernel);
	int workGroupDim = 16;
	size_t local_work_size[3] = {workGroupDim,workGroupDim,1};
	int numGroupsX = (size_t)ceil(((float)memoryManager->getWidth())/workGroupDim);
	int numGroupsY = (size_t)ceil(((float)memoryManager->getHeight())/workGroupDim);
	size_t global_work_size[3] = {workGroupDim * numGroupsX, workGroupDim * numGroupsY,1};
	targetKernel->enqueue(2,global_work_size, local_work_size);


}


template<typename T> tDeviceRC OCLDWT<T>::setKernelArgs(OCLKernel* myKernel){

	cl_int error_code =  DeviceSuccess;
	cl_kernel targetKernel = myKernel->getKernel();
	int argNum = 0;
	unsigned int width = memoryManager->getWidth();
	unsigned int height = memoryManager->getHeight();

	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem),  memoryManager->getPreprocessOut());
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(cl_mem), memoryManager->getDwtOut());
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(width), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, argNum++, sizeof(height), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	
	return DeviceSuccess;
}

