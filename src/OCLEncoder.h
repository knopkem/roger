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
struct ocl_args_d_t;
#include <vector>
#include "OCLMemoryManager.h"
#include "OCLEncodeDecode.h"
#include "OCLBPC.h"
#include "OCLRGBtoPlanar.h"


template<typename T>  class OCLEncoder :  public OCLEncodeDecode<T>
{
public:
    OCLEncoder(ocl_args_d_t* ocl, bool isLossy, bool outputDwt);
    ~OCLEncoder(void);
    void run(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision);
private:
    OCLDWTForward<T>* dwt;
    OCLBPC<T>* bpc;
    OCLRGBtoPlanar<T>* rgbToPlanar;
};
