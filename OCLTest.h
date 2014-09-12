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

#include "OCLUtil.h"
#include "OCLEncoder.h"
#include "OCLDecoder.h"

template< typename T, typename U > class OCLTest
{
public:
	OCLTest(bool lossy);
	~OCLTest(void);

	void test();
private:
	void testInit();
    void testRun(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision);
	void testFinish();
	U* getTestResults();

	OCLEncoder<T>* encoder;
	OCLDecoder<T>* decoder;
	bool lossy;

};

