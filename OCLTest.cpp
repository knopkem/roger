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

#include "OCLTest.h"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include "OCLEncoder.cpp"

using namespace cv;


#include "OCLBasic.h"
#include "OCLDeviceManager.h"
#include "OCLDWT.cpp"


#define OCL_SAMPLE_IMAGE_NAME "baboon.png"


OCLTest::OCLTest(void) : encoder(NULL)
{
}


OCLTest::~OCLTest(void)
{
}


// Read and normalize the input image
Mat ReadInputImage(const std::string &fileName, int flag, int alignCols, int alignRows)
{
    Size dsize;
    Mat img = imread(fileName, flag);

    if (! img.empty())
    {
        // Make sure that the input image size is fit:
        // number of rows is multiple of 8
        // number of columns is multiple of 64
        dsize.height = ((img.rows % alignRows) == 0) ? img.rows : (((img.rows + alignRows - 1) / alignRows) * alignRows);
        dsize.width = ((img.cols % alignCols) == 0) ? img.cols : (((img.cols + alignCols - 1) / alignCols) * alignCols);
        resize(img, img, dsize);
    }

    return img;
}
void OCLTest::test()
{

	testInit();

    // Read the input image
    Mat img_src = ReadInputImage(OCL_SAMPLE_IMAGE_NAME, CV_8UC3, 8, 64);
    if (img_src.empty())
    {
        LogError("Cannot read image file: %s\n", OCL_SAMPLE_IMAGE_NAME);
        return;
    }

    Mat img_dst = Mat::zeros(img_src.size(), CV_8UC1);
    int imageSize = img_src.cols * img_src.rows;



  //   imshow("Before:", img_src);
 //  waitKey();

	int* input = new int[imageSize];
    for (int i = 0; i < imageSize; ++i) {
        input[i] = img_src.data[i]-128;
    }

	//simulate RGB image
	std::vector<int*> components;
	components.push_back(input);
	//components.push_back(input);
	//components.push_back(input);


	double t = my_clock();
	int numIterations = 15;
	for (int j =0; j < numIterations; ++j) { 
	   testRun(components, img_src.cols, img_src.rows);
	}
	testFinish();
	t = my_clock() - t;
	fprintf(stdout, "encode time: %d micro seconds \n", (int)((t * 1000000)/numIterations));

	int* results = getTestResults();
	if (results) {
		for (int i = 0; i < imageSize; ++i){
			int temp =  results[i]+128;
			if (temp < 0)
				temp = 0;
			if (temp > 255)
				temp = 255;
			img_dst.data[i] = temp;

		}

	}

	encoder->unmapOutput(results);
	delete encoder;
	delete[] input;

    imshow("After:", img_dst);
    waitKey();



}

void OCLTest::testInit() {
	OCLDeviceManager* deviceManager = new OCLDeviceManager();
	deviceManager->init();
	encoder = new OCLEncoder<int>(deviceManager->getInfo(), false);

}


void OCLTest::testRun(std::vector<int*> components,int w,int h) {
	encoder->encode(components,w,h);
}

void OCLTest::testFinish() {
	encoder->finish();
}

int* OCLTest::getTestResults(){
	void* ptr;
	encoder->mapOutput(&ptr);
	return (int*)ptr;
}
