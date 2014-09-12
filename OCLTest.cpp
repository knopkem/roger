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
#include "OCLTest.h"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include "OCLEncoder.cpp"
#include "OCLDecoder.cpp"

using namespace cv;


#include "OCLBasic.h"
#include "OCLDeviceManager.h"
#include "OCLDWTForward.cpp"
#include "OCLDWTRev.cpp"

#define OCL_SAMPLE_IMAGE_NAME "baboon.png"


template<typename T, typename U>  OCLTest<T,U>::OCLTest(bool isLossy) : encoder(NULL), decoder(NULL), lossy(isLossy)
{
}


template<typename T, typename U> OCLTest<T,U>::~OCLTest(void)
{
	if (encoder)
		delete encoder;
	if (decoder)
		delete decoder;
}

template<typename T, typename U> void OCLTest<T,U>::test()
{
	testInit();

    // Read the input image
	Size dsize;
    Mat img_src = imread(OCL_SAMPLE_IMAGE_NAME, CV_8UC3);
	 int alignCols=8, alignRows=64;

    if (! img_src.empty())
    {
        // Make sure that the input image size is fit:
        // number of rows is multiple of 8
        // number of columns is multiple of 64
        dsize.height = ((img_src.rows % alignRows) == 0) ? img_src.rows : (((img_src.rows + alignRows - 1) / alignRows) * alignRows);
        dsize.width = ((img_src.cols % alignCols) == 0) ? img_src.cols : (((img_src.cols + alignCols - 1) / alignCols) * alignCols);
        resize(img_src, img_src, dsize);
    }
    if (img_src.empty())
    {
        LogError("Cannot read image file: %s\n", OCL_SAMPLE_IMAGE_NAME);
        return;
    }

    Mat img_dst = Mat::zeros(img_src.size(), CV_8UC1);
    int imageSize = img_src.cols * img_src.rows;



  //   imshow("Before:", img_src);
 //  waitKey();

	T* input = new T[imageSize];
    for (int i = 0; i < imageSize; ++i) {
        input[i] = img_src.data[i] - 128;
    }

	//simulate RGB image
	std::vector<T*> components;
	components.push_back(input);
	components.push_back(input);
	components.push_back(input);
	components.push_back(input);

	int levels = 5;
	 
	//dont time the first run
	testRun(components, img_src.cols, img_src.rows,levels);
	testFinish();

	double t = my_clock();
	int numIterations = 40;
	for (int j =0; j < numIterations; ++j) { 
	   testRun(components, img_src.cols, img_src.rows,levels);
	   testFinish();
	}
	t = my_clock() - t;
	fprintf(stdout, "encode time: %d micro seconds \n", (int)((t * 1000000)/numIterations));

	U* results = getTestResults();
	if (results) {
		size_t resultsIndex=0;
		for (int i = 0; i < imageSize; ++i){
			int temp =  results[resultsIndex] + 128 ;
			if (temp < 0)
				temp = 0;
			if (temp > 255)
				temp = 255;
			img_dst.data[i] = temp;
			resultsIndex+=components.size();

		}

	}

	encoder->unmapDWTOut(results);
	delete[] input;

    imshow("After:", img_dst);
    waitKey();



}

template<typename T, typename U> void OCLTest<T,U>::testInit() {
	OCLDeviceManager* deviceManager = new OCLDeviceManager();
	deviceManager->init();
	encoder = new OCLEncoder<T>(deviceManager->getInfo(), lossy);
	decoder = new OCLDecoder<T>(deviceManager->getInfo(), lossy);
}


template<typename T, typename U> void OCLTest<T,U>::testRun(std::vector<T*> components,size_t w,size_t h, size_t levels) {
	encoder->run(components,w,h, levels);
}

template<typename T, typename U> void OCLTest<T,U>::testFinish() {
	encoder->finish();
}

template<typename T, typename U> U* OCLTest<T,U>::getTestResults(){
	void* ptr;
	encoder->mapDWTOut(&ptr);
	return (U*)ptr;
}
