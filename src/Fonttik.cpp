//Copyright (C) 2022 Electronic Arts, Inc.  All rights reserved.

#include "fonttik/Fonttik.h"
#include <iostream>
#include <string>
#include "fonttik/Configuration.h"
#include "Image.h"
#include "TextboxDetectionFactory.h"
#include "TextboxRecognitionOpenCV.h"
#include "Guideline.h"
#include "AppSettings.h"
#include "TextDetectionParams.h"
#include "ContrastChecker.h"
#include "SizeChecker.h"
#include <limits>
#include "fonttik/Log.h"

namespace tik {
	Fonttik::Fonttik() {	};

	Fonttik::Fonttik(const Fonttik& other) {
		config = other.config;
		if (config != nullptr) {
			init(config);
		}
	}

	std::vector<double>* Fonttik::linearizationLUT = nullptr;

	Results* Fonttik::processMedia(Media& media) {
		//Save the logs or output through console
		if (config->getAppSettings()->saveLogs()) {

			tik::Log::RemoveFileLogger();
			tik::Log::SetFileLogger(media.getOutputPath().string() + "/" + media.getPath().stem().string() + ".txt");
		}

		Results* mediaRes = media.getResultsPointer();
		Frame* nextFrame = media.getFrame();

		//Process each frame and add received frame's results to the media results
		while (nextFrame != nullptr) {
			std::pair<FrameResults, FrameResults> res = processFrame(nextFrame);
			mediaRes->addSizeResults(res.first);
			mediaRes->addContrastResults(res.second);
			delete nextFrame;
			media.nextFrame();
			nextFrame = media.getFrame();
		}

		LOG_CORE_INFO("SIZE CHECK RESULT: {0}", (media.getResultsPointer()->sizePass() ? "PASS" : "FAIL"));
		LOG_CORE_INFO("CONTRAST CHECK RESULT: {0}", (media.getResultsPointer()->contrastPass() ? "PASS" : "FAIL"));

		return media.getResultsPointer();
	}

	std::pair<FrameResults, FrameResults> Fonttik::processFrame(Frame* frame) {
		//Ignore portions of image as specifid by configuration files
		applyFocusMask(*frame);
		//Detect relevant text
		std::vector<tik::Textbox> textBoxes = getTextBoxes(*frame);
		//Post processing to improve OpenCV's NMS algorithm, taking into account that text is aligned
		mergeTextBoxes(textBoxes);
		FrameResults sizeRes(-1);
		FrameResults contrastRes(-1);

		if (textBoxes.empty()) {
			LOG_CORE_INFO("No words recognized in image");
		}
		else {
			sizeRes = fontSizeCheck(*frame, textBoxes);
			contrastRes = textContrastCheck(*frame, textBoxes);
		}

		return std::make_pair(sizeRes, contrastRes);
	}

	void Fonttik::init(Configuration* configuration)
	{
		config = configuration;

		//load LUT for rgb linearization
		linearizationLUT = config->getRGBLookupTable();

		//Initialize EAST detection
		textboxDetection = TextboxDetectionFactory::CreateTextboxDetection(config->getAppSettings(), config->getTextDetectionParams());

		//Initialize text recognition only if text recognition is enabled in config

		//from https://docs.opencv.org/4.x/d4/d43/tutorial_dnn_text_spotting.html
		if (config->getAppSettings()->textRecognitionActive()) {
			// Load models weights
			TextRecognitionParams* recognitionParams = config->getTextRecognitionParams();
			textboxRecognition = new TextboxRecognitionOpenCV();
			textboxRecognition->init(recognitionParams);
		}

		//Create checkers
		contrastChecker = new ContrastChecker(config);
		sizeChecker = new SizeChecker(config, textboxRecognition);
	}

	void Fonttik::applyFocusMask(Frame& frame) {
		cv::Mat img = frame.getImageMatrix();
		cv::Mat mask = config->getAppSettings()->calculateMask(img.cols, img.rows);
		img = img & mask;
	}

	FrameResults Fonttik::fontSizeCheck(Frame& frame, std::vector<Textbox>& boxes) {
		return sizeChecker->check(frame, boxes);
	}

	FrameResults Fonttik::textContrastCheck(Frame& frame, std::vector<Textbox>& boxes) {
		return contrastChecker->check(frame, boxes);
	}

	double Fonttik::ContrastBetweenRegions(const cv::Mat& luminanceMap, const cv::Mat& maskA, const cv::Mat& maskB) {
		//Calculate the mean of the luminance for the light regions of the luminance
		double meanLight = Frame::LuminanceMeanWithMask(luminanceMap, maskA);

		//Invert mask to calculate mean of the darker colors
		double meanDark = Frame::LuminanceMeanWithMask(luminanceMap, maskB);

		return (std::max(meanLight, meanDark) + 0.05) / (std::min(meanLight, meanDark) + 0.05);
	}

	std::vector<Textbox> Fonttik::getTextBoxes(Frame& image) {
		return textboxDetection->detectBoxes(image.getImageMatrix());
	}

	void Fonttik::mergeTextBoxes(std::vector<Textbox>& textBoxes) {
		std::pair<float, float> threshold = config->getTextDetectionParams()->getMergeThreshold();
		if (threshold.first < 1.0 || threshold.second < 1.0) {
			ITextboxDetection::mergeTextBoxes(textBoxes, config->getTextDetectionParams());
		}
	}

	double Fonttik::linearize8bitRGB(const uchar& colorBits) {
		//Profiling this function kills performance
		
		//ref https://developer.mozilla.org/en-US/docs/Web/Accessibility/Understanding_Colors_and_Luminance
		double color = colorBits / 255.0;

		if (color <= 0.04045) {
			return color / 12.92;
		}
		else {
			double base = ((color + 0.055) / 1.1055);
			return pow(base, 2.4);
		}
	}

	cv::Mat Fonttik::calculateLuminance(cv::Mat imageMatrix) {
		cv::Mat luminanceMap;
		//Make sure that image has been loaded and we haven't previously calculated the luminance already
		if (!imageMatrix.empty()) {
			//Matrix to store linearized rgb
			cv::Mat linearBGR = cv::Mat::zeros(imageMatrix.size(), CV_64FC3);

			//Use look up tables if they have been correctly loaded during initialization
			if (linearizationLUT != nullptr && !linearizationLUT->empty()) {
				cv::LUT(imageMatrix, *linearizationLUT, linearBGR);
			}
			else {
				imageMatrix.forEach< cv::Vec3b>([&linearBGR](cv::Vec3b& pixel, const int* position)-> void {
					//at operator is used because we can't guarantee continuity
					//Can be extended to be more efficient in cases where continuity is guaranteed by 
					//adding a check with isContinuous()
					//check https://docs.opencv.org/4.x/d3/d63/classcv_1_1Mat.html#aa90cea495029c7d1ee0a41361ccecdf3
					linearBGR.at<cv::Vec3d>(position[0], position[1]) = {
						linearize8bitRGB(pixel[0]),
						linearize8bitRGB(pixel[1]),
						linearize8bitRGB(pixel[2]) };
					});
			}

			luminanceMap = cv::Mat::zeros(imageMatrix.size(), CV_64FC1); //1 channel (luminance)

			// Possible improvement by having a different LUT for each color channel and then adding themp up with:
			//from https://stackoverflow.com/questions/30666224/how-can-i-turn-a-three-channel-mat-into-a-summed-up-one-channel-mat
			//cv::transform(linearBGR, luminanceMap, cv::Matx13f(1, 1, 1));

			//Adds up all three channels into one luminance value channel
			//Extracted from Iris's RelativeLuminance.h
			linearBGR.forEach<cv::Vec3d>([&luminanceMap](cv::Vec3d& pixel, const int* position)-> void {
				//Y = 0.0722 * B + 0.7152 * G + 0.2126 * R where B, G and R
				//at is used because we can't guarantee continuity
				luminanceMap.at<double>(position[0], position[1]) = 0.0722 * pixel[0] + 0.7152 * pixel[1] + 0.2126 * pixel[2];
				});
		}

		return luminanceMap;
	}

	Fonttik::~Fonttik()
	{
		if (textboxDetection != nullptr) {
			delete textboxDetection;
		}
		textboxDetection = nullptr;

		if (textboxRecognition != nullptr) {
			delete textboxRecognition;
		}
		textboxRecognition = nullptr;

		if (contrastChecker != nullptr) {
			delete contrastChecker;
		}
		contrastChecker = nullptr;

		if (sizeChecker != nullptr) {
			delete sizeChecker;
		}
		sizeChecker = nullptr;

		config = nullptr;
	}
}
