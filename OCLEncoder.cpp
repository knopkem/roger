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

#include "OCLEncoder.h"
#include "OCLDWTForward.cpp"
#include "OCLUtil.h"
#include "OCLMemoryManager.cpp"
#include "OCLEncodeDecode.cpp"
#include "OCLBPC.cpp"
#include "OCLRGBtoPlanar.cpp"


template<typename T> OCLEncoder<T>::OCLEncoder(ocl_args_d_t* ocl, bool isLossy, bool outputDwt) : OCLEncodeDecode<T>(ocl, isLossy, outputDwt),
	dwt(new OCLDWTForward<T>(KernelInitInfoBase(_ocl->commandQueue,  "-I . -D WIN_SIZE_X=8 -D WIN_SIZE_Y=128"), memoryManager)),
	bpc(new OCLBPC<T>(KernelInitInfoBase(_ocl->commandQueue,  "-I . -D CODEBLOCKX=32 -D CODEBLOCKY=32"), memoryManager)),
	rgbToPlanar(new OCLRGBtoPlanar<T>(KernelInitInfoBase(_ocl->commandQueue,  "-I . -D WIN_SIZE_X=16 -D WIN_SIZE_Y=16"), memoryManager))

{

}


template<typename T> OCLEncoder<T>::~OCLEncoder(){
	if (dwt)
		delete dwt;
	if (bpc)
		delete bpc;
	if (rgbToPlanar)
		delete rgbToPlanar;
}

template<typename T> void OCLEncoder<T>::run(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision){
	OCLEncodeDecode::run(components,w,h,levels,precision);
	dwt->run(lossy, w,h, precision,128,0,levels);
	if (!memoryManager->isOnlyDwtOut() ) {
		if (components.size() > 1)
			rgbToPlanar->run();
		bpc->run(32,32);

	}
}
