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

#include "ocl_platform.h"
#include "OCLUtil.h"

#include <vector>
#include <stdint.h>

template< typename T >  class OCLMemoryManager
{
public:
	OCLMemoryManager(ocl_args_d_t* ocl);
	~OCLMemoryManager(void);
	size_t getWidth() {return width;}
	size_t getHeight() {return height;}
	size_t getNumLevels() {return _levels;}
	size_t getPrecision() {return _precision;}  
	cl_mem* getDWTOut(){ return &dwtOut;}
	cl_mem* getDwtIn(size_t level){
		if (level >= dwtIn.size())
			return NULL;
		return &dwtIn[level];
	}
	void init(std::vector<T*> components, size_t w, size_t h, size_t levels, bool lossy, size_t precision);

	tDeviceRC mapImage(cl_mem img, void** mappedPtr);
	tDeviceRC unmapImage(cl_mem, void* mappedPtr);

private:
	tDeviceRC hostToDWTIn();
	void fillHostInputBuffer(std::vector<T*> components, size_t w,	size_t h);
	void freeBuffers();
	T* rgbBuffer;

	ocl_args_d_t* ocl;
	size_t width;
	size_t height;
	size_t _levels;
	size_t _precision;
 
	std::vector<cl_mem> dwtIn;  
	cl_mem dwtOut;  //could be dwt or dwt + quantization

};


