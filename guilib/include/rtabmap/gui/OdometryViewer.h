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

#ifndef ODOMETRYVIEWER_H_
#define ODOMETRYVIEWER_H_

#include "rtabmap/gui/RtabmapGuiExp.h" // DLL export/import defines

#include "rtabmap/core/SensorData.h"
#include "rtabmap/core/OdometryInfo.h"
#include <QDialog>
#include "rtabmap/utilite/UEventsHandler.h"

class QSpinBox;
class QDoubleSpinBox;
class QLabel;

namespace rtabmap {

class ImageView;
class CloudViewer;

class RTABMAPGUI_EXP OdometryViewer : public QDialog, public UEventsHandler
{
	Q_OBJECT

public:
	OdometryViewer(int maxClouds = 10, int decimation = 2, float voxelSize = 0.0f, int qualityWarningThr=0, QWidget * parent = 0);
	virtual ~OdometryViewer();

public slots:
	virtual void clear();

protected:
	virtual void handleEvent(UEvent * event);

private slots:
	void processData(const rtabmap::SensorData & data, const rtabmap::OdometryInfo & info);

private:
	ImageView* imageView_;
	CloudViewer* cloudView_;
	bool processingData_;
	bool odomImageShow_;
	bool odomImageDepthShow_;

	Transform lastOdomPose_;
	int qualityWarningThr_;
	int id_;
	QList<std::string> addedClouds_;

	QSpinBox * maxCloudsSpin_;
	QDoubleSpinBox * voxelSpin_;
	QSpinBox * decimationSpin_;
	QLabel * timeLabel_;
	int validDecimationValue_;
};

} /* namespace rtabmap */
#endif /* ODOMETRYVIEWER_H_ */
