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

#include "OCLRGBtoPlanar.h"
#include "OCLMemoryManager.h"
#include <stdint.h>

template<typename T> OCLRGBtoPlanar<T>::OCLRGBtoPlanar(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) :
    initInfo(initInfo),
    memoryManager(memMgr),
    planar(new OCLKernel( KernelInitInfo(initInfo, "oclplanar.cl", "run") ))
{

}

template<typename T> OCLRGBtoPlanar<T>::~OCLRGBtoPlanar(void)
{
    if (planar)
        delete planar;
}

template<typename T>  void OCLRGBtoPlanar<T>::run() {

    if (setKernelArgs() != DeviceSuccess) {
        return;
    }
    size_t local_work_size[3] = {16,16};
    size_t global_work_size[3] = {memoryManager->getWidth(), memoryManager->getHeight(),1};
    planar->enqueue(2,global_work_size, local_work_size);
}

template<typename T> tDeviceRC OCLRGBtoPlanar<T>::setKernelArgs() {
    int numKernelArgs = 0;
    cl_kernel targetKernel = planar->getKernel();
    unsigned int width = static_cast<unsigned int>(memoryManager->getWidth());
    unsigned int height = static_cast<unsigned int>(memoryManager->getHeight());
    cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem), memoryManager->getDWTOut() );
    if (DeviceSuccess != error_code)
    {
        LogError("setKernelArgs returned %s.", TranslateOpenCLError(error_code));
        return error_code;
    }
    error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(width), &width);
    if (DeviceSuccess != error_code)
    {
        LogError("setKernelArgs returned %s.", TranslateOpenCLError(error_code));
        return error_code;
    }
    error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(height), &height);
    if (DeviceSuccess != error_code)
    {
        LogError("setKernelArgs returned %s.", TranslateOpenCLError(error_code));
        return error_code;
    }
    //set channels
    for (int i = 0; i < (int)memoryManager->getNumComponents(); ++i) {
        cl_int error_code = clSetKernelArg(targetKernel, numKernelArgs++, sizeof(cl_mem), memoryManager->getDWTOutByChannel(i) );
        if (DeviceSuccess != error_code)
        {
            LogError("setKernelArgs returned %s.", TranslateOpenCLError(error_code));
            return error_code;
        }
    }

    return DeviceSuccess;
}

