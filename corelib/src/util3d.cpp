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

#include <rtabmap/core/EpipolarGeometry.h>
#include <rtabmap/utilite/UMath.h>
#include <rtabmap/utilite/UStl.h>
#include <rtabmap/utilite/ULogger.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl/registration/transformation_estimation_2D.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/icp_nl.h>
#include <pcl/io/pcd_io.h>
#include <pcl/common/distances.h>
#include <pcl/surface/gp3.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/surface/mls.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/segmentation/sac_segmentation.h>

#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/video/tracking.hpp>
#include <rtabmap/core/VWDictionary.h>
#include <cmath>
#include <stdio.h>

#include "rtabmap/utilite/UConversion.h"
#include "rtabmap/utilite/UTimer.h"
#include "rtabmap/core/util3d.h"
#include "rtabmap/core/Signature.h"

#include <pcl/filters/random_sample.h>

namespace rtabmap
{

namespace util3d
{

cv::Mat bgrFromCloud(const pcl::PointCloud<pcl::PointXYZRGBA> & cloud, bool bgrOrder)
{
	cv::Mat frameBGR = cv::Mat(cloud.height,cloud.width,CV_8UC3);

	for(unsigned int h = 0; h < cloud.height; h++)
	{
		for(unsigned int w = 0; w < cloud.width; w++)
		{
			if(bgrOrder)
			{
				frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).b;
				frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
				frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).r;
			}
			else
			{
				frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).r;
				frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
				frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).b;
			}
		}
	}
	return frameBGR;
}

// return float image in meter
cv::Mat depthFromCloud(
		const pcl::PointCloud<pcl::PointXYZRGBA> & cloud,
		float & fx,
		float & fy,
		bool depth16U)
{
	cv::Mat frameDepth = cv::Mat(cloud.height,cloud.width,depth16U?CV_16UC1:CV_32FC1);
	fx = 0.0f; // needed to reconstruct the cloud
	fy = 0.0f; // needed to reconstruct the cloud
	for(unsigned int h = 0; h < cloud.height; h++)
	{
		for(unsigned int w = 0; w < cloud.width; w++)
		{
			float depth = cloud.at(h*cloud.width + w).z;
			if(depth16U)
			{
				depth *= 1000.0f;
				unsigned short depthMM = 0;
				if(depth <= (float)USHRT_MAX)
				{
					depthMM = (unsigned short)depth;
				}
				frameDepth.at<unsigned short>(h,w) = depthMM;
			}
			else
			{
				frameDepth.at<float>(h,w) = depth;
			}

			// update constants
			if(fx == 0.0f &&
			   uIsFinite(cloud.at(h*cloud.width + w).x) &&
			   uIsFinite(depth) &&
			   w != cloud.width/2 &&
			   depth > 0)
			{
				fx = cloud.at(h*cloud.width + w).x / ((float(w) - float(cloud.width)/2.0f) * depth);
				if(depth16U)
				{
					fx*=1000.0f;
				}
			}
			if(fy == 0.0f &&
			   uIsFinite(cloud.at(h*cloud.width + w).y) &&
			   uIsFinite(depth) &&
			   h != cloud.height/2 &&
			   depth > 0)
			{
				fy = cloud.at(h*cloud.width + w).y / ((float(h) - float(cloud.height)/2.0f) * depth);
				if(depth16U)
				{
					fy*=1000.0f;
				}
			}
		}
	}
	return frameDepth;
}

// return (unsigned short 16bits image in mm) (float 32bits image in m)
void rgbdFromCloud(const pcl::PointCloud<pcl::PointXYZRGBA> & cloud,
		cv::Mat & frameBGR,
		cv::Mat & frameDepth,
		float & fx,
		float & fy,
		bool bgrOrder,
		bool depth16U)
{
	frameDepth = cv::Mat(cloud.height,cloud.width,depth16U?CV_16UC1:CV_32FC1);
	frameBGR = cv::Mat(cloud.height,cloud.width,CV_8UC3);

	fx = 0.0f; // needed to reconstruct the cloud
	fy = 0.0f; // needed to reconstruct the cloud
	for(unsigned int h = 0; h < cloud.height; h++)
	{
		for(unsigned int w = 0; w < cloud.width; w++)
		{
			//rgb
			if(bgrOrder)
			{
				frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).b;
				frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
				frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).r;
			}
			else
			{
				frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).r;
				frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
				frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).b;
			}

			//depth
			float depth = cloud.at(h*cloud.width + w).z;
			if(depth16U)
			{
				depth *= 1000.0f;
				unsigned short depthMM = 0;
				if(depth <= (float)USHRT_MAX)
				{
					depthMM = (unsigned short)depth;
				}
				frameDepth.at<unsigned short>(h,w) = depthMM;
			}
			else
			{
				frameDepth.at<float>(h,w) = depth;
			}

			// update constants
			if(fx == 0.0f &&
			   uIsFinite(cloud.at(h*cloud.width + w).x) &&
			   uIsFinite(depth) &&
			   w != cloud.width/2 &&
			   depth > 0)
			{
				fx = 1.0f/(cloud.at(h*cloud.width + w).x / ((float(w) - float(cloud.width)/2.0f) * depth));
				if(depth16U)
				{
					fx/=1000.0f;
				}
			}
			if(fy == 0.0f &&
			   uIsFinite(cloud.at(h*cloud.width + w).y) &&
			   uIsFinite(depth) &&
			   h != cloud.height/2 &&
			   depth > 0)
			{
				fy = 1.0f/(cloud.at(h*cloud.width + w).y / ((float(h) - float(cloud.height)/2.0f) * depth));
				if(depth16U)
				{
					fy/=1000.0f;
				}
			}
		}
	}
}

cv::Mat cvtDepthFromFloat(const cv::Mat & depth32F)
{
	UASSERT(depth32F.empty() || depth32F.type() == CV_32FC1);
	cv::Mat depth16U;
	if(!depth32F.empty())
	{
		depth16U = cv::Mat(depth32F.rows, depth32F.cols, CV_16UC1);
		for(int i=0; i<depth32F.rows; ++i)
		{
			for(int j=0; j<depth32F.cols; ++j)
			{
				float depth = (depth32F.at<float>(i,j)*1000.0f);
				unsigned short depthMM = 0;
				if(depth <= (float)USHRT_MAX)
				{
					depthMM = (unsigned short)depth;
				}
				depth16U.at<unsigned short>(i, j) = depthMM;
			}
		}
	}
	return depth16U;
}

cv::Mat cvtDepthToFloat(const cv::Mat & depth16U)
{
	UASSERT(depth16U.empty() || depth16U.type() == CV_16UC1);
	cv::Mat depth32F;
	if(!depth16U.empty())
	{
		depth32F = cv::Mat(depth16U.rows, depth16U.cols, CV_32FC1);
		for(int i=0; i<depth16U.rows; ++i)
		{
			for(int j=0; j<depth16U.cols; ++j)
			{
				float depth = float(depth16U.at<unsigned short>(i,j))/1000.0f;
				depth32F.at<float>(i, j) = depth;
			}
		}
	}
	return depth32F;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr generateKeypoints3DDepth(
		const std::vector<cv::KeyPoint> & keypoints,
		const cv::Mat & depth,
		float fx,
		float fy,
		float cx,
		float cy,
		const Transform & transform)
{
	UASSERT(!depth.empty() && (depth.type() == CV_32FC1 || depth.type() == CV_16UC1));
	pcl::PointCloud<pcl::PointXYZ>::Ptr keypoints3d(new pcl::PointCloud<pcl::PointXYZ>);
	if(!depth.empty())
	{
		keypoints3d->resize(keypoints.size());
		for(unsigned int i=0; i!=keypoints.size(); ++i)
		{
			pcl::PointXYZ pt = util3d::projectDepthTo3D(
					depth,
					keypoints[i].pt.x,
					keypoints[i].pt.y,
					cx,
					cy,
					fx,
					fy,
					true);

			if(!transform.isNull() && !transform.isIdentity())
			{
				pt = pcl::transformPoint(pt, transform.toEigen3f());
			}
			keypoints3d->at(i) = pt;
		}
	}
	return keypoints3d;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr generateKeypoints3DDisparity(
		const std::vector<cv::KeyPoint> & keypoints,
		const cv::Mat & disparity,
		float fx,
		float baseline,
		float cx,
		float cy,
		const Transform & transform)
{
	UASSERT(!disparity.empty() && (disparity.type() == CV_16SC1 || disparity.type() == CV_32F));
	pcl::PointCloud<pcl::PointXYZ>::Ptr keypoints3d(new pcl::PointCloud<pcl::PointXYZ>);
	keypoints3d->resize(keypoints.size());
	for(unsigned int i=0; i!=keypoints.size(); ++i)
	{
		pcl::PointXYZ pt = util3d::projectDisparityTo3D(
				keypoints[i].pt,
				disparity,
				cx,
				cy,
				fx,
				baseline);

		if(pcl::isFinite(pt) && !transform.isNull() && !transform.isIdentity())
		{
			pt = pcl::transformPoint(pt, transform.toEigen3f());
		}
		keypoints3d->at(i) = pt;
	}
	return keypoints3d;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr generateKeypoints3DStereo(
		const std::vector<cv::KeyPoint> & keypoints,
		const cv::Mat & leftImage,
		const cv::Mat & rightImage,
		float fx,
		float baseline,
		float cx,
		float cy,
		const Transform & transform,
		int flowWinSize,
		int flowMaxLevel,
		int flowIterations,
		double flowEps)
{
	UASSERT(!leftImage.empty() && !rightImage.empty() &&
			leftImage.type() == CV_8UC1 && rightImage.type() == CV_8UC1 &&
			leftImage.rows == rightImage.rows && leftImage.cols == rightImage.cols);

	std::vector<cv::Point2f> leftCorners;
	cv::KeyPoint::convert(keypoints, leftCorners);

	// Find features in the new left image
	std::vector<unsigned char> status;
	std::vector<float> err;
	std::vector<cv::Point2f> rightCorners;
	UDEBUG("cv::calcOpticalFlowPyrLK() begin");
	cv::calcOpticalFlowPyrLK(
			leftImage,
			rightImage,
			leftCorners,
			rightCorners,
			status,
			err,
			cv::Size(flowWinSize, flowWinSize), flowMaxLevel,
			cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, flowIterations, flowEps),
			cv::OPTFLOW_LK_GET_MIN_EIGENVALS, 1e-4);
	UDEBUG("cv::calcOpticalFlowPyrLK() end");

	pcl::PointCloud<pcl::PointXYZ>::Ptr keypoints3d(new pcl::PointCloud<pcl::PointXYZ>);
	keypoints3d->resize(keypoints.size());
	float bad_point = std::numeric_limits<float>::quiet_NaN ();
	UASSERT(status.size() == keypoints.size());
	for(unsigned int i=0; i<status.size(); ++i)
	{
		pcl::PointXYZ pt(bad_point, bad_point, bad_point);
		if(status[i])
		{
			float disparity = leftCorners[i].x - rightCorners[i].x;
			if(disparity > 0.0f)
			{
				pcl::PointXYZ tmpPt = util3d::projectDisparityTo3D(
						leftCorners[i],
						disparity,
						cx, cy, fx, baseline);

				if(pcl::isFinite(tmpPt))
				{
					pt = tmpPt;
					if(!transform.isNull() && !transform.isIdentity())
					{
						pt = pcl::transformPoint(pt, transform.toEigen3f());
					}
				}
			}
		}

		keypoints3d->at(i) = pt;
	}
	return keypoints3d;
}

// cameraTransform, from ref to next
// return 3D points in ref referential
// If cameraTransform is not null, it will be used for triangulation instead of the camera transform computed by epipolar geometry
// when refGuess3D is passed and cameraTransform is null, scale will be estimated, returning scaled cloud and camera transform
std::multimap<int, pcl::PointXYZ> generateWords3DMono(
		const std::multimap<int, cv::KeyPoint> & refWords,
		const std::multimap<int, cv::KeyPoint> & nextWords,
		float fx,
		float fy,
		float cx,
		float cy,
		const Transform & localTransform,
		Transform & cameraTransform,
		int pnpIterations,
		float pnpReprojError,
		int pnpFlags,
		float ransacParam1,
		float ransacParam2,
		const std::multimap<int, pcl::PointXYZ> & refGuess3D,
		double * varianceOut)
{
	std::multimap<int, pcl::PointXYZ> words3D;
	std::list<std::pair<int, std::pair<cv::KeyPoint, cv::KeyPoint> > > pairs;
	if(EpipolarGeometry::findPairsUnique(refWords, nextWords, pairs) > 8)
	{
		std::vector<unsigned char> status;
		cv::Mat F = EpipolarGeometry::findFFromWords(pairs, status, ransacParam1, ransacParam2);
		if(!F.empty())
		{
			//get inliers
			//normalize coordinates
			int oi = 0;
			UASSERT(status.size() == pairs.size());
			std::list<std::pair<int, std::pair<cv::KeyPoint, cv::KeyPoint> > >::iterator iter=pairs.begin();
			std::vector<cv::Point2f> refCorners(status.size());
			std::vector<cv::Point2f> newCorners(status.size());
			std::vector<int> indexes(status.size());
			for(unsigned int i=0; i<status.size(); ++i)
			{
				if(status[i])
				{
					refCorners[oi] = iter->second.first.pt;
					newCorners[oi] = iter->second.second.pt;
					indexes[oi] = iter->first;
					++oi;
				}
				++iter;
			}
			refCorners.resize(oi);
			newCorners.resize(oi);
			indexes.resize(oi);

			UDEBUG("inliers=%d/%d", oi, pairs.size());
			if(oi > 3)
			{
				std::vector<cv::Point2f> refCornersRefined;
				std::vector<cv::Point2f> newCornersRefined;
				cv::correctMatches(F, refCorners, newCorners, refCornersRefined, newCornersRefined);
				refCorners = refCornersRefined;
				newCorners = newCornersRefined;

				cv::Mat x(3, (int)refCorners.size(), CV_64FC1);
				cv::Mat xp(3, (int)refCorners.size(), CV_64FC1);
				for(unsigned int i=0; i<refCorners.size(); ++i)
				{
					x.at<double>(0, i) = refCorners[i].x;
					x.at<double>(1, i) = refCorners[i].y;
					x.at<double>(2, i) = 1;

					xp.at<double>(0, i) = newCorners[i].x;
					xp.at<double>(1, i) = newCorners[i].y;
					xp.at<double>(2, i) = 1;
				}

				cv::Mat K = (cv::Mat_<double>(3,3) <<
					fx, 0, cx,
					0, fy, cy,
					0, 0, 1);
				cv::Mat Kinv = K.inv();
				cv::Mat E = K.t()*F*K;
				cv::Mat x_norm = Kinv * x;
				cv::Mat xp_norm = Kinv * xp;
				x_norm = x_norm.rowRange(0,2);
				xp_norm = xp_norm.rowRange(0,2);

				cv::Mat P = EpipolarGeometry::findPFromE(E, x_norm, xp_norm);
				if(!P.empty())
				{
					cv::Mat P0 = cv::Mat::zeros(3, 4, CV_64FC1);
					P0.at<double>(0,0) = 1;
					P0.at<double>(1,1) = 1;
					P0.at<double>(2,2) = 1;

					bool useCameraTransformGuess = !cameraTransform.isNull();
					//if camera transform is set, use it instead of the computed one from epipolar geometry
					if(useCameraTransformGuess)
					{
						Transform t = (localTransform.inverse()*cameraTransform*localTransform).inverse();
						P = (cv::Mat_<double>(3,4) <<
								(double)t.r11(), (double)t.r12(), (double)t.r13(), (double)t.x(),
								(double)t.r21(), (double)t.r22(), (double)t.r23(), (double)t.y(),
								(double)t.r31(), (double)t.r32(), (double)t.r33(), (double)t.z());
					}

					// triangulate the points
					//std::vector<double> reprojErrors;
					//pcl::PointCloud<pcl::PointXYZ>::Ptr cloud;
					//EpipolarGeometry::triangulatePoints(x_norm, xp_norm, P0, P, cloud, reprojErrors);
					cv::Mat pts4D;
					cv::triangulatePoints(P0, P, x_norm, xp_norm, pts4D);

					for(unsigned int i=0; i<indexes.size(); ++i)
					{
						//if(cloud->at(i).z > 0)
						//{
						//	words3D.insert(std::make_pair(indexes[i], util3d::transformPoint(cloud->at(i), localTransform)));
						//}
						pts4D.col(i) /= pts4D.at<double>(3,i);
						if(pts4D.at<double>(2,i) > 0)
						{
							words3D.insert(std::make_pair(indexes[i], util3d::transformPoint(pcl::PointXYZ(pts4D.at<double>(0,i), pts4D.at<double>(1,i), pts4D.at<double>(2,i)), localTransform)));
						}
					}

					if(!useCameraTransformGuess)
					{
						cv::Mat R, T;
						EpipolarGeometry::findRTFromP(P, R, T);

						Transform t(R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), T.at<double>(0),
									R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), T.at<double>(1),
									R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), T.at<double>(2));

						cameraTransform = (localTransform * t).inverse() * localTransform;
					}

					if(refGuess3D.size())
					{
						// scale estimation
						pcl::PointCloud<pcl::PointXYZ>::Ptr inliersRef(new pcl::PointCloud<pcl::PointXYZ>);
						pcl::PointCloud<pcl::PointXYZ>::Ptr inliersRefGuess(new pcl::PointCloud<pcl::PointXYZ>);
						util3d::findCorrespondences(
								words3D,
								refGuess3D,
								*inliersRef,
								*inliersRefGuess,
								0);

						if(inliersRef->size())
						{
							// estimate the scale
							float scale = 1.0f;
							float variance = 1.0f;
							if(!useCameraTransformGuess)
							{
								std::multimap<float, float> scales; // <variance, scale>
								for(unsigned int i=0; i<inliersRef->size(); ++i)
								{
									// using x as depth, assuming we are in global referential
									float s = inliersRefGuess->at(i).x/inliersRef->at(i).x;
									std::vector<float> errorSqrdDists(inliersRef->size());
									for(unsigned int j=0; j<inliersRef->size(); ++j)
									{
										pcl::PointXYZ refPt = inliersRef->at(j);
										refPt.x *= s;
										refPt.y *= s;
										refPt.z *= s;
										const pcl::PointXYZ & newPt = inliersRefGuess->at(j);
										errorSqrdDists[j] = uNormSquared(refPt.x-newPt.x, refPt.y-newPt.y, refPt.z-newPt.z);
									}
									std::sort(errorSqrdDists.begin(), errorSqrdDists.end());
									double median_error_sqr = (double)errorSqrdDists[errorSqrdDists.size () >> 1];
									float var = 2.1981 * median_error_sqr;
									//UDEBUG("scale %d = %f variance = %f", (int)i, s, variance);

									scales.insert(std::make_pair(var, s));
								}
								scale = scales.begin()->second;
								variance = scales.begin()->first;;
							}
							else
							{
								//compute variance at scale=1
								std::vector<float> errorSqrdDists(inliersRef->size());
								for(unsigned int j=0; j<inliersRef->size(); ++j)
								{
									const pcl::PointXYZ & refPt = inliersRef->at(j);
									const pcl::PointXYZ & newPt = inliersRefGuess->at(j);
									errorSqrdDists[j] = uNormSquared(refPt.x-newPt.x, refPt.y-newPt.y, refPt.z-newPt.z);
								}
								std::sort(errorSqrdDists.begin(), errorSqrdDists.end());
								double median_error_sqr = (double)errorSqrdDists[errorSqrdDists.size () >> 1];
								 variance = 2.1981 * median_error_sqr;
							}

							UDEBUG("scale used = %f (variance=%f)", scale, variance);
							if(varianceOut)
							{
								*varianceOut = variance;
							}

							if(!useCameraTransformGuess)
							{
								std::vector<cv::Point3f> objectPoints(indexes.size());
								std::vector<cv::Point2f> imagePoints(indexes.size());
								int oi=0;
								for(unsigned int i=0; i<indexes.size(); ++i)
								{
									std::multimap<int, pcl::PointXYZ>::iterator iter = words3D.find(indexes[i]);
									if(pcl::isFinite(iter->second))
									{
										iter->second.x *= scale;
										iter->second.y *= scale;
										iter->second.z *= scale;
										objectPoints[oi].x = iter->second.x;
										objectPoints[oi].y = iter->second.y;
										objectPoints[oi].z = iter->second.z;
										imagePoints[oi] = newCorners[i];
										++oi;
									}
								}
								objectPoints.resize(oi);
								imagePoints.resize(oi);

								//PnPRansac
								Transform guess = localTransform.inverse();
								cv::Mat R = (cv::Mat_<double>(3,3) <<
										(double)guess.r11(), (double)guess.r12(), (double)guess.r13(),
										(double)guess.r21(), (double)guess.r22(), (double)guess.r23(),
										(double)guess.r31(), (double)guess.r32(), (double)guess.r33());
								cv::Mat rvec(1,3, CV_64FC1);
								cv::Rodrigues(R, rvec);
								cv::Mat tvec = (cv::Mat_<double>(1,3) << (double)guess.x(), (double)guess.y(), (double)guess.z());
								std::vector<int> inliersV;
								cv::solvePnPRansac(
										objectPoints,
										imagePoints,
										K,
										cv::Mat(),
										rvec,
										tvec,
										true,
										pnpIterations,
										pnpReprojError,
										0,
										inliersV,
										pnpFlags);

								UDEBUG("PnP inliers = %d / %d", (int)inliersV.size(), (int)objectPoints.size());

								if(inliersV.size())
								{
									cv::Rodrigues(rvec, R);
									Transform pnp(R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), tvec.at<double>(0),
												   R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), tvec.at<double>(1),
												   R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), tvec.at<double>(2));

									cameraTransform = (localTransform * pnp).inverse();
								}
								else
								{
									UWARN("No inliers after PnP!");
									cameraTransform = Transform();
								}
							}
						}
						else
						{
							UWARN("Cannot compute the scale, no points corresponding between the generated ref words and words guess");
						}
					}
				}
			}
		}
	}
	UDEBUG("wordsSet=%d / %d", (int)words3D.size(), (int)refWords.size());

	return words3D;
}

std::multimap<int, cv::KeyPoint> aggregate(
		const std::list<int> & wordIds,
		const std::vector<cv::KeyPoint> & keypoints)
{
	std::multimap<int, cv::KeyPoint> words;
	std::vector<cv::KeyPoint>::const_iterator kpIter = keypoints.begin();
	for(std::list<int>::const_iterator iter=wordIds.begin(); iter!=wordIds.end(); ++iter)
	{
		words.insert(std::pair<int, cv::KeyPoint >(*iter, *kpIter));
		++kpIter;
	}
	return words;
}

std::list<std::pair<cv::Point2f, cv::Point2f> > findCorrespondences(
		const std::multimap<int, cv::KeyPoint> & words1,
		const std::multimap<int, cv::KeyPoint> & words2)
{
	std::list<std::pair<cv::Point2f, cv::Point2f> > correspondences;

	// Find pairs
	std::list<std::pair<int, std::pair<cv::KeyPoint, cv::KeyPoint> > > pairs;
	rtabmap::EpipolarGeometry::findPairsUnique(words1, words2, pairs);

	if(pairs.size() > 7) // 8 min?
	{
		// Find fundamental matrix
		std::vector<uchar> status;
		cv::Mat fundamentalMatrix = rtabmap::EpipolarGeometry::findFFromWords(pairs, status);
		//ROS_INFO("inliers = %d/%d", uSum(status), pairs.size());
		if(!fundamentalMatrix.empty())
		{
			int i = 0;
			//int goodCount = 0;
			for(std::list<std::pair<int, std::pair<cv::KeyPoint, cv::KeyPoint> > >::iterator iter=pairs.begin(); iter!=pairs.end(); ++iter)
			{
				if(status[i])
				{
					correspondences.push_back(std::pair<cv::Point2f, cv::Point2f>(iter->second.first.pt, iter->second.second.pt));
					//ROS_INFO("inliers kpts %f %f vs %f %f", iter->second.first.pt.x, iter->second.first.pt.y, iter->second.second.pt.x, iter->second.second.pt.y);
				}
				++i;
			}
		}
	}
	return correspondences;
}

void findCorrespondences(
		const std::multimap<int, pcl::PointXYZ> & words1,
		const std::multimap<int, pcl::PointXYZ> & words2,
		pcl::PointCloud<pcl::PointXYZ> & inliers1,
		pcl::PointCloud<pcl::PointXYZ> & inliers2,
		float maxDepth,
		std::set<int> * uniqueCorrespondences)
{
	std::list<int> ids = uUniqueKeys(words1);
	// Find pairs
	inliers1.resize(ids.size());
	inliers2.resize(ids.size());

	int oi=0;
	for(std::list<int>::iterator iter=ids.begin(); iter!=ids.end(); ++iter)
	{
		if(words1.count(*iter) == 1 && words2.count(*iter) == 1)
		{
			inliers1[oi] = words1.find(*iter)->second;
			inliers2[oi] = words2.find(*iter)->second;
			if(pcl::isFinite(inliers1[oi]) &&
			   pcl::isFinite(inliers2[oi]) &&
			   (inliers1[oi].x != 0 || inliers1[oi].y != 0 || inliers1[oi].z != 0) &&
			   (inliers2[oi].x != 0 || inliers2[oi].y != 0 || inliers2[oi].z != 0) &&
			   (maxDepth <= 0 || (inliers1[oi].x > 0 && inliers1[oi].x <= maxDepth && inliers2[oi].x>0 &&inliers2[oi].x<=maxDepth)))
			{
				++oi;
				if(uniqueCorrespondences)
				{
					uniqueCorrespondences->insert(*iter);
				}
			}
		}
	}
	inliers1.resize(oi);
	inliers2.resize(oi);
}

pcl::PointXYZ projectDepthTo3D(
		const cv::Mat & depthImage,
		float x, float y,
		float cx, float cy,
		float fx, float fy,
		bool smoothing,
		float maxZError)
{
	UASSERT(depthImage.type() == CV_16UC1 || depthImage.type() == CV_32FC1);

	pcl::PointXYZ pt;
	float bad_point = std::numeric_limits<float>::quiet_NaN ();

	int u = int(x+0.5f);
	int v = int(y+0.5f);

	if(!(u >=0 && u<depthImage.cols && v >=0 && v<depthImage.rows))
	{
		UERROR("!(x >=0 && x<depthImage.cols && y >=0 && y<depthImage.rows) cond failed! returning bad point. (x=%f (u=%d), y=%f (v=%d), cols=%d, rows=%d)",
				x,u,y,v,depthImage.cols, depthImage.rows);
		pt.x = pt.y = pt.z = bad_point;
		return pt;
	}

	bool isInMM = depthImage.type() == CV_16UC1; // is in mm?

	// Inspired from RGBDFrame::getGaussianMixtureDistribution() method from
	// https://github.com/ccny-ros-pkg/rgbdtools/blob/master/src/rgbd_frame.cpp
	// Window weights:
	//  | 1 | 2 | 1 |
	//  | 2 | 4 | 2 |
	//  | 1 | 2 | 1 |
	int u_start = std::max(u-1, 0);
	int v_start = std::max(v-1, 0);
	int u_end = std::min(u+1, depthImage.cols-1);
	int v_end = std::min(v+1, depthImage.rows-1);

	float depth = isInMM?(float)depthImage.at<uint16_t>(v,u)*0.001f:depthImage.at<float>(v,u);
	if(depth!=0.0f && uIsFinite(depth))
	{
		if(smoothing)
		{
			float sumWeights = 0.0f;
			float sumDepths = 0.0f;
			for(int uu = u_start; uu <= u_end; ++uu)
			{
				for(int vv = v_start; vv <= v_end; ++vv)
				{
					if(!(uu == u && vv == v))
					{
						float d = isInMM?(float)depthImage.at<uint16_t>(vv,uu)*0.001f:depthImage.at<float>(vv,uu);
						// ignore if not valid or depth difference is too high
						if(d != 0.0f && uIsFinite(d) && fabs(d - depth) < maxZError)
						{
							if(uu == u || vv == v)
							{
								sumWeights+=2.0f;
								d*=2.0f;
							}
							else
							{
								sumWeights+=1.0f;
							}
							sumDepths += d;
						}
					}
				}
			}
			// set window weight to center point
			depth *= 4.0f;
			sumWeights += 4.0f;

			// mean
			depth = (depth+sumDepths)/sumWeights;
		}

		// Use correct principal point from calibration
		cx = cx > 0.0f ? cx : float(depthImage.cols/2) - 0.5f; //cameraInfo.K.at(2)
		cy = cy > 0.0f ? cy : float(depthImage.rows/2) - 0.5f; //cameraInfo.K.at(5)

		// Fill in XYZ
		pt.x = (x - cx) * depth / fx;
		pt.y = (y - cy) * depth / fy;
		pt.z = depth;
	}
	else
	{
		pt.x = pt.y = pt.z = bad_point;
	}
	return pt;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr cloudFromDepth(
		const cv::Mat & imageDepth,
		float cx, float cy,
		float fx, float fy,
		int decimation)
{
	UASSERT(!imageDepth.empty() && (imageDepth.type() == CV_16UC1 || imageDepth.type() == CV_32FC1));
	UASSERT(imageDepth.rows % decimation == 0);
	UASSERT(imageDepth.cols % decimation == 0);

	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	if(decimation < 1)
	{
		return cloud;
	}

	//cloud.header = cameraInfo.header;
	cloud->height = imageDepth.rows/decimation;
	cloud->width  = imageDepth.cols/decimation;
	cloud->is_dense = false;

	cloud->resize(cloud->height * cloud->width);

	int count = 0 ;

	for(int h = 0; h < imageDepth.rows; h+=decimation)
	{
		for(int w = 0; w < imageDepth.cols; w+=decimation)
		{
			pcl::PointXYZ & pt = cloud->at((h/decimation)*cloud->width + (w/decimation));

			pcl::PointXYZ ptXYZ = projectDepthTo3D(imageDepth, w, h, cx, cy, fx, fy, false);
			pt.x = ptXYZ.x;
			pt.y = ptXYZ.y;
			pt.z = ptXYZ.z;
			++count;
		}
	}

	return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudFromDepthRGB(
		const cv::Mat & imageRgb,
		const cv::Mat & imageDepth,
		float cx, float cy,
		float fx, float fy,
		int decimation)
{
	UASSERT(imageRgb.rows == imageDepth.rows && imageRgb.cols == imageDepth.cols);
	UASSERT(!imageDepth.empty() && (imageDepth.type() == CV_16UC1 || imageDepth.type() == CV_32FC1));
	UASSERT(imageDepth.rows % decimation == 0);
	UASSERT(imageDepth.cols % decimation == 0);

	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
	if(decimation < 1)
	{
		return cloud;
	}

	bool mono;
	if(imageRgb.channels() == 3) // BGR
	{
		mono = false;
	}
	else if(imageRgb.channels() == 1) // Mono
	{
		mono = true;
	}
	else
	{
		return cloud;
	}

	//cloud.header = cameraInfo.header;
	cloud->height = imageDepth.rows/decimation;
	cloud->width  = imageDepth.cols/decimation;
	cloud->is_dense = false;
	cloud->resize(cloud->height * cloud->width);

	for(int h = 0; h < imageDepth.rows && h/decimation < (int)cloud->height; h+=decimation)
	{
		for(int w = 0; w < imageDepth.cols && w/decimation < (int)cloud->width; w+=decimation)
		{
			pcl::PointXYZRGB & pt = cloud->at((h/decimation)*cloud->width + (w/decimation));
			if(!mono)
			{
				pt.b = imageRgb.at<cv::Vec3b>(h,w)[0];
				pt.g = imageRgb.at<cv::Vec3b>(h,w)[1];
				pt.r = imageRgb.at<cv::Vec3b>(h,w)[2];
			}
			else
			{
				unsigned char v = imageRgb.at<unsigned char>(h,w);
				pt.b = v;
				pt.g = v;
				pt.r = v;
			}

			pcl::PointXYZ ptXYZ = projectDepthTo3D(imageDepth, w, h, cx, cy, fx, fy, false);
			pt.x = ptXYZ.x;
			pt.y = ptXYZ.y;
			pt.z = ptXYZ.z;
		}
	}
	return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr cloudFromDisparity(
		const cv::Mat & imageDisparity,
		float cx, float cy,
		float fx, float baseline,
		int decimation)
{
	UASSERT(imageDisparity.type() == CV_32FC1 || imageDisparity.type()==CV_16SC1);
	UASSERT(imageDisparity.rows % decimation == 0);
	UASSERT(imageDisparity.cols % decimation == 0);

	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	if(decimation < 1)
	{
		return cloud;
	}

	//cloud.header = cameraInfo.header;
	cloud->height = imageDisparity.rows/decimation;
	cloud->width  = imageDisparity.cols/decimation;
	cloud->is_dense = false;
	cloud->resize(cloud->height * cloud->width);

	if(imageDisparity.type()==CV_16SC1)
	{
		for(int h = 0; h < imageDisparity.rows && h/decimation < (int)cloud->height; h+=decimation)
		{
			for(int w = 0; w < imageDisparity.cols && w/decimation < (int)cloud->width; w+=decimation)
			{
				float disp = float(imageDisparity.at<short>(h,w))/16.0f;
				cloud->at((h/decimation)*cloud->width + (w/decimation)) = projectDisparityTo3D(cv::Point2f(w, h), disp, cx, cy, fx, baseline);
			}
		}
	}
	else
	{
		for(int h = 0; h < imageDisparity.rows && h/decimation < (int)cloud->height; h+=decimation)
		{
			for(int w = 0; w < imageDisparity.cols && w/decimation < (int)cloud->width; w+=decimation)
			{
				float disp = imageDisparity.at<float>(h,w);
				cloud->at((h/decimation)*cloud->width + (w/decimation)) = projectDisparityTo3D(cv::Point2f(w, h), disp, cx, cy, fx, baseline);
			}
		}
	}
	return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudFromDisparityRGB(
		const cv::Mat & imageRgb,
		const cv::Mat & imageDisparity,
		float cx, float cy,
		float fx, float baseline,
		int decimation)
{
	UASSERT(imageRgb.rows == imageDisparity.rows &&
			imageRgb.cols == imageDisparity.cols &&
			(imageDisparity.type() == CV_32FC1 || imageDisparity.type()==CV_16SC1));
	UASSERT(imageDisparity.rows % decimation == 0);
	UASSERT(imageDisparity.cols % decimation == 0);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
	if(decimation < 1)
	{
		return cloud;
	}

	bool mono;
	if(imageRgb.channels() == 3) // BGR
	{
		mono = false;
	}
	else if(imageRgb.channels() == 1) // Mono
	{
		mono = true;
	}
	else
	{
		return cloud;
	}

	//cloud.header = cameraInfo.header;
	cloud->height = imageRgb.rows/decimation;
	cloud->width  = imageRgb.cols/decimation;
	cloud->is_dense = false;
	cloud->resize(cloud->height * cloud->width);

	for(int h = 0; h < imageRgb.rows && h/decimation < (int)cloud->height; h+=decimation)
	{
		for(int w = 0; w < imageRgb.cols && w/decimation < (int)cloud->width; w+=decimation)
		{
			pcl::PointXYZRGB & pt = cloud->at((h/decimation)*cloud->width + (w/decimation));
			if(!mono)
			{
				pt.b = imageRgb.at<cv::Vec3b>(h,w)[0];
				pt.g = imageRgb.at<cv::Vec3b>(h,w)[1];
				pt.r = imageRgb.at<cv::Vec3b>(h,w)[2];
			}
			else
			{
				unsigned char v = imageRgb.at<unsigned char>(h,w);
				pt.b = v;
				pt.g = v;
				pt.r = v;
			}

			float disp = imageDisparity.type()==CV_16SC1?float(imageDisparity.at<short>(h,w))/16.0f:imageDisparity.at<float>(h,w);
			pcl::PointXYZ ptXYZ = projectDisparityTo3D(cv::Point2f(w, h), disp, cx, cy, fx, baseline);
			pt.x = ptXYZ.x;
			pt.y = ptXYZ.y;
			pt.z = ptXYZ.z;
		}
	}
	return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudFromStereoImages(
		const cv::Mat & imageLeft,
		const cv::Mat & imageRight,
		float cx, float cy,
		float fx, float baseline,
		int decimation)
{
	UASSERT(imageRight.type() == CV_8UC1);

	cv::Mat leftMono;
	if(imageLeft.channels() == 3)
	{
		cv::cvtColor(imageLeft, leftMono, CV_BGR2GRAY);
	}
	else
	{
		leftMono = imageLeft;
	}
	return rtabmap::util3d::cloudFromDisparityRGB(
			imageLeft,
			util3d::disparityFromStereoImages(leftMono, imageRight),
			cx, cy,
			fx, baseline,
			decimation);
}

cv::Mat disparityFromStereoImages(
		const cv::Mat & leftImage,
		const cv::Mat & rightImage)
{
	UASSERT(!leftImage.empty() && !rightImage.empty() &&
			(leftImage.type() == CV_8UC1 || leftImage.type() == CV_8UC3) && rightImage.type() == CV_8UC1 &&
			leftImage.cols == rightImage.cols &&
			leftImage.rows == rightImage.rows);

	cv::Mat leftMono;
	if(leftImage.channels() == 3)
	{
		cv::cvtColor(leftImage, leftMono, CV_BGR2GRAY);
	}
	else
	{
		leftMono = leftImage;
	}

	cv::StereoBM stereo(cv::StereoBM::BASIC_PRESET);
	stereo.state->SADWindowSize = 15;
	stereo.state->minDisparity = 0;
	stereo.state->numberOfDisparities = 64;
	stereo.state->preFilterSize = 9;
	stereo.state->preFilterCap = 31;
	stereo.state->uniquenessRatio = 15;
	stereo.state->textureThreshold = 10;
	stereo.state->speckleWindowSize = 100;
	stereo.state->speckleRange = 4;
	cv::Mat disparity;
	stereo(leftMono, rightImage, disparity, CV_16SC1);
	return disparity;
}

cv::Mat disparityFromStereoImages(
		const cv::Mat & leftImage,
		const cv::Mat & rightImage,
		const std::vector<cv::Point2f> & leftCorners,
		int flowWinSize,
		int flowMaxLevel,
		int flowIterations,
		double flowEps,
		float maxCorrespondencesSlope)
{
	UASSERT(!leftImage.empty() && !rightImage.empty() &&
			leftImage.type() == CV_8UC1 && rightImage.type() == CV_8UC1 &&
			leftImage.cols == rightImage.cols &&
			leftImage.rows == rightImage.rows);

	// Find features in the new left image
	std::vector<unsigned char> status;
	std::vector<float> err;
	std::vector<cv::Point2f> rightCorners;
	UDEBUG("cv::calcOpticalFlowPyrLK() begin");
	cv::calcOpticalFlowPyrLK(
			leftImage,
			rightImage,
			leftCorners,
			rightCorners,
			status,
			err,
			cv::Size(flowWinSize, flowWinSize), flowMaxLevel,
			cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, flowIterations, flowEps),
			cv::OPTFLOW_LK_GET_MIN_EIGENVALS, 1e-4);
	UDEBUG("cv::calcOpticalFlowPyrLK() end");

	return disparityFromStereoCorrespondences(leftImage, leftCorners, rightCorners, status, maxCorrespondencesSlope);
}

cv::Mat depthFromStereoImages(
		const cv::Mat & leftImage,
		const cv::Mat & rightImage,
		const std::vector<cv::Point2f> & leftCorners,
		float fx,
		float baseline,
		int flowWinSize,
		int flowMaxLevel,
		int flowIterations,
		double flowEps)
{
	UASSERT(!leftImage.empty() && !rightImage.empty() &&
			leftImage.type() == CV_8UC1 && rightImage.type() == CV_8UC1 &&
			leftImage.cols == rightImage.cols &&
			leftImage.rows == rightImage.rows);
	UASSERT(fx > 0.0f && baseline > 0.0f);

	// Find features in the new left image
	std::vector<unsigned char> status;
	std::vector<float> err;
	std::vector<cv::Point2f> rightCorners;
	UDEBUG("cv::calcOpticalFlowPyrLK() begin");
	cv::calcOpticalFlowPyrLK(
			leftImage,
			rightImage,
			leftCorners,
			rightCorners,
			status,
			err,
			cv::Size(flowWinSize, flowWinSize), flowMaxLevel,
			cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, flowIterations, flowEps),
			cv::OPTFLOW_LK_GET_MIN_EIGENVALS, 1e-4);
	UDEBUG("cv::calcOpticalFlowPyrLK() end");

	return depthFromStereoCorrespondences(leftImage, leftCorners, rightCorners, status, fx, baseline);
}

cv::Mat disparityFromStereoCorrespondences(
		const cv::Mat & leftImage,
		const std::vector<cv::Point2f> & leftCorners,
		const std::vector<cv::Point2f> & rightCorners,
		const std::vector<unsigned char> & mask,
		float maxSlope)
{
	UASSERT(!leftImage.empty() && leftCorners.size() == rightCorners.size());
	UASSERT(mask.size() == 0 || mask.size() == leftCorners.size());
	cv::Mat disparity = cv::Mat::zeros(leftImage.rows, leftImage.cols, CV_32FC1);
	for(unsigned int i=0; i<leftCorners.size(); ++i)
	{
		if(mask.size() == 0 || mask[i])
		{
			float d = leftCorners[i].x - rightCorners[i].x;
			float slope = fabs((leftCorners[i].y - rightCorners[i].y) / (leftCorners[i].x - rightCorners[i].x));
			if(d > 0.0f && slope < maxSlope)
			{
				disparity.at<float>(int(leftCorners[i].y+0.5f), int(leftCorners[i].x+0.5f)) = d;
			}
		}
	}
	return disparity;
}

cv::Mat depthFromStereoCorrespondences(
		const cv::Mat & leftImage,
		const std::vector<cv::Point2f> & leftCorners,
		const std::vector<cv::Point2f> & rightCorners,
		const std::vector<unsigned char> & mask,
		float fx, float baseline)
{
	UASSERT(!leftImage.empty() && leftCorners.size() == rightCorners.size());
	UASSERT(mask.size() == 0 || mask.size() == leftCorners.size());
	cv::Mat depth = cv::Mat::zeros(leftImage.rows, leftImage.cols, CV_32FC1);
	for(unsigned int i=0; i<leftCorners.size(); ++i)
	{
		if(mask.size() == 0 || mask[i])
		{
			float disparity = leftCorners[i].x - rightCorners[i].x;
			if(disparity > 0.0f)
			{
				float d = baseline * fx / disparity;
				depth.at<float>(int(leftCorners[i].y+0.5f), int(leftCorners[i].x+0.5f)) = d;
			}
		}
	}
	return depth;
}

// inspired from ROS image_geometry/src/stereo_camera_model.cpp
pcl::PointXYZ projectDisparityTo3D(
		const cv::Point2f & pt,
		float disparity,
		float cx, float cy, float fx, float baseline)
{
	if(disparity > 0.0f && baseline > 0.0f && fx > 0.0f)
	{
		float W = disparity/baseline;// + (right_.cx() - left_.cx()) / Tx;
		return pcl::PointXYZ((pt.x - cx)/W, (pt.y - cy)/W, fx/W);
	}
	float bad_point = std::numeric_limits<float>::quiet_NaN ();
	return pcl::PointXYZ(bad_point, bad_point, bad_point);
}

pcl::PointXYZ projectDisparityTo3D(
		const cv::Point2f & pt,
		const cv::Mat & disparity,
		float cx, float cy, float fx, float baseline)
{
	UASSERT(!disparity.empty() && (disparity.type() == CV_32FC1 || disparity.type() == CV_16SC1));
	int u = int(pt.x+0.5f);
	int v = int(pt.y+0.5f);
	float bad_point = std::numeric_limits<float>::quiet_NaN ();
	if(uIsInBounds(u, 0, disparity.cols) &&
	   uIsInBounds(v, 0, disparity.rows))
	{
		float d = disparity.type() == CV_16SC1?float(disparity.at<short>(v,u))/16.0f:disparity.at<float>(v,u);
		return projectDisparityTo3D(pt, d, cx, cy, fx, baseline);
	}
	return pcl::PointXYZ(bad_point, bad_point, bad_point);
}

cv::Mat depthFromDisparity(const cv::Mat & disparity,
		float fx, float baseline,
		int type)
{
	UASSERT(!disparity.empty() && (disparity.type() == CV_32FC1 || disparity.type() == CV_16SC1));
	UASSERT(type == CV_32FC1 || type == CV_16U);
	cv::Mat depth = cv::Mat::zeros(disparity.rows, disparity.cols, type);
	for (int i = 0; i < disparity.rows; i++)
	{
		for (int j = 0; j < disparity.cols; j++)
		{
			float disparity_value = disparity.type() == CV_16SC1?float(disparity.at<short>(i,j))/16.0f:disparity.at<float>(i,j);
			if (disparity_value > 0.0f)
			{
				// baseline * focal / disparity
				float d = baseline * fx / disparity_value;
				if(depth.type() == CV_32FC1)
				{
					depth.at<float>(i,j) = d;
				}
				else
				{
					depth.at<unsigned short>(i,j) = (unsigned short)(d*1000.0f);
				}
			}
		}
	}
	return depth;
}

cv::Mat registerDepth(
		const cv::Mat & depth,
		const cv::Mat & depthK,
		const cv::Mat & colorK,
		const rtabmap::Transform & transform)
{
	UASSERT(!transform.isNull());
	UASSERT(!depth.empty());
	UASSERT(depth.type() == CV_16UC1); // mm
	UASSERT(depthK.type() == CV_64FC1 && depthK.cols == 3 && depthK.cols == 3);
	UASSERT(colorK.type() == CV_64FC1 && colorK.cols == 3 && colorK.cols == 3);

	float fx = depthK.at<double>(0,0);
	float fy = depthK.at<double>(1,1);
	float cx = depthK.at<double>(0,2);
	float cy = depthK.at<double>(1,2);

	float rfx = colorK.at<double>(0,0);
	float rfy = colorK.at<double>(1,1);
	float rcx = colorK.at<double>(0,2);
	float rcy = colorK.at<double>(1,2);

	Eigen::Affine3f proj = transform.toEigen3f();
	Eigen::Vector4f P4,P3;
	P4[3] = 1;
	cv::Mat registered = cv::Mat::zeros(depth.rows, depth.cols, depth.type());

	for(int y=0; y<depth.rows; ++y)
	{
		for(int x=0; x<depth.cols; ++x)
		{
			//filtering
			float dz = float(depth.at<unsigned short>(y,x))*0.001f; // put in meter for projection
			if(dz>=0.0f)
			{
				// Project to 3D
				P4[0] = (x - cx) * dz / fx; // Optimization: we could have (x-cx)/fx in a lookup table
				P4[1] = (y - cy) * dz / fy; // Optimization: we could have (y-cy)/fy in a lookup table
				P4[2] = dz;

				P3 = proj * P4;
				float z = P3[2];
				float invZ = 1.0f/z;
				int dx = (rfx*P3[0])*invZ + rcx;
				int dy = (rfy*P3[1])*invZ + rcy;

				if(uIsInBounds(dx, 0, registered.cols) && uIsInBounds(dy, 0, registered.rows))
				{
					unsigned short z16 = z * 1000; //mm
					unsigned short &zReg = registered.at<unsigned short>(dy, dx);
					if(zReg == 0 || z16 < zReg)
					{
						zReg = z16;
					}
				}
			}
		}
	}
	return registered;
}

void fillRegisteredDepthHoles(cv::Mat & registeredDepth, bool vertical, bool horizontal, bool fillDoubleHoles)
{
	UASSERT(registeredDepth.type() == CV_16UC1);
	int margin = fillDoubleHoles?2:1;
	for(int x=1; x<registeredDepth.cols-margin; ++x)
	{
		for(int y=1; y<registeredDepth.rows-margin; ++y)
		{
			unsigned short & b = registeredDepth.at<unsigned short>(y, x);
			bool set = false;
			if(vertical)
			{
				const unsigned short & a = registeredDepth.at<unsigned short>(y-1, x);
				unsigned short & c = registeredDepth.at<unsigned short>(y+1, x);
				if(a && c)
				{
					unsigned short error = 0.01*((a+c)/2);
					if(((b == 0 && a && c) || (b > a+error && b > c+error)) &&
						(a>c?a-c<=error:c-a<=error))
					{
						b = (a+c)/2;
						set = true;
						if(!horizontal)
						{
							++y;
						}
					}
				}
				if(!set && fillDoubleHoles)
				{
					const unsigned short & d = registeredDepth.at<unsigned short>(y+2, x);
					if(a && d && (b==0 || c==0))
					{
						unsigned short error = 0.01*((a+d)/2);
						if(((b == 0 && a && d) || (b > a+error && b > d+error)) &&
						   ((c == 0 && a && d) || (c > a+error && c > d+error)) &&
							(a>d?a-d<=error:d-a<=error))
						{
							if(a>d)
							{
								unsigned short tmp = (a-d)/4;
								b = d + tmp;
								c = d + 3*tmp;
							}
							else
							{
								unsigned short tmp = (d-a)/4;
								b = a + tmp;
								c = a + 3*tmp;
							}
							set = true;
							if(!horizontal)
							{
								y+=2;
							}
						}
					}
				}
			}
			if(!set && horizontal)
			{
				const unsigned short & a = registeredDepth.at<unsigned short>(y, x-1);
				unsigned short & c = registeredDepth.at<unsigned short>(y, x+1);
				if(a && c)
				{
					unsigned short error = 0.01*((a+c)/2);
					if(((b == 0 && a && c) || (b > a+error && b > c+error)) &&
						(a>c?a-c<=error:c-a<=error))
					{
						b = (a+c)/2;
						set = true;
					}
				}
				if(!set && fillDoubleHoles)
				{
					const unsigned short & d = registeredDepth.at<unsigned short>(y, x+2);
					if(a && d && (b==0 || c==0))
					{
						unsigned short error = 0.01*((a+d)/2);
						if(((b == 0 && a && d) || (b > a+error && b > d+error)) &&
						   ((c == 0 && a && d) || (c > a+error && c > d+error)) &&
							(a>d?a-d<=error:d-a<=error))
						{
							if(a>d)
							{
								unsigned short tmp = (a-d)/4;
								b = d + tmp;
								c = d + 3*tmp;
							}
							else
							{
								unsigned short tmp = (d-a)/4;
								b = a + tmp;
								c = a + 3*tmp;
							}
						}
					}
				}
			}
		}
	}
}

cv::Mat laserScanFromPointCloud(const pcl::PointCloud<pcl::PointXYZ> & cloud)
{
	cv::Mat laserScan(1, (int)cloud.size(), CV_32FC2);
	for(unsigned int i=0; i<cloud.size(); ++i)
	{
		laserScan.at<cv::Vec2f>(i)[0] = cloud.at(i).x;
		laserScan.at<cv::Vec2f>(i)[1] = cloud.at(i).y;
	}
	return laserScan;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr laserScanToPointCloud(const cv::Mat & laserScan)
{
	UASSERT(laserScan.empty() || laserScan.type() == CV_32FC2);

	pcl::PointCloud<pcl::PointXYZ>::Ptr output(new pcl::PointCloud<pcl::PointXYZ>);
	output->resize(laserScan.cols);
	for(int i=0; i<laserScan.cols; ++i)
	{
		output->at(i).x = laserScan.at<cv::Vec2f>(i)[0];
		output->at(i).y = laserScan.at<cv::Vec2f>(i)[1];
	}
	return output;
}

void extractXYZCorrespondences(const std::multimap<int, pcl::PointXYZ> & words1,
									  const std::multimap<int, pcl::PointXYZ> & words2,
									  pcl::PointCloud<pcl::PointXYZ> & cloud1,
									  pcl::PointCloud<pcl::PointXYZ> & cloud2)
{
	const std::list<int> & ids = uUniqueKeys(words1);
	for(std::list<int>::const_iterator i=ids.begin(); i!=ids.end(); ++i)
	{
		if(words1.count(*i) == 1 && words2.count(*i) == 1)
		{
			const pcl::PointXYZ & pt1 = words1.find(*i)->second;
			const pcl::PointXYZ & pt2 = words2.find(*i)->second;
			if(pcl::isFinite(pt1) && pcl::isFinite(pt2))
			{
				cloud1.push_back(pt1);
				cloud2.push_back(pt2);
			}
		}
	}
}

void extractXYZCorrespondencesRANSAC(const std::multimap<int, pcl::PointXYZ> & words1,
									  const std::multimap<int, pcl::PointXYZ> & words2,
									  pcl::PointCloud<pcl::PointXYZ> & cloud1,
									  pcl::PointCloud<pcl::PointXYZ> & cloud2)
{
	std::list<std::pair<pcl::PointXYZ, pcl::PointXYZ> > pairs;
	const std::list<int> & ids = uUniqueKeys(words1);
	for(std::list<int>::const_iterator i=ids.begin(); i!=ids.end(); ++i)
	{
		if(words1.count(*i) == 1 && words2.count(*i) == 1)
		{
			const pcl::PointXYZ & pt1 = words1.find(*i)->second;
			const pcl::PointXYZ & pt2 = words2.find(*i)->second;
			if(pcl::isFinite(pt1) && pcl::isFinite(pt2))
			{
				pairs.push_back(std::pair<pcl::PointXYZ, pcl::PointXYZ>(pt1, pt2));
			}
		}
	}

	if(pairs.size() > 7)
	{
		// Remove outliers using fundamental matrix RANSAC
		std::vector<uchar> status(pairs.size(), 0);
		//Convert Keypoints to a structure that OpenCV understands
		//3 dimensions (Homogeneous vectors)
		cv::Mat points1(1, (int)pairs.size(), CV_32FC2);
		cv::Mat points2(1, (int)pairs.size(), CV_32FC2);

		float * points1data = points1.ptr<float>(0);
		float * points2data = points2.ptr<float>(0);

		// Fill the points here ...
		int i=0;
		for(std::list<std::pair<pcl::PointXYZ, pcl::PointXYZ> >::const_iterator iter = pairs.begin();
			iter != pairs.end();
			++iter )
		{
			points1data[i*2] = (*iter).first.x;
			points1data[i*2+1] = (*iter).first.y;

			points2data[i*2] = (*iter).second.x;
			points2data[i*2+1] = (*iter).second.y;

			++i;
		}

		// Find the fundamental matrix
		cv::Mat fundamentalMatrix = cv::findFundamentalMat(
					points1,
					points2,
					status,
					cv::FM_RANSAC,
					3.0,
					0.99);

		if(!fundamentalMatrix.empty())
		{
			int i = 0;
			for(std::list<std::pair<pcl::PointXYZ, pcl::PointXYZ> >::iterator iter=pairs.begin(); iter!=pairs.end(); ++iter)
			{
				if(status[i])
				{
					cloud1.push_back(iter->first);
					cloud2.push_back(iter->second);
				}
				++i;
			}
		}
	}
}

void extractXYZCorrespondences(const std::list<std::pair<cv::Point2f, cv::Point2f> > & correspondences,
									   const cv::Mat & depthImage1,
									   const cv::Mat & depthImage2,
									   float cx, float cy,
									   float fx, float fy,
									   float maxDepth,
									   pcl::PointCloud<pcl::PointXYZ> & cloud1,
									   pcl::PointCloud<pcl::PointXYZ> & cloud2)
{
	cloud1.resize(correspondences.size());
	cloud2.resize(correspondences.size());
	int oi=0;
	for(std::list<std::pair<cv::Point2f, cv::Point2f> >::const_iterator iter = correspondences.begin();
		iter!=correspondences.end();
		++iter)
	{
		pcl::PointXYZ pt1 = projectDepthTo3D(depthImage1, iter->first.x, iter->first.y, cx, cy, fx, fy, true);
		pcl::PointXYZ pt2 = projectDepthTo3D(depthImage2, iter->second.x, iter->second.y, cx, cy, fx, fy, true);
		if(pcl::isFinite(pt1) && pcl::isFinite(pt2) &&
		   (maxDepth <= 0 || (pt1.z <= maxDepth && pt2.z<=maxDepth)))
		{
			cloud1[oi] = pt1;
			cloud2[oi] = pt2;
			++oi;
		}
	}
	cloud1.resize(oi);
	cloud2.resize(oi);
}

template<typename PointT>
inline void extractXYZCorrespondencesImpl(const std::list<std::pair<cv::Point2f, cv::Point2f> > & correspondences,
							   const pcl::PointCloud<PointT> & cloud1,
							   const pcl::PointCloud<PointT> & cloud2,
							   pcl::PointCloud<pcl::PointXYZ> & inliers1,
							   pcl::PointCloud<pcl::PointXYZ> & inliers2,
							   char depthAxis)
{
	for(std::list<std::pair<cv::Point2f, cv::Point2f> >::const_iterator iter = correspondences.begin();
		iter!=correspondences.end();
		++iter)
	{
		PointT pt1 = cloud1.at(int(iter->first.y+0.5f) * cloud1.width + int(iter->first.x+0.5f));
		PointT pt2 = cloud2.at(int(iter->second.y+0.5f) * cloud2.width + int(iter->second.x+0.5f));
		if(pcl::isFinite(pt1) &&
		   pcl::isFinite(pt2))
		{
			inliers1.push_back(pcl::PointXYZ(pt1.x, pt1.y, pt1.z));
			inliers2.push_back(pcl::PointXYZ(pt2.x, pt2.y, pt2.z));
		}
	}
}

void extractXYZCorrespondences(const std::list<std::pair<cv::Point2f, cv::Point2f> > & correspondences,
							   const pcl::PointCloud<pcl::PointXYZ> & cloud1,
							   const pcl::PointCloud<pcl::PointXYZ> & cloud2,
							   pcl::PointCloud<pcl::PointXYZ> & inliers1,
							   pcl::PointCloud<pcl::PointXYZ> & inliers2,
							   char depthAxis)
{
	extractXYZCorrespondencesImpl(correspondences, cloud1, cloud2, inliers1, inliers2, depthAxis);
}
void extractXYZCorrespondences(const std::list<std::pair<cv::Point2f, cv::Point2f> > & correspondences,
							   const pcl::PointCloud<pcl::PointXYZRGB> & cloud1,
							   const pcl::PointCloud<pcl::PointXYZRGB> & cloud2,
							   pcl::PointCloud<pcl::PointXYZ> & inliers1,
							   pcl::PointCloud<pcl::PointXYZ> & inliers2,
							   char depthAxis)
{
	extractXYZCorrespondencesImpl(correspondences, cloud1, cloud2, inliers1, inliers2, depthAxis);
}

int countUniquePairs(const std::multimap<int, pcl::PointXYZ> & wordsA,
					 const std::multimap<int, pcl::PointXYZ> & wordsB)
{
	const std::list<int> & ids = uUniqueKeys(wordsA);
	int pairs = 0;
	for(std::list<int>::const_iterator i=ids.begin(); i!=ids.end(); ++i)
	{
		std::list<pcl::PointXYZ> ptsA = uValues(wordsA, *i);
		std::list<pcl::PointXYZ> ptsB = uValues(wordsB, *i);
		if(ptsA.size() == 1 && ptsB.size() == 1)
		{
			++pairs;
		}
	}
	return pairs;
}

void filterMaxDepth(pcl::PointCloud<pcl::PointXYZ> & inliers1,
					pcl::PointCloud<pcl::PointXYZ> & inliers2,
					float maxDepth,
					char depthAxis,
					bool removeDuplicates)
{
	std::list<pcl::PointXYZ> addedPts;
	if(maxDepth > 0.0f && inliers1.size() && inliers1.size() == inliers2.size())
	{
		pcl::PointCloud<pcl::PointXYZ> tmpInliers1;
		pcl::PointCloud<pcl::PointXYZ> tmpInliers2;
		for(unsigned int i=0; i<inliers1.size(); ++i)
		{
			if((depthAxis == 'x' && inliers1.at(i).x < maxDepth && inliers2.at(i).x < maxDepth) ||
			   (depthAxis == 'y' && inliers1.at(i).y < maxDepth && inliers2.at(i).y < maxDepth) ||
			   (depthAxis == 'z' && inliers1.at(i).z < maxDepth && inliers2.at(i).z < maxDepth))
			{
				bool dup = false;
				if(removeDuplicates)
				{
					for(std::list<pcl::PointXYZ>::iterator iter = addedPts.begin(); iter!=addedPts.end(); ++iter)
					{
						if(iter->x == inliers1.at(i).x &&
						   iter->y == inliers1.at(i).y &&
						   iter->z == inliers1.at(i).z)
						{
							dup = true;
						}
					}
					if(!dup)
					{
						addedPts.push_back(inliers1.at(i));
					}
				}

				if(!dup)
				{
					tmpInliers1.push_back(inliers1.at(i));
					tmpInliers2.push_back(inliers2.at(i));
				}
			}
		}
		inliers1 = tmpInliers1;
		inliers2 = tmpInliers2;
	}
}

// Get transform from cloud2 to cloud1
Transform transformFromXYZCorrespondences(
		const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud1,
		const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud2,
		double inlierThreshold,
		int iterations,
		bool refineModel,
		double refineModelSigma,
		int refineModelIterations,
		std::vector<int> * inliersOut,
		double * varianceOut)
{
	//NOTE: this method is a mix of two methods:
	//  - getRemainingCorrespondences() in pcl/registration/impl/correspondence_rejection_sample_consensus.hpp
	//  - refineModel() in pcl/sample_consensus/sac.h

	if(varianceOut)
	{
		*varianceOut = 1.0;
	}
	Transform transform;
	if(cloud1->size() >=3 && cloud1->size() == cloud2->size())
	{
		// RANSAC
		UDEBUG("iterations=%d inlierThreshold=%f", iterations, inlierThreshold);
		std::vector<int> source_indices (cloud2->size());
		std::vector<int> target_indices (cloud1->size());

		// Copy the query-match indices
		for (int i = 0; i < (int)cloud1->size(); ++i)
		{
			source_indices[i] = i;
			target_indices[i] = i;
		}

		// From the set of correspondences found, attempt to remove outliers
		// Create the registration model
		pcl::SampleConsensusModelRegistration<pcl::PointXYZ>::Ptr model;
		model.reset(new pcl::SampleConsensusModelRegistration<pcl::PointXYZ>(cloud2, source_indices));
		// Pass the target_indices
		model->setInputTarget (cloud1, target_indices);
		// Create a RANSAC model
		pcl::RandomSampleConsensus<pcl::PointXYZ> sac (model, inlierThreshold);
		sac.setMaxIterations(iterations);

		// Compute the set of inliers
		if(sac.computeModel())
		{
			std::vector<int> inliers;
			Eigen::VectorXf model_coefficients;

			sac.getInliers(inliers);
			sac.getModelCoefficients (model_coefficients);

			if (refineModel)
			{
				double inlier_distance_threshold_sqr = inlierThreshold * inlierThreshold;
				double error_threshold = inlierThreshold;
				double sigma_sqr = refineModelSigma * refineModelSigma;
				int refine_iterations = 0;
				bool inlier_changed = false, oscillating = false;
				std::vector<int> new_inliers, prev_inliers = inliers;
				std::vector<size_t> inliers_sizes;
				Eigen::VectorXf new_model_coefficients = model_coefficients;
				do
				{
					// Optimize the model coefficients
					model->optimizeModelCoefficients (prev_inliers, new_model_coefficients, new_model_coefficients);
					inliers_sizes.push_back (prev_inliers.size ());

					// Select the new inliers based on the optimized coefficients and new threshold
					model->selectWithinDistance (new_model_coefficients, error_threshold, new_inliers);
					UDEBUG("RANSAC refineModel: Number of inliers found (before/after): %d/%d, with an error threshold of %f.",
							(int)prev_inliers.size (), (int)new_inliers.size (), error_threshold);

					if (new_inliers.empty ())
					{
						++refine_iterations;
						if (refine_iterations >= refineModelIterations)
						{
							break;
						}
						continue;
					}

					// Estimate the variance and the new threshold
					double variance = model->computeVariance ();
					error_threshold = sqrt (std::min (inlier_distance_threshold_sqr, sigma_sqr * variance));

					UDEBUG ("RANSAC refineModel: New estimated error threshold: %f (variance=%f) on iteration %d out of %d.",
						  error_threshold, variance, refine_iterations, refineModelIterations);
					inlier_changed = false;
					std::swap (prev_inliers, new_inliers);

					// If the number of inliers changed, then we are still optimizing
					if (new_inliers.size () != prev_inliers.size ())
					{
						// Check if the number of inliers is oscillating in between two values
						if (inliers_sizes.size () >= 4)
						{
							if (inliers_sizes[inliers_sizes.size () - 1] == inliers_sizes[inliers_sizes.size () - 3] &&
							inliers_sizes[inliers_sizes.size () - 2] == inliers_sizes[inliers_sizes.size () - 4])
							{
								oscillating = true;
								break;
							}
						}
						inlier_changed = true;
						continue;
					}

					// Check the values of the inlier set
					for (size_t i = 0; i < prev_inliers.size (); ++i)
					{
						// If the value of the inliers changed, then we are still optimizing
						if (prev_inliers[i] != new_inliers[i])
						{
							inlier_changed = true;
							break;
						}
					}
				}
				while (inlier_changed && ++refine_iterations < refineModelIterations);

				// If the new set of inliers is empty, we didn't do a good job refining
				if (new_inliers.empty ())
				{
					UWARN ("RANSAC refineModel: Refinement failed: got an empty set of inliers!");
				}

				if (oscillating)
				{
					UDEBUG("RANSAC refineModel: Detected oscillations in the model refinement.");
				}

				std::swap (inliers, new_inliers);
				model_coefficients = new_model_coefficients;
			}

			if (inliers.size() >= 3)
			{
				if(inliersOut)
				{
					*inliersOut = inliers;
				}
				if(varianceOut)
				{
					*varianceOut = model->computeVariance();
				}

				// get best transformation
				Eigen::Matrix4f bestTransformation;
				bestTransformation.row (0) = model_coefficients.segment<4>(0);
				bestTransformation.row (1) = model_coefficients.segment<4>(4);
				bestTransformation.row (2) = model_coefficients.segment<4>(8);
				bestTransformation.row (3) = model_coefficients.segment<4>(12);

				transform = Transform::fromEigen4f(bestTransformation);
				UDEBUG("RANSAC inliers=%d/%d tf=%s", (int)inliers.size(), (int)cloud1->size(), transform.prettyPrint().c_str());

				return transform.inverse(); // inverse to get actual pose transform (not correspondences transform)
			}
			else
			{
				UDEBUG("RANSAC: Model with inliers < 3");
			}
		}
		else
		{
			UDEBUG("RANSAC: Failed to find model");
		}
	}
	else
	{
		UDEBUG("Not enough points to compute the transform");
	}
	return Transform();
}

// return transform from source to target (All points must be finite!!!)
Transform icp(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_source,
			  const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_target,
			  double maxCorrespondenceDistance,
			  int maximumIterations,
			  bool * hasConvergedOut,
			  double * variance,
			  int * inliers)
{
	pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
	// Set the input source and target
	icp.setInputTarget (cloud_target);
	icp.setInputSource (cloud_source);

	// Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
	icp.setMaxCorrespondenceDistance (maxCorrespondenceDistance);
	// Set the maximum number of iterations (criterion 1)
	icp.setMaximumIterations (maximumIterations);
	// Set the transformation epsilon (criterion 2)
	//icp.setTransformationEpsilon (transformationEpsilon);
	// Set the euclidean distance difference epsilon (criterion 3)
	//icp.setEuclideanFitnessEpsilon (1);
	//icp.setRANSACOutlierRejectionThreshold(maxCorrespondenceDistance);

	// Perform the alignment
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source_registered(new pcl::PointCloud<pcl::PointXYZ>);
	icp.align (*cloud_source_registered);
	bool hasConverged = icp.hasConverged();

	// compute variance
	if((inliers || variance) && hasConverged)
	{
		pcl::registration::CorrespondenceEstimation<pcl::PointXYZ, pcl::PointXYZ>::Ptr est;
		est.reset(new pcl::registration::CorrespondenceEstimation<pcl::PointXYZ, pcl::PointXYZ>);
		est->setInputTarget(cloud_target);
		est->setInputSource(cloud_source_registered);
		pcl::Correspondences correspondences;
		est->determineCorrespondences(correspondences, maxCorrespondenceDistance);
		if(variance)
		{
			if(correspondences.size()>=3)
			{
				std::vector<double> distances(correspondences.size());
				for(unsigned int i=0; i<correspondences.size(); ++i)
				{
					distances[i] = correspondences[i].distance;
				}

				//variance
				std::sort(distances.begin (), distances.end ());
				double median_error_sqr = distances[distances.size () >> 1];
				*variance = (2.1981 * median_error_sqr);
			}
			else
			{
				hasConverged = false;
				*variance = -1.0;
			}
		}

		if(inliers)
		{
			*inliers = (int)correspondences.size();
		}
	}
	else
	{
		if(inliers)
		{
			*inliers = 0;
		}
		if(variance)
		{
			*variance = -1;
		}
	}

	if(hasConvergedOut)
	{
		*hasConvergedOut = hasConverged;
	}

	return Transform::fromEigen4f(icp.getFinalTransformation());
}

// return transform from source to target (All points/normals must be finite!!!)
Transform icpPointToPlane(
		const pcl::PointCloud<pcl::PointNormal>::ConstPtr & cloud_source,
		const pcl::PointCloud<pcl::PointNormal>::ConstPtr & cloud_target,
		double maxCorrespondenceDistance,
		int maximumIterations,
		bool * hasConvergedOut,
		double * variance,
		int * inliers)
{
	pcl::IterativeClosestPoint<pcl::PointNormal, pcl::PointNormal> icp;
	// Set the input source and target
	icp.setInputTarget (cloud_target);
	icp.setInputSource (cloud_source);

	pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal, pcl::PointNormal>::Ptr est;
	est.reset(new pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal, pcl::PointNormal>);
	icp.setTransformationEstimation(est);

	// Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
	icp.setMaxCorrespondenceDistance (maxCorrespondenceDistance);
	// Set the maximum number of iterations (criterion 1)
	icp.setMaximumIterations (maximumIterations);
	// Set the transformation epsilon (criterion 2)
	//icp.setTransformationEpsilon (transformationEpsilon);
	// Set the euclidean distance difference epsilon (criterion 3)
	//icp.setEuclideanFitnessEpsilon (1);
	//icp.setRANSACOutlierRejectionThreshold(maxCorrespondenceDistance);

	// Perform the alignment
	pcl::PointCloud<pcl::PointNormal>::Ptr cloud_source_registered(new pcl::PointCloud<pcl::PointNormal>);
	icp.align (*cloud_source_registered);
	bool hasConverged = icp.hasConverged();

	// compute variance
	if((inliers || variance) && hasConverged)
	{
		pcl::registration::CorrespondenceEstimation<pcl::PointNormal, pcl::PointNormal>::Ptr est;
		est.reset(new pcl::registration::CorrespondenceEstimation<pcl::PointNormal, pcl::PointNormal>);
		est->setInputTarget(cloud_target);
		est->setInputSource(cloud_source_registered);
		pcl::Correspondences correspondences;
		est->determineCorrespondences(correspondences, maxCorrespondenceDistance);
		if(variance)
		{
			if(correspondences.size()>=3)
			{
				std::vector<double> distances(correspondences.size());
				for(unsigned int i=0; i<correspondences.size(); ++i)
				{
					distances[i] = correspondences[i].distance;
				}

				//variance
				std::sort(distances.begin (), distances.end ());
				double median_error_sqr = distances[distances.size () >> 1];
				*variance = (2.1981 * median_error_sqr);
			}
			else
			{
				hasConverged = false;
				*variance = -1.0;
			}
		}

		if(inliers)
		{
			*inliers = (int)correspondences.size();
		}
	}
	else
	{
		if(inliers)
		{
			*inliers = 0;
		}
		if(variance)
		{
			*variance = -1;
		}
	}

	if(hasConvergedOut)
	{
		*hasConvergedOut = hasConverged;
	}

	return Transform::fromEigen4f(icp.getFinalTransformation());
}

// return transform from source to target (All points must be finite!!!)
Transform icp2D(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_source,
			  const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_target,
			  double maxCorrespondenceDistance,
			  int maximumIterations,
			  bool * hasConvergedOut,
			  double * variance,
			  int * inliers)
{
	pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
	// Set the input source and target
	icp.setInputTarget (cloud_target);
	icp.setInputSource (cloud_source);

	pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ>::Ptr est;
	est.reset(new pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ>);
	icp.setTransformationEstimation(est);

	// Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
	icp.setMaxCorrespondenceDistance (maxCorrespondenceDistance);
	// Set the maximum number of iterations (criterion 1)
	icp.setMaximumIterations (maximumIterations);
	// Set the transformation epsilon (criterion 2)
	//icp.setTransformationEpsilon (transformationEpsilon);
	// Set the euclidean distance difference epsilon (criterion 3)
	//icp.setEuclideanFitnessEpsilon (1);
	//icp.setRANSACOutlierRejectionThreshold(maxCorrespondenceDistance);

	// Perform the alignment
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source_registered(new pcl::PointCloud<pcl::PointXYZ>);
	icp.align (*cloud_source_registered);
	bool hasConverged = icp.hasConverged();

	// compute variance
	if((inliers || variance) && hasConverged)
	{
		pcl::registration::CorrespondenceEstimation<pcl::PointXYZ, pcl::PointXYZ>::Ptr est;
		est.reset(new pcl::registration::CorrespondenceEstimation<pcl::PointXYZ, pcl::PointXYZ>);
		est->setInputTarget(cloud_target);
		est->setInputSource(cloud_source_registered);
		pcl::Correspondences correspondences;
		est->determineCorrespondences(correspondences, maxCorrespondenceDistance);
		if(variance)
		{
			if(correspondences.size()>=3)
			{
				std::vector<double> distances(correspondences.size());
				for(unsigned int i=0; i<correspondences.size(); ++i)
				{
					distances[i] = correspondences[i].distance;
				}

				//variance
				std::sort(distances.begin (), distances.end ());
				double median_error_sqr = distances[distances.size () >> 1];
				*variance = (2.1981 * median_error_sqr);
			}
			else
			{
				hasConverged = false;
				*variance = -1.0;
			}
		}

		if(inliers)
		{
			*inliers = (int)correspondences.size();
		}
	}
	else
	{
		if(inliers)
		{
			*inliers = 0;
		}
		if(variance)
		{
			*variance = -1;
		}
	}

	if(hasConvergedOut)
	{
		*hasConvergedOut = hasConverged;
	}

	return Transform::fromEigen4f(icp.getFinalTransformation());
}

pcl::PointCloud<pcl::PointNormal>::Ptr computeNormals(
		const pcl::PointCloud<pcl::PointXYZ>::Ptr & cloud,
		int normalKSearch)
{
	pcl::PointCloud<pcl::PointNormal>::Ptr cloud_with_normals(new pcl::PointCloud<pcl::PointNormal>);
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
	tree->setInputCloud (cloud);

	// Normal estimation*
	pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::Normal> n;
	pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);
	n.setInputCloud (cloud);
	n.setSearchMethod (tree);
	n.setKSearch (normalKSearch);
	n.compute (*normals);
	//* normals should not contain the point normals + surface curvatures

	// Concatenate the XYZ and normal fields*
	pcl::concatenateFields (*cloud, *normals, *cloud_with_normals);
	//* cloud_with_normals = cloud + normals*/

	return cloud_with_normals;
}

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr computeNormals(
		const pcl::PointCloud<pcl::PointXYZRGB>::Ptr & cloud,
		int normalKSearch)
{
	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud_with_normals(new pcl::PointCloud<pcl::PointXYZRGBNormal>);
	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGB>);
	tree->setInputCloud (cloud);

	// Normal estimation*
	pcl::NormalEstimationOMP<pcl::PointXYZRGB, pcl::Normal> n;
	pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);
	n.setInputCloud (cloud);
	n.setSearchMethod (tree);
	n.setKSearch (normalKSearch);
	n.compute (*normals);
	//* normals should not contain the point normals + surface curvatures

	// Concatenate the XYZ and normal fields*
	pcl::concatenateFields (*cloud, *normals, *cloud_with_normals);
	//* cloud_with_normals = cloud + normals*/

	return cloud_with_normals;
}

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr computeNormalsSmoothed(
		const pcl::PointCloud<pcl::PointXYZRGB>::Ptr & cloud,
		float smoothingSearchRadius,
		bool smoothingPolynomialFit)
{
	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud_with_normals(new pcl::PointCloud<pcl::PointXYZRGBNormal>);
	pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGB>);
	tree->setInputCloud (cloud);

	// Init object (second point type is for the normals, even if unused)
	pcl::MovingLeastSquares<pcl::PointXYZRGB, pcl::PointXYZRGBNormal> mls;

	mls.setComputeNormals (true);

	// Set parameters
	mls.setInputCloud (cloud);
	mls.setPolynomialFit (smoothingPolynomialFit);
	mls.setSearchMethod (tree);
	mls.setSearchRadius (smoothingSearchRadius);

	// Reconstruct
	mls.process (*cloud_with_normals);

	return cloud_with_normals;
}

// a kdtree is constructed with cloud_target, then nearest neighbor
// is computed for each cloud_source points.
int getCorrespondencesCount(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_source,
							const pcl::PointCloud<pcl::PointXYZ>::ConstPtr & cloud_target,
							float maxDistance)
{
	pcl::search::KdTree<pcl::PointXYZ>::Ptr kdTree(new pcl::search::KdTree<pcl::PointXYZ>);
	kdTree->setInputCloud(cloud_target);
	int count = 0;
	float sqrdMaxDistance = maxDistance * maxDistance;
	for(unsigned int i=0; i<cloud_source->size(); ++i)
	{
		std::vector<int> ind(1);
		std::vector<float> dist(1);
		if(kdTree->nearestKSearch(cloud_source->at(i), 1, ind, dist) && dist[0] < sqrdMaxDistance)
		{
			++count;
		}
	}
	return count;
}

/**
 * if a=[1 2 3 4 6 6], b=[1 1 2 4 5 6 6], results= [(2,2) (4,4)]
 * realPairsCount = 5
 */
void findCorrespondences(
		const std::multimap<int, cv::KeyPoint> & wordsA,
		const std::multimap<int, cv::KeyPoint> & wordsB,
		std::list<std::pair<cv::Point2f, cv::Point2f> > & pairs)
{
	const std::list<int> & ids = uUniqueKeys(wordsA);
	pairs.clear();
	for(std::list<int>::const_iterator i=ids.begin(); i!=ids.end(); ++i)
	{
		std::list<cv::KeyPoint> ptsA = uValues(wordsA, *i);
		std::list<cv::KeyPoint> ptsB = uValues(wordsB, *i);
		if(ptsA.size() == 1 && ptsB.size() == 1)
		{
			pairs.push_back(std::pair<cv::Point2f, cv::Point2f>(ptsA.front().pt, ptsB.front().pt));
		}
	}
}

pcl::PointCloud<pcl::PointXYZ>::Ptr cvMat2Cloud(
		const cv::Mat & matrix,
		const Transform & tranform)
{
	UASSERT(matrix.type() == CV_32FC2 || matrix.type() == CV_32FC3);
	UASSERT(matrix.rows == 1);

	Eigen::Affine3f t = tranform.toEigen3f();
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	cloud->resize(matrix.cols);
	if(matrix.channels() == 2)
	{
		for(int i=0; i<matrix.cols; ++i)
		{
			cloud->at(i).x = matrix.at<cv::Vec2f>(0,i)[0];
			cloud->at(i).y = matrix.at<cv::Vec2f>(0,i)[1];
			cloud->at(i).z = 0.0f;
			cloud->at(i) = pcl::transformPoint(cloud->at(i), t);
		}
	}
	else // channels=3
	{
		for(int i=0; i<matrix.cols; ++i)
		{
			cloud->at(i).x = matrix.at<cv::Vec3f>(0,i)[0];
			cloud->at(i).y = matrix.at<cv::Vec3f>(0,i)[1];
			cloud->at(i).z = matrix.at<cv::Vec3f>(0,i)[2];
			cloud->at(i) = pcl::transformPoint(cloud->at(i), t);
		}
	}
	return cloud;
}

// If "voxel" > 0, "samples" is ignored
pcl::PointCloud<pcl::PointXYZ>::Ptr getICPReadyCloud(
		const cv::Mat & depth,
		float fx,
		float fy,
		float cx,
		float cy,
		int decimation,
		double maxDepth,
		float voxel,
		int samples,
		const Transform & transform)
{
	UASSERT(!depth.empty() && (depth.type() == CV_16UC1 || depth.type() == CV_32FC1));
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud;
	cloud = cloudFromDepth(
			depth,
			cx,
			cy,
			fx,
			fy,
			decimation);

	if(cloud->size())
	{
		if(maxDepth>0.0)
		{
			cloud = passThrough<pcl::PointXYZ>(cloud, "z", 0, maxDepth);
		}

		if(cloud->size())
		{
			if(voxel>0)
			{
				cloud = voxelize<pcl::PointXYZ>(cloud, voxel);
			}
			else if(samples>0 && (int)cloud->size() > samples)
			{
				cloud = sampling<pcl::PointXYZ>(cloud, samples);
			}

			if(cloud->size())
			{
				if(!transform.isNull() && !transform.isIdentity())
				{
					cloud = transformPointCloud<pcl::PointXYZ>(cloud, transform);
				}
			}
		}
	}

	return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr concatenateClouds(const std::list<pcl::PointCloud<pcl::PointXYZ>::Ptr> & clouds)
{
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	for(std::list<pcl::PointCloud<pcl::PointXYZ>::Ptr>::const_iterator iter = clouds.begin(); iter!=clouds.end(); ++iter)
	{
		*cloud += *(*iter);
	}
	return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr concatenateClouds(const std::list<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> & clouds)
{
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
	for(std::list<pcl::PointCloud<pcl::PointXYZRGB>::Ptr>::const_iterator iter = clouds.begin(); iter!=clouds.end(); ++iter)
	{
		*cloud+=*(*iter);
	}
	return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr get3DFASTKpts(
		const cv::Mat & image,
		const cv::Mat & imageDepth,
		float constant,
		int fastThreshold,
		bool fastNonmaxSuppression,
		float maxDepth)
{
	// Extract words
	cv::FastFeatureDetector detector(fastThreshold, fastNonmaxSuppression);
	std::vector<cv::KeyPoint> kpts;
	detector.detect(image, kpts);

	pcl::PointCloud<pcl::PointXYZ>::Ptr points(new pcl::PointCloud<pcl::PointXYZ>);
	for(unsigned int i=0; i<kpts.size(); ++i)
	{
		pcl::PointXYZ pt = projectDepthTo3D(imageDepth, kpts[i].pt.x, kpts[i].pt.y, 0, 0, 1.0f/constant, 1.0f/constant, true);
		if(uIsFinite(pt.z) && (maxDepth <= 0 || pt.z <= maxDepth))
		{
			points->push_back(pt);
		}
	}
	UDEBUG("points %d -> %d", (int)kpts.size(), (int)points->size());
	return points;
}

pcl::PolygonMesh::Ptr createMesh(
		const pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr & cloudWithNormals,
		float gp3SearchRadius,
		float gp3Mu,
		int gp3MaximumNearestNeighbors,
		float gp3MaximumSurfaceAngle,
		float gp3MinimumAngle,
		float gp3MaximumAngle,
		bool gp3NormalConsistency)
{
	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloudWithNormalsNoNaN = removeNaNNormalsFromPointCloud<pcl::PointXYZRGBNormal>(cloudWithNormals);

	// Create search tree*
	pcl::search::KdTree<pcl::PointXYZRGBNormal>::Ptr tree2 (new pcl::search::KdTree<pcl::PointXYZRGBNormal>);
	tree2->setInputCloud (cloudWithNormalsNoNaN);

	// Initialize objects
	pcl::GreedyProjectionTriangulation<pcl::PointXYZRGBNormal> gp3;
	pcl::PolygonMesh::Ptr mesh(new pcl::PolygonMesh);

	// Set the maximum distance between connected points (maximum edge length)
	gp3.setSearchRadius (gp3SearchRadius);

	// Set typical values for the parameters
	gp3.setMu (gp3Mu);
	gp3.setMaximumNearestNeighbors (gp3MaximumNearestNeighbors);
	gp3.setMaximumSurfaceAngle(gp3MaximumSurfaceAngle); // 45 degrees
	gp3.setMinimumAngle(gp3MinimumAngle); // 10 degrees
	gp3.setMaximumAngle(gp3MaximumAngle); // 120 degrees
	gp3.setNormalConsistency(gp3NormalConsistency);

	// Get result
	gp3.setInputCloud (cloudWithNormalsNoNaN);
	gp3.setSearchMethod (tree2);
	gp3.reconstruct (*mesh);

	return mesh;
}

void occupancy2DFromLaserScan(
		const cv::Mat & scan,
		cv::Mat & ground,
		cv::Mat & obstacles,
		float cellSize)
{
	if(scan.empty())
	{
		return;
	}

	std::map<int, Transform> poses;
	poses.insert(std::make_pair(1, Transform::getIdentity()));

	pcl::PointCloud<pcl::PointXYZ>::Ptr obstaclesCloud = util3d::laserScanToPointCloud(scan);
	//obstaclesCloud = util3d::voxelize<pcl::PointXYZ>(obstaclesCloud, cellSize);

	std::map<int, pcl::PointCloud<pcl::PointXYZ>::Ptr> scans;
	scans.insert(std::make_pair(1, obstaclesCloud));

	float xMin, yMin;
	cv::Mat map8S = create2DMap(poses, scans, cellSize, false, xMin, yMin);

	// find ground cells
	std::list<int> groundIndices;
	for(unsigned int i=0; i< map8S.total(); ++i)
	{
		if(map8S.data[i] == 0)
		{
			groundIndices.push_back(i);
		}
	}

	// Convert to position matrices, get points to each center of the cells
	ground = cv::Mat();
	if(groundIndices.size())
	{
		ground = cv::Mat((int)groundIndices.size(), 1, CV_32FC2);
		int i=0;
		for(std::list<int>::iterator iter=groundIndices.begin();iter!=groundIndices.end(); ++iter)
		{
			int x = *iter / map8S.cols;
			int y = *iter - x*map8S.cols;
			ground.at<cv::Vec2f>(i)[0] = (float(y)+0.5)*cellSize + xMin;
			ground.at<cv::Vec2f>(i)[1] = (float(x)+0.5)*cellSize + yMin;
			++i;
		}
	}

	// copy directly obstacles precise positions
	obstacles = cv::Mat();
	if(obstaclesCloud->size())
	{
		obstacles = cv::Mat((int)obstaclesCloud->size(), 1, CV_32FC2);
		for(unsigned int i=0;i<obstaclesCloud->size(); ++i)
		{
			obstacles.at<cv::Vec2f>(i)[0] = obstaclesCloud->at(i).x;
			obstacles.at<cv::Vec2f>(i)[1] = obstaclesCloud->at(i).y;
		}
	}
}

/**
 * Create 2d Occupancy grid (CV_8S) from 2d occupancy
 * -1 = unknown
 * 0 = empty space
 * 100 = obstacle
 * @param poses
 * @param occupancy <empty, occupied>
 * @param cellSize m
 * @param xMin
 * @param yMin
 * @param minMapSize minimum width (m)
 * @param erode
 */
cv::Mat create2DMapFromOccupancyLocalMaps(
		const std::map<int, Transform> & poses,
		const std::map<int, std::pair<cv::Mat, cv::Mat> > & occupancy,
		float cellSize,
		float & xMin,
		float & yMin,
		float minMapSize,
		bool erode)
{
	UASSERT(minMapSize >= 0.0f);
	UDEBUG("cellSize=%f m, minMapSize=%f m, erode=%d", cellSize, minMapSize, erode?1:0);
	UTimer timer;

	std::map<int, cv::Mat> emptyLocalMaps;
	std::map<int, cv::Mat> occupiedLocalMaps;

	float minX=-minMapSize/2.0, minY=-minMapSize/2.0, maxX=minMapSize/2.0, maxY=minMapSize/2.0;
	bool undefinedSize = minMapSize == 0.0f;
	float x=0.0f,y=0.0f,z=0.0f,roll=0.0f,pitch=0.0f,yaw=0.0f,cosT=0.0f,sinT=0.0f;
	cv::Mat affineTransform(2,3,CV_32FC1);
	for(std::map<int, Transform>::const_iterator iter = poses.begin(); iter!=poses.end(); ++iter)
	{
		if(uContains(occupancy, iter->first))
		{
			UASSERT(!iter->second.isNull());
			const std::pair<cv::Mat, cv::Mat> & pair = occupancy.at(iter->first);

			iter->second.getTranslationAndEulerAngles(x,y,z,roll,pitch,yaw);
			cosT = cos(yaw);
			sinT = sin(yaw);
			affineTransform.at<float>(0,0) = cosT;
			affineTransform.at<float>(0,1) = -sinT;
			affineTransform.at<float>(1,0) = sinT;
			affineTransform.at<float>(1,1) = cosT;
			affineTransform.at<float>(0,2) = x;
			affineTransform.at<float>(1,2) = y;

			if(undefinedSize)
			{
				minX = maxX = x;
				minY = maxY = y;
				undefinedSize = false;
			}
			else
			{
				if(minX > x)
					minX = x;
				else if(maxX < x)
					maxX = x;

				if(minY > y)
					minY = y;
				else if(maxY < y)
					maxY = y;
			}

			//ground
			if(pair.first.rows)
			{
				UASSERT(pair.first.type() == CV_32FC2);
				cv::Mat ground(pair.first.rows, pair.first.cols, pair.first.type());
				cv::transform(pair.first, ground, affineTransform);
				for(int i=0; i<ground.rows; ++i)
				{
					if(minX > ground.at<float>(i,0))
						minX = ground.at<float>(i,0);
					else if(maxX < ground.at<float>(i,0))
						maxX = ground.at<float>(i,0);

					if(minY > ground.at<float>(i,1))
						minY = ground.at<float>(i,1);
					else if(maxY < ground.at<float>(i,1))
						maxY = ground.at<float>(i,1);
				}
				emptyLocalMaps.insert(std::make_pair(iter->first, ground));
			}

			//obstacles
			if(pair.second.rows)
			{
				UASSERT(pair.second.type() == CV_32FC2);
				cv::Mat obstacles(pair.second.rows, pair.second.cols, pair.second.type());
				cv::transform(pair.second, obstacles, affineTransform);
				for(int i=0; i<obstacles.rows; ++i)
				{
					if(minX > obstacles.at<float>(i,0))
						minX = obstacles.at<float>(i,0);
					else if(maxX < obstacles.at<float>(i,0))
						maxX = obstacles.at<float>(i,0);

					if(minY > obstacles.at<float>(i,1))
						minY = obstacles.at<float>(i,1);
					else if(maxY < obstacles.at<float>(i,1))
						maxY = obstacles.at<float>(i,1);
				}
				occupiedLocalMaps.insert(std::make_pair(iter->first, obstacles));
			}
		}
	}
	UDEBUG("timer=%fs", timer.ticks());

	cv::Mat map;
	if(minX != maxX && minY != maxY)
	{
		//Get map size
		float margin = cellSize*10.0f;
		xMin = minX-margin;
		yMin = minY-margin;
		float xMax = maxX+margin;
		float yMax = maxY+margin;
		if(fabs((yMax - yMin) / cellSize) > 99999 ||
		   fabs((xMax - xMin) / cellSize) > 99999)
		{
			UERROR("Large map size!! map min=(%f, %f) max=(%f,%f). "
					"There's maybe an error with the poses provided! The map will not be created!",
					xMin, yMin, xMax, yMax);
		}
		else
		{
			UDEBUG("map min=(%f, %f) max=(%f,%f)", xMin, yMin, xMax, yMax);


			map = cv::Mat::ones((yMax - yMin) / cellSize + 0.5f, (xMax - xMin) / cellSize + 0.5f, CV_8S)*-1;
			for(std::map<int, Transform>::const_iterator kter = poses.begin(); kter!=poses.end(); ++kter)
			{
				std::map<int, cv::Mat >::iterator iter = emptyLocalMaps.find(kter->first);
				std::map<int, cv::Mat >::iterator jter = occupiedLocalMaps.find(kter->first);
				if(iter!=emptyLocalMaps.end())
				{
					for(int i=0; i<iter->second.rows; ++i)
					{
						cv::Point2i pt((iter->second.at<float>(i,0)-xMin)/cellSize + 0.5f, (iter->second.at<float>(i,1)-yMin)/cellSize + 0.5f);
						map.at<char>(pt.y, pt.x) = 0; // free space
					}
				}
				if(jter!=occupiedLocalMaps.end())
				{
					for(int i=0; i<jter->second.rows; ++i)
					{
						cv::Point2i pt((jter->second.at<float>(i,0)-xMin)/cellSize + 0.5f, (jter->second.at<float>(i,1)-yMin)/cellSize + 0.5f);
						map.at<char>(pt.y, pt.x) = 100; // obstacles
					}
				}

				//UDEBUG("empty=%d occupied=%d", empty, occupied);
			}

			// fill holes and remove empty from obstacle borders
			cv::Mat updatedMap = map.clone();
			std::list<std::pair<int, int> > obstacleIndices;
			for(int i=2; i<map.rows-2; ++i)
			{
				for(int j=2; j<map.cols-2; ++j)
				{
					if(map.at<char>(i, j) == -1 &&
						map.at<char>(i+1, j) != -1 &&
						map.at<char>(i-1, j) != -1 &&
						map.at<char>(i, j+1) != -1 &&
						map.at<char>(i, j-1) != -1)
					{
						updatedMap.at<char>(i, j) = 0;
					}
					else if(map.at<char>(i, j) == 100)
					{
						// obstacle/empty/unknown -> remove empty
						// unknown/empty/obstacle -> remove empty
						if(map.at<char>(i-1, j) == 0 &&
							map.at<char>(i-2, j) == -1)
						{
							updatedMap.at<char>(i-1, j) = -1;
						}
						else if(map.at<char>(i+1, j) == 0 &&
								map.at<char>(i+2, j) == -1)
						{
							updatedMap.at<char>(i+1, j) = -1;
						}
						if(map.at<char>(i, j-1) == 0 &&
							map.at<char>(i, j-2) == -1)
						{
							updatedMap.at<char>(i, j-1) = -1;
						}
						else if(map.at<char>(i, j+1) == 0 &&
								map.at<char>(i, j+2) == -1)
						{
							updatedMap.at<char>(i, j+1) = -1;
						}

						if(erode)
						{
							obstacleIndices.push_back(std::make_pair(i, j));
						}
					}
					else if(map.at<char>(i, j) == 0)
					{
						// obstacle/empty/obstacle -> remove empty
						if(map.at<char>(i-1, j) == 100 &&
							map.at<char>(i+1, j) == 100)
						{
							updatedMap.at<char>(i, j) = -1;
						}
						else if(map.at<char>(i, j-1) == 100 &&
							map.at<char>(i, j+1) == 100)
						{
							updatedMap.at<char>(i, j) = -1;
						}
					}

				}
			}
			map = updatedMap;

			if(erode)
			{
				// remove obstacles which touch to empty cells but not unknown cells
				cv::Mat erodedMap = map.clone();
				for(std::list<std::pair<int,int> >::iterator iter = obstacleIndices.begin();
					iter!= obstacleIndices.end();
					++iter)
				{
					int i = iter->first;
					int j = iter->second;
					bool touchEmpty = map.at<char>(i+1, j) == 0 ||
						map.at<char>(i-1, j) == 0 ||
						map.at<char>(i, j+1) == 0 ||
						map.at<char>(i, j-1) == 0;
					if(touchEmpty && map.at<char>(i+1, j) != -1 &&
						map.at<char>(i-1, j) != -1 &&
						map.at<char>(i, j+1) != -1 &&
						map.at<char>(i, j-1) != -1)
					{
						erodedMap.at<char>(i, j) = 0; // empty
					}
				}
				map = erodedMap;
			}
		}
	}
	UDEBUG("timer=%fs", timer.ticks());
	return map;
}

/**
 * Create 2d Occupancy grid (CV_8S)
 * -1 = unknown
 * 0 = empty space
 * 100 = obstacle
 * @param poses
 * @param scans
 * @param cellSize m
 * @param unknownSpaceFilled if false no fill, otherwise a virtual laser sweeps the unknown space from each pose (stopping on detected obstacle)
 * @param xMin
 * @param yMin
 */
cv::Mat create2DMap(const std::map<int, Transform> & poses,
		const std::map<int, pcl::PointCloud<pcl::PointXYZ>::Ptr > & scans,
		float cellSize,
		bool unknownSpaceFilled,
		float & xMin,
		float & yMin,
		float minMapSize)
{
	UDEBUG("poses=%d, scans = %d", poses.size(), scans.size());
	std::map<int, pcl::PointCloud<pcl::PointXYZ>::Ptr > localScans;

	pcl::PointCloud<pcl::PointXYZ> minMax;
	if(minMapSize > 0.0f)
	{
		minMax.push_back(pcl::PointXYZ(-minMapSize/2.0, -minMapSize/2.0, 0));
		minMax.push_back(pcl::PointXYZ(minMapSize/2.0, minMapSize/2.0, 0));
	}
	for(std::map<int, Transform>::const_iterator iter = poses.begin(); iter!=poses.end(); ++iter)
	{
		if(uContains(scans, iter->first) && scans.at(iter->first)->size())
		{
			UASSERT(!iter->second.isNull());
			pcl::PointCloud<pcl::PointXYZ>::Ptr cloud = transformPointCloud<pcl::PointXYZ>(scans.at(iter->first), iter->second);
			pcl::PointXYZ min, max;
			pcl::getMinMax3D(*cloud, min, max);
			minMax.push_back(min);
			minMax.push_back(max);
			minMax.push_back(pcl::PointXYZ(iter->second.x(), iter->second.y(), iter->second.z()));
			localScans.insert(std::make_pair(iter->first, cloud));
		}
	}

	cv::Mat map;
	if(minMax.size())
	{
		//Get map size
		pcl::PointXYZ min, max;
		pcl::getMinMax3D(minMax, min, max);

		// Added X2 to make sure that all points are inside the map (when rounded to integer)
		float marging = cellSize*10.0f;
		xMin = min.x-marging;
		yMin = min.y-marging;
		float xMax = max.x+marging;
		float yMax = max.y+marging;

		UDEBUG("map min=(%f, %f) max=(%f,%f)", xMin, yMin, xMax, yMax);

		UTimer timer;

		map = cv::Mat::ones((yMax - yMin) / cellSize + 0.5f, (xMax - xMin) / cellSize + 0.5f, CV_8S)*-1;
		std::vector<float> maxSquaredLength(localScans.size(), 0.0f);
		int j=0;
		for(std::map<int, pcl::PointCloud<pcl::PointXYZ>::Ptr >::iterator iter = localScans.begin(); iter!=localScans.end(); ++iter)
		{
			const Transform & pose = poses.at(iter->first);
			cv::Point2i start((pose.x()-xMin)/cellSize + 0.5f, (pose.y()-yMin)/cellSize + 0.5f);
			for(unsigned int i=0; i<iter->second->size(); ++i)
			{
				cv::Point2i end((iter->second->points[i].x-xMin)/cellSize + 0.5f, (iter->second->points[i].y-yMin)/cellSize + 0.5f);
				map.at<char>(end.y, end.x) = 100; // obstacle
				rayTrace(start, end, map, true); // trace free space

				if(unknownSpaceFilled)
				{
					float dx = iter->second->points[i].x - pose.x();
					float dy = iter->second->points[i].y - pose.y();
					float l = dx*dx + dy*dy;
					if(l > maxSquaredLength[j])
					{
						maxSquaredLength[j] = l;
					}
				}
			}
			++j;
		}
		UDEBUG("Ray trace known space=%fs", timer.ticks());

		// now fill unknown spaces
		if(unknownSpaceFilled)
		{
			j=0;
			for(std::map<int, pcl::PointCloud<pcl::PointXYZ>::Ptr >::iterator iter = localScans.begin(); iter!=localScans.end(); ++iter)
			{
				if(iter->second->size() > 1 && maxSquaredLength[j] > 0.0f)
				{
					float maxLength = sqrt(maxSquaredLength[j]);
					if(maxLength > cellSize)
					{
						// compute angle
						float a = (CV_PI/2.0f) /  (maxLength / cellSize);
						//UWARN("a=%f PI/256=%f", a, CV_PI/256.0f);
						UASSERT_MSG(a >= 0 && a < 5.0f*CV_PI/8.0f, uFormat("a=%f length=%f cell=%f", a, maxLength, cellSize).c_str());

						const Transform & pose = poses.at(iter->first);
						cv::Point2i start((pose.x()-xMin)/cellSize + 0.5f, (pose.y()-yMin)/cellSize + 0.5f);

						//UWARN("maxLength = %f", maxLength);
						//rotate counterclockwise from the first point until we pass the last point
						cv::Mat rotation = (cv::Mat_<float>(2,2) << cos(a), -sin(a),
																	 sin(a), cos(a));
						cv::Mat origin(2,1,CV_32F), endFirst(2,1,CV_32F), endLast(2,1,CV_32F);
						origin.at<float>(0) = pose.x();
						origin.at<float>(1) = pose.y();
						endFirst.at<float>(0) = iter->second->points[0].x;
						endFirst.at<float>(1) = iter->second->points[0].y;
						endLast.at<float>(0) = iter->second->points[iter->second->points.size()-1].x;
						endLast.at<float>(1) = iter->second->points[iter->second->points.size()-1].y;
						//UWARN("origin = %f %f", origin.at<float>(0), origin.at<float>(1));
						//UWARN("endFirst = %f %f", endFirst.at<float>(0), endFirst.at<float>(1));
						//UWARN("endLast = %f %f", endLast.at<float>(0), endLast.at<float>(1));
						cv::Mat tmp = (endFirst - origin);
						cv::Mat endRotated = rotation*((tmp/cv::norm(tmp))*maxLength) + origin;
						cv::Mat endLastVector(3,1,CV_32F), endRotatedVector(3,1,CV_32F);
						endLastVector.at<float>(0) = endLast.at<float>(0) - origin.at<float>(0);
						endLastVector.at<float>(1) = endLast.at<float>(1) - origin.at<float>(1);
						endLastVector.at<float>(2) = 0.0f;
						endRotatedVector.at<float>(0) = endRotated.at<float>(0) - origin.at<float>(0);
						endRotatedVector.at<float>(1) = endRotated.at<float>(1) - origin.at<float>(1);
						endRotatedVector.at<float>(2) = 0.0f;
						//UWARN("endRotated = %f %f", endRotated.at<float>(0), endRotated.at<float>(1));
						while(endRotatedVector.cross(endLastVector).at<float>(2) > 0.0f)
						{
							cv::Point2i end((endRotated.at<float>(0)-xMin)/cellSize + 0.5f, (endRotated.at<float>(1)-yMin)/cellSize + 0.5f);
							//end must be inside the grid
							end.x = end.x < 0?0:end.x;
							end.x = end.x >= map.cols?map.cols-1:end.x;
							end.y = end.y < 0?0:end.y;
							end.y = end.y >= map.rows?map.rows-1:end.y;
							rayTrace(start, end, map, true); // trace free space

							// next point
							endRotated = rotation*(endRotated - origin) + origin;
							endRotatedVector.at<float>(0) = endRotated.at<float>(0) - origin.at<float>(0);
							endRotatedVector.at<float>(1) = endRotated.at<float>(1) - origin.at<float>(1);
							//UWARN("endRotated = %f %f", endRotated.at<float>(0), endRotated.at<float>(1));
						}
					}
				}
				++j;
			}
			UDEBUG("Fill empty space=%fs", timer.ticks());
		}
	}
	return map;
}

void rayTrace(const cv::Point2i & start, const cv::Point2i & end, cv::Mat & grid, bool stopOnObstacle)
{
	UASSERT_MSG(start.x >= 0 && start.x < grid.cols, uFormat("start.x=%d grid.cols=%d", start.x, grid.cols).c_str());
	UASSERT_MSG(start.y >= 0 && start.y < grid.rows, uFormat("start.y=%d grid.rows=%d", start.y, grid.rows).c_str());
	UASSERT_MSG(end.x >= 0 && end.x < grid.cols, uFormat("end.x=%d grid.cols=%d", end.x, grid.cols).c_str());
	UASSERT_MSG(end.y >= 0 && end.y < grid.rows, uFormat("end.x=%d grid.cols=%d", end.y, grid.rows).c_str());

	cv::Point2i ptA, ptB;
	ptA = start;
	ptB = end;

	float slope = float(ptB.y - ptA.y)/float(ptB.x - ptA.x);

	bool swapped = false;
	if(slope<-1.0f || slope>1.0f)
	{
		// swap x and y
		slope = 1.0f/slope;

		int tmp = ptA.x;
		ptA.x = ptA.y;
		ptA.y = tmp;

		tmp = ptB.x;
		ptB.x = ptB.y;
		ptB.y = tmp;

		swapped = true;
	}

	float b = ptA.y - slope*ptA.x;
	for(int x=ptA.x; ptA.x<ptB.x?x<ptB.x:x>ptB.x; ptA.x<ptB.x?++x:--x)
	{
		int upperbound = float(x)*slope + b;
		int lowerbound = upperbound;
		if(x != ptA.x)
		{
			lowerbound = (ptA.x<ptB.x?x+1:x-1)*slope + b;
		}

		if(lowerbound > upperbound)
		{
			int tmp = upperbound;
			upperbound = lowerbound;
			lowerbound = tmp;
		}

		if(!swapped)
		{
			UASSERT_MSG(lowerbound >= 0 && lowerbound < grid.rows, uFormat("lowerbound=%f grid.rows=%d x=%d slope=%f b=%f x=%f", lowerbound, grid.rows, x, slope, b, x).c_str());
			UASSERT_MSG(upperbound >= 0 && upperbound < grid.rows, uFormat("upperbound=%f grid.rows=%d x+1=%d slope=%f b=%f x=%f", upperbound, grid.rows, x+1, slope, b, x).c_str());
		}
		else
		{
			UASSERT_MSG(lowerbound >= 0 && lowerbound < grid.cols, uFormat("lowerbound=%f grid.cols=%d x=%d slope=%f b=%f x=%f", lowerbound, grid.cols, x, slope, b, x).c_str());
			UASSERT_MSG(upperbound >= 0 && upperbound < grid.cols, uFormat("upperbound=%f grid.cols=%d x+1=%d slope=%f b=%f x=%f", upperbound, grid.cols, x+1, slope, b, x).c_str());
		}

		for(int y = lowerbound; y<=(int)upperbound; ++y)
		{
			char * v;
			if(swapped)
			{
				v = &grid.at<char>(x, y);
			}
			else
			{
				v = &grid.at<char>(y, x);
			}
			if(*v == 100 && stopOnObstacle)
			{
				return;
			}
			else
			{
				*v = 0; // free space
			}
		}
	}
}

//convert to gray scaled map
cv::Mat convertMap2Image8U(const cv::Mat & map8S)
{
	UASSERT(map8S.channels() == 1 && map8S.type() == CV_8S);
	cv::Mat map8U = cv::Mat(map8S.rows, map8S.cols, CV_8U);
	for (int i = 0; i < map8S.rows; ++i)
	{
		for (int j = 0; j < map8S.cols; ++j)
		{
			char v = map8S.at<char>(i, j);
			unsigned char gray;
			if(v == 0)
			{
				gray = 178;
			}
			else if(v == 100)
			{
				gray = 0;
			}
			else // -1
			{
				gray = 89;
			}
			map8U.at<unsigned char>(i, j) = gray;
		}
	}
	return map8U;
}

pcl::IndicesPtr concatenate(const std::vector<pcl::IndicesPtr> & indices)
{
	//compute total size
	unsigned int totalSize = 0;
	for(unsigned int i=0; i<indices.size(); ++i)
	{
		totalSize += (unsigned int)indices[i]->size();
	}
	pcl::IndicesPtr ind(new std::vector<int>(totalSize));
	unsigned int io = 0;
	for(unsigned int i=0; i<indices.size(); ++i)
	{
		for(unsigned int j=0; j<indices[i]->size(); ++j)
		{
			ind->at(io++) = indices[i]->at(j);
		}
	}
	return ind;
}

pcl::IndicesPtr concatenate(const pcl::IndicesPtr & indicesA, const pcl::IndicesPtr & indicesB)
{
	pcl::IndicesPtr ind(new std::vector<int>(*indicesA));
	ind->resize(ind->size()+indicesB->size());
	unsigned int oi = (unsigned int)indicesA->size();
	for(unsigned int i=0; i<indicesB->size(); ++i)
	{
		ind->at(oi++) = indicesB->at(i);
	}
	return ind;
}

cv::Mat decimate(const cv::Mat & image, int decimation)
{
	UASSERT(decimation >= 1);
	cv::Mat out;
	if(!image.empty())
	{
		if(decimation > 1)
		{
			if((image.type() == CV_32FC1 || image.type()==CV_16UC1))
			{
				UASSERT_MSG(image.rows % decimation == 0 && image.cols % decimation == 0, "Decimation of depth images should be exact!");

				out = cv::Mat(image.rows/decimation, image.cols/decimation, image.type());
				if(image.type() == CV_32FC1)
				{
					for(int j=0; j<out.rows; ++j)
					{
						for(int i=0; i<out.cols; ++i)
						{
							out.at<float>(j, i) = image.at<float>(j*decimation, i*decimation);
						}
					}
				}
				else // CV_16UC1
				{
					for(int j=0; j<out.rows; ++j)
					{
						for(int i=0; i<out.cols; ++i)
						{
							out.at<unsigned short>(j, i) = image.at<unsigned short>(j*decimation, i*decimation);
						}
					}
				}
			}
			else
			{
				cv::resize(image, out, cv::Size(), 1.0f/float(decimation), 1.0f/float(decimation), cv::INTER_AREA);
			}
		}
		else
		{
			out = image;
		}
	}
	return out;
}

void savePCDWords(
		const std::string & fileName,
		const std::multimap<int, pcl::PointXYZ> & words,
		const Transform & transform)
{
	if(words.size())
	{
		pcl::PointCloud<pcl::PointXYZ> cloud;
		cloud.resize(words.size());
		int i=0;
		for(std::multimap<int, pcl::PointXYZ>::const_iterator iter=words.begin(); iter!=words.end(); ++iter)
		{
			cloud[i++] = util3d::transformPoint(iter->second, transform);
		}
		pcl::io::savePCDFile(fileName, cloud);
	}
}

}

}
