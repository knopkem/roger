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

#include "OCLDeviceManager.h"

OCLDeviceManager::OCLDeviceManager() : ocl_gpu(NULL), ocl_cpu(NULL)
{
}


OCLDeviceManager::~OCLDeviceManager(void)
{
    if (ocl_gpu) {
        delete ocl_gpu;
    }
    if (ocl_cpu) {
        delete ocl_cpu;
    }
}

ocl_args_d_t* OCLDeviceManager::getInfo() {
    ocl_args_d_t* info = getInfo(GPU);
    if (!info)
        info = getInfo(CPU);
    return info;
}

ocl_args_d_t* OCLDeviceManager::getInfo(eDeviceType type) {

    switch(type) {

    case CPU:
        return ocl_cpu;
        break;
    case GPU:
        return ocl_gpu;
        break;
    }
    return NULL;
}

int OCLDeviceManager::init() {
    int rcGPU = init(GPU);
    int rcCPU = init(CPU);
    return rcGPU || rcCPU;
}

int OCLDeviceManager::init(eDeviceType type) {
    bool isCpu = type == CPU;
    ocl_args_d_t** oclArgs;
    if (isCpu) {
        if (ocl_cpu)
            return 0;
        ocl_cpu = new ocl_args_d_t();
        oclArgs = &ocl_cpu;

    } else {

        if (ocl_gpu)
            return 0;
        ocl_gpu = new ocl_args_d_t();
        oclArgs = &ocl_gpu;

    }
    data_args_d_t args;
    args.preferGpu = !isCpu;
    args.preferCpu = isCpu;
    args.vendorName = NULL;
    int error_code;
    error_code = InitOpenCL(*oclArgs, &args);
    if (CL_SUCCESS != error_code)
    {
        LogError("InitOpenCL returned %s.", TranslateOpenCLError(error_code));
        delete *oclArgs;
        *oclArgs = NULL;;
    }
    return error_code;

}