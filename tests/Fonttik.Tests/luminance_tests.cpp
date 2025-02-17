//Copyright (C) 2022 Electronic Arts, Inc.  All rights reserved.

#include <gtest/gtest.h>
#include "fonttik/Media.h"
#include "fonttik/Fonttik.h"
#include "fonttik/Configuration.h"
#include "fonttik/Log.h"

namespace tik {
	class LuminanceTests : public ::testing::Test {
	protected:
		void SetUp() override {
			fonttik = Fonttik();
			config = Configuration("config/config_resolution.json");
			tik::Log::InitCoreLogger(false, false);
			fonttik.init(&config);

			whiteMedia = Media::CreateMedia("config/luminance/white.png");
			blackWhiteMedia = Media::CreateMedia("config/luminance/blackWhite.png");

			whiteImg = whiteMedia->getFrame();
			blackWhiteImg = blackWhiteMedia->getFrame();
		}

		void TearDown() override {
			delete whiteImg;
			delete blackWhiteImg;
			delete blackWhiteMedia;
			delete whiteMedia;
		}

		Fonttik fonttik;
		Configuration config;
		Media* whiteMedia, * blackWhiteMedia;
		Frame* whiteImg, * blackWhiteImg;
	};


	//White Luminance is 1
	TEST_F(LuminanceTests, WhiteImage) {
		cv::Mat luminanceMap = whiteImg->getFrameLuminance();

		cv::Mat mask = cv::Mat::ones({ luminanceMap.cols, luminanceMap.rows }, CV_8UC1);

		double mean = Frame::LuminanceMeanWithMask(luminanceMap, mask);

		ASSERT_DOUBLE_EQ(mean, 1);
	}

	//Mean of both images is different
	//Mean of left half of blackWhite is the same as white
	TEST_F(LuminanceTests, ApplyMask) {
		//Black and white mean
		cv::Mat bwLuminanceMap = blackWhiteImg->getFrameLuminance();
		cv::Mat whiteLuminanceMap = whiteImg->getFrameLuminance();

		cv::Mat mask = cv::Mat::zeros({ bwLuminanceMap.cols, bwLuminanceMap.rows }, CV_8UC1);
		double meanTwoHalves = Frame::LuminanceMeanWithMask(bwLuminanceMap, mask);

		cv::Mat half = mask({ 0,0,bwLuminanceMap.cols / 2,bwLuminanceMap.rows });
		cv::bitwise_not(half, half);

		//White half mean
		double meanHalf = Frame::LuminanceMeanWithMask(bwLuminanceMap, mask);

		//White image
		mask = cv::Mat::ones({ whiteLuminanceMap.cols, whiteLuminanceMap.rows }, CV_8UC1);
		double meanWhite = Frame::LuminanceMeanWithMask(whiteLuminanceMap, mask);

		ASSERT_DOUBLE_EQ(meanHalf, meanWhite);
		ASSERT_NE(meanWhite, meanTwoHalves);
	}

	//Black on White has a contrast of 21 according to current legal procedure https://snook.ca/technical/colour_contrast/colour.html#fg=FFFFFF,bg=000000
	TEST_F(LuminanceTests, MaxContrast) {
		cv::Mat luminanceMap = blackWhiteImg->getFrameLuminance();
		cv::Size size = { luminanceMap.cols,luminanceMap.rows };

		cv::Mat a = cv::Mat::zeros(size, CV_8UC1);
		cv::Mat halfA = a({ 0,0,size.width / 2,size.height });
		cv::bitwise_not(halfA, halfA);

		cv::Mat b;
		cv::bitwise_not(a, b);

		double contrast = Fonttik::ContrastBetweenRegions(luminanceMap, a, b);

		ASSERT_DOUBLE_EQ(contrast, 21);

	}

	//The order of the regions should not affect the contrast ratio
	TEST_F(LuminanceTests, Commutative) {
		cv::Mat luminanceMap = blackWhiteImg->getFrameLuminance();
		cv::Size size = { luminanceMap.cols,luminanceMap.rows };

		cv::Mat a = cv::Mat::zeros(size, CV_8UC1);
		cv::Mat halfA = a({ 0,0,size.width / 2,size.height });
		cv::bitwise_not(halfA, halfA);

		cv::Mat b;
		cv::bitwise_not(a, b);

		double contrastA = Fonttik::ContrastBetweenRegions(luminanceMap, a, b);
		double contrastB = Fonttik::ContrastBetweenRegions(luminanceMap, b, a);

		ASSERT_DOUBLE_EQ(contrastA, contrastB);
	}
}