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

using namespace cv;


#include "OCLBasic.h"
#include "OCLDeviceManager.h"


#define OCL_SAMPLE_IMAGE_NAME "man.bmp"


OCLTest::OCLTest(void)
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
    
    // Read the input image
    Mat img_src = ReadInputImage(OCL_SAMPLE_IMAGE_NAME, CV_8UC1, 8, 64);
    if (img_src.empty())
    {
        LogError("Cannot read image file: %s\n", OCL_SAMPLE_IMAGE_NAME);
        return;
    }

    Mat img_dst = Mat::zeros(img_src.size(), CV_8UC1);
    int imageSize = img_src.cols * img_src.rows;

	OCLDeviceManager* deviceManager = new OCLDeviceManager();
	deviceManager->init();

   //  imshow("Before:", img_src);
   //  waitKey();

	unsigned char* input = new unsigned char[imageSize];
    for (int i = 0; i < imageSize; ++i) {
        input[i] = img_src.ptr()[i];
    }

	
	std::vector<unsigned char*> components;
	components.push_back(input);
	double t = my_clock();
	int numIterations = 10;
	for (int j =0; j < numIterations; ++j) { 
	   testRun();
	}
	testFinish();
	t = my_clock() - t;
	fprintf(stdout, "encode time: %d micro seconds \n", (int)((t * 1000000)/numIterations));

	int* results = getTestResults();
	if (results) {
		for (int i = 0; i < imageSize; ++i)
			img_dst.ptr()[i] =  results[i] + 128;

	}
    imshow("After:", img_dst);
    waitKey();

}

void OCLTest::testRun() {
}

void OCLTest::testFinish() {
}

int* OCLTest::getTestResults(){

	return NULL;
}
