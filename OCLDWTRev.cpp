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

#include "OCLDWTRev.h"
#include "OCLMemoryManager.h"
#include <stdint.h>
#include "OCLDWT.cpp"



template<typename T> OCLDWTRev<T>::OCLDWTRev(KernelInitInfoBase initInfo, OCLMemoryManager<T>* memMgr) : OCLDWT<T>(initInfo, memMgr),
					reverse53(new OCLKernel( KernelInitInfo(initInfo, "ocldwt53rev.cl", "run") )),
					reverse97(new OCLKernel( KernelInitInfo(initInfo, "ocldwt97rev.cl", "run") ))
				
											
{
}


template<typename T> OCLDWTRev<T>::~OCLDWTRev(void)
{
	if (reverse53)
		delete reverse53;
	if (reverse97)
		delete reverse97;
}

template<typename T> void OCLDWTRev<T>::decode(bool lossy, std::vector<T*> components,	int w,	int h, int windowX, int windowY) {

	run(lossy?reverse97:reverse53, lossy, components,w,h,windowX, windowY);
}

