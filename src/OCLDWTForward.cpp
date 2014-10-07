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

#include "OCLDWTForward.h"
#include "OCLMemoryManager.h"
#include <stdint.h>
#include "OCLDWT.cpp"


//Quantization:
static float norms[4][6] = {
    {1.000f, 1.965f, 4.177f, 8.403f, 16.90f, 33.84f},
    {2.022f, 3.989f, 8.355f, 17.04f, 34.27f, 68.63f},
    {2.022f, 3.989f, 8.355f, 17.04f, 34.27f, 68.63f},
    {2.080f, 3.865f, 8.307f, 17.18f, 34.71f, 69.59f}
};


template<typename T> OCLDWTForward<T>::OCLDWTForward(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : OCLDWT<T>(initInfo, memMgr),
    forward53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53.cl", "run") )),
    forward97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97.cl", memMgr->isOnlyDwtOut() ? "run" : "runWithQuantization") ))


{
}

template<typename T> OCLDWTForward<T>::~OCLDWTForward(void)
{
    if (forward53)
        delete forward53;
    if (forward97)
        delete forward97;
}

template<typename T> void OCLDWTForward<T>::doRun(bool lossy, size_t w, size_t h, size_t windowX, size_t windowY, size_t level, size_t levels) {

    OCLKernel* targetKernel = lossy?forward97:forward53;
    const size_t steps = divRndUp(w, 15 * windowX);
    //set basic dwt kernel arguments
    if (setKernelArgs(targetKernel,static_cast<unsigned int>(w),
                      static_cast<unsigned int>(h),
                      static_cast<unsigned int>(steps),
                      static_cast<unsigned int>(level),
                      static_cast<unsigned int>(levels)
                     ) != DeviceSuccess)
        return;
    // set dwt + quantization kernel arguments
    if (lossy && !memoryManager->isOnlyDwtOut() ) {
        float quantLL =  1.0f/getStep(levels,level, 0, memoryManager->getPrecision());
        float quantLH =  1.0f/getStep(levels,level, 1, memoryManager->getPrecision());
        float quantHH =  1.0f/getStep(levels,level, 3, memoryManager->getPrecision());
        if (setKernelArgsQuant(targetKernel, quantLL, quantLH, quantHH)
                != DeviceSuccess)
            return;

    }
    size_t local_work_size[3] = {1,windowY,1};
    if (lossy) {
        size_t global_offset[3] = {0,-4,0};   //left boundary

        //add one extra windowY to make up for group overlap due to boundary
        size_t global_work_size[3] = {divRndUp(w, windowX * steps), (divRndUp(h, windowY) + 1)* windowY,1};
        targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);

    } else {
        size_t global_offset[3] = {0,-2,0};   //left boundary

        //add one extra windowY to make up for group overlap due to boundary
        size_t global_work_size[3] = {divRndUp(w, windowX * steps), (divRndUp(h, windowY) + 1)* windowY,1};
        targetKernel->enqueue(2,global_offset, global_work_size, local_work_size);
    }

}


//Each resolution has four bands LL LH HL HH
//Orientation Code for bands     0  1  2  3
//Gain for the bands:            0  1  1  2
//Number of bits per sample      precision + gain
//base_stepsize                  (1 << (gain)) / norm


//Get logarithm of an integer and round downwards
//@return Returns log2(a)
template<typename T> int OCLDWTForward<T>::int_floorlog2(int a) {
    int l;
    for (l = 0; a > 1; l) {
        a >>= 1;
    }
    return l;
}

template<typename T> float OCLDWTForward<T>::getStep(size_t numresolutions, size_t level, size_t orient, size_t prec) {

    int gain = (orient == 0) ? 0 : (((orient == 1) || (orient == 2)) ? 1 : 2);
    int numbps = prec + gain;
    double norm = norms[orient][level];
    int base_stepsize = floor( ((1 << (gain)) / norm) * 8192 );
    int p = int_floorlog2(base_stepsize) - 13;
    int n = 11 - int_floorlog2(base_stepsize);
    double mant = (n < 0 ? base_stepsize >> -n : base_stepsize << n) & 0x7ff;
    double expn = numbps - p;

    expn=8.5f; //?
    mant=8.0f; //?
    return (1.0 + mant/2048.0f) * pow(2, numbps - expn);

}

template<typename T> void OCLDWTForward<T>::run(bool lossy, size_t w,	size_t h, size_t windowX, size_t windowY, size_t level, size_t levels) {

    doRun(lossy, w,h,windowX, windowY,level,levels);
    if(level < levels-1) {
        // copy output's LL band back into input buffer
        const size_t llSizeX = divRndUp(w, 2);
        const size_t llSizeY = divRndUp(h, 2);

        level++;

        // run remaining levels of FDWT
        run(lossy, llSizeX, llSizeY, windowX, windowY, level,levels);
    }
}
