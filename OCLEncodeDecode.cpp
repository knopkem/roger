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

#include "OCLEncodeDecode.h"
#include "OCLUtil.h"
#include "OCLMemoryManager.cpp"




template<typename T> OCLEncodeDecode<T>::OCLEncodeDecode(ocl_args_d_t* ocl, bool isLossy) :
	_ocl(ocl), 
	lossy(isLossy),
	memoryManager(new OCLMemoryManager<T>(ocl))
{

}


template<typename T> OCLEncodeDecode<T>::~OCLEncodeDecode(){
}

template<typename T> void OCLEncodeDecode<T>::finish(void){

	clFinish(_ocl->commandQueue);
}

template<typename T> void OCLEncodeDecode<T>::run(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision){
	memoryManager->init(components,w,h,levels,lossy,precision);
}

template<typename T>  tDeviceRC OCLEncodeDecode<T>::mapDWTOut(void** mappedPtr){
	return memoryManager->mapImage(*memoryManager->getDWTOut(), mappedPtr);
}
template<typename T> tDeviceRC OCLEncodeDecode<T>::unmapDWTOut(void* mappedPtr){

	return memoryManager->unmapImage(*memoryManager->getDWTOut(), mappedPtr);
}

