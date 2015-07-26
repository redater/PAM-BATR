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

#include "rtabmap/gui/DatabaseViewer.h"
#include "rtabmap/gui/CloudViewer.h"
#include "ui_DatabaseViewer.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QGraphicsLineItem>
#include <QtGui/QCloseEvent>
#include <QGraphicsOpacityEffect>
#include <QtCore/QBuffer>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <rtabmap/utilite/ULogger.h>
#include <rtabmap/utilite/UDirectory.h>
#include <rtabmap/utilite/UConversion.h>
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui.hpp>
#include <rtabmap/utilite/UTimer.h>
#include "rtabmap/core/Memory.h"
#include "rtabmap/core/DBDriver.h"
#include "rtabmap/gui/KeypointItem.h"
#include "rtabmap/gui/UCv2Qt.h"
#include "rtabmap/core/util3d.h"
#include "rtabmap/core/Signature.h"
#include "rtabmap/core/Features2d.h"
#include "rtabmap/core/Compression.h"
#include "rtabmap/core/Graph.h"
#include "rtabmap/gui/DataRecorder.h"
#include "rtabmap/core/SensorData.h"
#include "ExportDialog.h"
#include "DetailedProgressDialog.h"

#include <pcl/io/pcd_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/common/transforms.h>

namespace rtabmap {

DatabaseViewer::DatabaseViewer(QWidget * parent) :
	QMainWindow(parent),
	memory_(0),
	savedMaximized_(false),
	firstCall_(true)
{
	pathDatabase_ = QDir::homePath()+"/Documents/RTAB-Map"; //use home directory by default

	if(!UDirectory::exists(pathDatabase_.toStdString()))
	{
		pathDatabase_ = QDir::homePath();
	}

	ui_ = new Ui_DatabaseViewer();
	ui_->setupUi(this);
	ui_->buttonBox->setVisible(false);
	connect(ui_->buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(close()));

	QString title("RTAB-Map Database Viewer[*]");
	this->setWindowTitle(title);

	ui_->dockWidget_constraints->setVisible(false);
	ui_->dockWidget_graphView->setVisible(false);
	ui_->dockWidget_icp->setVisible(false);
	ui_->dockWidget_visual->setVisible(false);
	ui_->dockWidget_stereoView->setVisible(false);
	ui_->dockWidget_view3d->setVisible(false);
	ui_->dockWidget_detectMoreLC->setVisible(false);
	ui_->dockWidget_graphOptimizer->setVisible(false);
	ui_->dockWidget_gridcloud->setVisible(false);

	ui_->constraintsViewer->setCameraLockZ(false);
	ui_->constraintsViewer->setCameraFree();

	this->readSettings();

	if(RTABMAP_NONFREE == 0)
	{
		ui_->comboBox_featureType->setItemData(0, 0, Qt::UserRole - 1);
		ui_->comboBox_featureType->setItemData(1, 0, Qt::UserRole - 1);

		if(ui_->comboBox_featureType->currentIndex() <= 1)
		{
			UWARN("SURF/SIFT not available, setting feature default to FAST/BRIEF.");
			ui_->comboBox_featureType->setCurrentIndex(4);
			ui_->comboBox_nnType->setCurrentIndex(3);
		}
	}
	if(!graph::G2OOptimizer::available())
	{
		ui_->comboBox_graphOptimizer->setItemData(1, 0, Qt::UserRole - 1);
		if(ui_->comboBox_graphOptimizer->currentIndex() == 1)
		{
			UWARN("g2o is not available, setting optimization default to TORO.");
			ui_->comboBox_graphOptimizer->setCurrentIndex(0);
		}
	}

	ui_->menuView->addAction(ui_->dockWidget_constraints->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_graphView->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_icp->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_visual->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_stereoView->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_view3d->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_detectMoreLC->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_graphOptimizer->toggleViewAction());
	ui_->menuView->addAction(ui_->dockWidget_gridcloud->toggleViewAction());
	connect(ui_->dockWidget_graphView->toggleViewAction(), SIGNAL(triggered()), this, SLOT(updateGraphView()));

	connect(ui_->actionQuit, SIGNAL(triggered()), this, SLOT(close()));

	// connect actions with custom slots
	ui_->actionSave_config->setShortcut(QKeySequence::Save);
	connect(ui_->actionSave_config, SIGNAL(triggered()), this, SLOT(writeSettings()));
	connect(ui_->actionOpen_database, SIGNAL(triggered()), this, SLOT(openDatabase()));
	connect(ui_->actionExport, SIGNAL(triggered()), this, SLOT(exportDatabase()));
	connect(ui_->actionExtract_images, SIGNAL(triggered()), this, SLOT(extractImages()));
	connect(ui_->actionGenerate_graph_dot, SIGNAL(triggered()), this, SLOT(generateGraph()));
	connect(ui_->actionGenerate_local_graph_dot, SIGNAL(triggered()), this, SLOT(generateLocalGraph()));
	connect(ui_->actionGenerate_TORO_graph_graph, SIGNAL(triggered()), this, SLOT(generateTOROGraph()));
	connect(ui_->actionView_3D_map, SIGNAL(triggered()), this, SLOT(view3DMap()));
	connect(ui_->actionGenerate_3D_map_pcd, SIGNAL(triggered()), this, SLOT(generate3DMap()));
	connect(ui_->actionDetect_more_loop_closures, SIGNAL(triggered()), this, SLOT(detectMoreLoopClosures()));
	connect(ui_->actionRefine_all_neighbor_links, SIGNAL(triggered()), this, SLOT(refineAllNeighborLinks()));
	connect(ui_->actionRefine_all_loop_closure_links, SIGNAL(triggered()), this, SLOT(refineAllLoopClosureLinks()));
	connect(ui_->actionVisual_Refine_all_neighbor_links, SIGNAL(triggered()), this, SLOT(refineVisuallyAllNeighborLinks()));
	connect(ui_->actionVisual_Refine_all_loop_closure_links, SIGNAL(triggered()), this, SLOT(refineVisuallyAllLoopClosureLinks()));

	//ICP buttons
	connect(ui_->pushButton_refine, SIGNAL(clicked()), this, SLOT(refineConstraint()));
	connect(ui_->pushButton_refineVisually, SIGNAL(clicked()), this, SLOT(refineConstraintVisually()));
	connect(ui_->pushButton_add, SIGNAL(clicked()), this, SLOT(addConstraint()));
	connect(ui_->pushButton_reset, SIGNAL(clicked()), this, SLOT(resetConstraint()));
	connect(ui_->pushButton_reject, SIGNAL(clicked()), this, SLOT(rejectConstraint()));
	ui_->pushButton_refine->setEnabled(false);
	ui_->pushButton_refineVisually->setEnabled(false);
	ui_->pushButton_add->setEnabled(false);
	ui_->pushButton_reset->setEnabled(false);
	ui_->pushButton_reject->setEnabled(false);

	ui_->actionGenerate_TORO_graph_graph->setEnabled(false);

	ui_->horizontalSlider_A->setTracking(false);
	ui_->horizontalSlider_B->setTracking(false);
	ui_->horizontalSlider_A->setEnabled(false);
	ui_->horizontalSlider_B->setEnabled(false);
	connect(ui_->horizontalSlider_A, SIGNAL(valueChanged(int)), this, SLOT(sliderAValueChanged(int)));
	connect(ui_->horizontalSlider_B, SIGNAL(valueChanged(int)), this, SLOT(sliderBValueChanged(int)));
	connect(ui_->horizontalSlider_A, SIGNAL(sliderMoved(int)), this, SLOT(sliderAMoved(int)));
	connect(ui_->horizontalSlider_B, SIGNAL(sliderMoved(int)), this, SLOT(sliderBMoved(int)));

	ui_->horizontalSlider_neighbors->setTracking(false);
	ui_->horizontalSlider_loops->setTracking(false);
	ui_->horizontalSlider_neighbors->setEnabled(false);
	ui_->horizontalSlider_loops->setEnabled(false);
	connect(ui_->horizontalSlider_neighbors, SIGNAL(valueChanged(int)), this, SLOT(sliderNeighborValueChanged(int)));
	connect(ui_->horizontalSlider_loops, SIGNAL(valueChanged(int)), this, SLOT(sliderLoopValueChanged(int)));
	connect(ui_->horizontalSlider_neighbors, SIGNAL(sliderMoved(int)), this, SLOT(sliderNeighborValueChanged(int)));
	connect(ui_->horizontalSlider_loops, SIGNAL(sliderMoved(int)), this, SLOT(sliderLoopValueChanged(int)));
	connect(ui_->checkBox_showOptimized, SIGNAL(stateChanged(int)), this, SLOT(updateConstraintView()));
	connect(ui_->checkBox_show3DWords, SIGNAL(stateChanged(int)), this, SLOT(updateConstraintView()));
	ui_->checkBox_showOptimized->setEnabled(false);

	ui_->horizontalSlider_iterations->setTracking(false);
	ui_->horizontalSlider_iterations->setEnabled(false);
	ui_->spinBox_optimizationsFrom->setEnabled(false);
	connect(ui_->horizontalSlider_iterations, SIGNAL(valueChanged(int)), this, SLOT(sliderIterationsValueChanged(int)));
	connect(ui_->horizontalSlider_iterations, SIGNAL(sliderMoved(int)), this, SLOT(sliderIterationsValueChanged(int)));
	connect(ui_->spinBox_iterations, SIGNAL(editingFinished()), this, SLOT(updateGraphView()));
	connect(ui_->spinBox_optimizationsFrom, SIGNAL(editingFinished()), this, SLOT(updateGraphView()));
	connect(ui_->checkBox_ignoreCovariance, SIGNAL(stateChanged(int)), this, SLOT(updateGraphView()));
	connect(ui_->checkBox_ignorePoseCorrection, SIGNAL(stateChanged(int)), this, SLOT(updateGraphView()));
	connect(ui_->comboBox_graphOptimizer, SIGNAL(currentIndexChanged(int)), this, SLOT(updateGraphView()));
	connect(ui_->checkBox_2dslam, SIGNAL(stateChanged(int)), this, SLOT(updateGraphView()));
	connect(ui_->spinBox_optimizationDepth, SIGNAL(editingFinished()), this, SLOT(updateGraphView()));
	connect(ui_->checkBox_gridErode, SIGNAL(stateChanged(int)), this, SLOT(updateGraphView()));
	connect(ui_->groupBox_posefiltering, SIGNAL(clicked(bool)), this, SLOT(updateGraphView()));
	connect(ui_->doubleSpinBox_posefilteringRadius, SIGNAL(editingFinished()), this, SLOT(updateGraphView()));
	connect(ui_->doubleSpinBox_posefilteringAngle, SIGNAL(editingFinished()), this, SLOT(updateGraphView()));

	connect(ui_->groupBox_gridFromProjection, SIGNAL(clicked(bool)), this, SLOT(updateGrid()));
	connect(ui_->doubleSpinBox_gridCellSize, SIGNAL(editingFinished()), this, SLOT(updateGrid()));
	connect(ui_->spinBox_projDecimation, SIGNAL(editingFinished()), this, SLOT(updateGrid()));
	connect(ui_->doubleSpinBox_projMaxDepth, SIGNAL(editingFinished()), this, SLOT(updateGrid()));


	// connect configuration changed
	connect(ui_->graphViewer, SIGNAL(configChanged()), this, SLOT(configModified()));
	//connect(ui_->graphicsView_A, SIGNAL(configChanged()), this, SLOT(configModified()));
	//connect(ui_->graphicsView_B, SIGNAL(configChanged()), this, SLOT(configModified()));
	// Graph view
	connect(ui_->spinBox_iterations, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_ignoreCovariance, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	connect(ui_->comboBox_graphOptimizer, SIGNAL(currentIndexChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_2dslam, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	connect(ui_->spinBox_optimizationDepth, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_gridErode, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	connect(ui_->groupBox_gridFromProjection, SIGNAL(clicked(bool)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_gridCellSize, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->spinBox_projDecimation, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_projMaxDepth, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->groupBox_posefiltering, SIGNAL(clicked(bool)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_posefilteringRadius, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_posefilteringAngle, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	// ICP parameters
	connect(ui_->spinBox_icp_decimation, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_icp_maxDepth, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_icp_voxel, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_icp_maxCorrespDistance, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->spinBox_icp_iteration, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_icp_p2plane, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	connect(ui_->spinBox_icp_normalKSearch, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_icp_2d, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	// Visual parameters
	connect(ui_->groupBox_visual_recomputeFeatures, SIGNAL(clicked(bool)), this, SLOT(configModified()));
	connect(ui_->comboBox_featureType, SIGNAL(currentIndexChanged(int)), this, SLOT(configModified()));
	connect(ui_->comboBox_nnType, SIGNAL(currentIndexChanged(int)), this, SLOT(configModified()));
	connect(ui_->checkBox_visual_2d, SIGNAL(stateChanged(int)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_visual_nndr, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->spinBox_visual_minCorrespondences, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_visual_maxCorrespDistance, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->spinBox_visual_iteration, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_visual_maxDepth, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_detectMore_radius, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->doubleSpinBox_detectMore_angle, SIGNAL(valueChanged(double)), this, SLOT(configModified()));
	connect(ui_->spinBox_detectMore_iterations, SIGNAL(valueChanged(int)), this, SLOT(configModified()));
	// dockwidget
	QList<QDockWidget*> dockWidgets = this->findChildren<QDockWidget*>();
	for(int i=0; i<dockWidgets.size(); ++i)
	{
		connect(dockWidgets[i], SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(configModified()));
		connect(dockWidgets[i]->toggleViewAction(), SIGNAL(toggled(bool)), this, SLOT(configModified()));
	}
	ui_->dockWidget_constraints->installEventFilter(this);
	ui_->dockWidget_graphView->installEventFilter(this);
	ui_->dockWidget_icp->installEventFilter(this);
	ui_->dockWidget_stereoView->installEventFilter(this);
	ui_->dockWidget_visual->installEventFilter(this);
	ui_->dockWidget_view3d->installEventFilter(this);
	ui_->dockWidget_detectMoreLC->installEventFilter(this);
	ui_->dockWidget_graphOptimizer->installEventFilter(this);
	ui_->dockWidget_gridcloud->installEventFilter(this);
}

DatabaseViewer::~DatabaseViewer()
{
	delete ui_;
	if(memory_)
	{
		delete memory_;
	}
}

void DatabaseViewer::showCloseButton(bool visible)
{
	ui_->buttonBox->setVisible(visible);
}

void DatabaseViewer::configModified()
{
	this->setWindowModified(true);
}

QString DatabaseViewer::getIniFilePath() const
{
	QString privatePath = QDir::homePath() + "/.rtabmap";
	if(!QDir(privatePath).exists())
	{
		QDir::home().mkdir(".rtabmap");
	}
	return privatePath + "/dbviewer.ini";
}

void DatabaseViewer::readSettings()
{
	QString path = getIniFilePath();
	QSettings settings(path, QSettings::IniFormat);
	settings.beginGroup("DatabaseViewer");

	//load window state / geometry
	QByteArray bytes;
	bytes = settings.value("geometry", QByteArray()).toByteArray();
	if(!bytes.isEmpty())
	{
		this->restoreGeometry(bytes);
	}
	bytes = settings.value("state", QByteArray()).toByteArray();
	if(!bytes.isEmpty())
	{
		this->restoreState(bytes);
	}
	savedMaximized_ = settings.value("maximized", false).toBool();

	// GraphViewer settings
	ui_->graphViewer->loadSettings(settings, "GraphView");

	settings.beginGroup("optimization");
	ui_->spinBox_iterations->setValue(settings.value("iterations", ui_->spinBox_iterations->value()).toInt());
	ui_->checkBox_ignoreCovariance->setChecked(settings.value("ignoreCovariance", ui_->checkBox_ignoreCovariance->isChecked()).toBool());
	ui_->checkBox_ignorePoseCorrection->setChecked(settings.value("ignorePoseCorrection", ui_->checkBox_ignorePoseCorrection->isChecked()).toBool());
	ui_->comboBox_graphOptimizer->setCurrentIndex(settings.value("strategy", ui_->comboBox_graphOptimizer->currentIndex()).toInt());
	ui_->checkBox_2dslam->setChecked(settings.value("slam2d", ui_->checkBox_2dslam->isChecked()).toBool());
	ui_->spinBox_optimizationDepth->setValue(settings.value("depth", ui_->spinBox_optimizationDepth->value()).toInt());
	ui_->checkBox_gridErode->setChecked(settings.value("erode", ui_->checkBox_gridErode->isChecked()).toBool());
	settings.endGroup();

	settings.beginGroup("grid");
	ui_->groupBox_gridFromProjection->setChecked(settings.value("gridFromProj", ui_->groupBox_gridFromProjection->isChecked()).toBool());
	ui_->doubleSpinBox_gridCellSize->setValue(settings.value("gridCellSize", ui_->doubleSpinBox_gridCellSize->value()).toDouble());
	ui_->spinBox_projDecimation->setValue(settings.value("projDecimation", ui_->spinBox_projDecimation->value()).toInt());
	ui_->doubleSpinBox_projMaxDepth->setValue(settings.value("projMaxDepth", ui_->doubleSpinBox_projMaxDepth->value()).toDouble());
	ui_->groupBox_posefiltering->setChecked(settings.value("poseFiltering", ui_->groupBox_posefiltering->isChecked()).toBool());
	ui_->doubleSpinBox_posefilteringRadius->setValue(settings.value("poseFilteringRadius", ui_->doubleSpinBox_posefilteringRadius->value()).toDouble());
	ui_->doubleSpinBox_posefilteringAngle->setValue(settings.value("poseFilteringAngle", ui_->doubleSpinBox_posefilteringAngle->value()).toDouble());
	settings.endGroup();

	// ImageViews
	//ui_->graphicsView_A->loadSettings(settings, "ImageViewA");
	//ui_->graphicsView_B->loadSettings(settings, "ImageViewB");

	// ICP parameters
	settings.beginGroup("icp");
	ui_->spinBox_icp_decimation->setValue(settings.value("decimation", ui_->spinBox_icp_decimation->value()).toInt());
	ui_->doubleSpinBox_icp_maxDepth->setValue(settings.value("maxDepth", ui_->doubleSpinBox_icp_maxDepth->value()).toDouble());
	ui_->doubleSpinBox_icp_voxel->setValue(settings.value("voxel", ui_->doubleSpinBox_icp_voxel->value()).toDouble());
	ui_->doubleSpinBox_icp_maxCorrespDistance->setValue(settings.value("maxCorrDist", ui_->doubleSpinBox_icp_maxCorrespDistance->value()).toDouble());
	ui_->spinBox_icp_iteration->setValue(settings.value("iterations", ui_->spinBox_icp_iteration->value()).toInt());
	ui_->checkBox_icp_p2plane->setChecked(settings.value("point2place", ui_->checkBox_icp_p2plane->isChecked()).toBool());
	ui_->spinBox_icp_normalKSearch->setValue(settings.value("normalKSearch", ui_->spinBox_icp_normalKSearch->value()).toInt());
	ui_->checkBox_icp_2d->setChecked(settings.value("icp2d", ui_->checkBox_icp_2d->isChecked()).toBool());
	settings.endGroup();

	// Visual parameters
	settings.beginGroup("visual");
	ui_->groupBox_visual_recomputeFeatures->setChecked(settings.value("reextract", ui_->groupBox_visual_recomputeFeatures->isChecked()).toBool());
	ui_->comboBox_featureType->setCurrentIndex(settings.value("featureType", ui_->comboBox_featureType->currentIndex()).toInt());
	ui_->comboBox_nnType->setCurrentIndex(settings.value("nnType", ui_->comboBox_nnType->currentIndex()).toInt());
	ui_->checkBox_visual_2d->setChecked(settings.value("force2d", ui_->checkBox_visual_2d->isChecked()).toBool());
	ui_->doubleSpinBox_visual_nndr->setValue(settings.value("nndr", ui_->doubleSpinBox_visual_nndr->value()).toDouble());
	ui_->spinBox_visual_minCorrespondences->setValue(settings.value("minCorr", ui_->spinBox_visual_minCorrespondences->value()).toInt());
	ui_->doubleSpinBox_visual_maxCorrespDistance->setValue(settings.value("maxCorrDist", ui_->doubleSpinBox_visual_maxCorrespDistance->value()).toDouble());
	ui_->spinBox_visual_iteration->setValue(settings.value("iterations", ui_->spinBox_visual_iteration->value()).toDouble());
	ui_->doubleSpinBox_visual_maxDepth->setValue(settings.value("maxDepth", ui_->doubleSpinBox_visual_maxDepth->value()).toDouble());
	ui_->doubleSpinBox_detectMore_radius->setValue(settings.value("detectMoreRadius", ui_->doubleSpinBox_detectMore_radius->value()).toDouble());
	ui_->doubleSpinBox_detectMore_angle->setValue(settings.value("detectMoreAngle", ui_->doubleSpinBox_detectMore_angle->value()).toDouble());
	ui_->spinBox_detectMore_iterations->setValue(settings.value("detectMoreIterations", ui_->spinBox_detectMore_iterations->value()).toInt());
	settings.endGroup();

	settings.endGroup(); // DatabaseViewer
}

void DatabaseViewer::writeSettings()
{
	QString path = getIniFilePath();
	QSettings settings(path, QSettings::IniFormat);
	settings.beginGroup("DatabaseViewer");

	//save window state / geometry
	if(!this->isMaximized())
	{
		settings.setValue("geometry", this->saveGeometry());
	}
	settings.setValue("state", this->saveState());
	settings.setValue("maximized", this->isMaximized());
	savedMaximized_ = this->isMaximized();

	// save GraphViewer settings
	ui_->graphViewer->saveSettings(settings, "GraphView");

	// save optimization settings
	settings.beginGroup("optimization");
	settings.setValue("iterations", ui_->spinBox_iterations->value());
	settings.setValue("ignoreCovariance", ui_->checkBox_ignoreCovariance->isChecked());
	settings.setValue("ignorePoseCorrection", ui_->checkBox_ignorePoseCorrection->isChecked());
	settings.setValue("strategy", ui_->comboBox_graphOptimizer->currentIndex());
	settings.setValue("slam2d", ui_->checkBox_2dslam->isChecked());
	settings.setValue("depth", ui_->spinBox_optimizationDepth->value());
	settings.setValue("erode", ui_->checkBox_gridErode->isChecked());
	settings.endGroup();

	// save Grid settings
	settings.beginGroup("grid");
	settings.setValue("gridFromProj", ui_->groupBox_gridFromProjection->isChecked());
	settings.setValue("gridCellSize", ui_->doubleSpinBox_gridCellSize->value());
	settings.setValue("projDecimation", ui_->spinBox_projDecimation->value());
	settings.setValue("projMaxDepth", ui_->doubleSpinBox_projMaxDepth->value());
	settings.setValue("poseFiltering", ui_->groupBox_posefiltering->isChecked());
	settings.setValue("poseFilteringRadius", ui_->doubleSpinBox_posefilteringRadius->value());
	settings.setValue("poseFilteringAngle", ui_->doubleSpinBox_posefilteringAngle->value());
	settings.endGroup();

	// ImageViews
	//ui_->graphicsView_A->saveSettings(settings, "ImageViewA");
	//ui_->graphicsView_B->saveSettings(settings, "ImageViewB");

	// save ICP parameters
	settings.beginGroup("icp");
	settings.setValue("decimation", ui_->spinBox_icp_decimation->value());
	settings.setValue("maxDepth", ui_->doubleSpinBox_icp_maxDepth->value());
	settings.setValue("voxel", ui_->doubleSpinBox_icp_voxel->value());
	settings.setValue("maxCorrDist", ui_->doubleSpinBox_icp_maxCorrespDistance->value());
	settings.setValue("iterations", ui_->spinBox_icp_iteration->value());
	settings.setValue("point2place", ui_->checkBox_icp_p2plane->isChecked());
	settings.setValue("normalKSearch", ui_->spinBox_icp_normalKSearch->value());
	settings.setValue("icp2d", ui_->checkBox_icp_2d->isChecked());
	settings.endGroup();

	// save Visual parameters
	settings.beginGroup("visual");
	settings.setValue("reextract", ui_->groupBox_visual_recomputeFeatures->isChecked());
	settings.setValue("featureType", ui_->comboBox_featureType->currentIndex());
	settings.setValue("nnType", ui_->comboBox_nnType->currentIndex());
	settings.setValue("force2d", ui_->checkBox_visual_2d->isChecked());
	settings.setValue("nndr", ui_->doubleSpinBox_visual_nndr->value());
	settings.setValue("minCorr", ui_->spinBox_visual_minCorrespondences->value());
	settings.setValue("maxCorrDist", ui_->doubleSpinBox_visual_maxCorrespDistance->value());
	settings.setValue("iterations", ui_->spinBox_visual_iteration->value());
	settings.setValue("maxDepth", ui_->doubleSpinBox_visual_maxDepth->value());
	settings.setValue("detectMoreRadius", ui_->doubleSpinBox_detectMore_radius->value());
	settings.setValue("detectMoreAngle", ui_->doubleSpinBox_detectMore_angle->value());
	settings.setValue("detectMoreIterations", ui_->spinBox_detectMore_iterations->value());
	settings.endGroup();

	settings.endGroup(); // DatabaseViewer

	this->setWindowModified(false);
}

void DatabaseViewer::openDatabase()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Select file"), pathDatabase_, tr("Databases (*.db)"));
	if(!path.isEmpty())
	{
		openDatabase(path);
	}
}

bool DatabaseViewer::openDatabase(const QString & path)
{
	UDEBUG("Open database \"%s\"", path.toStdString().c_str());
	if(QFile::exists(path))
	{
		if(memory_)
		{
			delete memory_;
			memory_ = 0;
			ids_.clear();
			idToIndex_.clear();
			neighborLinks_.clear();
			loopLinks_.clear();
			graphes_.clear();
			poses_.clear();
			links_.clear();
			linksAdded_.clear();
			linksRefined_.clear();
			linksRemoved_.clear();
			localMaps_.clear();
			ui_->actionGenerate_TORO_graph_graph->setEnabled(false);
			ui_->checkBox_showOptimized->setEnabled(false);
		}

		std::string driverType = "sqlite3";
		rtabmap::ParametersMap parameters;
		parameters.insert(rtabmap::ParametersPair(rtabmap::Parameters::kDbSqlite3InMemory(), "false"));
		parameters.insert(rtabmap::ParametersPair(rtabmap::Parameters::kMemIncrementalMemory(), "false"));
		parameters.insert(rtabmap::ParametersPair(rtabmap::Parameters::kMemInitWMWithAllNodes(), "true"));
		// use BruteForce dictionary because we don't know which type of descriptors are saved in database
		parameters.insert(rtabmap::ParametersPair(rtabmap::Parameters::kKpNNStrategy(), "3"));

		memory_ = new rtabmap::Memory();

		if(!memory_->init(path.toStdString(), false, parameters))
		{
			QMessageBox::warning(this, "Database error", tr("Can't open database \"%1\"").arg(path));
		}
		else
		{
			pathDatabase_ = UDirectory::getDir(path.toStdString()).c_str();
			updateIds();
			return true;
		}
	}
	else
	{
		QMessageBox::warning(this, "Database error", tr("Database \"%1\" does not exist.").arg(path));
	}
	return false;
}

void DatabaseViewer::closeEvent(QCloseEvent* event)
{
	//write settings before quit?
	bool save = false;
	if(this->isWindowModified())
	{
		QMessageBox::Button b=QMessageBox::question(this,
				tr("Database Viewer"),
				tr("There are unsaved changed settings. Save them?"),
				QMessageBox::Save | QMessageBox::Cancel | QMessageBox::Discard);
		if(b == QMessageBox::Save)
		{
			save = true;
		}
		else if(b != QMessageBox::Discard)
		{
			event->ignore();
			return;
		}
	}

	if(save)
	{
		writeSettings();
	}

	if(linksAdded_.size() || linksRefined_.size() || linksRemoved_.size())
	{
		QMessageBox::StandardButton button = QMessageBox::question(this,
				tr("Links modified"),
				tr("Some links are modified (%1 added, %2 refined, %3 removed), do you want to save them?")
				.arg(linksAdded_.size()).arg(linksRefined_.size()).arg(linksRemoved_.size()),
				QMessageBox::Cancel | QMessageBox::Yes | QMessageBox::No,
				QMessageBox::Cancel);

		if(button == QMessageBox::Yes)
		{
			// Added links
			for(std::multimap<int, rtabmap::Link>::iterator iter=linksAdded_.begin(); iter!=linksAdded_.end(); ++iter)
			{
				std::multimap<int, rtabmap::Link>::iterator refinedIter = rtabmap::graph::findLink(linksRefined_, iter->second.from(), iter->second.to());
				if(refinedIter != linksRefined_.end())
				{
					memory_->addLink(
							refinedIter->second.to(),
							refinedIter->second.from(),
							refinedIter->second.transform(),
							refinedIter->second.type(),
							refinedIter->second.rotVariance(),
							refinedIter->second.transVariance());
				}
				else
				{
					memory_->addLink(
							iter->second.to(),
							iter->second.from(),
							iter->second.transform(),
							iter->second.type(),
							iter->second.rotVariance(),
							iter->second.transVariance());
				}
			}

			//Refined links
			for(std::multimap<int, rtabmap::Link>::iterator iter=linksRefined_.begin(); iter!=linksRefined_.end(); ++iter)
			{
				if(!containsLink(linksAdded_, iter->second.from(), iter->second.to()))
				{
					memory_->updateLink(
							iter->second.from(),
							iter->second.to(),
							iter->second.transform(),
							iter->second.rotVariance(),
							iter->second.transVariance());
				}
			}

			// Rejected links
			for(std::multimap<int, rtabmap::Link>::iterator iter=linksRemoved_.begin(); iter!=linksRemoved_.end(); ++iter)
			{
				memory_->removeLink(iter->second.to(), iter->second.from());
			}
		}

		if(button == QMessageBox::Yes || button == QMessageBox::No)
		{
			event->accept();
		}
		else
		{
			event->ignore();
		}
	}
	else
	{
		event->accept();
	}

	if(event->isAccepted())
	{
		if(memory_)
		{
			delete memory_;
			memory_ = 0;
		}
	}
}

void DatabaseViewer::showEvent(QShowEvent* anEvent)
{
	this->setWindowModified(false);
}

void DatabaseViewer::moveEvent(QMoveEvent* anEvent)
{
	if(this->isVisible())
	{
		// HACK, there is a move event when the window is shown the first time.
		if(!firstCall_)
		{
			this->configModified();
		}
		firstCall_ = false;
	}
}

void DatabaseViewer::resizeEvent(QResizeEvent* anEvent)
{
	if(this->isVisible())
	{
		this->configModified();
	}
}

bool DatabaseViewer::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Resize && qobject_cast<QDockWidget*>(obj))
	{
		this->setWindowModified(true);
	}
	return QWidget::eventFilter(obj, event);
}


void DatabaseViewer::exportDatabase()
{
	if(!memory_ || ids_.size() == 0)
	{
		return;
	}

	rtabmap::ExportDialog dialog;

	if(dialog.exec())
	{
		if(!dialog.outputPath().isEmpty())
		{
			int framesIgnored = dialog.framesIgnored();
			int sessionExported = dialog.sessionExported();
			QString path = dialog.outputPath();
			rtabmap::DataRecorder recorder;
			QList<int> ids;
			if(sessionExported < 0)
			{
				ids = ids_;
			}
			else
			{
				for(int i=0; i<ids_.size(); ++i)
				{
					Transform odomPose;
					int weight = -1;
					int mapId = -1;
					std::string label;
					double stamp = 0;
					std::vector<unsigned char> userData;
					if(memory_->getNodeInfo(ids_[i], odomPose, mapId, weight, label, stamp, userData, true))
					{
						if(sessionExported == mapId)
						{
							ids.push_back(ids_[i]);
						}
						else if(mapId > sessionExported)
						{
							break;
						}
					}
				}
			}
			if(recorder.init(path, false))
			{
				rtabmap::DetailedProgressDialog progressDialog(this);
				progressDialog.setMaximumSteps(ids.size() / (1+framesIgnored) + 1);
				progressDialog.show();

				for(int i=0; i<ids.size(); i+=1+framesIgnored)
				{
					int id = ids.at(i);

					Signature data = memory_->getSignatureData(id, true);
					rtabmap::SensorData sensorData = data.toSensorData();
					if(!dialog.isUserDataExported())
					{
						sensorData.setUserData(std::vector<unsigned char>());
					}
					recorder.addData(sensorData);

					progressDialog.appendText(tr("Exported node %1").arg(id));
					progressDialog.incrementStep();
					QApplication::processEvents();
				}
				progressDialog.setValue(progressDialog.maximumSteps());
				progressDialog.appendText("Export finished!");
			}
			else
			{
				UERROR("DataRecorder init failed?!");
			}
		}
		else
		{
			QMessageBox::warning(this, tr("Cannot export database"), tr("An output path must be set!"));
		}
	}
}

void DatabaseViewer::extractImages()
{
	if(!memory_ || ids_.size() == 0)
	{
		return;
	}

	QString path = QFileDialog::getExistingDirectory(this, tr("Select directory where to save images..."), QDir::homePath());
	if(!path.isNull())
	{
		for(int i=0; i<ids_.size(); i+=1)
		{
			int id = ids_.at(i);
			cv::Mat compressedRgb = memory_->getImageCompressed(id);
			if(!compressedRgb.empty())
			{
				cv::Mat imageMat = rtabmap::uncompressImage(compressedRgb);
				cv::imwrite(QString("%1/%2.png").arg(path).arg(id).toStdString(), imageMat);
				UINFO(QString("Saved %1/%2.png").arg(path).arg(id).toStdString().c_str());
			}
		}
	}
}

void DatabaseViewer::updateIds()
{
	if(!memory_)
	{
		return;
	}

	std::set<int> ids = memory_->getAllSignatureIds();
	ids_ = QList<int>::fromStdList(std::list<int>(ids.begin(), ids.end()));
	idToIndex_.clear();
	for(int i=0; i<ids_.size(); ++i)
	{
		idToIndex_.insert(ids_[i], i);
	}

	poses_.clear();
	links_.clear();
	linksAdded_.clear();
	linksRefined_.clear();
	linksRemoved_.clear();
	ui_->label_optimizeFrom->setText(tr("Optimize from"));
	if(memory_->getLastWorkingSignature())
	{
		//get constraints only for parent links

		memory_->getMetricConstraints(std::vector<int>(ids.begin(), ids.end()), poses_, links_, true);

		if(poses_.size())
		{
			bool nullPoses = poses_.begin()->second.isNull();
			for(std::map<int,Transform>::iterator iter=poses_.begin(); iter!=poses_.end(); ++iter)
			{
				if((!iter->second.isNull() && nullPoses) ||
					(iter->second.isNull() && !nullPoses))
				{
					if(iter->second.isNull())
					{
						UWARN("Pose %d is null!", iter->first);
					}
					UWARN("Mixed valid and null poses! Ignoring graph...");
					poses_.clear();
					links_.clear();
					break;
				}
			}
			if(nullPoses)
			{
				poses_.clear();
				links_.clear();
			}

			int first = *ids.begin();
			ui_->spinBox_optimizationsFrom->setRange(first, memory_->getLastWorkingSignature()->id());
			ui_->spinBox_optimizationsFrom->setValue(memory_->getLastWorkingSignature()->id());
			ui_->label_optimizeFrom->setText(tr("Optimize from [%1, %2]").arg(first).arg(memory_->getLastWorkingSignature()->id()));
		}
	}

	ui_->actionGenerate_TORO_graph_graph->setEnabled(false);
	graphes_.clear();
	neighborLinks_.clear();
	loopLinks_.clear();
	for(std::multimap<int, rtabmap::Link>::iterator iter = links_.begin(); iter!=links_.end(); ++iter)
	{
		if(!iter->second.transform().isNull())
		{
			if(iter->second.type() == rtabmap::Link::kNeighbor)
			{
				neighborLinks_.append(iter->second);
			}
			else
			{
				loopLinks_.append(iter->second);
			}
		}
		else
		{
			UERROR("Transform null for link from %d to %d", iter->first, iter->second.to());
		}
	}

	UINFO("Loaded %d ids", ids_.size());

	if(ids_.size())
	{
		ui_->horizontalSlider_A->setMinimum(0);
		ui_->horizontalSlider_B->setMinimum(0);
		ui_->horizontalSlider_A->setMaximum(ids_.size()-1);
		ui_->horizontalSlider_B->setMaximum(ids_.size()-1);
		ui_->horizontalSlider_A->setEnabled(true);
		ui_->horizontalSlider_B->setEnabled(true);
		ui_->horizontalSlider_A->setSliderPosition(0);
		ui_->horizontalSlider_B->setSliderPosition(0);
		sliderAValueChanged(0);
		sliderBValueChanged(0);
	}
	else
	{
		ui_->horizontalSlider_A->setEnabled(false);
		ui_->horizontalSlider_B->setEnabled(false);
		ui_->label_idA->setText("NaN");
		ui_->label_idB->setText("NaN");
	}

	if(neighborLinks_.size())
	{
		ui_->horizontalSlider_neighbors->setMinimum(0);
		ui_->horizontalSlider_neighbors->setMaximum(neighborLinks_.size()-1);
		ui_->horizontalSlider_neighbors->setEnabled(true);
		ui_->horizontalSlider_neighbors->setSliderPosition(0);
	}
	else
	{
		ui_->horizontalSlider_neighbors->setEnabled(false);
	}

	if(ids_.size())
	{
		updateLoopClosuresSlider();
		updateGraphView();
	}
}

void DatabaseViewer::generateGraph()
{
	if(!memory_)
	{
		QMessageBox::warning(this, tr("Cannot generate a graph"), tr("A database must must loaded first...\nUse File->Open database."));
		return;
	}

	QString path = QFileDialog::getSaveFileName(this, tr("Save File"), pathDatabase_+"/Graph.dot", tr("Graphiz file (*.dot)"));
	if(!path.isEmpty())
	{
		memory_->generateGraph(path.toStdString());
	}
}

void DatabaseViewer::generateLocalGraph()
{
	if(!ids_.size() || !memory_)
	{
		QMessageBox::warning(this, tr("Cannot generate a graph"), tr("The database is empty..."));
		return;
	}
	bool ok = false;
	int id = QInputDialog::getInt(this, tr("Around which location?"), tr("Location ID"), ids_.first(), ids_.first(), ids_.last(), 1, &ok);

	if(ok)
	{
		int margin = QInputDialog::getInt(this, tr("Depth around the location?"), tr("Margin"), 4, 1, 100, 1, &ok);
		if(ok)
		{
			QString path = QFileDialog::getSaveFileName(this, tr("Save File"), pathDatabase_+"/Graph" + QString::number(id) + ".dot", tr("Graphiz file (*.dot)"));
			if(!path.isEmpty())
			{
				std::map<int, int> ids = memory_->getNeighborsId(id, margin, -1, false);

				if(ids.size() > 0)
				{
					ids.insert(std::pair<int,int>(id, 0));
					std::set<int> idsSet;
					for(std::map<int, int>::iterator iter = ids.begin(); iter!=ids.end(); ++iter)
					{
						idsSet.insert(idsSet.end(), iter->first);
						UINFO("Node %d", iter->first);
					}
					UINFO("idsSet=%d", idsSet.size());
					memory_->generateGraph(path.toStdString(), idsSet);
				}
				else
				{
					QMessageBox::critical(this, tr("Error"), tr("No neighbors found for signature %1.").arg(id));
				}
			}
		}
	}
}

void DatabaseViewer::generateTOROGraph()
{
	std::multimap<int, Link> links = updateLinksWithModifications(links_);
	if(!graphes_.size() || !links.size())
	{
		QMessageBox::warning(this, tr("Cannot generate a TORO graph"), tr("No poses or no links..."));
		return;
	}
	bool ok = false;
	int id = QInputDialog::getInt(this, tr("Which iteration?"), tr("Iteration (0 -> %1)").arg((int)graphes_.size()-1), (int)graphes_.size()-1, 0, (int)graphes_.size()-1, 1, &ok);

	if(ok)
	{
		QString path = QFileDialog::getSaveFileName(this, tr("Save File"), pathDatabase_+"/constraints" + QString::number(id) + ".graph", tr("TORO file (*.graph)"));
		if(!path.isEmpty())
		{
			graph::TOROOptimizer::saveGraph(path.toStdString(), uValueAt(graphes_, id), links);
		}
	}
}

void DatabaseViewer::view3DMap()
{
	if(!ids_.size() || !memory_)
	{
		QMessageBox::warning(this, tr("Cannot view 3D map"), tr("The database is empty..."));
		return;
	}
	if(graphes_.empty())
	{
		this->updateGraphView();
		if(graphes_.empty() || ui_->horizontalSlider_iterations->maximum() != (int)graphes_.size()-1)
		{
			QMessageBox::warning(this, tr("Cannot generate a graph"), tr("No graph in database?!"));
			return;
		}
	}
	bool ok = false;
	QStringList items;
	items.append("1");
	items.append("2");
	items.append("4");
	items.append("8");
	items.append("16");
	QString item = QInputDialog::getItem(this, tr("Decimation?"), tr("Image decimation"), items, 2, false, &ok);
	if(ok)
	{
		int decimation = item.toInt();
		double maxDepth = QInputDialog::getDouble(this, tr("Camera depth?"), tr("Maximum depth (m, 0=no max):"), 4.0, 0, 10, 2, &ok);
		if(ok)
		{
			std::map<int, Transform> optimizedPoses = uValueAt(graphes_, ui_->horizontalSlider_iterations->value());
			if(ui_->groupBox_posefiltering->isChecked())
			{
				optimizedPoses = graph::radiusPosesFiltering(optimizedPoses,
						ui_->doubleSpinBox_posefilteringRadius->value(),
						ui_->doubleSpinBox_posefilteringAngle->value()*CV_PI/180.0);
			}
			if(optimizedPoses.size() > 0)
			{
				rtabmap::DetailedProgressDialog progressDialog(this);
				progressDialog.setMaximumSteps((int)optimizedPoses.size());
				progressDialog.show();

				// create a window
				QDialog * window = new QDialog(this, Qt::Window);
				window->setModal(this->isModal());
				window->setWindowTitle(tr("3D Map"));
				window->setMinimumWidth(800);
				window->setMinimumHeight(600);

				rtabmap::CloudViewer * viewer = new rtabmap::CloudViewer(window);

				QVBoxLayout *layout = new QVBoxLayout();
				layout->addWidget(viewer);
				viewer->setCameraLockZ(false);
				window->setLayout(layout);
				connect(window, SIGNAL(finished(int)), viewer, SLOT(clear()));

				window->show();

				for(std::map<int, Transform>::const_iterator iter = optimizedPoses.begin(); iter!=optimizedPoses.end(); ++iter)
				{
					rtabmap::Transform pose = iter->second;
					if(!pose.isNull())
					{
						Signature data = memory_->getSignatureData(iter->first, true);
						pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;
						UASSERT(data.getImageRaw().empty() || data.getImageRaw().type()==CV_8UC3 || data.getImageRaw().type() == CV_8UC1);
						UASSERT(data.getDepthRaw().empty() || data.getDepthRaw().type()==CV_8UC1 || data.getDepthRaw().type() == CV_16UC1 || data.getDepthRaw().type() == CV_32FC1);
						if(data.getDepthRaw().type() == CV_8UC1)
						{
							cv::Mat leftImg;
							if(data.getImageRaw().channels() == 3)
							{
								cv::cvtColor(data.getImageRaw(), leftImg, CV_BGR2GRAY);
							}
							else
							{
								leftImg = data.getImageRaw();
							}
							cloud = rtabmap::util3d::cloudFromDisparityRGB(
									data.getImageRaw(),
								util3d::disparityFromStereoImages(leftImg, data.getDepthRaw()),
								data.getCx(), data.getCy(),
								data.getFx(), data.getFy(),
								decimation);
						}
						else
						{
							cloud = rtabmap::util3d::cloudFromDepthRGB(
									data.getImageRaw(),
									data.getDepthRaw(),
									data.getCx(), data.getCy(),
									data.getFx(), data.getFy(),
									decimation);
						}

						if(maxDepth)
						{
							cloud = rtabmap::util3d::passThrough<pcl::PointXYZRGB>(cloud, "z", 0, maxDepth);
						}

						cloud = rtabmap::util3d::transformPointCloud<pcl::PointXYZRGB>(cloud, data.getLocalTransform());

						QColor color = Qt::red;
						int mapId, weight;
						Transform odomPose;
						std::string label;
						double stamp;
						std::vector<unsigned char> userData;
						if(memory_->getNodeInfo(iter->first, odomPose, mapId, weight, label, stamp, userData, true))
						{
							color = (Qt::GlobalColor)(mapId % 12 + 7 );
						}

						viewer->addCloud(uFormat("cloud%d", iter->first), cloud, pose, color);

						UINFO("Generated %d (%d points)", iter->first, cloud->size());
						progressDialog.appendText(QString("Generated %1 (%2 points)").arg(iter->first).arg(cloud->size()));
						progressDialog.incrementStep();
						QApplication::processEvents();
					}
				}
				progressDialog.setValue(progressDialog.maximumSteps());
			}
			else
			{
				QMessageBox::critical(this, tr("Error"), tr("No neighbors found for node %1.").arg(ui_->spinBox_optimizationsFrom->value()));
			}
		}
	}
}

void DatabaseViewer::generate3DMap()
{
	if(!ids_.size() || !memory_)
	{
		QMessageBox::warning(this, tr("Cannot generate a graph"), tr("The database is empty..."));
		return;
	}
	bool ok = false;
	QStringList items;
	items.append("1");
	items.append("2");
	items.append("4");
	items.append("8");
	items.append("16");
	QString item = QInputDialog::getItem(this, tr("Decimation?"), tr("Image decimation"), items, 2, false, &ok);
	if(ok)
	{
		int decimation = item.toInt();
		double maxDepth = QInputDialog::getDouble(this, tr("Camera depth?"), tr("Maximum depth (m, 0=no max):"), 4.0, 0, 10, 2, &ok);
		if(ok)
		{
			QString path = QFileDialog::getExistingDirectory(this, tr("Save directory"), pathDatabase_);
			if(!path.isEmpty())
			{
				std::map<int, Transform> optimizedPoses = uValueAt(graphes_, ui_->horizontalSlider_iterations->value());
				if(ui_->groupBox_posefiltering->isChecked())
				{
					optimizedPoses = graph::radiusPosesFiltering(optimizedPoses,
							ui_->doubleSpinBox_posefilteringRadius->value(),
							ui_->doubleSpinBox_posefilteringAngle->value()*CV_PI/180.0);
				}
				if(optimizedPoses.size() > 0)
				{
					rtabmap::DetailedProgressDialog progressDialog;
					progressDialog.setMaximumSteps((int)optimizedPoses.size());
					progressDialog.show();

					for(std::map<int, Transform>::const_iterator iter = optimizedPoses.begin(); iter!=optimizedPoses.end(); ++iter)
					{
						const rtabmap::Transform & pose = iter->second;
						if(!pose.isNull())
						{
							Signature data = memory_->getSignatureData(iter->first, true);
							pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;
							UASSERT(data.getImageRaw().empty() || data.getImageRaw().type()==CV_8UC3 || data.getImageRaw().type() == CV_8UC1);
							UASSERT(data.getDepthRaw().empty() || data.getDepthRaw().type()==CV_8UC1 || data.getDepthRaw().type() == CV_16UC1 || data.getDepthRaw().type() == CV_32FC1);
							if(data.getDepthRaw().type() == CV_8UC1)
							{
								cv::Mat leftImg;
								if(data.getImageRaw().channels() == 3)
								{
									cv::cvtColor(data.getImageRaw(), leftImg, CV_BGR2GRAY);
								}
								else
								{
									leftImg = data.getImageRaw();
								}
								cloud = rtabmap::util3d::cloudFromDisparityRGB(
										data.getImageRaw(),
									util3d::disparityFromStereoImages(leftImg, data.getDepthRaw()),
									data.getCx(), data.getCy(),
									data.getFx(), data.getFy(),
									decimation);
							}
							else
							{
								cloud = rtabmap::util3d::cloudFromDepthRGB(
										data.getImageRaw(),
										data.getDepthRaw(),
										data.getCx(), data.getCy(),
										data.getFx(), data.getFy(),
										decimation);
							}

							if(maxDepth)
							{
								cloud = rtabmap::util3d::passThrough<pcl::PointXYZRGB>(cloud, "z", 0, maxDepth);
							}

							cloud = rtabmap::util3d::transformPointCloud<pcl::PointXYZRGB>(cloud, pose*data.getLocalTransform());
							std::string name = uFormat("%s/node%d.pcd", path.toStdString().c_str(), iter->first);
							pcl::io::savePCDFile(name, *cloud);
							UINFO("Saved %s (%d points)", name.c_str(), cloud->size());
							progressDialog.appendText(QString("Saved %1 (%2 points)").arg(name.c_str()).arg(cloud->size()));
							progressDialog.incrementStep();
							QApplication::processEvents();
						}
					}
					progressDialog.setValue(progressDialog.maximumSteps());

					QMessageBox::information(this, tr("Finished"), tr("%1 clouds generated to %2.").arg(optimizedPoses.size()).arg(path));
				}
				else
				{
					QMessageBox::critical(this, tr("Error"), tr("No neighbors found for node %1.").arg(ui_->spinBox_optimizationsFrom->value()));
				}
			}
		}
	}
}

void DatabaseViewer::detectMoreLoopClosures()
{
	const std::map<int, Transform> & optimizedPoses = graphes_.back();

	int iterations = ui_->spinBox_detectMore_iterations->value();
	UASSERT(iterations > 0);
	int added = 0;
	for(int n=0; n<iterations; ++n)
	{
		UINFO("iteration %d/%d", n+1, iterations);
		std::multimap<int, int> clusters = rtabmap::graph::radiusPosesClustering(
				optimizedPoses,
				ui_->doubleSpinBox_detectMore_radius->value(),
				ui_->doubleSpinBox_detectMore_angle->value()*CV_PI/180.0);
		std::set<int> addedLinks;
		for(std::multimap<int, int>::iterator iter=clusters.begin(); iter!= clusters.end(); ++iter)
		{
			int from = iter->first;
			int to = iter->second;
			if(from < to)
			{
				from = iter->second;
				to = iter->first;
			}
			if(!findActiveLink(from, to).isValid() && !containsLink(linksRemoved_, from, to) &&
				addedLinks.find(from) == addedLinks.end() && addedLinks.find(to) == addedLinks.end())
			{
				if(addConstraint(from, to, true, false))
				{
					UINFO("Added new loop closure between %d and %d.", from, to);
					++added;
					addedLinks.insert(from);
					addedLinks.insert(to);
				}
			}
		}
		UINFO("Iteration %d/%d: added %d loop closures.", n+1, iterations, (int)addedLinks.size()/2);
		if(addedLinks.size() == 0)
		{
			break;
		}
	}
	if(added)
	{
		this->updateGraphView();
	}
	UINFO("Total added %d loop closures.", added);
}

void DatabaseViewer::refineAllNeighborLinks()
{
	if(neighborLinks_.size())
	{
		rtabmap::DetailedProgressDialog progressDialog(this);
		progressDialog.setMaximumSteps(neighborLinks_.size());
		progressDialog.show();

		for(int i=0; i<neighborLinks_.size(); ++i)
		{
			int from = neighborLinks_[i].from();
			int to = neighborLinks_[i].to();
			this->refineConstraint(neighborLinks_[i].from(), neighborLinks_[i].to(), false);

			progressDialog.appendText(tr("Refined link %1->%2 (%3/%4)").arg(from).arg(to).arg(i+1).arg(neighborLinks_.size()));
			progressDialog.incrementStep();
			QApplication::processEvents();
		}
		this->updateGraphView();

		progressDialog.setValue(progressDialog.maximumSteps());
		progressDialog.appendText("Refining links finished!");
	}
}

void DatabaseViewer::refineAllLoopClosureLinks()
{
	if(loopLinks_.size())
	{
		rtabmap::DetailedProgressDialog progressDialog(this);
		progressDialog.setMaximumSteps(loopLinks_.size());
		progressDialog.show();

		for(int i=0; i<loopLinks_.size(); ++i)
		{
			int from = loopLinks_[i].from();
			int to = loopLinks_[i].to();
			this->refineConstraint(loopLinks_[i].from(), loopLinks_[i].to(), false);

			progressDialog.appendText(tr("Refined link %1->%2 (%3/%4)").arg(from).arg(to).arg(i+1).arg(loopLinks_.size()));
			progressDialog.incrementStep();
			QApplication::processEvents();
		}
		this->updateGraphView();

		progressDialog.setValue(progressDialog.maximumSteps());
		progressDialog.appendText("Refining links finished!");
	}
}

void DatabaseViewer::refineVisuallyAllNeighborLinks()
{
	if(neighborLinks_.size())
	{
		rtabmap::DetailedProgressDialog progressDialog(this);
		progressDialog.setMaximumSteps(neighborLinks_.size());
		progressDialog.show();

		for(int i=0; i<neighborLinks_.size(); ++i)
		{
			int from = neighborLinks_[i].from();
			int to = neighborLinks_[i].to();
			this->refineConstraintVisually(neighborLinks_[i].from(), neighborLinks_[i].to(), false);

			progressDialog.appendText(tr("Refined link %1->%2 (%3/%4)").arg(from).arg(to).arg(i+1).arg(neighborLinks_.size()));
			progressDialog.incrementStep();
			QApplication::processEvents();
		}
		this->updateGraphView();

		progressDialog.setValue(progressDialog.maximumSteps());
		progressDialog.appendText("Refining links finished!");
	}
}

void DatabaseViewer::refineVisuallyAllLoopClosureLinks()
{
	if(loopLinks_.size())
	{
		rtabmap::DetailedProgressDialog progressDialog(this);
		progressDialog.setMaximumSteps(loopLinks_.size());
		progressDialog.show();

		for(int i=0; i<loopLinks_.size(); ++i)
		{
			int from = loopLinks_[i].from();
			int to = loopLinks_[i].to();
			this->refineConstraintVisually(loopLinks_[i].from(), loopLinks_[i].to(), false);

			progressDialog.appendText(tr("Refined link %1->%2 (%3/%4)").arg(from).arg(to).arg(i+1).arg(loopLinks_.size()));
			progressDialog.incrementStep();
			QApplication::processEvents();
		}
		this->updateGraphView();

		progressDialog.setValue(progressDialog.maximumSteps());
		progressDialog.appendText("Refining links finished!");
	}
}

void DatabaseViewer::sliderAValueChanged(int value)
{
	this->update(value,
			ui_->label_indexA,
			ui_->label_parentsA,
			ui_->label_childrenA,
			ui_->label_weightA,
			ui_->label_labelA,
			ui_->label_stampA,
			ui_->graphicsView_A,
			ui_->widget_cloudA,
			ui_->label_idA);
}

void DatabaseViewer::sliderBValueChanged(int value)
{
	this->update(value,
			ui_->label_indexB,
			ui_->label_parentsB,
			ui_->label_childrenB,
			ui_->label_weightB,
			ui_->label_labelB,
			ui_->label_stampB,
			ui_->graphicsView_B,
			ui_->widget_cloudB,
			ui_->label_idB);
}

void DatabaseViewer::update(int value,
						QLabel * labelIndex,
						QLabel * labelParents,
						QLabel * labelChildren,
						QLabel * weight,
						QLabel * label,
						QLabel * stamp,
						rtabmap::ImageView * view,
						rtabmap::CloudViewer * view3D,
						QLabel * labelId,
						bool updateConstraintView)
{
	UTimer timer;
	labelIndex->setText(QString::number(value));
	labelParents->clear();
	labelChildren->clear();
	weight->clear();
	label->clear();
	stamp->clear();
	QRectF rect;
	if(value >= 0 && value < ids_.size())
	{
		view->clear();
		int id = ids_.at(value);
		int mapId = -1;
		labelId->setText(QString::number(id));
		if(id>0)
		{
			//image
			QImage img;
			QImage imgDepth;
			if(memory_)
			{
				Signature data = memory_->getSignatureData(id, true);
				if(!data.getImageRaw().empty())
				{
					img = uCvMat2QImage(data.getImageRaw());
				}
				if(!data.getDepthRaw().empty())
				{
					imgDepth = uCvMat2QImage(data.getDepthRaw());
				}

				if(data.getWords().size())
				{
					view->setFeatures(data.getWords());
				}

				Transform odomPose;
				int w;
				std::string l;
				double s;
				std::vector<unsigned char> d;
				memory_->getNodeInfo(id, odomPose, mapId, w, l, s, d, true);

				weight->setNum(data.getWeight());
				label->setText(data.getLabel().c_str());
				if(data.getStamp()!=0.0)
				{
					stamp->setText(QDateTime::fromMSecsSinceEpoch(data.getStamp()*1000.0).toString("dd.MM.yyyy hh:mm:ss.zzz"));
				}

				//stereo
				if(!data.getDepthRaw().empty() && data.getDepthRaw().type() == CV_8UC1)
				{
					this->updateStereo(&data);
				}

				// 3d view
				if(view3D->isVisible() && !data.getDepthRaw().empty())
				{
					pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;
					if(data.getDepthRaw().type() == CV_8UC1)
					{
						cloud = util3d::cloudFromStereoImages(
								data.getImageRaw(),
								data.getDepthRaw(),
								data.getCx(), data.getCy(),
								data.getFx(), data.getFy(),
								1);
					}
					else
					{
						cloud = util3d::cloudFromDepthRGB(
								data.getImageRaw(),
								data.getDepthRaw(),
								data.getCx(), data.getCy(),
								data.getFx(), data.getFy(),
								1);
					}
					view3D->addOrUpdateCloud("0", cloud, data.getLocalTransform());
					view3D->update();
				}
			}

			if(!imgDepth.isNull())
			{
				view->setImageDepth(imgDepth);
				rect = imgDepth.rect();
			}
			else
			{
				ULOGGER_DEBUG("Image depth is empty");
			}
			if(!img.isNull())
			{
				view->setImage(img);
				rect = img.rect();
			}
			else
			{
				ULOGGER_DEBUG("Image is empty");
			}

			// loops
			std::map<int, rtabmap::Link> loopClosures;
			loopClosures = memory_->getLoopClosureLinks(id, true);
			if(loopClosures.size())
			{
				QString strParents, strChildren;
				for(std::map<int, rtabmap::Link>::iterator iter=loopClosures.begin(); iter!=loopClosures.end(); ++iter)
				{
					if(iter->first < id)
					{
						strChildren.append(QString("%1 ").arg(iter->first));
					}
					else
					{
						strParents.append(QString("%1 ").arg(iter->first));
					}
				}
				labelParents->setText(strParents);
				labelChildren->setText(strChildren);
			}
		}

		if(mapId>=0)
		{
			labelId->setText(QString("%1 [%2]").arg(id).arg(mapId));
		}
		else
		{
			labelId->setText(QString::number(id));
		}
	}
	else
	{
		ULOGGER_ERROR("Slider index out of range ?");
	}

	updateConstraintButtons();
	updateWordsMatching();

	if(updateConstraintView)
	{
		// update constraint view
		int from = ids_.at(ui_->horizontalSlider_A->value());
		int to = ids_.at(ui_->horizontalSlider_B->value());
		bool set = false;
		for(int i=0; i<loopLinks_.size() || i<neighborLinks_.size(); ++i)
		{
			if(i < loopLinks_.size())
			{
				if((loopLinks_[i].from() == from && loopLinks_[i].to() == to) ||
				   (loopLinks_[i].from() == to && loopLinks_[i].to() == from))
				{
					if(i != ui_->horizontalSlider_loops->value())
					{
						ui_->horizontalSlider_loops->blockSignals(true);
						ui_->horizontalSlider_loops->setValue(i);
						ui_->horizontalSlider_loops->blockSignals(false);
						this->updateConstraintView(loopLinks_.at(i), false);
					}
					ui_->horizontalSlider_neighbors->blockSignals(true);
					ui_->horizontalSlider_neighbors->setValue(0);
					ui_->horizontalSlider_neighbors->blockSignals(false);
					set = true;
					break;
				}
			}
			if(i < neighborLinks_.size())
			{
				if((neighborLinks_[i].from() == from && neighborLinks_[i].to() == to) ||
				   (neighborLinks_[i].from() == to && neighborLinks_[i].to() == from))
				{
					if(i != ui_->horizontalSlider_neighbors->value())
					{
						ui_->horizontalSlider_neighbors->blockSignals(true);
						ui_->horizontalSlider_neighbors->setValue(i);
						ui_->horizontalSlider_neighbors->blockSignals(false);
						this->updateConstraintView(neighborLinks_.at(i), false);
					}
					ui_->horizontalSlider_loops->blockSignals(true);
					ui_->horizontalSlider_loops->setValue(0);
					ui_->horizontalSlider_loops->blockSignals(false);
					set = true;
					break;
				}
			}
		}
		if(!set)
		{
			ui_->horizontalSlider_loops->blockSignals(true);
			ui_->horizontalSlider_neighbors->blockSignals(true);
			ui_->horizontalSlider_loops->setValue(0);
			ui_->horizontalSlider_neighbors->setValue(0);
			ui_->constraintsViewer->removeAllClouds();
			ui_->constraintsViewer->update();
			ui_->horizontalSlider_loops->blockSignals(false);
			ui_->horizontalSlider_neighbors->blockSignals(false);
		}
	}

	if(rect.isValid())
	{
		view->setSceneRect(rect);
	}
}

void DatabaseViewer::updateStereo(const Signature * data)
{
	if(data && ui_->dockWidget_stereoView->isVisible() && !data->getImageRaw().empty() && !data->getDepthRaw().empty() && data->getDepthRaw().type() == CV_8UC1)
	{
		cv::Mat leftMono;
		if(data->getImageRaw().channels() == 3)
		{
			cv::cvtColor(data->getImageRaw(), leftMono, CV_BGR2GRAY);
		}
		else
		{
			leftMono = data->getImageRaw();
		}

		UTimer timer;

		// generate kpts
		std::vector<cv::KeyPoint> kpts;
		cv::Rect roi = Feature2D::computeRoi(leftMono, "0.03 0.03 0.04 0.04");
		ParametersMap parameters;
		parameters.insert(ParametersPair(Parameters::kKpWordsPerImage(), "1000"));
		parameters.insert(ParametersPair(Parameters::kGFTTMinDistance(), "5"));
		Feature2D::Type type = Feature2D::kFeatureGfttBrief;
		Feature2D * kptDetector = Feature2D::create(type, parameters);
		kpts = kptDetector->generateKeypoints(leftMono, roi);
		delete kptDetector;

		float timeKpt = timer.ticks();

		std::vector<cv::Point2f> leftCorners;
		cv::KeyPoint::convert(kpts, leftCorners);

		// Find features in the new left image
		std::vector<unsigned char> status;
		std::vector<float> err;
		std::vector<cv::Point2f> rightCorners;
		cv::calcOpticalFlowPyrLK(
				leftMono,
				data->getDepthRaw(),
				leftCorners,
				rightCorners,
				status,
				err,
				cv::Size(Parameters::defaultStereoWinSize(), Parameters::defaultStereoWinSize()), Parameters::defaultStereoMaxLevel(),
				cv::TermCriteria(cv::TermCriteria::COUNT+cv::TermCriteria::EPS, Parameters::defaultStereoIterations(), Parameters::defaultStereoEps()));

		float timeFlow = timer.ticks();

		pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
		cloud->resize(kpts.size());
		float bad_point = std::numeric_limits<float>::quiet_NaN ();
		UASSERT(status.size() == kpts.size());
		int oi = 0;
		for(unsigned int i=0; i<status.size(); ++i)
		{
			pcl::PointXYZ pt(bad_point, bad_point, bad_point);
			if(status[i])
			{
				float disparity = leftCorners[i].x - rightCorners[i].x;
				if(disparity > 0.0f)
				{
					if(fabs((leftCorners[i].y-rightCorners[i].y) / (leftCorners[i].x-rightCorners[i].x)) < Parameters::defaultStereoMaxSlope())
					{
						pcl::PointXYZ tmpPt = util3d::projectDisparityTo3D(
								leftCorners[i],
								disparity,
								data->getCx(), data->getCy(), data->getFx(), data->getFy());

						if(pcl::isFinite(tmpPt))
						{
							pt = pcl::transformPoint(tmpPt, data->getLocalTransform().toEigen3f());
							if(fabs(pt.x) > 2 || fabs(pt.y) > 2 || fabs(pt.z) > 2)
							{
								status[i] = 100; //blue
							}
							cloud->at(oi++) = pt;
						}
					}
					else
					{
						status[i] = 101; //yellow
					}
				}
				else
				{
					status[i] = 102; //magenta
				}
			}
		}
		cloud->resize(oi);

		UINFO("correspondences = %d/%d (%f) (time kpt=%fs flow=%fs)",
				(int)cloud->size(), (int)leftCorners.size(), float(cloud->size())/float(leftCorners.size()), timeKpt, timeFlow);

		ui_->stereoViewer->updateCameraTargetPosition(Transform::getIdentity());
		ui_->stereoViewer->addOrUpdateCloud("stereo", cloud);
		ui_->stereoViewer->update();

		std::vector<cv::KeyPoint> rightKpts;
		cv::KeyPoint::convert(rightCorners, rightKpts);
		std::vector<cv::DMatch> good_matches(kpts.size());
		for(unsigned int i=0; i<good_matches.size(); ++i)
		{
			good_matches[i].trainIdx = i;
			good_matches[i].queryIdx = i;
		}


		//
		//cv::Mat imageMatches;
		//cv::drawMatches( leftMono, kpts, data->getDepthRaw(), rightKpts,
		//			   good_matches, imageMatches, cv::Scalar::all(-1), cv::Scalar::all(-1),
		//			   std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );

		//ui_->graphicsView_stereo->setImage(uCvMat2QImage(imageMatches));

		ui_->graphicsView_stereo->clear();
		ui_->graphicsView_stereo->setLinesShown(true);
		ui_->graphicsView_stereo->setFeaturesShown(false);
		ui_->graphicsView_stereo->setImageDepthShown(true);

		ui_->graphicsView_stereo->setImage(uCvMat2QImage(data->getImageRaw()));
		ui_->graphicsView_stereo->setImageDepth(uCvMat2QImage(data->getDepthRaw()));

		// Draw lines between corresponding features...
		for(unsigned int i=0; i<kpts.size(); ++i)
		{
			QColor c = Qt::green;
			if(status[i] == 0)
			{
				c = Qt::red;
			}
			else if(status[i] == 100)
			{
				c = Qt::blue;
			}
			else if(status[i] == 101)
			{
				c = Qt::yellow;
			}
			else if(status[i] == 102)
			{
				c = Qt::magenta;
			}
			ui_->graphicsView_stereo->addLine(
					kpts[i].pt.x,
					kpts[i].pt.y,
					rightKpts[i].pt.x,
					rightKpts[i].pt.y,
					c);
		}
		ui_->graphicsView_stereo->update();
	}
}

void DatabaseViewer::updateWordsMatching()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	if(from && to)
	{
		int alpha = 70;
		ui_->graphicsView_A->clearLines();
		ui_->graphicsView_A->setFeaturesColor(QColor(255, 255, 0, alpha)); // yellow
		ui_->graphicsView_B->clearLines();
		ui_->graphicsView_B->setFeaturesColor(QColor(255, 255, 0, alpha)); // yellow

		const QMultiMap<int, KeypointItem*> & wordsA = ui_->graphicsView_A->getFeatures();
		const QMultiMap<int, KeypointItem*> & wordsB = ui_->graphicsView_B->getFeatures();
		if(wordsA.size() && wordsB.size())
		{
			QList<int> ids =  wordsA.uniqueKeys();
			for(int i=0; i<ids.size(); ++i)
			{
				if(wordsA.count(ids[i]) == 1 && wordsB.count(ids[i]) == 1)
				{
					// PINK features
					ui_->graphicsView_A->setFeatureColor(ids[i], Qt::magenta);
					ui_->graphicsView_B->setFeatureColor(ids[i], Qt::magenta);

					// Add lines
					// Draw lines between corresponding features...
					float scaleX = ui_->graphicsView_A->viewScale();
					float deltaX = ui_->graphicsView_A->width()/scaleX;
					float deltaY = 0;

					const KeypointItem * kptA = wordsA.value(ids[i]);
					const KeypointItem * kptB = wordsB.value(ids[i]);
					ui_->graphicsView_A->addLine(
							kptA->rect().x()+kptA->rect().width()/2,
							kptA->rect().y()+kptA->rect().height()/2,
							kptB->rect().x()+kptB->rect().width()/2+deltaX,
							kptB->rect().y()+kptB->rect().height()/2+deltaY,
							Qt::cyan);

					ui_->graphicsView_B->addLine(
							kptA->rect().x()+kptA->rect().width()/2-deltaX,
							kptA->rect().y()+kptA->rect().height()/2-deltaY,
							kptB->rect().x()+kptB->rect().width()/2,
							kptB->rect().y()+kptB->rect().height()/2,
							Qt::cyan);
				}
			}
			ui_->graphicsView_A->update();
			ui_->graphicsView_B->update();
		}
	}
}

void DatabaseViewer::sliderAMoved(int value)
{
	ui_->label_indexA->setText(QString::number(value));
	if(value>=0 && value < ids_.size())
	{
		ui_->label_idA->setText(QString::number(ids_.at(value)));
	}
	else
	{
		ULOGGER_ERROR("Slider index out of range ?");
	}
}

void DatabaseViewer::sliderBMoved(int value)
{
	ui_->label_indexB->setText(QString::number(value));
	if(value>=0 && value < ids_.size())
	{
		ui_->label_idB->setText(QString::number(ids_.at(value)));
	}
	else
	{
		ULOGGER_ERROR("Slider index out of range ?");
	}
}

void DatabaseViewer::sliderNeighborValueChanged(int value)
{
	this->updateConstraintView(neighborLinks_.at(value));
}

void DatabaseViewer::sliderLoopValueChanged(int value)
{
	this->updateConstraintView(loopLinks_.at(value));
}

// only called when ui_->checkBox_showOptimized state changed
void DatabaseViewer::updateConstraintView()
{
	Link link = this->findActiveLink(ui_->horizontalSlider_A->value(), ui_->horizontalSlider_B->value());
	if(link.type() == Link::kNeighbor)
	{
		this->updateConstraintView(neighborLinks_.at(ui_->horizontalSlider_neighbors->value()), false);
	}
	else
	{
		this->updateConstraintView(loopLinks_.at(ui_->horizontalSlider_loops->value()), false);
	}
}

void DatabaseViewer::updateConstraintView(
		const rtabmap::Link & linkIn,
		bool updateImageSliders,
		const pcl::PointCloud<pcl::PointXYZ>::Ptr & cloudFrom,
		const pcl::PointCloud<pcl::PointXYZ>::Ptr & cloudTo,
		const pcl::PointCloud<pcl::PointXYZ>::Ptr & scanFrom,
		const pcl::PointCloud<pcl::PointXYZ>::Ptr & scanTo)
{
	std::multimap<int, Link>::iterator iter = rtabmap::graph::findLink(linksRefined_, linkIn.from(), linkIn.to());
	rtabmap::Link link = linkIn;
	if(iter != linksRefined_.end())
	{
		link = iter->second;
	}
	rtabmap::Transform t = link.transform();

	ui_->label_constraint->clear();
	ui_->label_constraint_opt->clear();
	ui_->checkBox_showOptimized->setEnabled(false);
	UASSERT(!t.isNull() && memory_);

	ui_->label_type->setNum(link.type());
	ui_->label_variance->setText(QString("%1, %2").arg(sqrt(link.rotVariance())).arg(sqrt(link.transVariance())));
	ui_->label_constraint->setText(QString("%1").arg(t.prettyPrint().c_str()).replace(" ", "\n"));
	if(link.type() == Link::kNeighbor &&
	   graphes_.size() &&
	   (int)graphes_.size()-1 == ui_->horizontalSlider_iterations->maximum())
	{
		std::map<int, rtabmap::Transform> & graph = uValueAt(graphes_, ui_->horizontalSlider_iterations->value());
		if(link.type() == Link::kNeighbor)
		{
			std::map<int, rtabmap::Transform>::iterator iterFrom = graph.find(link.from());
			std::map<int, rtabmap::Transform>::iterator iterTo = graph.find(link.to());
			if(iterFrom != graph.end() && iterTo != graph.end())
			{
				ui_->checkBox_showOptimized->setEnabled(true);
				Transform topt = iterFrom->second.inverse()*iterTo->second;
				float diff = topt.getDistance(t);
				Transform v1 = t.rotation()*Transform(1,0,0,0,0,0);
				Transform v2 = topt.rotation()*Transform(1,0,0,0,0,0);
				float a = pcl::getAngle3D(Eigen::Vector4f(v1.x(), v1.y(), v1.z(), 0), Eigen::Vector4f(v2.x(), v2.y(), v2.z(), 0));
				a = (a *180.0f) / CV_PI;
				ui_->label_constraint_opt->setText(QString("%1\n(error=%2% a=%3)").arg(QString(topt.prettyPrint().c_str()).replace(" ", "\n")).arg((diff/t.getNorm())*100.0f).arg(a));

				if(ui_->checkBox_showOptimized->isChecked())
				{
					t = topt;
				}
			}
		}
	}

	if(updateImageSliders)
	{
		ui_->horizontalSlider_A->blockSignals(true);
		ui_->horizontalSlider_B->blockSignals(true);
		// set from on left and to on right		{
		ui_->horizontalSlider_A->setValue(idToIndex_.value(link.from()));
		ui_->horizontalSlider_B->setValue(idToIndex_.value(link.to()));
		ui_->horizontalSlider_A->blockSignals(false);
		ui_->horizontalSlider_B->blockSignals(false);
		this->update(idToIndex_.value(link.from()),
					ui_->label_indexA,
					ui_->label_parentsA,
					ui_->label_childrenA,
					ui_->label_weightA,
					ui_->label_labelA,
					ui_->label_stampA,
					ui_->graphicsView_A,
					ui_->widget_cloudA,
					ui_->label_idA,
					false); // don't update constraints view!
		this->update(idToIndex_.value(link.to()),
					ui_->label_indexB,
					ui_->label_parentsB,
					ui_->label_childrenB,
					ui_->label_weightB,
					ui_->label_labelB,
					ui_->label_stampB,
					ui_->graphicsView_B,
					ui_->widget_cloudB,
					ui_->label_idB,
					false); // don't update constraints view!
	}

	if(ui_->constraintsViewer->isVisible())
	{
		Signature dataFrom, dataTo;

		dataFrom = memory_->getSignatureData(link.from(), true);
		UASSERT(dataFrom.getImageRaw().empty() || dataFrom.getImageRaw().type()==CV_8UC3 || dataFrom.getImageRaw().type() == CV_8UC1);
		UASSERT(dataFrom.getDepthRaw().empty() || dataFrom.getDepthRaw().type()==CV_8UC1 || dataFrom.getDepthRaw().type() == CV_16UC1 || dataFrom.getDepthRaw().type() == CV_32FC1);

		dataTo = memory_->getSignatureData(link.to(), true);
		UASSERT(dataTo.getImageRaw().empty() || dataTo.getImageRaw().type()==CV_8UC3 || dataTo.getImageRaw().type() == CV_8UC1);
		UASSERT(dataTo.getDepthRaw().empty() || dataTo.getDepthRaw().type()==CV_8UC1 || dataTo.getDepthRaw().type() == CV_16UC1 || dataTo.getDepthRaw().type() == CV_32FC1);


		if(cloudFrom->size() == 0 && cloudTo->size() == 0)
		{
			//cloud 3d
			if(!ui_->checkBox_show3DWords->isChecked())
			{
				pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudFrom;
				if(dataFrom.getDepthRaw().type() == CV_8UC1)
				{
					cloudFrom = rtabmap::util3d::cloudFromStereoImages(
							dataFrom.getImageRaw(),
							dataFrom.getDepthRaw(),
							dataFrom.getCx(), dataFrom.getCy(),
							dataFrom.getFx(), dataFrom.getFy(),
							1);
				}
				else
				{
					cloudFrom = rtabmap::util3d::cloudFromDepthRGB(
							dataFrom.getImageRaw(),
							dataFrom.getDepthRaw(),
							dataFrom.getCx(), dataFrom.getCy(),
							dataFrom.getFx(), dataFrom.getFy(),
							1);
				}

				cloudFrom = rtabmap::util3d::removeNaNFromPointCloud<pcl::PointXYZRGB>(cloudFrom);
				cloudFrom = rtabmap::util3d::transformPointCloud<pcl::PointXYZRGB>(cloudFrom, dataFrom.getLocalTransform());

				pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudTo;
				if(dataTo.getDepthRaw().type() == CV_8UC1)
				{
					cloudTo = rtabmap::util3d::cloudFromStereoImages(
							dataTo.getImageRaw(),
							dataTo.getDepthRaw(),
							dataTo.getCx(), dataTo.getCy(),
							dataTo.getFx(), dataTo.getFy(),
							1);
				}
				else
				{
					cloudTo = rtabmap::util3d::cloudFromDepthRGB(
							dataTo.getImageRaw(),
							dataTo.getDepthRaw(),
							dataTo.getCx(), dataTo.getCy(),
							dataTo.getFx(), dataTo.getFy(),
							1);
				}

				cloudTo = rtabmap::util3d::removeNaNFromPointCloud<pcl::PointXYZRGB>(cloudTo);
				cloudTo = rtabmap::util3d::transformPointCloud<pcl::PointXYZRGB>(cloudTo, t*dataTo.getLocalTransform());

				if(cloudFrom->size())
				{
					ui_->constraintsViewer->addOrUpdateCloud("cloud0", cloudFrom, Transform::getIdentity(), Qt::red);
				}
				if(cloudTo->size())
				{
					ui_->constraintsViewer->addOrUpdateCloud("cloud1", cloudTo, Transform::getIdentity(), Qt::cyan);
				}
			}
			else
			{
				const Signature * sFrom = memory_->getSignature(link.from());
				const Signature * sTo = memory_->getSignature(link.to());
				if(sFrom && sTo)
				{
					pcl::PointCloud<pcl::PointXYZ>::Ptr cloudFrom(new pcl::PointCloud<pcl::PointXYZ>);
					pcl::PointCloud<pcl::PointXYZ>::Ptr cloudTo(new pcl::PointCloud<pcl::PointXYZ>);
					cloudFrom->resize(sFrom->getWords3().size());
					cloudTo->resize(sTo->getWords3().size());
					int i=0;
					for(std::multimap<int, pcl::PointXYZ>::const_iterator iter=sFrom->getWords3().begin();
						iter!=sFrom->getWords3().end();
						++iter)
					{
						cloudFrom->at(i++) = iter->second;
					}
					i=0;
					for(std::multimap<int, pcl::PointXYZ>::const_iterator iter=sTo->getWords3().begin();
						iter!=sTo->getWords3().end();
						++iter)
					{
						cloudTo->at(i++) = iter->second;
					}

					if(cloudFrom->size())
					{
						cloudFrom = rtabmap::util3d::removeNaNFromPointCloud<pcl::PointXYZ>(cloudFrom);
					}
					if(cloudTo->size())
					{
						cloudTo = rtabmap::util3d::removeNaNFromPointCloud<pcl::PointXYZ>(cloudTo);
						cloudTo = rtabmap::util3d::transformPointCloud<pcl::PointXYZ>(cloudTo, t);
					}

					if(cloudFrom->size())
					{
						ui_->constraintsViewer->addOrUpdateCloud("cloud0", cloudFrom, Transform::getIdentity(), Qt::red);
					}
					else
					{
						UWARN("Empty 3D words for node %d", link.from());
					}
					if(cloudTo->size())
					{
						ui_->constraintsViewer->addOrUpdateCloud("cloud1", cloudTo, Transform::getIdentity(), Qt::cyan);
					}
					else
					{
						UWARN("Empty 3D words for node %d", link.to());
					}
				}
				else
				{
					UERROR("Not found signature %d or %d in RAM", link.from(), link.to());
				}
			}
		}
		else
		{
			if(cloudFrom->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("cloud0", cloudFrom, Transform::getIdentity(), Qt::red);
			}
			if(cloudTo->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("cloud1", cloudTo, Transform::getIdentity(), Qt::cyan);
			}
		}

		if(scanFrom->size() == 0 && scanTo->size() == 0)
		{
			//cloud 2d
			pcl::PointCloud<pcl::PointXYZ>::Ptr scanA, scanB;
			scanA = rtabmap::util3d::laserScanToPointCloud(dataFrom.getLaserScanRaw());
			scanB = rtabmap::util3d::laserScanToPointCloud(dataTo.getLaserScanRaw());
			scanB = rtabmap::util3d::transformPointCloud<pcl::PointXYZ>(scanB, t);
			if(scanA->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("scan0", scanA, Transform::getIdentity(), Qt::yellow);
			}
			if(scanB->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("scan1", scanB, Transform::getIdentity(), Qt::magenta);
			}
		}
		else
		{
			if(scanFrom->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("scan0", scanFrom, Transform::getIdentity(), Qt::yellow);
			}
			if(scanTo->size())
			{
				ui_->constraintsViewer->addOrUpdateCloud("scan1", scanTo, Transform::getIdentity(), Qt::magenta);
			}
		}

		//update cordinate
		ui_->constraintsViewer->updateCameraTargetPosition(t);
		ui_->constraintsViewer->clearTrajectory();

		ui_->constraintsViewer->update();
	}

	// update buttons
	updateConstraintButtons();
}

void DatabaseViewer::updateConstraintButtons()
{
	ui_->pushButton_refine->setEnabled(false);
	ui_->pushButton_refineVisually->setEnabled(false);
	ui_->pushButton_reset->setEnabled(false);
	ui_->pushButton_add->setEnabled(false);
	ui_->pushButton_reject->setEnabled(false);

	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	if(from!=to && from && to)
	{
		if((!containsLink(links_, from ,to) && !containsLink(linksAdded_, from ,to)) ||
			containsLink(linksRemoved_, from ,to))
		{
			ui_->pushButton_add->setEnabled(true);
		}
	}

	Link currentLink = findActiveLink(from ,to);

	if(currentLink.isValid() &&
		((currentLink.from() == from && currentLink.to() == to) || (currentLink.from() == to && currentLink.to() == from)))
	{
		if(!containsLink(linksRemoved_, from ,to))
		{
			ui_->pushButton_reject->setEnabled(currentLink.type() != Link::kNeighbor);
		}

		//check for modified link
		bool modified = false;
		std::multimap<int, Link>::iterator iter = rtabmap::graph::findLink(linksRefined_, currentLink.from(), currentLink.to());
		if(iter != linksRefined_.end())
		{
			currentLink = iter->second;
			ui_->pushButton_reset->setEnabled(true);
			modified = true;
		}
		if(!modified)
		{
			ui_->pushButton_reset->setEnabled(false);
		}
		ui_->pushButton_refine->setEnabled(true);
		ui_->pushButton_refineVisually->setEnabled(true);
	}
}

void DatabaseViewer::sliderIterationsValueChanged(int value)
{
	if(memory_ && value >=0 && value < (int)graphes_.size())
	{
		if(ui_->dockWidget_graphView->isVisible() && localMaps_.size() == 0)
		{
			//update scans
			UINFO("Update local maps list...");

			for(int i=0; i<ids_.size(); ++i)
			{
				UTimer time;
				bool added = false;
				if(ui_->groupBox_gridFromProjection->isChecked())
				{
					Signature data = memory_->getSignatureData(ids_.at(i), true);
					if(!data.getDepthRaw().empty())
					{
						pcl::PointCloud<pcl::PointXYZ>::Ptr cloud;
						if(data.getDepthRaw().type() == CV_8UC1)
						{
							cloud = rtabmap::util3d::cloudFromDisparity(
									util3d::disparityFromStereoImages(data.getImageRaw(), data.getDepthRaw()),
									data.getCx(),
									data.getCy(),
									data.getFx(),
									data.getFy(),
									ui_->spinBox_projDecimation->value());
						}
						else
						{
							cloud = util3d::cloudFromDepth(
									data.getDepthRaw(),
									data.getCx(),
									data.getCy(),
									data.getFx(),
									data.getFy(),
									ui_->spinBox_projDecimation->value());
						}
						if(cloud->size())
						{
							cloud = util3d::passThrough<pcl::PointXYZ>(cloud, "z", 0, ui_->doubleSpinBox_projMaxDepth->value());
						}

						if(cloud->size())
						{
							cloud = util3d::voxelize<pcl::PointXYZ>(cloud, ui_->doubleSpinBox_gridCellSize->value());
							cloud = util3d::transformPointCloud<pcl::PointXYZ>(cloud, data.getLocalTransform());

							UTimer timer;
							float cellSize = ui_->doubleSpinBox_gridCellSize->value();
							float groundNormalMaxAngle = M_PI_4;
							int minClusterSize = 20;
							cv::Mat ground, obstacles;
							util3d::occupancy2DFromCloud3D<pcl::PointXYZ>(
									cloud,
									ground, obstacles,
									cellSize,
									groundNormalMaxAngle,
									minClusterSize);
							if(!ground.empty() || !obstacles.empty())
							{
								localMaps_.insert(std::make_pair(ids_.at(i), std::make_pair(ground, obstacles)));
								added = true;
							}
						}
					}
				}
				else
				{
					Signature data = memory_->getSignatureData(ids_.at(i), false);
					if(!data.getLaserScanCompressed().empty())
					{
						pcl::PointCloud<pcl::PointXYZ>::Ptr cloud;
						cv::Mat laserScan;
						data.uncompressDataConst(0, 0, &laserScan);
						cv::Mat ground, obstacles;
						util3d::occupancy2DFromLaserScan(laserScan, ground, obstacles, ui_->doubleSpinBox_gridCellSize->value());
						localMaps_.insert(std::make_pair(ids_.at(i), std::make_pair(ground, obstacles)));
						added = true;
					}
				}
				if(added)
				{
					UINFO("Processed grid map %d/%d (%fs)", i+1, (int)ids_.size(), time.ticks());
				}
			}
			UINFO("Update local maps list... done");
		}
		std::map<int, rtabmap::Transform> & graph = uValueAt(graphes_, value);
		std::multimap<int, Link> links = updateLinksWithModifications(links_);
		ui_->graphViewer->updateGraph(graph, links);
		if(graph.size() && localMaps_.size() && ui_->graphViewer->isGridMapVisible())
		{
			float xMin, yMin;
			float cell = ui_->doubleSpinBox_gridCellSize->value();
			cv::Mat map;
			QTime time;
			time.start();
			if(ui_->groupBox_posefiltering->isChecked())
			{
				std::map<int, rtabmap::Transform> graphFiltered = graph::radiusPosesFiltering(graph,
						ui_->doubleSpinBox_posefilteringRadius->value(),
						ui_->doubleSpinBox_posefilteringAngle->value()*CV_PI/180.0);
				map = rtabmap::util3d::create2DMapFromOccupancyLocalMaps(graphFiltered, localMaps_, cell, xMin, yMin, 0, ui_->checkBox_gridErode->isChecked());
			}
			else
			{
				map = rtabmap::util3d::create2DMapFromOccupancyLocalMaps(graph, localMaps_, cell, xMin, yMin, 0, ui_->checkBox_gridErode->isChecked());
			}
			if(!map.empty())
			{
				ui_->graphViewer->updateMap(rtabmap::util3d::convertMap2Image8U(map), cell, xMin, yMin);
			}
			ui_->label_timeGrid->setNum(double(time.elapsed())/1000.0);
		}
		ui_->graphViewer->update();
		ui_->label_iterations->setNum(value);

		//compute total length (neighbor links)
		float length = 0.0f;
		for(std::multimap<int, rtabmap::Link>::const_iterator iter=links.begin(); iter!=links.end(); ++iter)
		{
			std::map<int, rtabmap::Transform>::const_iterator jterA = graph.find(iter->first);
			std::map<int, rtabmap::Transform>::const_iterator jterB = graph.find(iter->second.to());
			if(jterA != graph.end() && jterB != graph.end())
			{
				const rtabmap::Transform & poseA = jterA->second;
				const rtabmap::Transform & poseB = jterB->second;
				if(iter->second.type() == rtabmap::Link::kNeighbor)
				{
					Eigen::Vector3f vA, vB;
					poseA.getTranslation(vA[0], vA[1], vA[2]);
					poseB.getTranslation(vB[0], vB[1], vB[2]);
					length += (vB - vA).norm();
				}
			}
		}
		ui_->label_pathLength->setNum(length);
	}
}
void DatabaseViewer::updateGraphView()
{
	if(poses_.size())
	{
		if(!uContains(poses_, ui_->spinBox_optimizationsFrom->value()))
		{
			QMessageBox::warning(this, tr(""), tr("Graph optimization from id (%1) for which node is not linked to graph.\n Minimum=%2, Maximum=%3")
						.arg(ui_->spinBox_optimizationsFrom->value())
						.arg(poses_.begin()->first)
						.arg(poses_.rbegin()->first));
			return;
		}

		graphes_.clear();

		std::map<int, rtabmap::Transform> finalPoses;
		graphes_.push_back(poses_);
		ui_->actionGenerate_TORO_graph_graph->setEnabled(true);
		std::multimap<int, rtabmap::Link> links;
		if(ui_->checkBox_ignorePoseCorrection->isChecked())
		{
			std::multimap<int, Link> tmp = links_;
			for(std::multimap<int, Link>::iterator iter=tmp.begin(); iter!=tmp.end(); ++iter)
			{
				if(iter->second.type() == Link::kNeighbor)
				{
					Transform poseFrom = uValue(poses_, iter->second.from(), Transform());
					Transform poseTo = uValue(poses_, iter->second.to(), Transform());
					if(!poseFrom.isNull() && !poseTo.isNull())
					{
						iter->second.setTransform(poseFrom.inverse() * poseTo); // recompute raw odom transformation

					}
				}
			}
			links = updateLinksWithModifications(tmp);
		}
		else
		{
			links = updateLinksWithModifications(links_);
		}
		graph::Optimizer * optimizer = 0;
		if(ui_->comboBox_graphOptimizer->currentIndex() == graph::Optimizer::kTypeG2O)
		{
			optimizer = new graph::G2OOptimizer(ui_->spinBox_iterations->value(), ui_->checkBox_2dslam->isChecked(), ui_->checkBox_ignoreCovariance->isChecked());
		}
		else
		{
			optimizer = new graph::TOROOptimizer(ui_->spinBox_iterations->value(), ui_->checkBox_2dslam->isChecked(), ui_->checkBox_ignoreCovariance->isChecked());
		}
		int fromId = ui_->spinBox_optimizationsFrom->value();
		std::map<int, rtabmap::Transform> posesOut;
		std::multimap<int, rtabmap::Link> linksOut;
		optimizer->getConnectedGraph(
				fromId,
				poses_,
				links,
				posesOut,
				linksOut,
				ui_->spinBox_optimizationDepth->value());

		QTime time;
		time.start();
		finalPoses = optimizer->optimize(fromId, posesOut, linksOut, &graphes_);
		ui_->label_timeOptimization->setNum(double(time.elapsed())/1000.0);
		graphes_.push_back(finalPoses);
		delete optimizer;
	}
	if(graphes_.size())
	{
		ui_->horizontalSlider_iterations->setMaximum((int)graphes_.size()-1);
		ui_->horizontalSlider_iterations->setValue((int)graphes_.size()-1);
		ui_->horizontalSlider_iterations->setEnabled(true);
		ui_->spinBox_optimizationsFrom->setEnabled(true);
		sliderIterationsValueChanged((int)graphes_.size()-1);
	}
	else
	{
		ui_->horizontalSlider_iterations->setEnabled(false);
		ui_->spinBox_optimizationsFrom->setEnabled(false);
	}
}

void DatabaseViewer::updateGrid()
{
	if((sender() != ui_->spinBox_projDecimation && sender() != ui_->doubleSpinBox_projMaxDepth) ||
	   (sender() == ui_->spinBox_projDecimation && ui_->groupBox_gridFromProjection->isChecked()) ||
	   (sender() == ui_->doubleSpinBox_projMaxDepth && ui_->groupBox_gridFromProjection->isChecked()))
	{
		localMaps_.clear();
		updateGraphView();
	}
}

Link DatabaseViewer::findActiveLink(int from, int to)
{
	Link link;
	std::multimap<int, Link>::iterator findIter = rtabmap::graph::findLink(linksRefined_, from ,to);
	if(findIter != linksRefined_.end())
	{
		link = findIter->second;
	}
	else
	{
		findIter = rtabmap::graph::findLink(linksAdded_, from ,to);
		if(findIter != linksAdded_.end())
		{
			link = findIter->second;
		}
		else if(!containsLink(linksRemoved_, from ,to))
		{
			findIter = rtabmap::graph::findLink(links_, from ,to);
			if(findIter != links_.end())
			{
				link = findIter->second;
			}
		}
	}
	return link;
}

bool DatabaseViewer::containsLink(std::multimap<int, Link> & links, int from, int to)
{
	return rtabmap::graph::findLink(links, from, to) != links.end();
}

void DatabaseViewer::refineConstraint()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	refineConstraint(from, to, true);
}

void DatabaseViewer::refineConstraint(int from, int to, bool updateGraph)
{
	if(from == to)
	{
		UWARN("Cannot refine link to same node");
		return;
	}

	Link currentLink =  findActiveLink(from, to);
	if(!currentLink.isValid())
	{
		UERROR("Not found link! (%d->%d)", from, to);
		return;
	}
	Transform t = currentLink.transform();
	if(ui_->checkBox_showOptimized->isChecked() &&
	   currentLink.type() == Link::kNeighbor &&
	   graphes_.size() &&
	   (int)graphes_.size()-1 == ui_->horizontalSlider_iterations->maximum())
	{
		std::map<int, rtabmap::Transform> & graph = uValueAt(graphes_, ui_->horizontalSlider_iterations->value());
		if(currentLink.type() == Link::kNeighbor)
		{
			std::map<int, rtabmap::Transform>::iterator iterFrom = graph.find(currentLink.from());
			std::map<int, rtabmap::Transform>::iterator iterTo = graph.find(currentLink.to());
			if(iterFrom != graph.end() && iterTo != graph.end())
			{
				Transform topt = iterFrom->second.inverse()*iterTo->second;
				t = topt;
			}
		}
	}


	bool hasConverged = false;
	double variance = -1.0;
	int correspondences = 0;
	Transform transform;

	Signature dataFrom, dataTo;
	dataFrom = memory_->getSignatureData(currentLink.from(), false);
	dataTo = memory_->getSignatureData(currentLink.to(), false);

	pcl::PointCloud<pcl::PointXYZ>::Ptr cloudA(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloudB(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr scanA(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr scanB(new pcl::PointCloud<pcl::PointXYZ>);
	if(ui_->checkBox_icp_2d->isChecked())
	{
		//2D
		cv::Mat oldLaserScan = rtabmap::uncompressData(dataFrom.getLaserScanCompressed());
		cv::Mat newLaserScan = rtabmap::uncompressData(dataTo.getLaserScanCompressed());

		if(!oldLaserScan.empty() && !newLaserScan.empty())
		{
			// 2D
			scanA = util3d::cvMat2Cloud(oldLaserScan);
			scanB = util3d::cvMat2Cloud(newLaserScan, t);

			//voxelize
			if(ui_->doubleSpinBox_icp_voxel->value() > 0.0f)
			{
				scanA = util3d::voxelize<pcl::PointXYZ>(scanA, ui_->doubleSpinBox_icp_voxel->value());
				scanB = util3d::voxelize<pcl::PointXYZ>(scanB, ui_->doubleSpinBox_icp_voxel->value());
			}

			if(scanB->size() && scanA->size())
			{
				transform = util3d::icp2D(scanB,
						scanA,
						ui_->doubleSpinBox_icp_maxCorrespDistance->value(),
						ui_->spinBox_icp_iteration->value(),
					   &hasConverged,
					   &variance,
					   &correspondences);
			}
		}
	}
	else
	{
		//3D
		cv::Mat depthA = rtabmap::uncompressImage(dataFrom.getDepthCompressed());
		cv::Mat depthB = rtabmap::uncompressImage(dataTo.getDepthCompressed());

		if(depthA.type() == CV_8UC1)
		{
			cv::Mat leftMono;
			cv::Mat left = rtabmap::uncompressImage(dataFrom.getImageCompressed());
			if(left.channels() > 1)
			{
				cv::cvtColor(left, leftMono, CV_BGR2GRAY);
			}
			else
			{
				leftMono = left;
			}
			cloudA = util3d::cloudFromDisparity(util3d::disparityFromStereoImages(leftMono, depthA), dataFrom.getCx(), dataFrom.getCy(), dataFrom.getFx(), dataFrom.getFy(), ui_->spinBox_icp_decimation->value());
			if(ui_->doubleSpinBox_icp_maxDepth->value() > 0)
			{
				cloudA = util3d::passThrough<pcl::PointXYZ>(cloudA, "z", 0, ui_->doubleSpinBox_icp_maxDepth->value());
			}
			if(ui_->doubleSpinBox_icp_voxel->value() > 0)
			{
				cloudA = util3d::voxelize<pcl::PointXYZ>(cloudA, ui_->doubleSpinBox_icp_voxel->value());
			}
			cloudA = util3d::transformPointCloud<pcl::PointXYZ>(cloudA, dataFrom.getLocalTransform());
		}
		else
		{
			cloudA = util3d::getICPReadyCloud(depthA,
					dataFrom.getFx(), dataFrom.getFy(), dataFrom.getCx(), dataFrom.getCy(),
					ui_->spinBox_icp_decimation->value(),
					ui_->doubleSpinBox_icp_maxDepth->value(),
					ui_->doubleSpinBox_icp_voxel->value(),
					0, // no sampling
					dataFrom.getLocalTransform());
		}
		if(depthB.type() == CV_8UC1)
		{
			cv::Mat leftMono;
			cv::Mat left = rtabmap::uncompressImage(dataTo.getImageCompressed());
			if(left.channels() > 1)
			{
				cv::cvtColor(left, leftMono, CV_BGR2GRAY);
			}
			else
			{
				leftMono = left;
			}
			cloudB = util3d::cloudFromDisparity(util3d::disparityFromStereoImages(leftMono, depthB), dataTo.getCx(), dataTo.getCy(), dataTo.getFx(), dataTo.getFy(), ui_->spinBox_icp_decimation->value());
			if(ui_->doubleSpinBox_icp_maxDepth->value() > 0)
			{
				cloudB = util3d::passThrough<pcl::PointXYZ>(cloudB, "z", 0, ui_->doubleSpinBox_icp_maxDepth->value());
			}
			if(ui_->doubleSpinBox_icp_voxel->value() > 0)
			{
				cloudB = util3d::voxelize<pcl::PointXYZ>(cloudB, ui_->doubleSpinBox_icp_voxel->value());
			}
			cloudB = util3d::transformPointCloud<pcl::PointXYZ>(cloudB, t * dataTo.getLocalTransform());
		}
		else
		{
			cloudB = util3d::getICPReadyCloud(depthB,
					dataTo.getFx(), dataTo.getFy(), dataTo.getCx(), dataTo.getCy(),
					ui_->spinBox_icp_decimation->value(),
					ui_->doubleSpinBox_icp_maxDepth->value(),
					ui_->doubleSpinBox_icp_voxel->value(),
					0, // no sampling
					t * dataTo.getLocalTransform());
		}

		if(ui_->checkBox_icp_p2plane->isChecked())
		{
			pcl::PointCloud<pcl::PointNormal>::Ptr cloudANormals = util3d::computeNormals(cloudA, ui_->spinBox_icp_normalKSearch->value());
			pcl::PointCloud<pcl::PointNormal>::Ptr cloudBNormals = util3d::computeNormals(cloudB, ui_->spinBox_icp_normalKSearch->value());

			cloudANormals = util3d::removeNaNNormalsFromPointCloud<pcl::PointNormal>(cloudANormals);
			if(cloudA->size() != cloudANormals->size())
			{
				UWARN("removed nan normals...");
			}

			cloudBNormals = util3d::removeNaNNormalsFromPointCloud<pcl::PointNormal>(cloudBNormals);
			if(cloudB->size() != cloudBNormals->size())
			{
				UWARN("removed nan normals...");
			}

			transform = util3d::icpPointToPlane(cloudBNormals,
					cloudANormals,
					ui_->doubleSpinBox_icp_maxCorrespDistance->value(),
					ui_->spinBox_icp_iteration->value(),
					&hasConverged,
					&variance,
					&correspondences);
		}
		else
		{
			transform = util3d::icp(cloudB,
					cloudA,
					ui_->doubleSpinBox_icp_maxCorrespDistance->value(),
					ui_->spinBox_icp_iteration->value(),
					&hasConverged,
					&variance,
					&correspondences);
		}
	}

	if(hasConverged && !transform.isNull())
	{
		Link newLink(currentLink.from(), currentLink.to(), currentLink.type(), transform*t, variance, variance);

		bool updated = false;
		std::multimap<int, Link>::iterator iter = linksRefined_.find(currentLink.from());
		while(iter != linksRefined_.end() && iter->first == currentLink.from())
		{
			if(iter->second.to() == currentLink.to() &&
			   iter->second.type() == currentLink.type())
			{
				iter->second = newLink;
				updated = true;
				break;
			}
			++iter;
		}
		if(!updated)
		{
			linksRefined_.insert(std::make_pair<int, Link>(newLink.from(), newLink));

			if(updateGraph)
			{
				this->updateGraphView();
			}
		}

		if(ui_->dockWidget_constraints->isVisible())
		{
			cloudB = util3d::transformPointCloud<pcl::PointXYZ>(cloudB, transform);
			scanB = util3d::transformPointCloud<pcl::PointXYZ>(scanB, transform);
			this->updateConstraintView(newLink, true, cloudA, cloudB, scanA, scanB);
		}
	}
}

void DatabaseViewer::refineConstraintVisually()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	refineConstraintVisually(from, to, true);
}

void DatabaseViewer::refineConstraintVisually(int from, int to, bool updateGraph)
{
	if(from == to)
	{
		UWARN("Cannot refine link to same node");
		return;
	}

	Link currentLink =  findActiveLink(from, to);
	if(!currentLink.isValid())
	{
		UERROR("Not found link! (%d->%d)", from, to);
		return;
	}

	Transform t;
	std::string rejectedMsg;
	double variance = -1.0;
	int inliers = -1;
	if(ui_->groupBox_visual_recomputeFeatures->isChecked())
	{
		// create a fake memory to regenerate features
		ParametersMap parameters;
		parameters.insert(ParametersPair(Parameters::kKpDetectorStrategy(), uNumber2Str(ui_->comboBox_featureType->currentIndex())));
		parameters.insert(ParametersPair(Parameters::kKpNNStrategy(), uNumber2Str(ui_->comboBox_nnType->currentIndex())));
		parameters.insert(ParametersPair(Parameters::kLccBowInlierDistance(), uNumber2Str(ui_->doubleSpinBox_visual_maxCorrespDistance->value())));
		parameters.insert(ParametersPair(Parameters::kKpMaxDepth(), uNumber2Str(ui_->doubleSpinBox_visual_maxDepth->value())));
		parameters.insert(ParametersPair(Parameters::kKpNndrRatio(), uNumber2Str(ui_->doubleSpinBox_visual_nndr->value())));
		parameters.insert(ParametersPair(Parameters::kLccBowIterations(), uNumber2Str(ui_->spinBox_visual_iteration->value())));
		parameters.insert(ParametersPair(Parameters::kLccBowMinInliers(), uNumber2Str(ui_->spinBox_visual_minCorrespondences->value())));
		parameters.insert(ParametersPair(Parameters::kMemGenerateIds(), "false"));
		parameters.insert(ParametersPair(Parameters::kMemRehearsalSimilarity(), "1.0"));
		parameters.insert(ParametersPair(Parameters::kKpWordsPerImage(), "0"));
		Memory tmpMemory(parameters);

		// Add signatures
		SensorData dataFrom = memory_->getSignatureData(from, true).toSensorData();
		SensorData dataTo = memory_->getSignatureData(to, true).toSensorData();

		if(from > to)
		{
			tmpMemory.update(dataTo);
			tmpMemory.update(dataFrom);
		}
		else
		{
			tmpMemory.update(dataFrom);
			tmpMemory.update(dataTo);
		}


		t = tmpMemory.computeVisualTransform(to, from, &rejectedMsg, &inliers, &variance);
	}
	else
	{
		ParametersMap parameters;
		parameters.insert(ParametersPair(Parameters::kLccBowInlierDistance(), uNumber2Str(ui_->doubleSpinBox_visual_maxCorrespDistance->value())));
		parameters.insert(ParametersPair(Parameters::kLccBowMaxDepth(), uNumber2Str(ui_->doubleSpinBox_visual_maxDepth->value())));
		parameters.insert(ParametersPair(Parameters::kLccBowIterations(), uNumber2Str(ui_->spinBox_visual_iteration->value())));
		parameters.insert(ParametersPair(Parameters::kLccBowMinInliers(), uNumber2Str(ui_->spinBox_visual_minCorrespondences->value())));
		memory_->parseParameters(parameters);
		t = memory_->computeVisualTransform(to, from, &rejectedMsg, &inliers, &variance);
	}

	if(!t.isNull())
	{
		Link newLink(currentLink.from(), currentLink.to(), currentLink.type(), t, variance, variance);

		bool updated = false;
		std::multimap<int, Link>::iterator iter = linksRefined_.find(currentLink.from());
		while(iter != linksRefined_.end() && iter->first == currentLink.from())
		{
			if(iter->second.to() == currentLink.to() &&
			   iter->second.type() == currentLink.type())
			{
				iter->second = newLink;
				updated = true;
				break;
			}
			++iter;
		}
		if(!updated)
		{
			linksRefined_.insert(std::make_pair<int, Link>(newLink.from(), newLink));

			if(updateGraph)
			{
				this->updateGraphView();
			}
		}
		if(ui_->dockWidget_constraints->isVisible())
		{
			this->updateConstraintView(newLink);
		}
	}
}

void DatabaseViewer::addConstraint()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	addConstraint(from, to, false, true);
}

bool DatabaseViewer::addConstraint(int from, int to, bool silent, bool updateGraph)
{
	if(from == to)
	{
		UWARN("Cannot add link to same node");
		return false;
	}

	bool updateSlider = false;
	if(!containsLink(linksAdded_, from, to) &&
	   !containsLink(links_, from, to))
	{
		UASSERT(!containsLink(linksRemoved_, from, to));
		UASSERT(!containsLink(linksRefined_, from, to));

		Transform t;
		std::string rejectedMsg;
		double variance = -1.0;
		int inliers = -1;
		if(ui_->groupBox_visual_recomputeFeatures->isChecked())
		{
			// create a fake memory to regenerate features
			ParametersMap parameters;
			parameters.insert(ParametersPair(Parameters::kKpDetectorStrategy(), uNumber2Str(ui_->comboBox_featureType->currentIndex())));
			parameters.insert(ParametersPair(Parameters::kKpNNStrategy(), uNumber2Str(ui_->comboBox_nnType->currentIndex())));
			parameters.insert(ParametersPair(Parameters::kLccBowInlierDistance(), uNumber2Str(ui_->doubleSpinBox_visual_maxCorrespDistance->value())));
			parameters.insert(ParametersPair(Parameters::kKpMaxDepth(), uNumber2Str(ui_->doubleSpinBox_visual_maxDepth->value())));
			parameters.insert(ParametersPair(Parameters::kKpNndrRatio(), uNumber2Str(ui_->doubleSpinBox_visual_nndr->value())));
			parameters.insert(ParametersPair(Parameters::kLccBowIterations(), uNumber2Str(ui_->spinBox_visual_iteration->value())));
			parameters.insert(ParametersPair(Parameters::kLccBowMinInliers(), uNumber2Str(ui_->spinBox_visual_minCorrespondences->value())));
			parameters.insert(ParametersPair(Parameters::kMemGenerateIds(), "false"));
			parameters.insert(ParametersPair(Parameters::kMemRehearsalSimilarity(), "1.0"));
			parameters.insert(ParametersPair(Parameters::kKpWordsPerImage(), "0"));
			Memory tmpMemory(parameters);

			// Add signatures
			SensorData dataFrom = memory_->getSignatureData(from, true).toSensorData();
			SensorData dataTo = memory_->getSignatureData(to, true).toSensorData();

			if(from > to)
			{
				tmpMemory.update(dataTo);
				tmpMemory.update(dataFrom);
			}
			else
			{
				tmpMemory.update(dataFrom);
				tmpMemory.update(dataTo);
			}


			t = tmpMemory.computeVisualTransform(to, from, &rejectedMsg, &inliers, &variance);

			if(!silent)
			{
				ui_->graphicsView_A->setFeatures(tmpMemory.getSignature(from)->getWords());
				ui_->graphicsView_B->setFeatures(tmpMemory.getSignature(to)->getWords());
				updateWordsMatching();
			}
		}
		else
		{
			ParametersMap parameters;
			parameters.insert(ParametersPair(Parameters::kLccBowInlierDistance(), uNumber2Str(ui_->doubleSpinBox_visual_maxCorrespDistance->value())));
			parameters.insert(ParametersPair(Parameters::kLccBowMaxDepth(), uNumber2Str(ui_->doubleSpinBox_visual_maxDepth->value())));
			parameters.insert(ParametersPair(Parameters::kLccBowIterations(), uNumber2Str(ui_->spinBox_visual_iteration->value())));
			parameters.insert(ParametersPair(Parameters::kLccBowMinInliers(), uNumber2Str(ui_->spinBox_visual_minCorrespondences->value())));
			memory_->parseParameters(parameters);
			t = memory_->computeVisualTransform(to, from, &rejectedMsg, &inliers, &variance);
		}

		if(t.isNull())
		{
			if(!silent)
			{
				QMessageBox::warning(this,
						tr("Add link"),
						tr("Cannot find a transformation between nodes %1 and %2: %3").arg(from).arg(to).arg(rejectedMsg.c_str()));
			}
		}
		else
		{
			if(ui_->checkBox_visual_2d->isChecked())
			{
				// We are 2D here, make sure the guess has only YAW rotation
				float x,y,z,r,p,yaw;
				t.getTranslationAndEulerAngles(x,y,z, r,p,yaw);
				t = Transform::fromEigen3f(pcl::getTransformation(x,y,0, 0, 0, yaw));
			}

			// transform is valid, make a link
			if(from>to)
			{
				linksAdded_.insert(std::make_pair(from, Link(from, to, Link::kUserClosure, t, variance, variance)));
			}
			else
			{
				linksAdded_.insert(std::make_pair(to, Link(to, from, Link::kUserClosure, t.inverse(), variance, variance)));
			}
			updateSlider = true;
		}
	}
	else if(containsLink(linksRemoved_, from, to))
	{
		//simply remove from linksRemoved
		linksRemoved_.erase(rtabmap::graph::findLink(linksRemoved_, from, to));
		updateSlider = true;
	}

	if(updateSlider)
	{
		updateLoopClosuresSlider(from, to);
		if(updateGraph)
		{
			this->updateGraphView();
		}
	}
	return updateSlider;
}

void DatabaseViewer::resetConstraint()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	if(from < to)
	{
		int tmp = to;
		to = from;
		from = tmp;
	}

	if(from == to)
	{
		UWARN("Cannot reset link to same node");
		return;
	}


	std::multimap<int, Link>::iterator iter = rtabmap::graph::findLink(linksRefined_, from, to);
	if(iter != linksRefined_.end())
	{
		linksRefined_.erase(iter);
		this->updateGraphView();
	}

	iter = rtabmap::graph::findLink(links_, from, to);
	if(iter != links_.end())
	{
		this->updateConstraintView(iter->second);
	}
	iter = rtabmap::graph::findLink(linksAdded_, from, to);
	if(iter != linksAdded_.end())
	{
		this->updateConstraintView(iter->second);
	}
}

void DatabaseViewer::rejectConstraint()
{
	int from = ids_.at(ui_->horizontalSlider_A->value());
	int to = ids_.at(ui_->horizontalSlider_B->value());
	if(from < to)
	{
		int tmp = to;
		to = from;
		from = tmp;
	}

	if(from == to)
	{
		UWARN("Cannot reject link to same node");
		return;
	}

	bool removed = false;

	// find the original one
	std::multimap<int, Link>::iterator iter;
	iter = rtabmap::graph::findLink(links_, from, to);
	if(iter != links_.end())
	{
		if(iter->second.type() == Link::kNeighbor)
		{
			UWARN("Cannot reject neighbor links (%d->%d)", from, to);
			return;
		}
		linksRemoved_.insert(*iter);
		removed = true;
	}

	// remove from refined and added
	iter = rtabmap::graph::findLink(linksRefined_, from, to);
	if(iter != linksRefined_.end())
	{
		linksRefined_.erase(iter);
		removed = true;
	}
	iter = rtabmap::graph::findLink(linksAdded_, from, to);
	if(iter != linksAdded_.end())
	{
		linksAdded_.erase(iter);
		removed = true;
	}
	if(removed)
	{
		this->updateGraphView();
	}
	updateLoopClosuresSlider();
}

std::multimap<int, rtabmap::Link> DatabaseViewer::updateLinksWithModifications(
		const std::multimap<int, rtabmap::Link> & edgeConstraints)
{
	std::multimap<int, rtabmap::Link> links;
	for(std::multimap<int, rtabmap::Link>::const_iterator iter=edgeConstraints.begin();
		iter!=edgeConstraints.end();
		++iter)
	{
		std::multimap<int, rtabmap::Link>::iterator findIter;

		findIter = rtabmap::graph::findLink(linksRemoved_, iter->second.from(), iter->second.to());
		if(findIter != linksRemoved_.end())
		{
			if(!(iter->second.from() == findIter->second.from() &&
			   iter->second.to() == findIter->second.to() &&
			   iter->second.type() == findIter->second.type()))
			{
				UWARN("Links (%d->%d,%d) and (%d->%d,%d) are not equal!?",
						iter->second.from(), iter->second.to(), iter->second.type(),
						findIter->second.from(), findIter->second.to(), findIter->second.type());
			}
			else
			{
				//UINFO("Removed link (%d->%d, %d)", iter->second.from(), iter->second.to(), iter->second.type());
				continue; // don't add this link
			}
		}

		findIter = rtabmap::graph::findLink(linksRefined_, iter->second.from(), iter->second.to());
		if(findIter!=linksRefined_.end())
		{
			if(iter->second.from() == findIter->second.from() &&
			   iter->second.to() == findIter->second.to() &&
			   iter->second.type() == findIter->second.type())
			{
				links.insert(*findIter); // add the refined link
				//UINFO("Updated link (%d->%d, %d)", iter->second.from(), iter->second.to(), iter->second.type());
				continue;
			}
			else
			{
				UWARN("Links (%d->%d,%d) and (%d->%d,%d) are not equal!?",
						iter->second.from(), iter->second.to(), iter->second.type(),
						findIter->second.from(), findIter->second.to(), findIter->second.type());
			}
		}

		links.insert(*iter); // add original link
	}

	//look for added links
	for(std::multimap<int, rtabmap::Link>::const_iterator iter=linksAdded_.begin();
		iter!=linksAdded_.end();
		++iter)
	{
		//UINFO("Added link (%d->%d, %d)", iter->second.from(), iter->second.to(), iter->second.type());
		links.insert(*iter);
	}

	return links;
}

void DatabaseViewer::updateLoopClosuresSlider(int from, int to)
{
	int size = loopLinks_.size();
	loopLinks_.clear();
	std::multimap<int, Link> links = updateLinksWithModifications(links_);
	int position = ui_->horizontalSlider_loops->value();
	for(std::multimap<int, rtabmap::Link>::iterator iter = links.begin(); iter!=links.end(); ++iter)
	{
		if(!iter->second.transform().isNull())
		{
			if(iter->second.type() != rtabmap::Link::kNeighbor)
			{
				if((iter->second.from() == from && iter->second.to() == to) ||
				   (iter->second.to() == from && iter->second.from() == to))
				{
					position = loopLinks_.size();
				}
				loopLinks_.append(iter->second);
			}
		}
		else
		{
			UERROR("Transform null for link from %d to %d", iter->first, iter->second.to());
		}
	}

	if(loopLinks_.size())
	{
		if(loopLinks_.size() == 1)
		{
			// just to be able to move the cursor of the loop slider
			loopLinks_.push_back(loopLinks_.front());
		}
		ui_->horizontalSlider_loops->setMinimum(0);
		ui_->horizontalSlider_loops->setMaximum(loopLinks_.size()-1);
		ui_->horizontalSlider_loops->setEnabled(true);
		if(position != ui_->horizontalSlider_loops->value())
		{
			ui_->horizontalSlider_loops->setValue(position);
		}
		else if(size != loopLinks_.size())
		{
			this->updateConstraintView(loopLinks_.at(position));
		}
	}
	else
	{
		ui_->horizontalSlider_loops->setEnabled(false);
		ui_->constraintsViewer->removeAllClouds();
		ui_->constraintsViewer->update();
		updateConstraintButtons();
	}
}

} // namespace rtabmap
