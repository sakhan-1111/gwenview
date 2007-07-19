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
#include "loadingdocumentimpl.moc"

// Qt
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

// KDE
#include <kdebug.h>
#include <kurl.h>

// Local
#include "document.h"
#include "documentloadedimpl.h"
#include "imageutils.h"
#include "jpegcontent.h"
#include "jpegdocumentloadedimpl.h"

namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << k_funcinfo << x << endl
#else
#define LOG(x) ;
#endif

/**
 * CancellableBuffer
 * This class acts like QBuffer, but will simulates a truncated file if the
 * TSThread which was passed to its constructor has been asked for cancellation
 */
class CancellableBuffer : public QBuffer {
public:
	CancellableBuffer()
	: mCancel(false)
	{}

	void cancel() {
		QMutexLocker lock(&mMutex);
		mCancel = true;
	}

	bool atEnd() const {
		if (testCancel()) {
			LOG("cancel detected");
			return true;
		}
		return QBuffer::atEnd();
	}

	qint64 bytesAvailable() const {
		if (testCancel()) {
			LOG("cancel detected");
			return 0;
		}
		return QBuffer::bytesAvailable();
	}

	bool canReadLine() const {
		if (testCancel()) {
			LOG("cancel detected");
			return 0;
		}
		return QBuffer::canReadLine();
	}

	qint64 readData(char * data, qint64 maxSize) {
		if (testCancel()) {
			LOG("cancel detected");
			return 0;
		}
		return QBuffer::readData(data, maxSize);
	}

	qint64 readLineData(char * data, qint64 maxSize) {
		if (testCancel()) {
			LOG("cancel detected");
			return 0;
		}
		return QBuffer::readLineData(data, maxSize);
	}

private:
	bool testCancel() const {
		QMutexLocker lock(&mMutex);
		return mCancel;
	}

	mutable QMutex mMutex;
	bool mCancel;
};


class LoadingThread : public QThread {
public:
	LoadingThread()
	: mJpegContent(0) {
	}

	~LoadingThread() {
		delete mJpegContent;
	}

	virtual void run() {
		QMutexLocker lock(&mMutex);
		QString path = mUrl.path();
		mFormat = QImageReader::imageFormat(path);
		QFile file(path);
		bool ok = file.open(QIODevice::ReadOnly);
		if (!ok) {
			return;
		}
		mData = file.readAll();
		mBuffer.setBuffer(&mData);
		mBuffer.open(QIODevice::ReadOnly);

		ok = mImage.load(&mBuffer, mFormat.data());
		if (!ok) {
			return;
		}
		if (mFormat == "jpeg") {
			mJpegContent = new JpegContent();
			if (!mJpegContent->load(path)) {
				return;
			}
			Gwenview::Orientation orientation = mJpegContent->orientation();
			QMatrix matrix = ImageUtils::transformMatrix(orientation);
			mImage = mImage.transformed(matrix);
		}
	}

	void cancel() {
		mBuffer.cancel();
	}

	void setUrl(const KUrl& url) {
		QMutexLocker lock(&mMutex);
		mUrl = url;
	}

	const QByteArray& format() const {
		QMutexLocker lock(&mMutex);
		return mFormat;
	}

	const QByteArray& data() const {
		QMutexLocker lock(&mMutex);
		return mData;
	}

	const QImage& image() const {
		QMutexLocker lock(&mMutex);
		return mImage;
	}

	JpegContent* popJpegContent() {
		QMutexLocker lock(&mMutex);
		JpegContent* tmp = mJpegContent;
		mJpegContent = 0;
		return tmp;
	}

private:
	mutable QMutex mMutex;
	KUrl mUrl;
	CancellableBuffer mBuffer;
	QByteArray mData;
	QByteArray mFormat;
	QImage mImage;
	JpegContent* mJpegContent;
};


struct LoadingDocumentImplPrivate {
	LoadingThread mThread;
};


LoadingDocumentImpl::LoadingDocumentImpl(Document* document)
: AbstractDocumentImpl(document)
, d(new LoadingDocumentImplPrivate) {
}


LoadingDocumentImpl::~LoadingDocumentImpl() {
	LOG("");
	if (d->mThread.isRunning()) {
		LOG("");
		d->mThread.cancel();
		d->mThread.wait();
	}
	delete d;
}

void LoadingDocumentImpl::init() {
	d->mThread.setUrl(document()->url());
	connect(&d->mThread, SIGNAL(finished()), SLOT(slotImageLoaded()) );
	d->mThread.start();
}


bool LoadingDocumentImpl::isLoaded() const {
	return false;
}


void LoadingDocumentImpl::slotImageLoaded() {
	Q_ASSERT(d->mThread.isFinished());
	setDocumentImage(d->mThread.image());
	loaded();
	QByteArray format = d->mThread.format();
	setDocumentFormat(format);
	if (format == "jpeg") {
		JpegDocumentLoadedImpl* impl = new JpegDocumentLoadedImpl(
			document(),
			d->mThread.popJpegContent());
		switchToImpl(impl);
	} else {
		switchToImpl(new DocumentLoadedImpl(document()));
	}
}


Document::SaveResult LoadingDocumentImpl::save(const KUrl&, const QByteArray&) {
	return Document::SR_OtherError;
}

void LoadingDocumentImpl::setImage(const QImage&) {
	kWarning() << k_funcinfo << " should not be called\n";
}

} // namespace
