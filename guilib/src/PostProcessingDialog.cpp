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

#include "PostProcessingDialog.h"
#include "ui_postProcessingDialog.h"

#include <QPushButton>

namespace rtabmap {

PostProcessingDialog::PostProcessingDialog(QWidget * parent) :
	QDialog(parent)
{
	_ui = new Ui_PostProcessingDialog();
	_ui->setupUi(this);

	connect(_ui->detectMoreLoopClosures, SIGNAL(clicked(bool)), this, SLOT(updateButtonBox()));
	connect(_ui->refineNeighborLinks, SIGNAL(stateChanged(int)), this, SLOT(updateButtonBox()));
	connect(_ui->refineLoopClosureLinks, SIGNAL(stateChanged(int)), this, SLOT(updateButtonBox()));
	connect(_ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked()), this, SLOT(restoreDefaults()));

	connect(_ui->detectMoreLoopClosures, SIGNAL(clicked(bool)), this, SIGNAL(configChanged()));
	connect(_ui->clusterRadius, SIGNAL(valueChanged(double)), this, SIGNAL(configChanged()));
	connect(_ui->clusterAngle, SIGNAL(valueChanged(double)), this, SIGNAL(configChanged()));
	connect(_ui->iterations, SIGNAL(valueChanged(int)), this, SIGNAL(configChanged()));
	connect(_ui->reextractFeatures, SIGNAL(stateChanged(int)), this, SIGNAL(configChanged()));
	connect(_ui->refineNeighborLinks, SIGNAL(stateChanged(int)), this, SIGNAL(configChanged()));
	connect(_ui->refineLoopClosureLinks, SIGNAL(stateChanged(int)), this, SIGNAL(configChanged()));
}

PostProcessingDialog::~PostProcessingDialog()
{
	delete _ui;
}

void PostProcessingDialog::saveSettings(QSettings & settings, const QString & group) const
{
	if(!group.isEmpty())
	{
		settings.beginGroup(group);
	}
	settings.setValue("detect_more_lc", this->isDetectMoreLoopClosures());
	settings.setValue("cluster_radius", this->clusterRadius());
	settings.setValue("cluster_angle", this->clusterAngle());
	settings.setValue("iterations", this->iterations());
	settings.setValue("reextract_features", this->isReextractFeatures());
	settings.setValue("refine_neigbors", this->isRefineNeighborLinks());
	settings.setValue("refine_lc", this->isRefineLoopClosureLinks());
	if(!group.isEmpty())
	{
		settings.endGroup();
	}
}

void PostProcessingDialog::loadSettings(QSettings & settings, const QString & group)
{
	if(!group.isEmpty())
	{
		settings.beginGroup(group);
	}
	this->setDetectMoreLoopClosures(settings.value("detect_more_lc", this->isDetectMoreLoopClosures()).toBool());
	this->setClusterRadius(settings.value("cluster_radius", this->clusterRadius()).toDouble());
	this->setClusterAngle(settings.value("cluster_angle", this->clusterAngle()).toDouble());
	this->setIterations(settings.value("iterations", this->iterations()).toInt());
	this->setReextractFeatures(settings.value("reextract_features", this->isReextractFeatures()).toBool());
	this->setRefineNeighborLinks(settings.value("refine_neigbors", this->isRefineNeighborLinks()).toBool());
	this->setRefineLoopClosureLinks(settings.value("refine_lc", this->isRefineLoopClosureLinks()).toBool());
	if(!group.isEmpty())
	{
		settings.endGroup();
	}
}

void PostProcessingDialog::restoreDefaults()
{
	setDetectMoreLoopClosures(true);
	setClusterRadius(0.3);
	setClusterAngle(30);
	setIterations(1);
	setReextractFeatures(false);
	setRefineNeighborLinks(false);
	setRefineLoopClosureLinks(false);
}

void PostProcessingDialog::updateButtonBox()
{
	_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
			isDetectMoreLoopClosures() || isRefineNeighborLinks() || isRefineLoopClosureLinks());
}

bool PostProcessingDialog::isDetectMoreLoopClosures() const
{
	return _ui->detectMoreLoopClosures->isChecked();
}

double PostProcessingDialog::clusterRadius() const
{
	return _ui->clusterRadius->value();
}

double PostProcessingDialog::clusterAngle() const
{
	return _ui->clusterAngle->value();
}

int PostProcessingDialog::iterations() const
{
	return _ui->iterations->value();
}

bool PostProcessingDialog::isReextractFeatures() const
{
	return _ui->reextractFeatures->isChecked();
}

bool PostProcessingDialog::isRefineNeighborLinks() const
{
	return _ui->refineNeighborLinks->isChecked();
}

bool PostProcessingDialog::isRefineLoopClosureLinks() const
{
	return _ui->refineLoopClosureLinks->isChecked();
}

//setters
void PostProcessingDialog::setDetectMoreLoopClosures(bool on)
{
	_ui->detectMoreLoopClosures->setChecked(on);
}
void PostProcessingDialog::setClusterRadius(double radius)
{
	_ui->clusterRadius->setValue(radius);
}
void PostProcessingDialog::setClusterAngle(double angle)
{
	_ui->clusterAngle->setValue(angle);
}
void PostProcessingDialog::setIterations(int iterations)
{
	_ui->iterations->setValue(iterations);
}
void PostProcessingDialog::setReextractFeatures(bool on)
{
	_ui->reextractFeatures->setChecked(on);
}
void PostProcessingDialog::setRefineNeighborLinks(bool on)
{
	_ui->refineNeighborLinks->setChecked(on);
}
void PostProcessingDialog::setRefineLoopClosureLinks(bool on)
{
	_ui->refineLoopClosureLinks->setChecked(on);
}

}
