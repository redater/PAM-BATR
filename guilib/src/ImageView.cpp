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

#include "rtabmap/gui/ImageView.h"

#include <QtGui/QWheelEvent>
#include <QtCore/qmath.h>
#include <QMenu>
#include <QFileDialog>
#include <QtCore/QDir>
#include <QAction>
#include <QGraphicsEffect>
#include <QInputDialog>
#include <QVBoxLayout>
#include "rtabmap/utilite/ULogger.h"
#include "rtabmap/gui/KeypointItem.h"

namespace rtabmap {

ImageView::ImageView(QWidget * parent) :
		QWidget(parent),
		_savedFileName((QDir::homePath()+ "/") + "picture" + ".png"),
		_alpha(100),
		_imageItem(0),
		_imageDepthItem(0)
{
	_graphicsView = new QGraphicsView(this);
	_graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	_graphicsView->setScene(new QGraphicsScene(this));
	_graphicsView->setVisible(false);

	this->setLayout(new QVBoxLayout(this));
	this->layout()->addWidget(_graphicsView);
	this->layout()->setContentsMargins(0,0,0,0);

	_menu = new QMenu(tr(""), this);
	_showImage = _menu->addAction(tr("Show image"));
	_showImage->setCheckable(true);
	_showImage->setChecked(true);
	_showImageDepth = _menu->addAction(tr("Show image depth"));
	_showImageDepth->setCheckable(true);
	_showImageDepth->setChecked(false);
	_showFeatures = _menu->addAction(tr("Show features"));
	_showFeatures->setCheckable(true);
	_showFeatures->setChecked(true);
	_showLines = _menu->addAction(tr("Show lines"));
	_showLines->setCheckable(true);
	_showLines->setChecked(true);
	_graphicsViewMode = _menu->addAction(tr("Graphics view"));
	_graphicsViewMode->setCheckable(true);
	_graphicsViewMode->setChecked(false);
	_graphicsViewScaled = _menu->addAction(tr("Scale image"));
	_graphicsViewScaled->setCheckable(true);
	_graphicsViewScaled->setChecked(true);
	_graphicsViewScaled->setEnabled(false);
	_setAlpha = _menu->addAction(tr("Set alpha..."));
	_saveImage = _menu->addAction(tr("Save picture..."));
	_saveImage->setEnabled(false);

	connect(_graphicsView->scene(), SIGNAL(sceneRectChanged(const QRectF &)), this, SLOT(sceneRectChanged(const QRectF &)));
}

ImageView::~ImageView() {
	clear();
}

void ImageView::saveSettings(QSettings & settings, const QString & group) const
{
	if(!group.isEmpty())
	{
		settings.beginGroup(group);
	}
	settings.setValue("image_shown", this->isImageShown());
	settings.setValue("depth_shown", this->isImageDepthShown());
	settings.setValue("features_shown", this->isFeaturesShown());
	settings.setValue("lines_shown", this->isLinesShown());
	settings.setValue("alpha", this->getAlpha());
	settings.setValue("graphics_view", this->isGraphicsViewMode());
	settings.setValue("graphics_view_scale", this->isGraphicsViewScaled());
	if(!group.isEmpty())
	{
		settings.endGroup();
	}
}

void ImageView::loadSettings(QSettings & settings, const QString & group)
{
	if(!group.isEmpty())
	{
		settings.beginGroup(group);
	}
	this->setImageShown(settings.value("image_shown", this->isImageShown()).toBool());
	this->setImageDepthShown(settings.value("depth_shown", this->isImageDepthShown()).toBool());
	this->setFeaturesShown(settings.value("features_shown", this->isFeaturesShown()).toBool());
	this->setLinesShown(settings.value("lines_shown", this->isLinesShown()).toBool());
	this->setAlpha(settings.value("alpha", this->getAlpha()).toInt());
	this->setGraphicsViewMode(settings.value("graphics_view", this->isGraphicsViewMode()).toBool());
	this->setGraphicsViewScaled(settings.value("graphics_view_scale", this->isGraphicsViewScaled()).toBool());
	if(!group.isEmpty())
	{
		settings.endGroup();
	}
}

bool ImageView::isImageShown() const
{
	return _showImage->isChecked();
}

bool ImageView::isImageDepthShown() const
{
	return _showImageDepth->isChecked();
}

bool ImageView::isFeaturesShown() const
{
	return _showFeatures->isChecked();
}

bool ImageView::isGraphicsViewMode() const
{
	return _graphicsViewMode->isChecked();
}

bool ImageView::isGraphicsViewScaled() const
{
	return _graphicsViewScaled->isChecked();
}

const QColor & ImageView::getBackgroundColor() const
{
	return _graphicsView->backgroundBrush().color();
}


void ImageView::setFeaturesShown(bool shown)
{
	_showFeatures->setChecked(shown);
	for(QMultiMap<int, KeypointItem*>::iterator iter=_features.begin(); iter!=_features.end(); ++iter)
	{
		iter.value()->setVisible(_showFeatures->isChecked());
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setImageShown(bool shown)
{
	_showImage->setChecked(shown);
	if(_imageItem)
	{
		_imageItem->setVisible(_showImage->isChecked());
		this->updateOpacity();
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setImageDepthShown(bool shown)
{
	_showImageDepth->setChecked(shown);
	if(_imageDepthItem)
	{
		_imageDepthItem->setVisible(_showImageDepth->isChecked());
		this->updateOpacity();
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

bool ImageView::isLinesShown() const
{
	return _showLines->isChecked();
}

void ImageView::setLinesShown(bool shown)
{
	_showLines->setChecked(shown);
	for(int i=0; i<_lines.size(); ++i)
	{
		_lines.at(i)->setVisible(_showLines->isChecked());
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

float ImageView::viewScale() const
{
	if(_graphicsView->isVisible())
	{
		return _graphicsView->transform().m11();
	}
	else
	{
		float scale, offsetX, offsetY;
		computeScaleOffsets(this->rect(), scale, offsetX, offsetY);
		return scale;
	}
}

void ImageView::setGraphicsViewMode(bool on)
{
	_graphicsViewMode->setChecked(on);
	_graphicsView->setVisible(on);
	_graphicsViewScaled->setEnabled(on);

	if(on)
	{
		for(QMultiMap<int, KeypointItem*>::iterator iter=_features.begin(); iter!=_features.end(); ++iter)
		{
			_graphicsView->scene()->addItem(iter.value());
		}

		for(QList<QGraphicsLineItem*>::iterator iter=_lines.begin(); iter!=_lines.end(); ++iter)
		{
			_graphicsView->scene()->addItem(*iter);
		}

		//update images
		if(_imageItem)
		{
			_imageItem->setPixmap(_image);
		}
		else
		{
			_imageItem = _graphicsView->scene()->addPixmap(_image);
			_imageItem->setVisible(_showImage->isChecked());
			_showImage->setEnabled(true);
		}

		if(_imageDepthItem)
		{
			_imageDepthItem->setPixmap(_imageDepth);
		}
		else
		{
			_imageDepthItem = _graphicsView->scene()->addPixmap(_imageDepth);
			_imageDepthItem->setVisible(_showImageDepth->isChecked());
			_showImageDepth->setEnabled(true);
		}
		this->updateOpacity();

		if(_graphicsViewScaled->isChecked())
		{
			_graphicsView->fitInView(_graphicsView->sceneRect(), Qt::KeepAspectRatio);
		}
		else
		{
			_graphicsView->resetTransform();
		}
	}
	else
	{
		this->update();
	}
}

void ImageView::setGraphicsViewScaled(bool scaled)
{
	_graphicsViewScaled->setChecked(scaled);

	if(scaled)
	{
		_graphicsView->fitInView(_graphicsView->sceneRect(), Qt::KeepAspectRatio);
	}
	else
	{
		_graphicsView->resetTransform();
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setBackgroundColor(const QColor & color)
{
	_graphicsView->setBackgroundBrush(QBrush(color));

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::computeScaleOffsets(const QRect & targetRect, float & scale, float & offsetX, float & offsetY) const
{
	scale = 1.0f;
	offsetX = 0.0f;
	offsetY = 0.0f;

	if(!_graphicsView->scene()->sceneRect().isNull())
	{
		float w = _graphicsView->scene()->width();
		float h = _graphicsView->scene()->height();
		float widthRatio = float(targetRect.width()) / w;
		float heightRatio = float(targetRect.height()) / h;

		//printf("w=%f, h=%f, wR=%f, hR=%f, sW=%d, sH=%d\n", w, h, widthRatio, heightRatio, this->rect().width(), this->rect().height());
		if(widthRatio < heightRatio)
		{
			scale = widthRatio;
		}
		else
		{
			scale = heightRatio;
		}

		//printf("ratio=%f\n",ratio);

		w *= scale;
		h *= scale;

		if(w < targetRect.width())
		{
			offsetX = (targetRect.width() - w)/2.0f;
		}
		if(h < targetRect.height())
		{
			offsetY = (targetRect.height() - h)/2.0f;
		}
		//printf("offsetX=%f, offsetY=%f\n",offsetX, offsetY);
	}
}

void ImageView::sceneRectChanged(const QRectF & rect)
{
	_saveImage->setEnabled(rect.isValid());
}

void ImageView::paintEvent(QPaintEvent *event)
{
	if(_graphicsViewMode->isChecked())
	{
		QWidget::paintEvent(event);
	}
	else
	{
		if(!_graphicsView->scene()->sceneRect().isNull())
		{
			//Scale
			float ratio, offsetX, offsetY;
			this->computeScaleOffsets(event->rect(), ratio, offsetX, offsetY);
			QPainter painter(this);

			//Background
			painter.save();
			painter.setBrush(_graphicsView->backgroundBrush());
			painter.drawRect(event->rect());
			painter.restore();

			painter.translate(offsetX, offsetY);
			painter.scale(ratio, ratio);

			painter.save();
			if(_showImage->isChecked() && !_image.isNull() &&
			   _showImageDepth->isChecked() && !_imageDepth.isNull())
			{
				painter.setOpacity(0.5);
			}

			if(_showImage->isChecked() && !_image.isNull())
			{
				painter.drawPixmap(QPoint(0,0), _image);
			}

			if(_showImageDepth->isChecked() && !_imageDepth.isNull())
			{
				painter.drawPixmap(QPoint(0,0), _imageDepth);
			}
			painter.restore();

			if(_showFeatures->isChecked())
			{
				for(QMultiMap<int, rtabmap::KeypointItem *>::iterator iter = _features.begin(); iter != _features.end(); ++iter)
				{
					QColor color = iter.value()->pen().color();
					painter.save();
					painter.setPen(color);
					painter.setBrush(color);
					painter.drawEllipse(iter.value()->rect());
					painter.restore();
				}
			}

			if(_showLines->isChecked())
			{
				for(QList<QGraphicsLineItem*>::iterator iter = _lines.begin(); iter != _lines.end(); ++iter)
				{
					QColor color = (*iter)->pen().color();
					painter.save();
					painter.setPen(color);
					painter.drawLine((*iter)->line());
					painter.restore();
				}
			}
		}
	}
}

void ImageView::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if(_graphicsView->isVisible() && _graphicsViewScaled->isChecked())
	{
		_graphicsView->fitInView(_graphicsView->sceneRect(), Qt::KeepAspectRatio);
	}
}

void ImageView::contextMenuEvent(QContextMenuEvent * e)
{
	QAction * action = _menu->exec(e->globalPos());
	if(action == _saveImage)
	{
		if(!_graphicsView->scene()->sceneRect().isNull())
		{
			QString text;
#ifdef QT_SVG_LIB
			text = QFileDialog::getSaveFileName(this, tr("Save figure to ..."), _savedFileName, "*.png *.xpm *.jpg *.pdf *.svg");
#else
			text = QFileDialog::getSaveFileName(this, tr("Save figure to ..."), _savedFileName, "*.png *.xpm *.jpg *.pdf");
#endif
			if(!text.isEmpty())
			{
				_savedFileName = text;
				QImage img(_graphicsView->sceneRect().width(), _graphicsView->sceneRect().height(), QImage::Format_ARGB32_Premultiplied);
				QPainter p(&img);
				if(_graphicsView->isVisible())
				{
					_graphicsView->scene()->render(&p, _graphicsView->sceneRect(), _graphicsView->sceneRect());
				}
				else
				{
					this->render(&p, QPoint(), _graphicsView->sceneRect().toRect());
				}
				img.save(text);
			}
		}
	}
	else if(action == _showFeatures)
	{
		this->setFeaturesShown(_showFeatures->isChecked());
		emit configChanged();
	}
	else if(action == _showImage)
	{
		this->setImageShown(_showImage->isChecked());
		emit configChanged();
	}
	else if(action == _showImageDepth)
	{
		this->setImageDepthShown(_showImageDepth->isChecked());
		emit configChanged();
	}
	else if(action == _showLines)
	{
		this->setLinesShown(_showLines->isChecked());
		emit configChanged();
	}
	else if(action == _graphicsViewMode)
	{
		this->setGraphicsViewMode(_graphicsViewMode->isChecked());
		emit configChanged();
	}
	else if(action == _graphicsViewScaled)
	{
		this->setGraphicsViewScaled(_graphicsViewScaled->isChecked());
		emit configChanged();
	}
	else if(action == _setAlpha)
	{
		bool ok = false;
		int value = QInputDialog::getInt(this, tr("Set features and lines alpha"), tr("alpha (0-255)"), _alpha, 0, 255, 10, &ok);
		if(ok)
		{
			this->setAlpha(value);
			emit configChanged();
		}
	}

	if(action == _showImage || action ==_showImageDepth)
	{
		this->updateOpacity();
		emit configChanged();
	}
}

void ImageView::updateOpacity()
{
	if(_imageItem && _imageDepthItem)
	{
		if(_imageItem->isVisible() && _imageDepthItem->isVisible())
		{
			QGraphicsOpacityEffect * effect = new QGraphicsOpacityEffect();
			QGraphicsOpacityEffect * effect2 = new QGraphicsOpacityEffect();
			effect->setOpacity(0.5);
			effect2->setOpacity(0.5);
			_imageItem->setGraphicsEffect(effect);
			_imageDepthItem->setGraphicsEffect(effect2);
		}
		else
		{
			_imageItem->setGraphicsEffect(0);
			_imageDepthItem->setGraphicsEffect(0);
		}
	}
	else if(_imageItem)
	{
		_imageItem->setGraphicsEffect(0);
	}
	else if(_imageDepthItem)
	{
		_imageDepthItem->setGraphicsEffect(0);
	}
}

void ImageView::setFeatures(const std::multimap<int, cv::KeyPoint> & refWords, const QColor & color)
{
	qDeleteAll(_features);
	_features.clear();

	for(std::multimap<int, cv::KeyPoint>::const_iterator iter = refWords.begin(); iter != refWords.end(); ++iter )
	{
		addFeature(iter->first, iter->second, color);
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setFeatures(const std::vector<cv::KeyPoint> & features, const QColor & color)
{
	qDeleteAll(_features);
	_features.clear();

	for(unsigned int i = 0; i< features.size(); ++i )
	{
		addFeature(i, features[i], color);
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::addFeature(int id, const cv::KeyPoint & kpt, QColor color)
{
	color.setAlpha(this->getAlpha());
	rtabmap::KeypointItem * item = new rtabmap::KeypointItem(id, kpt, color);
	_features.insert(id, item);
	item->setVisible(isFeaturesShown());
	item->setZValue(1);

	if(_graphicsView->isVisible())
	{
		_graphicsView->scene()->addItem(item);
	}
}

void ImageView::addLine(float x1, float y1, float x2, float y2, QColor color)
{
	color.setAlpha(this->getAlpha());
	QGraphicsLineItem * item  = new QGraphicsLineItem(x1, y1, x2, y2);
	item->setPen(QPen(color));
	_lines.push_back(item);
	item->setVisible(isLinesShown());
	item->setZValue(1);

	if(_graphicsView->isVisible())
	{
		_graphicsView->scene()->addItem(item);
	}
}

void ImageView::setImage(const QImage & image)
{
	_image = QPixmap::fromImage(image);
	if(_graphicsView->isVisible())
	{
		if(_imageItem)
		{
			_imageItem->setPixmap(_image);
		}
		else
		{
			_imageItem = _graphicsView->scene()->addPixmap(_image);
			_imageItem->setVisible(_showImage->isChecked());
			_showImage->setEnabled(true);
			this->updateOpacity();
		}
	}
	else
	{
		this->setSceneRect(image.rect());
		this->update();
	}
}

void ImageView::setImageDepth(const QImage & imageDepth)
{
	_imageDepth = QPixmap::fromImage(imageDepth);
	if(_graphicsView->isVisible())
	{
		if(_imageDepthItem)
		{
			_imageDepthItem->setPixmap(_imageDepth);
		}
		else
		{
			_imageDepthItem = _graphicsView->scene()->addPixmap(_imageDepth);
			_imageDepthItem->setVisible(_showImageDepth->isChecked());
			_showImageDepth->setEnabled(true);
			this->updateOpacity();
		}
	}
	else
	{
		this->setSceneRect(imageDepth.rect());
		this->update();
	}
}

void ImageView::setFeatureColor(int id, QColor color)
{
	color.setAlpha(getAlpha());
	QList<KeypointItem*> items = _features.values(id);
	if(items.size())
	{
		for(int i=0; i<items.size(); ++i)
		{
			items[i]->setColor(color);
		}
	}
	else
	{
		UWARN("Not found feature %d", id);
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setFeaturesColor(QColor color)
{
	color.setAlpha(getAlpha());
	for(QMultiMap<int, KeypointItem*>::iterator iter=_features.begin(); iter!=_features.end(); ++iter)
	{
		iter.value()->setColor(color);
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setAlpha(int alpha)
{
	UASSERT(alpha >=0 && alpha <= 255);
	_alpha = alpha;
	for(QMultiMap<int, KeypointItem*>::iterator iter=_features.begin(); iter!=_features.end(); ++iter)
	{
		QColor c = iter.value()->pen().color();
		c.setAlpha(_alpha);
		iter.value()->setPen(QPen(c));
		iter.value()->setBrush(QBrush(c));
	}

	for(QList<QGraphicsLineItem*>::iterator iter=_lines.begin(); iter!=_lines.end(); ++iter)
	{
		QColor c = (*iter)->pen().color();
		c.setAlpha(_alpha);
		(*iter)->setPen(QPen(c));
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::setSceneRect(const QRectF & rect)
{
	_graphicsView->scene()->setSceneRect(rect);

	if(_graphicsViewScaled->isChecked())
	{
		_graphicsView->fitInView(_graphicsView->sceneRect(), Qt::KeepAspectRatio);
	}
	else
	{
		_graphicsView->resetTransform();
	}

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::clearLines()
{
	qDeleteAll(_lines);
	_lines.clear();

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

void ImageView::clear()
{
	qDeleteAll(_features);
	_features.clear();

	qDeleteAll(_lines);
	_lines.clear();

	if(_imageItem)
	{
		_graphicsView->scene()->removeItem(_imageItem);
		delete _imageItem;
		_imageItem = 0;
		_showImage->setEnabled(false);
	}
	_image = QPixmap();

	if(_imageDepthItem)
	{
		_graphicsView->scene()->removeItem(_imageDepthItem);
		delete _imageDepthItem;
		_imageDepthItem = 0;
		_showImageDepth->setEnabled(false);
	}
	_imageDepth = QPixmap();

	if(!_graphicsView->isVisible())
	{
		this->update();
	}
}

QSize ImageView::sizeHint() const
{
	return _graphicsView->sizeHint();
}

}
