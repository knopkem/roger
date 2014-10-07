/*  Copyright 2014 Aaron Boxer (boxerab@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
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

template<typename T>  void OCLBPC<T>::run(size_t codeblockX, size_t codeblockY) {
    size_t local_work_size[3] = {codeblockX, codeblockY/4};
    size_t global_work_size[3] = {memoryManager->getWidth(), memoryManager->getHeight()/4,1};
    for (int i  =0; i < 4; ++i) {

        if (setKernelArgs(memoryManager->getDWTOutByChannel(i)) != DeviceSuccess) {
            return;
        }
        bpc->enqueue(2,global_work_size, local_work_size);
    }



}

template<typename T> tDeviceRC OCLBPC<T>::setKernelArgs(cl_mem* channel) {
    int numKernelArgs = 0;
    cl_kernel targetKernel = bpc->getKernel();
    cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem),channel);
    if (DeviceSuccess != error_code)
    {
        LogError("setKernelArgs returned %s.", TranslateOpenCLError(error_code));
        return error_code;
    }

    return DeviceSuccess;
}

