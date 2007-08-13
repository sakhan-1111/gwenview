// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2007 Aurélien Gâteau

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

// KDE
#include <kfileitem.h>

// Local
#include "archiveutils.h"
namespace Gwenview {


namespace ArchiveUtils {

typedef QMap<QString,QString> MimeTypeProtocols;

static const char* KDE_PROTOCOL = "X-KDE-LocalProtocol";

static const MimeTypeProtocols& mimeTypeProtocols() {
	static MimeTypeProtocols map;
	if (map.isEmpty()) {
		KMimeType::List list = KMimeType::allMimeTypes();
		KMimeType::List::Iterator it=list.begin(), end=list.end();
		for (; it!=end; ++it) {
			if ( (*it)->propertyNames().indexOf(KDE_PROTOCOL)!= -1 ) {
				QString protocol = (*it)->property(KDE_PROTOCOL).toString();
				map[(*it)->name()] = protocol;
			}
		}
	}
	return map;
}

bool fileItemIsArchive(const KFileItem& item) {
	return mimeTypeProtocols().contains(item.mimetype());
}

bool fileItemIsDirOrArchive(const KFileItem& item) {
	return item.isDir() || fileItemIsArchive(item);
}

/*
bool protocolIsArchive(const QString& protocol) {
	const MimeTypeProtocols& map=mimeTypeProtocols();
	MimeTypeProtocols::ConstIterator it;
	for (it=map.begin();it!=map.end();++it) {
		if (it.data()==protocol) return true;
	}
	return false;
}
*/
QStringList mimeTypes() {
	return mimeTypeProtocols().keys();
}

/*
QString protocolForMimeType(const QString& mimeType) {
	return mimeTypeProtocols()[mimeType];
}
*/

} // namespace ArchiveUtils

} // namespace Gwenview
