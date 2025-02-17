//Copyright (C) 2022 Electronic Arts, Inc.  All rights reserved.

#pragma once
#include <array>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tik {

class EASTDetectionParams {

public:
	EASTDetectionParams() :
		detectionModel("frozen_east_text_detection.pb"), nmsThreshold(0.4),
		detScale(1.0), detMean({ 123.68, 116.78, 103.94 }) {};
		
	void init(json eastConfig) {
		detectionModel = eastConfig["detectionModel"];
		nmsThreshold = eastConfig["nmsThreshold"];
		detScale = eastConfig["detectionScale"];
		json mean = eastConfig["detectionMean"];
		detMean = { mean[0],mean[1] ,mean[2] };
	}

	float getNMSThreshold() const { return nmsThreshold; }
	double getDetectionScale() const { return detScale; }
	std::array<double, 3> getDetectionMean() const { return detMean; }
	std::string getDetectionModel() const { return detectionModel; }
private:
	std::string detectionModel;
	float nmsThreshold; //Non maximum supresison threshold
	double detScale; //Scaes pixel individually after mean substraction
	std::array<double, 3> detMean;//This values will be substracted from the corresponding channel

};

class DBDetectionParams {
	
public:
	DBDetectionParams() {};
	void init(json dbConfig) {
		detectionModel = dbConfig["detectionModel"];
		binThresh = dbConfig["binaryThreshold"];
		polyThresh = dbConfig["polygonThreshold"];
		maxCandidates = dbConfig["maxCandidates"];
		unclipRatio = dbConfig["unclipRatio"];
		scale = dbConfig["scale"];
		json m = dbConfig["detectionMean"];
		mean = { m[0],m[1] ,m[2] };
		json sz = dbConfig["inputSize"];
		inputSize = { sz[0],sz[1]};
			
	}
	std::string getDetectionModel() const { return detectionModel; }
	float getBinaryThreshold() const { return binThresh; }
	float getPolygonThreshold() const { return polyThresh; }
	uint getMaxCandidates() const { return maxCandidates; }
	float getUnclipRatio() const { return unclipRatio; }
	std::array<double, 3> getMean() const { return mean; }
	std::array<int, 2> getInputSize() const { return inputSize; }
	double getScale() const { return scale; }

private:
	std::string detectionModel = "DB_IC15_resnet50.onnx";
	float binThresh = 0.3;
	float polyThresh = 0.5;
	uint maxCandidates = 200;
	double unclipRatio = 2.0; //Equivalent to non max suppression
	float scale = 1.0 / 255;
	std::array<double, 3> mean = { 123.68, 116.78, 103.94 };//This values will be substracted from the corresponding channel
	std::array<int, 2> inputSize = { 736,736 };//This values will be substracted from the corresponding channel



};
class TextDetectionParams {

public:
	TextDetectionParams() : confThreshold(0.5), mergeThreshold({ 1.0,1.0 }), rotationThresholdRadians(0.17) {}

	virtual void init(json textDetection) {
		json merge = textDetection["mergeThreshold"];
		float degreeThreshold = textDetection["rotationThresholdDegrees"];

		confThreshold = textDetection["confidence"];
		mergeThreshold = std::make_pair(merge["x"], merge["y"]);
		rotationThresholdRadians = degreeThreshold * (CV_PI / 180);

		preferredBackend = getBackendParam(textDetection["preferredBackend"]);
		preferredTarget = getTargetParam(textDetection["preferredTarget"]);

		eastCfg.init(textDetection["EAST"]);
		dbCfg.init(textDetection["DB"]);
	}
		
	float getConfidenceThreshold() const { return confThreshold; }
	float getRotationThresholdRadians() const { return rotationThresholdRadians; }		
	std::pair<float, float> getMergeThreshold() const { return mergeThreshold; }
	void setMergeThreshold(std::pair<float, float> threshold) { mergeThreshold = threshold; }
	const EASTDetectionParams* getEASTParams() const { return &eastCfg; }
	const DBDetectionParams* getDBParams() const { return &dbCfg; }
	short getPreferredBackend() const { return static_cast<short>(preferredBackend); }
	short getPreferredTarget() const { return static_cast<short>(preferredTarget); }

private:
	std::string activeBackground;
	float confThreshold; //Confidence threshold	
	std::pair<float, float> mergeThreshold; //If overlap in both axes surpasses this value, textboxes will be merged
	float rotationThresholdRadians; //Text that excedes this inclination will be ignored (not part of the HUD)
	EASTDetectionParams eastCfg;
	DBDetectionParams dbCfg;

	enum class PreferredBackend { DEFAULT = 0, CUDA = 5 }; // equal to cv::dnn::Backend
	enum class PreferredTarget { CPU = 0, OPENCL = 1, CUDA = 6 }; // equal to cv::dnn::Target

	PreferredBackend preferredBackend = PreferredBackend::DEFAULT;
	PreferredTarget preferredTarget = PreferredTarget::CPU;

	PreferredBackend getBackendParam(std::string param)
	{
		return param == "CUDA" ? PreferredBackend::CUDA : PreferredBackend::DEFAULT;
		//return PreferredBackend::DEFAULT;
	}

	PreferredTarget getTargetParam(std::string param)
	{
		return param == "CUDA" ? PreferredTarget::CUDA : (param == "OPENCL" ? PreferredTarget::OPENCL : PreferredTarget::CPU);
		//return param == "OPENCL" ? PreferredTarget::OPENCL : PreferredTarget::CPU;
	}

};

}