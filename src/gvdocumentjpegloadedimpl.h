// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2004 Aur�lien G�teau
 
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
*/
#ifndef GVDOCUMENTJPEGLOADEDIMPL_H
#define GVDOCUMENTJPEGLOADEDIMPL_H

// Qt
#include <qimage.h>

// Local
#include "gvdocumentloadedimpl.h"

class GVDocument;

class GVDocumentJPEGLoadedImplPrivate;

class GVDocumentJPEGLoadedImpl : public GVDocumentLoadedImpl {
Q_OBJECT
public:
	GVDocumentJPEGLoadedImpl(GVDocument* document, QByteArray& rawData, const QString& tempFilePath);
	~GVDocumentJPEGLoadedImpl();
	
	QString comment() const;
	void setComment(const QString&);
	GVDocument::CommentState commentState() const;
	
	void modify(GVImageUtils::Orientation);

protected:
	bool localSave(const QString&, const QCString& format) const;
	
private:
	GVDocumentJPEGLoadedImplPrivate* d;

private slots:
	void finishLoading();
};

#endif /* GVDOCUMENTJPEGLOADEDIMPL_H */
