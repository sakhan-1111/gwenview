// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "croptool.h"

// Qt
#include <QPainter>
#include <QScrollBar>
#include <QRect>

// KDE

// Local
#include "imageview.h"

static const int HANDLE_SIZE = 10;

namespace Gwenview {


enum CropHandle {
	CH_Top = 1,
	CH_Left = 2,
	CH_Right = 4,
	CH_Bottom = 8,
	CH_TopLeft = CH_Top | CH_Left,
	CH_BottomLeft = CH_Bottom | CH_Left,
	CH_TopRight = CH_Top | CH_Right,
	CH_BottomRight = CH_Bottom | CH_Right
};


struct CropToolPrivate {
	CropTool* mCropTool;
	QRect mRect;
	QList<CropHandle> mCropHandleList;

	QRect handleViewportRect(CropHandle handle) {
		QRect viewportCropRect = mCropTool->imageView()->mapToViewport(mRect);
		int left, top;
		if (handle & CH_Top) {
			top = viewportCropRect.top() - HANDLE_SIZE;
		} else if (handle & CH_Bottom) {
			top = viewportCropRect.bottom();
		} else {
			top = viewportCropRect.top() + (viewportCropRect.height() - HANDLE_SIZE) / 2;
		}

		if (handle & CH_Left) {
			left = viewportCropRect.left() - HANDLE_SIZE;
		} else if (handle & CH_Right) {
			left = viewportCropRect.right();
		} else {
			left = viewportCropRect.left() + (viewportCropRect.width() - HANDLE_SIZE) / 2;
		}

		return QRect(left, top, HANDLE_SIZE, HANDLE_SIZE);
	}
};


CropTool::CropTool(QObject* parent)
: AbstractImageViewTool(parent)
, d(new CropToolPrivate) {
	d->mCropTool = this;
	d->mCropHandleList << CH_Left << CH_Right << CH_Top << CH_Bottom << CH_TopLeft << CH_TopRight << CH_BottomLeft << CH_BottomRight;
}


CropTool::~CropTool() {
	delete d;
}


void CropTool::setRect(const QRect& rect) {
	d->mRect = rect;
	imageView()->viewport()->update();
}


void CropTool::paint(QPainter* painter) {
	QRect rect = imageView()->mapToViewport(d->mRect);
	painter->drawRect(rect);

	Q_FOREACH(CropHandle handle, d->mCropHandleList) {
		rect = d->handleViewportRect(handle);
		painter->fillRect(rect, Qt::black);
	}
}

} // namespace
