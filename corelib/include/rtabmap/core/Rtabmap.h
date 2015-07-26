/*
Copyright (c) 2010-2014, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RTABMAP_H_
#define RTABMAP_H_

#include "rtabmap/core/RtabmapExp.h" // DLL export/import defines

#include "rtabmap/core/Parameters.h"
#include "rtabmap/core/SensorData.h"
#include "rtabmap/core/Statistics.h"
#include "rtabmap/core/Link.h"

#include <opencv2/core/core.hpp>
#include <list>
#include <stack>
#include <set>

namespace rtabmap
{

class EpipolarGeometry;
class Memory;
class BayesFilter;
class Signature;
namespace graph {
class Optimizer;
}

class RTABMAP_EXP Rtabmap
{
public:
	enum VhStrategy {kVhNone, kVhEpipolar, kVhUndef};

public:
	static std::string getVersion();
	static void readParameters(const std::string & configFile, ParametersMap & parameters);
	static void writeParameters(const std::string & configFile, const ParametersMap & parameters);

public:
	Rtabmap();
	virtual ~Rtabmap();

	bool process(const cv::Mat & image, int id=0); // for convenience, an id is automatically generated if id=0
	bool process(const SensorData & data); // for convenience

	void init(const ParametersMap & parameters, const std::string & databasePath = "");
	void init(const std::string & configFile = "", const std::string & databasePath = "");

	void close();

	const std::string & getWorkingDir() const {return _wDir;}
	int getLoopClosureId() const {return _loopClosureHypothesis.first;}
	float getLoopClosureValue() const {return _loopClosureHypothesis.second;}
	int getHighestHypothesisId() const {return _highestHypothesis.first;}
	float getHighestHypothesisValue() const {return _highestHypothesis.second;}
	int getLastLocationId() const;
	std::list<int> getWM() const; // working memory
	std::set<int> getSTM() const; // short-term memory
	int getWMSize() const; // working memory size
	int getSTMSize() const; // short-term memory size
	std::map<int, int> getWeights() const;
	int getTotalMemSize() const;
	double getLastProcessTime() const {return _lastProcessTime;};
	std::multimap<int, cv::KeyPoint> getWords(int locationId) const;
	bool isInSTM(int locationId) const;
	bool isIDsGenerated() const;
	const Statistics & getStatistics() const;
	//bool getMetricData(int locationId, cv::Mat & rgb, cv::Mat & depth, float & depthConstant, Transform & pose, Transform & localTransform) const;
	const std::map<int, Transform> & getLocalOptimizedPoses() const {return _optimizedPoses;}
	Transform getPose(int locationId) const;
	Transform getMapCorrection() const {return _mapCorrection;}
	const Memory * getMemory() const {return _memory;}
	float getGoalReachedRadius() const {return _goalReachedRadius;}
	float getLocalRadius() const {return _localRadius;}

	float getTimeThreshold() const {return _maxTimeAllowed;} // in ms
	void setTimeThreshold(float maxTimeAllowed); // in ms

	int triggerNewMap();
	bool labelLocation(int id, const std::string & label);
	bool setUserData(int id, const std::vector<unsigned char> & data);
	void generateDOTGraph(const std::string & path, int id=0, int margin=5);
	void generateTOROGraph(const std::string & path, bool optimized, bool global);
	void resetMemory();
	void dumpPrediction() const;
	void dumpData() const;
	void parseParameters(const ParametersMap & parameters);
	void setWorkingDirectory(std::string path);
	void rejectLoopClosure(int oldId, int newId);
	void get3DMap(std::map<int, Signature> & signatures,
			std::map<int, Transform> & poses,
			std::multimap<int, Link> & constraints,
			std::map<int, int> & mapIds,
			std::map<int, double> & stamps,
			std::map<int, std::string> & labels,
			std::map<int, std::vector<unsigned char> > & userDatas,
			bool optimized,
			bool global) const;
	void getGraph(std::map<int, Transform> & poses,
			std::multimap<int, Link> & constraints,
			std::map<int, int> & mapIds,
			std::map<int, double> & stamps,
			std::map<int, std::string> & labels,
			std::map<int, std::vector<unsigned char> > & userDatas,
			bool optimized,
			bool global);
	void clearPath();
	bool computePath(int targetNode, bool global);
	bool computePath(const Transform & targetPose, bool global);
	const std::vector<std::pair<int, Transform> > & getPath() const {return _path;}
	std::vector<std::pair<int, Transform> > getPathNextPoses() const;
	std::vector<int> getPathNextNodes() const;
	int getPathCurrentGoalId() const;
	const Transform & getPathTransformToGoal() const {return _pathTransformToGoal;}

	std::map<int, Transform> getForwardWMPoses(int fromId, int maxNearestNeighbors, float radius, int maxDiffID) const;
	std::list<std::map<int, Transform> > getPaths(std::map<int, Transform> poses) const;
	void adjustLikelihood(std::map<int, float> & likelihood) const;
	std::pair<int, float> selectHypothesis(const std::map<int, float> & posterior,
											const std::map<int, float> & likelihood) const;

private:
	void optimizeCurrentMap(int id,
			bool lookInDatabase,
			std::map<int, Transform> & optimizedPoses,
			std::multimap<int, Link> * constraints = 0) const;
	void updateGoalIndex();
	bool computePath(int targetNode, const std::map<int, Transform> & nodes, const std::multimap<int, rtabmap::Link> & constraints);

	void setupLogFiles(bool overwrite = false);
	void flushStatisticLogs();

private:
	// Modifiable parameters
	bool _publishStats;
	bool _publishLastSignature;
	bool _publishPdf;
	bool _publishLikelihood;
	float _maxTimeAllowed; // in ms
	unsigned int _maxMemoryAllowed; // signatures count in WM
	float _loopThr;
	float _loopRatio;
	unsigned int _maxRetrieved;
	unsigned int _maxLocalRetrieved;
	bool _statisticLogsBufferedInRAM;
	bool _statisticLogged;
	bool _statisticLoggedHeaders;
	bool _rgbdSlamMode;
	float _rgbdLinearUpdate;
	float _rgbdAngularUpdate;
	float _newMapOdomChangeDistance;
	int _globalLoopClosureIcpType;
	bool _poseScanMatching;
	bool _localLoopClosureDetectionTime;
	bool _localLoopClosureDetectionSpace;
	float _localRadius;
	int _localDetectMaxDiffID;
	float _localPathFilteringRadius;
	std::string _databasePath;
	bool _optimizeFromGraphEnd;
	bool _reextractLoopClosureFeatures;
	int _reextractNNType;
	float _reextractNNDR;
	int _reextractFeatureType;
	int _reextractMaxWords;
	bool _startNewMapOnLoopClosure;
	float _goalReachedRadius; // meters
	bool _planVirtualLinks;
	int _planVirtualLinksMaxDiffID;
	bool _goalsSavedInUserData;

	std::pair<int, float> _loopClosureHypothesis;
	std::pair<int, float> _highestHypothesis;
	double _lastProcessTime;

	// Abstract classes containing all loop closure
	// strategies for a type of signature or configuration.
	EpipolarGeometry * _epipolarGeometry;
	BayesFilter * _bayesFilter;
	graph::Optimizer * _graphOptimizer;
	ParametersMap _modifiedParameters;

	Memory * _memory;

	FILE* _foutFloat;
	FILE* _foutInt;
	std::list<std::string> _bufferedLogsF;
	std::list<std::string> _bufferedLogsI;

	Statistics statistics_;

	std::string _wDir;

	std::map<int, Transform> _optimizedPoses;
	std::multimap<int, Link> _constraints;
	Transform _mapCorrection;
	Transform _mapTransform; // for localization mode

	// Planning stuff
	std::vector<std::pair<int,Transform> > _path;
	unsigned int _pathCurrentIndex;
	unsigned int _pathGoalIndex;
	Transform _pathTransformToGoal;

};

#endif /* RTABMAP_H_ */

} // namespace rtabmap
