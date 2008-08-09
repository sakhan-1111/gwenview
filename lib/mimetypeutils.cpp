// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright 2006 Aurelien Gateau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "mimetypeutils.h"
#include "mimetypeutils_p.moc"

// Qt
#include <QApplication>
#include <QStringList>

// KDE
#include <kapplication.h>
#include <kdebug.h>
#include <kfileitem.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kio/netaccess.h>
#include <kmimetype.h>
#include <kurl.h>

#include <kimageio.h>

// Local
#include "archiveutils.h"


namespace Gwenview {

namespace MimeTypeUtils {


const QStringList& dirMimeTypes() {
	static QStringList list;
	if (list.isEmpty()) {
		list << "inode/directory";
		list += ArchiveUtils::mimeTypes();
	}
	return list;
}


const QStringList& rasterImageMimeTypes() {
	static QStringList list;
	if (list.isEmpty()) {
		list=KImageIO::mimeTypes(KImageIO::Reading);
	}
	return list;
}


const QStringList& svgImageMimeTypes() {
	static QStringList list;
	if (list.isEmpty()) {
		list.append("image/svg+xml");
		// FIXME svgz
	}
	return list;
}


const QStringList& imageMimeTypes() {
	static QStringList list;
	if (list.isEmpty()) {
		list = rasterImageMimeTypes();
		list += svgImageMimeTypes();
	}

	return list;
}


const QStringList& videoMimeTypes() {
	static QStringList list;
	if (list.isEmpty()) {
#ifdef __GNUC__
	#warning implement MimeTypeUtils::videoMimeTypes()
#endif
	}

	return list;
}


QString urlMimeType(const KUrl& url) {
	// Try a simple guess, using extension for remote urls
	QString mimeType = KMimeType::findByUrl(url)->name();
	if (mimeType == "application/octet-stream") {
		// No luck, look deeper. This can happens with http urls if the filename
		// does not provide any extension.
		mimeType = KIO::NetAccess::mimetype(url, KApplication::kApplication()->activeWindow());
	}
	return mimeType;
}


QString urlMimeTypeByContent(const KUrl& url) {
	const int HEADER_SIZE = 30;
	if (url.isLocalFile()) {
		return KMimeType::findByFileContent(url.path())->name();
	}

	KIO::TransferJob* job = KIO::get(url);
	DataAccumulator accumulator(job);
	while (!accumulator.finished() && accumulator.data().size() < HEADER_SIZE) {
		qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	return KMimeType::findByContent(accumulator.data())->name();
}

Kind mimeTypeKind(const QString& mimeType) {
	if (mimeType.startsWith("inode/directory")) {
		return KIND_DIR;
	}
	if (ArchiveUtils::mimeTypes().contains(mimeType)) {
		return KIND_ARCHIVE;
	}
	if (rasterImageMimeTypes().contains(mimeType)) {
		return KIND_RASTER_IMAGE;
	}
	if (svgImageMimeTypes().contains(mimeType)) {
		return KIND_SVG_IMAGE;
	}

	return KIND_FILE;
}


Kind fileItemKind(const KFileItem& item) {
	return mimeTypeKind(item.mimetype());
}


Kind urlKind(const KUrl& url) {
	return mimeTypeKind(urlMimeType(url));
}


DataAccumulator::DataAccumulator(KIO::TransferJob* job)
: QObject()
, mFinished(false)
{
	connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
		SLOT(slotDataReceived(KIO::Job*, const QByteArray&)) );
	connect(job, SIGNAL(result(KJob*)),
		SLOT(slotFinished()) );
}


void DataAccumulator::slotDataReceived(KIO::Job*, const QByteArray& data) {
	mData += data;
}


void DataAccumulator::slotFinished() {
	mFinished = true;
}


} // namespace MimeTypeUtils
} // namespace Gwenview
