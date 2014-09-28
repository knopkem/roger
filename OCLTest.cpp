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


template<typename T, typename U>  OCLTest<T,U>::OCLTest(bool isLossy, bool outputDwt) : encoder(NULL),
																						decoder(NULL),
																						lossy(isLossy),
																						outputDwt(outputDwt)
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
    Mat img_src = imread(OCL_SAMPLE_IMAGE_NAME, 1);



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
	
	Mat channel[3];
    split(img_src, channel);

	std::vector<T*> components;
	T* input[4]; 
	for (int chan = 0; chan < 4; ++chan) {
		int comp = chan;
		if (comp == 3)
			comp = 2;
		input[chan] = new T[imageSize];
		for (int i = 0; i < imageSize; ++i) {
			input[chan][i] = (T)( (channel[comp].data[i]*16) - 2048);
		}
		components.push_back(input[chan]);
	}

	int levels = 5;
	int precision = 8;
	 
	//dont time the first run
	testRun(components, img_src.cols, img_src.rows,levels,precision);
	testFinish();

	double t = my_clock();
	int numIterations = 40;
	for (int j =0; j < numIterations; ++j) { 
	   testRun(components, img_src.cols, img_src.rows,levels,precision);
	   testFinish();
	}
	t = my_clock() - t;
	fprintf(stdout, "encode time: %d micro seconds \n", (int)((t * 1000000)/numIterations));

	U* results = getTestResults();
	if (results) {
		size_t resultsIndex=0;
		for (int i = 0; i < imageSize; ++i){
			int temp =  (int)((results[resultsIndex] + 2048)/16) ;
			if (temp < 0)
				temp = 0;
			if (temp > 255)
				temp = 255;
			img_dst.data[i] = temp;
			resultsIndex+=components.size();

		}

	}

	encoder->unmapDWTOut(results);
	for (int chan = 0; chan < 4; ++chan)
		delete[] input[chan];

    imshow("After:", img_dst);
    waitKey();



}

template<typename T, typename U> void OCLTest<T,U>::testInit() {
	OCLDeviceManager* deviceManager = new OCLDeviceManager();
	deviceManager->init();
	encoder = new OCLEncoder<T>(deviceManager->getInfo(), lossy, outputDwt);
	decoder = new OCLDecoder<T>(deviceManager->getInfo(), lossy);
}


template<typename T, typename U> void OCLTest<T,U>::testRun(std::vector<T*> components,size_t w,size_t h, size_t levels, size_t precision) {
	encoder->run(components,w,h, levels,precision);
}

template<typename T, typename U> void OCLTest<T,U>::testFinish() {
	encoder->finish();
}

template<typename T, typename U> U* OCLTest<T,U>::getTestResults(){
	void* ptr;
	encoder->mapDWTOut(&ptr);
	return (U*)ptr;
}
