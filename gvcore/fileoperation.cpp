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

// Qt
#include <qcursor.h>
#include <qpopupmenu.h>
#include <qobject.h>

// KDE
#include <kconfig.h>
#include <kiconloader.h>
#include <klocale.h>

// Local
#include "fileopobject.h"
#include "fileoperation.h"
#include "fileoperationconfig.h"

namespace Gwenview {


//-FileOperations--------------------------------------------------
void FileOperation::copyTo(const KURL::List& srcURL,QWidget* parent) {
	FileOpObject* op=new FileOpCopyToObject(srcURL,parent);
	(*op)();
}

void FileOperation::linkTo(const KURL::List& srcURL,QWidget* parent) {
	FileOpObject* op=new FileOpLinkToObject(srcURL,parent);
	(*op)();
}

void FileOperation::moveTo(const KURL::List& srcURL,QWidget* parent,QObject* receiver,const char* slot) {
	FileOpObject* op=new FileOpMoveToObject(srcURL,parent);
	if (receiver && slot) QObject::connect(op,SIGNAL(success()),receiver,slot);
	(*op)();
}


void FileOperation::del(const KURL::List& url,QWidget* parent,QObject* receiver,const char* slot) {
	FileOpObject* op;
	if (FileOperationConfig::self()->deleteToTrash()) {
		op=new FileOpTrashObject(url,parent);
	} else {
		op=new FileOpRealDeleteObject(url,parent);
	}
	if (receiver && slot) QObject::connect(op,SIGNAL(success()),receiver,slot);
	(*op)();
}


void FileOperation::rename(const KURL& url,QWidget* parent,QObject* receiver,const char* slot) {
	FileOpObject* op=new FileOpRenameObject(url,parent);
	if (receiver && slot) QObject::connect(op,SIGNAL(renamed(const QString&)),receiver,slot);
	(*op)();
}


void FileOperation::openDropURLMenu(QWidget* parent, const KURL::List& urls, const KURL& target, bool* wasMoved) {
	QPopupMenu menu(parent);
	if (wasMoved) *wasMoved=false;

	int moveItemID = menu.insertItem( SmallIcon("goto"), i18n("&Move Here") );
	int copyItemID = menu.insertItem( SmallIcon("editcopy"), i18n("&Copy Here") );
	int linkItemID = menu.insertItem( SmallIcon("www"), i18n("&Link Here") );
	menu.insertSeparator();
	menu.insertItem( SmallIcon("cancel"), i18n("Cancel") );

	menu.setMouseTracking(true);
	int id = menu.exec(QCursor::pos());

	// Handle menu choice
	if (id==copyItemID) {
		KIO::copy(urls, target, true);
	} else if (id==moveItemID) {
		KIO::move(urls, target, true);
		if (wasMoved) *wasMoved=true;
	} else if (id==linkItemID) {
		KIO::link(urls, target, true);
	}
}


} // namespace
