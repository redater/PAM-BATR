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

#ifndef ODOMETRY_H_
#define ODOMETRY_H_

#include <rtabmap/core/RtabmapExp.h>

#include <rtabmap/core/Transform.h>
#include <rtabmap/core/SensorData.h>
#include <rtabmap/core/Parameters.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

class UTimer;

namespace rtabmap {

class Feature2D;
class OdometryInfo;

class RTABMAP_EXP Odometry
{
public:
	virtual ~Odometry() {}
	Transform process(const SensorData & data, OdometryInfo * info = 0);
	virtual void reset(const Transform & initialPose = Transform::getIdentity());

	//getters
	const Transform & getPose() const {return _pose;}
	const std::string & getRoiRatios() const {return _roiRatios;}
	int getMinInliers() const {return _minInliers;}
	float getInlierDistance() const {return _inlierDistance;}
	int getIterations() const {return _iterations;}
	int getRefineIterations() const {return _refineIterations;}
	float getMaxDepth() const {return _maxDepth;}
	bool isInfoDataFilled() const {return _fillInfoData;}
	bool isPnPEstimationUsed() const {return _pnpEstimation;}
	double getPnPReprojError() const {return _pnpReprojError;}
	int  getPnPFlags() const {return _pnpFlags;}

private:
	virtual Transform computeTransform(const SensorData & image, OdometryInfo * info = 0) = 0;

private:
	std::string _roiRatios;
	int _minInliers;
	float _inlierDistance;
	int _iterations;
	int _refineIterations;
	float _maxDepth;
	int _resetCountdown;
	bool _force2D;
	bool _fillInfoData;
	bool _pnpEstimation;
	double _pnpReprojError;
	int _pnpFlags;
	Transform _pose;
	int _resetCurrentCount;

protected:
	Odometry(const rtabmap::ParametersMap & parameters);
};

class Memory;

class RTABMAP_EXP OdometryBOW : public Odometry
{
public:
	OdometryBOW(const rtabmap::ParametersMap & parameters = rtabmap::ParametersMap());
	virtual ~OdometryBOW();

	virtual void reset(const Transform & initialPose = Transform::getIdentity());
	const std::multimap<int, pcl::PointXYZ> & getLocalMap() const {return localMap_;}
	const Memory * getMemory() const {return _memory;}

private:
	virtual Transform computeTransform(const SensorData & image, OdometryInfo * info = 0);

private:
	//Parameters
	int _localHistoryMaxSize;

	Memory * _memory;
	std::multimap<int, pcl::PointXYZ> localMap_;
};

class RTABMAP_EXP OdometryOpticalFlow : public Odometry
{
public:
	OdometryOpticalFlow(const rtabmap::ParametersMap & parameters = rtabmap::ParametersMap());
	virtual ~OdometryOpticalFlow();

	virtual void reset(const Transform & initialPose = Transform::getIdentity());

	const cv::Mat & getLastFrame() const {return refFrame_;}
	const std::vector<cv::Point2f> & getLastCorners() const {return refCorners_;}
	const pcl::PointCloud<pcl::PointXYZ>::Ptr & getLastCorners3D() const {return refCorners3D_;}

private:
	virtual Transform computeTransform(const SensorData & image, OdometryInfo * info = 0);
	Transform computeTransformStereo(const SensorData & image, OdometryInfo * info);
	Transform computeTransformRGBD(const SensorData & image, OdometryInfo * info);
	Transform computeTransformMono(const SensorData & image, OdometryInfo * info);
private:
	//Parameters:
	int flowWinSize_;
	int flowIterations_;
	double flowEps_;
	int flowMaxLevel_;

	int stereoWinSize_;
	int stereoIterations_;
	double stereoEps_;
	int stereoMaxLevel_;
	float stereoMaxSlope_;

	int subPixWinSize_;
	int subPixIterations_;
	double subPixEps_;

	Feature2D * feature2D_;

	cv::Mat refFrame_;
	cv::Mat refRightFrame_;
	std::vector<cv::Point2f> refCorners_;
	pcl::PointCloud<pcl::PointXYZ>::Ptr refCorners3D_;
};

class RTABMAP_EXP OdometryMono : public Odometry
{
public:
	OdometryMono(const rtabmap::ParametersMap & parameters = rtabmap::ParametersMap());
	virtual ~OdometryMono();
	virtual void reset(const Transform & initialPose);

private:
	virtual Transform computeTransform(const SensorData & data, OdometryInfo * info = 0);
private:
	//Parameters:
	int flowWinSize_;
	int flowIterations_;
	double flowEps_;
	int flowMaxLevel_;

	Memory * memory_;
	int localHistoryMaxSize_;
	float initMinFlow_;
	float initMinTranslation_;
	float minTranslation_;
	float fundMatrixReprojError_;
	float fundMatrixConfidence_;

	cv::Mat refDepth_;
	std::map<int, cv::Point2f> cornersMap_;
	std::multimap<int, cv::Point3f> localMap_;
	std::map<int, std::multimap<int, pcl::PointXYZ> > keyFrameWords3D_;
	std::map<int, Transform> keyFramePoses_;
	float maxVariance_;
};

class RTABMAP_EXP OdometryICP : public Odometry
{
public:
	OdometryICP(int decimation = 4,
			float voxelSize = 0.005f,
			int samples = 0,
			float maxCorrespondenceDistance = 0.05f,
			int maxIterations = 30,
			float correspondenceRatio = 0.7f,
			bool pointToPlane = true,
			const ParametersMap & odometryParameter = rtabmap::ParametersMap());
	virtual void reset(const Transform & initialPose = Transform::getIdentity());

private:
	virtual Transform computeTransform(const SensorData & image, OdometryInfo * info = 0);

private:
	int _decimation;
	float _voxelSize;
	float _samples;
	float _maxCorrespondenceDistance;
	int	_maxIterations;
	float _correspondenceRatio;
	bool _pointToPlane;

	pcl::PointCloud<pcl::PointNormal>::Ptr _previousCloudNormal; // for point ot plane
	pcl::PointCloud<pcl::PointXYZ>::Ptr _previousCloud; // for point to point
};

} /* namespace rtabmap */
#endif /* ODOMETRY_H_ */
