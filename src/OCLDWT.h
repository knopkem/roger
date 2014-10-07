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

#include "OCLKernel.h"
#include <vector>
#include "OCLMemoryManager.h"



template<typename T> class OCLDWT
{
public:
    OCLDWT(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr);
    ~OCLDWT(void);
protected:
    tDeviceRC setKernelArgs(OCLKernel* myKernel, unsigned int width, unsigned int height, unsigned int steps,unsigned int level, unsigned int levels);
    tDeviceRC setKernelArgsQuant(OCLKernel* myKernel, float quantLL, float quantLH, float quantHH);
    KernelInitInfoBase initInfo;
    OCLMemoryManager<T>* memoryManager;
    int numKernelArgs;

};

