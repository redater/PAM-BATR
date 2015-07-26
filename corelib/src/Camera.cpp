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

#include "rtabmap/core/Camera.h"
#include "rtabmap/core/DBDriver.h"

#include <rtabmap/utilite/UEventsManager.h>
#include <rtabmap/utilite/UConversion.h>
#include <rtabmap/utilite/UStl.h>
#include <rtabmap/utilite/UConversion.h>
#include <rtabmap/utilite/UFile.h>
#include <rtabmap/utilite/UDirectory.h>
#include <rtabmap/utilite/UTimer.h>

#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <cmath>

namespace rtabmap
{

Camera::Camera(float imageRate,
		unsigned int imageWidth,
		unsigned int imageHeight) :
	_imageRate(imageRate),
	_imageWidth(imageWidth),
	_imageHeight(imageHeight),
	_mirroring(false),
	_frameRateTimer(new UTimer())
{
}

Camera::~Camera()
{
	if(_frameRateTimer)
	{
		delete _frameRateTimer;
	}
}

void Camera::setImageSize(unsigned int width, unsigned int height)
{
	_imageWidth = width;
	_imageHeight = height;
}

void Camera::getImageSize(unsigned int & width, unsigned int & height)
{
	width = _imageWidth;
	height = _imageHeight;
}

void Camera::setCalibration(const std::string & fileName)
{
	if(UFile::getExtension(fileName).compare("yaml") == 0)
	{
		cv::FileStorage fs;
		fs.open(fileName, cv::FileStorage::READ);

		if (!fs.isOpened())
		{
			UERROR("Failed to open file \"%s\"", fileName.c_str());
			return;
		}

		cv::Mat k,d;

		cv::FileNode n = fs["camera_matrix"];
		int rows = n["rows"];
		int cols = n["cols"];
		std::vector<double> data;
		n["data"] >> data;
		if(rows > 0 && cols > 0 && (int)data.size() == rows*cols)
		{
			k = cv::Mat(rows, cols, CV_64FC1, data.data()).clone();
		}

		cv::FileNode nd = fs["distortion_coefficients"];
		rows = nd["rows"];
		cols = nd["cols"];
		data.clear();
		nd["data"] >> data;
		if(rows > 0 && cols > 0 && (int)data.size() == rows*cols)
		{
			d = cv::Mat(rows, cols, CV_64FC1, data.data()).clone();
		}

		if(k.empty())
		{
			UERROR("Failed to load \"camera_matrix\" matrix.");
		}
		if(d.empty())
		{
			UERROR("Failed to load \"distortion_coefficients\" matrix.");
		}
		if(!k.empty() && !d.empty())
		{
			this->setCalibration(k, d);
		}
	}
	else
	{
		UERROR("Calibration file must be in \"*.yaml\" format");
	}
}

void Camera::setCalibration(const cv::Mat & cameraMatrix, const cv::Mat & distorsionCoefficients)
{
	UASSERT(cameraMatrix.type() == CV_64FC1 &&
			cameraMatrix.rows == 3 &&
			cameraMatrix.cols == 3);
	UASSERT(distorsionCoefficients.type() == CV_64FC1 &&
			distorsionCoefficients.rows ==1 &&
			(distorsionCoefficients.cols == 4 || distorsionCoefficients.cols == 5 || distorsionCoefficients.cols == 8));

	_k = cameraMatrix;
	_d = distorsionCoefficients;
}

void Camera::resetCalibration()
{
	_k = cv::Mat();
	_d = cv::Mat();
}

cv::Mat Camera::takeImage()
{
	cv::Mat img;
	float imageRate = _imageRate==0.0f?33.0f:_imageRate; // limit to 33Hz if infinity
	if(imageRate>0)
	{
		int sleepTime = (1000.0f/imageRate - 1000.0f*_frameRateTimer->getElapsedTime());
		if(sleepTime > 2)
		{
			uSleep(sleepTime-2);
		}

		// Add precision at the cost of a small overhead
		while(_frameRateTimer->getElapsedTime() < 1.0/double(imageRate)-0.000001)
		{
			//
		}

		double slept = _frameRateTimer->getElapsedTime();
		_frameRateTimer->start();
		UDEBUG("slept=%fs vs target=%fs", slept, 1.0/double(imageRate));
	}

	UTimer timer;
	img = this->captureImage();
	if(!img.empty() && !_k.empty() && !_d.empty())
	{
		cv::Mat temp = img.clone();
		cv::undistort(temp, img, _k, _d);
	}
	if(!img.empty() && _mirroring)
	{
		cv::flip(img,img,1);
	}
	UDEBUG("Time capturing image = %fs", timer.ticks());
	return img;
}

/////////////////////////
// CameraImages
/////////////////////////
CameraImages::CameraImages(const std::string & path,
					 int startAt,
					 bool refreshDir,
					 float imageRate,
					 unsigned int imageWidth,
					 unsigned int imageHeight) :
	Camera(imageRate, imageWidth, imageHeight),
	_path(path),
	_startAt(startAt),
	_refreshDir(refreshDir),
	_count(0),
	_dir(0)
{

}

CameraImages::~CameraImages(void)
{
	if(_dir)
	{
		delete _dir;
	}
}

bool CameraImages::init()
{
	UDEBUG("");
	if(_dir)
	{
		_dir->setPath(_path, "jpg ppm png bmp pnm tiff");
	}
	else
	{
		_dir = new UDirectory(_path, "jpg ppm png bmp pnm tiff");
	}
	_count = 0;
	if(_path[_path.size()-1] != '\\' && _path[_path.size()-1] != '/')
	{
		_path.append("/");
	}
	if(!_dir->isValid())
	{
		ULOGGER_ERROR("Directory path is not valid \"%s\"", _path.c_str());
	}
	else if(_dir->getFileNames().size() == 0)
	{
		UWARN("Directory is empty \"%s\"", _path.c_str());
	}
	return _dir->isValid();
}

cv::Mat CameraImages::captureImage()
{
	cv::Mat img;
	UDEBUG("");
	if(_dir->isValid())
	{
		if(_refreshDir)
		{
			_dir->update();
		}
		if(_startAt == 0)
		{
			const std::list<std::string> & fileNames = _dir->getFileNames();
			if(fileNames.size())
			{
				if(_lastFileName.empty() || uStrNumCmp(_lastFileName,*fileNames.rbegin()) < 0)
				{
					_lastFileName = *fileNames.rbegin();
					std::string fullPath = _path + _lastFileName;
					img = cv::imread(fullPath.c_str());
				}
			}
		}
		else
		{
			std::string fileName;
			std::string fullPath;
			fileName = _dir->getNextFileName();
			if(fileName.size())
			{
				fullPath = _path + fileName;
				while(++_count < _startAt && (fileName = _dir->getNextFileName()).size())
				{
					fullPath = _path + fileName;
				}
				if(fileName.size())
				{
					ULOGGER_DEBUG("Loading image : %s", fullPath.c_str());

#if CV_MAJOR_VERSION >2 || (CV_MAJOR_VERSION >=2 && CV_MINOR_VERSION >=4)
					img = cv::imread(fullPath.c_str(), cv::IMREAD_UNCHANGED);
#else
					img = cv::imread(fullPath.c_str(), -1);
#endif
					UDEBUG("width=%d, height=%d, channels=%d, elementSize=%d, total=%d",
							img.cols, img.rows, img.channels(), img.elemSize(), img.total());

					// FIXME : it seems that some png are incorrectly loaded with opencv c++ interface, where c interface works...
					if(img.depth() != CV_8U)
					{
						// The depth should be 8U
						UWARN("Cannot read the image correctly, falling back to old OpenCV C interface...");
						IplImage * i = cvLoadImage(fullPath.c_str());
						img = cv::Mat(i, true);
						cvReleaseImage(&i);
					}

					if(img.channels()>3)
					{
						UWARN("Conversion from 4 channels to 3 channels (file=%s)", fullPath.c_str());
						cv::Mat out;
						cv::cvtColor(img, out, CV_BGRA2BGR);
						img = out;
					}
				}
			}
		}
	}
	else
	{
		UWARN("Directory is not set, camera must be initialized.");
	}

	unsigned int w;
	unsigned int h;
	this->getImageSize(w, h);

	if(!img.empty() &&
	   w &&
	   h &&
	   w != (unsigned int)img.cols &&
	   h != (unsigned int)img.rows)
	{
		cv::Mat resampled;
		cv::resize(img, resampled, cv::Size(w, h));
		img = resampled;
	}
	return img;
}



/////////////////////////
// CameraVideo
/////////////////////////
CameraVideo::CameraVideo(int usbDevice,
						 float imageRate,
						 unsigned int imageWidth,
						 unsigned int imageHeight) :
	Camera(imageRate, imageWidth, imageHeight),
	_src(kUsbDevice),
	_usbDevice(usbDevice)
{

}

CameraVideo::CameraVideo(const std::string & filePath,
						   float imageRate,
						   unsigned int imageWidth,
						   unsigned int imageHeight) :
	Camera(imageRate, imageWidth, imageHeight),
	_filePath(filePath),
	_src(kVideoFile),
	_usbDevice(0)
{
}

CameraVideo::~CameraVideo()
{
	_capture.release();
}

bool CameraVideo::init()
{
	if(_capture.isOpened())
	{
		_capture.release();
	}

	if(_src == kUsbDevice)
	{
		unsigned int w;
		unsigned int h;
		this->getImageSize(w, h);

		ULOGGER_DEBUG("CameraVideo::init() Usb device initialization on device %d with imgSize=[%d,%d]", _usbDevice, w, h);
		_capture.open(_usbDevice);

		if(w && h)
		{
			_capture.set(CV_CAP_PROP_FRAME_WIDTH, double(w));
			_capture.set(CV_CAP_PROP_FRAME_HEIGHT, double(h));
		}
	}
	else if(_src == kVideoFile)
	{
		ULOGGER_DEBUG("Camera: filename=\"%s\"", _filePath.c_str());
		_capture.open(_filePath.c_str());
	}
	else
	{
		ULOGGER_ERROR("Camera: Unknown source...");
	}
	if(!_capture.isOpened())
	{
		ULOGGER_ERROR("Camera: Failed to create a capture object!");
		_capture.release();
		return false;
	}
	return true;
}

cv::Mat CameraVideo::captureImage()
{
	cv::Mat img;
	if(_capture.isOpened())
	{
		if(_capture.read(img))
		{
			unsigned int w;
			unsigned int h;
			this->getImageSize(w, h);

			if(!img.empty() &&
			   w &&
			   h &&
			   w != (unsigned int)img.cols &&
			   h != (unsigned int)img.rows)
			{
				cv::Mat resampled;
				cv::resize(img, resampled, cv::Size(w, h));
				img = resampled;
			}
			else
			{
				// clone required
				img = img.clone();
			}
		}
		else if(_usbDevice)
		{
			UERROR("Camera has been disconnected!");
		}
	}
	else
	{
		ULOGGER_WARN("The camera must be initialized before requesting an image.");
	}
	return img;
}

} // namespace rtabmap
