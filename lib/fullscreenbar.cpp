// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
// Self
#include "fullscreenbar.moc"

// Qt
#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include <QBitmap>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimeLine>
#include <QTimer>
#include <QToolButton>

// KDE
#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>

// Local

namespace Gwenview {


class BarIndicator : public QWidget {
public:
	BarIndicator(QWidget* parent)
	: QWidget(parent)
	, mOpacity(1.)
	, mPixmap(DesktopIcon("arrow-down"))
	{}

	qreal opacity() const {
		return mOpacity;
	}

	void setOpacity(qreal opacity) {
		mOpacity = opacity;
		update();
	}

	virtual QSize sizeHint() const {
		return mPixmap.size();
	}

protected:
	virtual void paintEvent(QPaintEvent*) {
		QPainter painter(this);
		painter.setOpacity(mOpacity);
		painter.drawPixmap(0, 0, mPixmap);
	}

private:
	qreal mOpacity;
	QPixmap mPixmap;
};


static const int SLIDE_DURATION = 150;
static const int AUTO_HIDE_TIMEOUT = 3000;


struct FullScreenBarPrivate {
	FullScreenBar* that;
	QTimeLine* mTimeLine;
	QTimer* mAutoHideTimer;

	BarIndicator* mBarIndicator;
	QTimer* mAutoHideBarIndicatorTimer;
	QTimeLine* mBarIndicatorTimeLine;

	void startTimeLine() {
		if (mTimeLine->state() != QTimeLine::Running) {
			mTimeLine->start();
		}
	}

	void hideCursor() {
		QBitmap empty(32, 32);
		empty.clear();
		QCursor blankCursor(empty, empty);
		QApplication::setOverrideCursor(blankCursor);
	}

	void setupBarIndicator() {
		mBarIndicator = new BarIndicator(that->parentWidget());

		const int screenWidth = that->sizeHint().width();
		const int indicatorWidth = mBarIndicator->sizeHint().width() * 2;
		const int indicatorHeight = mBarIndicator->sizeHint().height();
		mBarIndicator->setGeometry(
			(screenWidth - indicatorWidth) / 2, 0,
			indicatorWidth, indicatorHeight
			);

		that->updateBarIndicatorOpacity(0);

		mAutoHideBarIndicatorTimer = new QTimer(that);
		mAutoHideBarIndicatorTimer->setInterval(1500);
		mAutoHideBarIndicatorTimer->setSingleShot(true);
		QObject::connect(mAutoHideBarIndicatorTimer, SIGNAL(timeout()),
			that, SLOT(hideBarIndicator()) );

		mBarIndicatorTimeLine = new QTimeLine(500);
		QObject::connect(mBarIndicatorTimeLine, SIGNAL(valueChanged(qreal)),
			that, SLOT(updateBarIndicatorOpacity(qreal)) );
	}

	void showBarIndicator() {
		mAutoHideBarIndicatorTimer->start();
		if (mBarIndicatorTimeLine->state() == QTimeLine::Running && mBarIndicatorTimeLine->direction() == QTimeLine::Forward) {
			return;
		}
		if (mBarIndicatorTimeLine->currentValue() == 1.0) {
			return;
		}

		mBarIndicator->raise();

		mBarIndicatorTimeLine->setDirection(QTimeLine::Forward);
		if (mBarIndicatorTimeLine->state() != QTimeLine::Running) {
			mBarIndicatorTimeLine->start();
		}
	}
};


FullScreenBar::FullScreenBar(QWidget* parent)
: QFrame(parent)
, d(new FullScreenBarPrivate) {
	d->that = this;
	setObjectName("fullScreenBar");

	d->mTimeLine = new QTimeLine(SLIDE_DURATION, this);
	connect(d->mTimeLine, SIGNAL(valueChanged(qreal)), SLOT(moveBar(qreal)) );
	connect(d->mTimeLine, SIGNAL(finished()), SLOT(slotTimeLineFinished()) );

	d->mAutoHideTimer = new QTimer(this);
	d->mAutoHideTimer->setInterval(AUTO_HIDE_TIMEOUT);
	d->mAutoHideTimer->setSingleShot(true);
	connect(d->mAutoHideTimer, SIGNAL(timeout()), SLOT(autoHide()) );

	d->setupBarIndicator();

	hide();
}


FullScreenBar::~FullScreenBar() {
	delete d;
}

QSize FullScreenBar::sizeHint() const {
	int width = QApplication::desktop()->screenGeometry(this).width();
	return QSize(width, QFrame::sizeHint().height());
}

void FullScreenBar::moveBar(qreal value) {
	move(0, -height() + int(value * height()) );

	// For some reason, if Gwenview is started with command line options to
	// start a slideshow, the bar might end up below the view. Calling raise()
	// here fixes it.
	raise();
}


void FullScreenBar::updateBarIndicatorOpacity(qreal value) {
	d->mBarIndicator->setOpacity(value);
	raise();
}


void FullScreenBar::hideBarIndicator() {
	d->mBarIndicatorTimeLine->setDirection(QTimeLine::Backward);
	if (d->mBarIndicatorTimeLine->state() != QTimeLine::Running) {
		d->mBarIndicatorTimeLine->start();
	}
}


void FullScreenBar::setActivated(bool activated) {
	if (activated) {
		// Delay installation of event filter because switching to fullscreen
		// cause a few window adjustments, which seems to generate unwanted
		// mouse events, which cause the bar to slide in.
		QTimer::singleShot(500, this, SLOT(delayedInstallEventFilter()));

		// Make sure the widget is not partially visible on start
		move(0, -150);
		raise();
		show();
		d->hideCursor();
	} else {
		qApp->removeEventFilter(this);
		hide();
		d->mAutoHideTimer->stop();
		QApplication::restoreOverrideCursor();
	}
}


void FullScreenBar::delayedInstallEventFilter() {
	qApp->installEventFilter(this);
}


void FullScreenBar::autoHide() {
	Q_ASSERT(parentWidget());
	// Do not use QCursor::pos() directly, as it won't work in Xinerama because
	// rect().topLeft() is not always (0,0)
	QPoint pos = parentWidget()->mapFromGlobal(QCursor::pos());

	if (rect().contains(pos) || qApp->activePopupWidget()) {
		// Do nothing if the cursor is over the bar
		d->mAutoHideTimer->start();
		return;
	}
	d->hideCursor();
	slideOut();
}


void FullScreenBar::slideOut() {
	d->mTimeLine->setDirection(QTimeLine::Backward);
	d->startTimeLine();
}


void FullScreenBar::slideIn() {
	// Make sure auto hide timer does not kick in while we are sliding in
	d->mAutoHideTimer->stop();
	d->mTimeLine->setDirection(QTimeLine::Forward);
	d->startTimeLine();
}


bool FullScreenBar::eventFilter(QObject* object, QEvent* event) {
	if (event->type() == QEvent::MouseMove) {
		QApplication::restoreOverrideCursor();
		if (y() == 0) {
			// The bar is fully visible, restart timer
			d->mAutoHideTimer->start();
		} else {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->y() < height()) {
				// Mouse is not far from bar, show it
				//d->mBarIndicator->hide();
				slideIn();
			} else if (geometry().bottom() <= 0) {
				// Bar is totally invisible and mouse is far from it. Show bar
				// indicator
				d->showBarIndicator();
			}
		}
		return false;
	}

	// Filtering message on tooltip text for CJK to remove accelerators.
	// Quoting ktoolbar.cpp:
	// """
	// CJK languages use more verbose accelerator marker: they add a Latin
	// letter in parenthesis, and put accelerator on that. Hence, the default
	// removal of ampersand only may not be enough there, instead the whole
	// parenthesis construct should be removed. Provide these filtering i18n
	// messages so that translators can use Transcript for custom removal.
	// """
	if (event->type() == QEvent::Show || event->type() == QEvent::Paint) {
		QToolButton* button = qobject_cast<QToolButton*>(object);
		if (button && !button->actions().isEmpty()) {
			QAction* action = button->actions().first();
			button->setToolTip(i18nc("@info:tooltip of custom toolbar button", "%1", action->toolTip()));
		}
	}

	return false;
}


void FullScreenBar::slotTimeLineFinished() {
	if (d->mTimeLine->direction() == QTimeLine::Forward) {
		d->mAutoHideTimer->start();
	}
}


} // namespace
