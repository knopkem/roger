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
#include "OCLDWT.h"
#include <vector>
#include "OCLMemoryManager.h"



template<typename T> class OCLDWTForward : public OCLDWT<T>
{
public:
    OCLDWTForward(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr);
    ~OCLDWTForward(void);

    void run(bool lossy, size_t w,	size_t h,size_t windowX, size_t windowY, size_t level, size_t levels);
private:
    void doRun(bool lossy, size_t w,	size_t h,size_t windowX, size_t windowY, size_t level, size_t levels);
    OCLKernel* forward53;
    OCLKernel* forward97;
    int int_floorlog2(int a);
    float getStep(size_t numresolutions, size_t level, size_t orient, size_t prec);

};

