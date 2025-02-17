//Copyright (C) 2022 Electronic Arts, Inc.  All rights reserved.

#include "AppSettings.h"
#include <tuple>
#include <opencv2/imgcodecs.hpp>
#include "Video.h"

namespace tik {
	void AppSettings::init(nlohmann::json settings) {

		if (settings["detectionBackend"] == "DB") {
			detectionBackend = DB;
		}
		else if (settings["detectionBackend"] == "EAST") {
			detectionBackend = EAST;
		}
		else {
			throw "Invalid Backend";
		}
		//Load all of the focus and ignore regions from config

		std::vector<cv::Rect2f> focus;
		for (auto it = settings["focusMask"].begin(); it != settings["focusMask"].end(); ++it)
		{
			focus.push_back(RectFromJson<float>(*it));
		}

		std::vector<cv::Rect2f> ignore;
		for (auto it = settings["ignoreMask"].begin(); it != settings["ignoreMask"].end(); ++it)
		{
			ignore.push_back(RectFromJson<float>(*it));
		}
		if (!focus.empty()) {
			setFocusMask(focus, ignore);
		}

		//Load individual values from json
		dbgSaveLuminanceMap = settings["saveLuminanceMap"];
		dbgSaveTexboxOutline = settings["saveTextboxOutline"];
		dbgSaveRawTextboxOutline = settings["saveSeparateTexboxes"];
		dbgSaveSeparateTextboxes = settings["saveHistograms"];
		dbgSaveHistograms = settings["saveRawTextboxOutline"];
		dbgSaveLuminanceMasks = settings["saveLuminanceMasks"];
		useTextRecognition = settings["useTextRecognition"];
		printResultValues = settings["printValuesOnResults"];
		dbgSaveLogs = settings["saveLogs"];
		useDPI = settings["useDPI"];
		targetDPI = settings["targetDPI"];
		targetResolution = settings["targetResolution"];

		outlineColors[PASS] = ColorFromJson(settings["textboxOutlineColors"]["pass"]);
		outlineColors[WARNING] = ColorFromJson(settings["textboxOutlineColors"]["warning"]);
		outlineColors[FAIL] = ColorFromJson(settings["textboxOutlineColors"]["fail"]);
		outlineColors[UNRECOGNIZED] = ColorFromJson(settings["textboxOutlineColors"]["unrecognized"]);

		//Load and set video processing configuration
		int framesToSkip = settings["videoFramesToSkip"];
		int videoFrameOutputInterval = settings["videoImageOutputInterval"];
		Video::setFramesToSkip(framesToSkip);
		Video::setFrameOutputInterval(videoFrameOutputInterval);
	}

	void AppSettings::setFocusMask(std::vector<cv::Rect2f> focus, std::vector<cv::Rect2f> ignore) {
		if (!focus.empty()) {
			focusMasks = focus;
		}
		else {
			focusMasks = { {0,0,1,1} }; //If there are no focus regions, we will analyse everything
		}

		ignoreMasks = ignore;
	}

	cv::Mat AppSettings::calculateMask(int width, int height) {
		//by default everything is ignored
		cv::Mat mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

		//Regions inside focus masks are not ignored
		for (cv::Rect2f rect : focusMasks) {
			cv::Rect absRect(rect.x * width, rect.y * height,
				rect.width * width, rect.height * height);

			cv::Mat subMatrix = mat(absRect);
			subMatrix.setTo(cv::Scalar(255, 255, 255));
		}

		//Ignore masks will be ignored even if inside focus regions
		for (cv::Rect2f rect : ignoreMasks) {
			cv::Rect absRect(rect.x * width, rect.y * height,
				rect.width * width, rect.height * height);

			cv::Mat subMatrix = mat(absRect);
			subMatrix.setTo(cv::Scalar(0, 0, 0));
		}
		return mat;
	}

	int AppSettings::getSpecifiedSize() const {
		return (useDPI) ? targetDPI : targetResolution;
	}

	template<typename T>
	cv::Rect_<T> AppSettings::RectFromJson(nlohmann::json data) {
		return { data["x"], data["y"], data["w"], data["h"] };
	}

	cv::Scalar AppSettings::ColorFromJson(nlohmann::json data) {
		cv::Scalar v;
		int size = data.size();
		for (int i = 0; i < 4; i++) {
			//Fills empty positions with 1s, mainly for alpha channel
			v[i] = (i<size)?static_cast<int>(data[i]):1;
		}
		//swaps from rgb to bgr
		int aux = v[2];
		v[2] = v[0];
		v[0] = aux;

		return v;
	}
}