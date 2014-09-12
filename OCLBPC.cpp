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

#include "OCLBPC.h"
#include "OCLMemoryManager.h"
#include <stdint.h>



template<typename T> OCLBPC<T>::OCLBPC(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) :
				    initInfo(initInfo),
					memoryManager(memMgr),
	               	bpc(new OCLKernel( KernelInitInfo(initInfo, "oclbpc.cl", "run") ))
				
											
{
}


template<typename T> OCLBPC<T>::~OCLBPC(void)
{
	if (bpc)
		delete bpc;
}

template<typename T>  void OCLBPC<T>::run(size_t codeblockX, size_t codeblockY){

	if (setKernelArgs(static_cast<unsigned int>(codeblockX), 
		              static_cast<unsigned int>(codeblockY),
					  8) != DeviceSuccess) {
        return;
	}
	 size_t local_work_size[3] = {codeblockX, codeblockY};
	 size_t global_work_size[3] = {divRndUp(memoryManager->getWidth(), codeblockX), divRndUp(memoryManager->getHeight(), codeblockY),1};
	 bpc->enqueue(2,global_work_size, local_work_size);
}

template<typename T> tDeviceRC OCLBPC<T>::setKernelArgs(unsigned int codeblockX, unsigned int codeblockY, unsigned int precision){
	int numKernelArgs = 0;
	cl_kernel targetKernel = bpc->getKernel();
	unsigned int width = static_cast<unsigned int>(memoryManager->getWidth());
	unsigned int height = static_cast<unsigned int>(memoryManager->getHeight());
	cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem), memoryManager->getDWTOut() );
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(width), &width);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(height), &height);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}

	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(codeblockX), &codeblockX);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(codeblockY), &codeblockY);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(precision), &precision);
	if (DeviceSuccess != error_code)
	{
		LogError("Error: setKernelArgs returned %s.\n", TranslateOpenCLError(error_code));
		return error_code;
	}
	
	return DeviceSuccess;
}

