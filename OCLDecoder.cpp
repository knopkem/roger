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

#include "OCLDecoder.h"
#include "OCLDWTRev.cpp"
#include "OCLUtil.h"
#include "OCLMemoryManager.cpp"
#include "OCLEncodeDecode.cpp"




template<typename T> OCLDecoder<T>::OCLDecoder(ocl_args_d_t* ocl, bool isLossy) : OCLEncodeDecode<T>(ocl,lossy),
	dwt(new OCLDWTRev<T>(KernelInitInfoBase(_ocl->commandQueue,  "-I . -D WIN_SIZE_X=128 -D WIN_SIZE_Y=8"), memoryManager))
{

}

template<typename T> OCLDecoder<T>::~OCLDecoder(){
	if (dwt)
		delete dwt;
}

template<typename T> void OCLDecoder<T>::run(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision){
	OCLEncodeDecode::run(components,w,h,levels);
	dwt->run(lossy, w,h, 128,8);
}

