/* This file is based on kfiledetailview.cpp from the KDE libs. Original
   copyright follows.
*/
/* This file is part of the KDE libraries
   Copyright (C) 1997 Stephan Kulow <coolo@kde.org>
                 2000, 2001 Carsten Pfeiffer <pfeiffer@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.	If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

// Qt includes
#include <qevent.h>
#include <qheader.h>
#include <qkeycode.h>
#include <qpainter.h>
#include <qpixmap.h>

// KDE includes
#include <kapplication.h>
#include <kdebug.h>
#include <kfileitem.h>
#include <kglobalsettings.h>
#include <kicontheme.h>
#include <klocale.h>
#include <kurldrag.h>

// Our includes
#include "gvfiledetailviewitem.h"
#include "gvfiledetailview.moc"


GVFileDetailView::GVFileDetailView(QWidget *parent, const char *name)
	: KListView(parent, name), GVFileView(), mViewedItem(0L)
{
	mSortingCol = COL_NAME;
	mBlockSortingSignal = false;

	addColumn( i18n( "Name" ) );
	addColumn( i18n( "Size" ) );
	addColumn( i18n( "Date" ) );
	addColumn( i18n( "Permissions" ) );
	addColumn( i18n( "Owner" ) );
	addColumn( i18n( "Group" ) );
	setShowSortIndicator( TRUE );
	setAllColumnsShowFocus( TRUE );

	connect( header(), SIGNAL( sectionClicked(int)),
			 SLOT(slotSortingChanged(int) ));


	connect( this, SIGNAL( returnPressed(QListViewItem *) ),
			 SLOT( slotActivate( QListViewItem *) ) );

	connect( this, SIGNAL( clicked(QListViewItem *, const QPoint&, int)),
			 SLOT( selected( QListViewItem *) ) );
	connect( this, SIGNAL( doubleClicked(QListViewItem *, const QPoint&, int)),
			 SLOT( slotActivate( QListViewItem *) ) );

	connect( this, SIGNAL(contextMenuRequested( QListViewItem *,
												const QPoint &, int )),
			 this, SLOT( slotActivateMenu( QListViewItem *, const QPoint& )));

	// FIXME: Do not start a multi selection when going to previous or next
	// image
	QListView::setSelectionMode( QListView::Extended );
	connect( this, SIGNAL( selectionChanged() ),
			 SLOT( slotSelectionChanged() ));

	setSorting( sorting() );


	mResolver =
		new KMimeTypeResolver<GVFileDetailViewItem,GVFileDetailView>( this );

	setDragEnabled(true);
}


GVFileDetailView::~GVFileDetailView()
{
	delete mResolver;
}


void GVFileDetailView::setSelected( const KFileItem *info, bool enable )
{
	if ( !info ) return;

	// we can only hope that this casts works
	GVFileDetailViewItem *item = (GVFileDetailViewItem*)info->extraData( this );

	if ( item ) KListView::setSelected( item, enable );
}

void GVFileDetailView::setCurrentItem( const KFileItem *item )
{
	if ( !item ) return;
	GVFileDetailViewItem *it = (GVFileDetailViewItem*) item->extraData( this );
	if ( it ) KListView::setCurrentItem( it );
}

KFileItem * GVFileDetailView::currentFileItem() const
{
	GVFileDetailViewItem *current = static_cast<GVFileDetailViewItem*>( currentItem() );
	if ( current ) return current->fileInfo();

	return 0L;
}

void GVFileDetailView::clearSelection()
{
	KListView::clearSelection();
}

void GVFileDetailView::selectAll()
{
	KListView::selectAll( true );
}

void GVFileDetailView::invertSelection()
{
	KListView::invertSelection();
}

void GVFileDetailView::slotActivateMenu (QListViewItem *item,const QPoint& pos )
{
	if ( !item ) {
		sig->activateMenu( 0, pos );
		return;
	}
	GVFileDetailViewItem *i = (GVFileDetailViewItem*) item;
	sig->activateMenu( i->fileInfo(), pos );
}

void GVFileDetailView::clearView()
{
	mResolver->m_lstPendingMimeIconItems.clear();
	mViewedItem=0L;
	KListView::clear();
}

void GVFileDetailView::insertItem( KFileItem *i )
{
	KFileView::insertItem( i );

	GVFileDetailViewItem *item = new GVFileDetailViewItem( (QListView*) this, i );

	setSortingKey( item, i );

	i->setExtraData( this, item );

	if ( !i->isMimeTypeKnown() )
		mResolver->m_lstPendingMimeIconItems.append( item );
}

void GVFileDetailView::slotActivate( QListViewItem *item )
{
	if ( !item ) return;

	const KFileItem *fi = ( (GVFileDetailViewItem*)item )->fileInfo();
	if ( fi ) sig->activate( fi );
}

void GVFileDetailView::selected( QListViewItem *item )
{
	if ( !item ) return;

	if ( KGlobalSettings::singleClick() ) {
		const KFileItem *fi = ( (GVFileDetailViewItem*)item )->fileInfo();
		if ( fi && (fi->isDir() || !onlyDoubleClickSelectsFiles()) )
			sig->activate( fi );
	}
}

void GVFileDetailView::highlighted( QListViewItem *item )
{
	if ( !item ) return;

	const KFileItem *fi = ( (GVFileDetailViewItem*)item )->fileInfo();
	if ( fi ) sig->highlightFile( fi );
}


bool GVFileDetailView::isSelected( const KFileItem *i ) const
{
	if ( !i ) return false;

	GVFileDetailViewItem *item = (GVFileDetailViewItem*) i->extraData( this );
	return (item && item->isSelected());
}


void GVFileDetailView::updateView( bool b )
{
	if ( !b ) return;

	QListViewItemIterator it( (QListView*)this );
	for ( ; it.current(); ++it ) {
		GVFileDetailViewItem *item=static_cast<GVFileDetailViewItem *>(it.current());
		item->setPixmap( 0, item->fileInfo()->pixmap(KIcon::SizeSmall) );
	}
}

void GVFileDetailView::updateView( const KFileItem *i )
{
	if ( !i ) return;

	GVFileDetailViewItem *item = (GVFileDetailViewItem*) i->extraData( this );
	if ( !item ) return;

	item->init();
	setSortingKey( item, i );

	//item->repaint(); // only repaints if visible
}

void GVFileDetailView::setSortingKey( GVFileDetailViewItem *item,
									 const KFileItem *i )
{
	// see also setSorting()
	QDir::SortSpec spec = KFileView::sorting();

	if ( spec & QDir::Time )
		item->setKey( sortingKey( i->time( KIO::UDS_MODIFICATION_TIME ),
								  i->isDir(), spec ));
	else if ( spec & QDir::Size )
		item->setKey( sortingKey( i->size(), i->isDir(), spec ));

	else // Name or Unsorted
		item->setKey( sortingKey( i->text(), i->isDir(), spec ));
}


void GVFileDetailView::removeItem( const KFileItem *i )
{
	if ( !i ) return;

	GVFileDetailViewItem *item = (GVFileDetailViewItem*) i->extraData( this );
	mResolver->m_lstPendingMimeIconItems.remove( item );
	if(mViewedItem==item) mViewedItem=0L;
	delete item;

	KFileView::removeItem( i );
}

void GVFileDetailView::slotSortingChanged( int col )
{
	QDir::SortSpec sort = sorting();
	int sortSpec = -1;
	bool reversed = col == mSortingCol && (sort & QDir::Reversed) == 0;
	mSortingCol = col;

	switch( col ) {
	case COL_NAME:
		sortSpec = (sort & ~QDir::SortByMask | QDir::Name);
		break;
	case COL_SIZE:
		sortSpec = (sort & ~QDir::SortByMask | QDir::Size);
		break;
	case COL_DATE:
		sortSpec = (sort & ~QDir::SortByMask | QDir::Time);
		break;

	// the following columns have no equivalent in QDir, so we set it
	// to QDir::Unsorted and remember the column (mSortingCol)
	case COL_OWNER:
	case COL_GROUP:
	case COL_PERM:
		// grmbl, QDir::Unsorted == SortByMask.
		sortSpec = (sort & ~QDir::SortByMask);// | QDir::Unsorted;
		break;
	default:
		break;
	}

	if ( reversed )
		sortSpec |= QDir::Reversed;
	else
		sortSpec &= ~QDir::Reversed;

	if ( sort & QDir::IgnoreCase )
		sortSpec |= QDir::IgnoreCase;
	else
		sortSpec &= ~QDir::IgnoreCase;


	KFileView::setSorting( static_cast<QDir::SortSpec>( sortSpec ) );

	KFileItem *item;
	KFileItemListIterator it( *items() );

	if ( sortSpec & QDir::Time ) {
		for ( ; (item = it.current()); ++it )
			viewItem(item)->setKey( sortingKey( item->time( KIO::UDS_MODIFICATION_TIME ), item->isDir(), sortSpec ));
	}

	else if ( sortSpec & QDir::Size ) {
		for ( ; (item = it.current()); ++it )
			viewItem(item)->setKey( sortingKey( item->size(), item->isDir(),
												sortSpec ));
	}
	else { // Name or Unsorted -> use column text
		for ( ; (item = it.current()); ++it ) {
			GVFileDetailViewItem *i = viewItem( item );
			i->setKey( sortingKey( i->text(mSortingCol), item->isDir(),
								   sortSpec ));
		}
	}

	KListView::setSorting( mSortingCol, !reversed );
	KListView::sort();

	if ( !mBlockSortingSignal ) sig->changeSorting( static_cast<QDir::SortSpec>( sortSpec ) );
}


void GVFileDetailView::setSorting( QDir::SortSpec spec )
{
	int col = 0;
	if ( spec & QDir::Time )
		col = COL_DATE;
	else if ( spec & QDir::Size )
		col = COL_SIZE;
	else if ( spec & QDir::Unsorted )
		col = mSortingCol;
	else
		col = COL_NAME;

	// inversed, because slotSortingChanged will reverse it
	if ( spec & QDir::Reversed )
		spec = (QDir::SortSpec) (spec & ~QDir::Reversed);
	else
		spec = (QDir::SortSpec) (spec | QDir::Reversed);

	mSortingCol = col;
	KFileView::setSorting( (QDir::SortSpec) spec );


	// don't emit sortingChanged() when called via setSorting()
	mBlockSortingSignal = true; // can't use blockSignals()
	slotSortingChanged( col );
	mBlockSortingSignal = false;
}

void GVFileDetailView::ensureItemVisible( const KFileItem *i )
{
	if ( !i ) return;

	GVFileDetailViewItem *item = (GVFileDetailViewItem*) i->extraData( this );
		
	if ( item ) KListView::ensureItemVisible( item );
}

// we're in multiselection mode
void GVFileDetailView::slotSelectionChanged()
{
	sig->highlightFile( 0L );
}

KFileItem * GVFileDetailView::firstFileItem() const
{
	GVFileDetailViewItem *item = static_cast<GVFileDetailViewItem*>( firstChild() );
	if ( item ) return item->fileInfo();
	return 0L;
}

KFileItem * GVFileDetailView::nextItem( const KFileItem *fileItem ) const
{
	if ( fileItem ) {
		GVFileDetailViewItem *item = viewItem( fileItem );
		if ( item && item->itemBelow() )
			return ((GVFileDetailViewItem*) item->itemBelow())->fileInfo();
		else
			return 0L;
	}
	else
		return firstFileItem();
}

KFileItem * GVFileDetailView::prevItem( const KFileItem *fileItem ) const
{
	if ( fileItem ) {
		GVFileDetailViewItem *item = viewItem( fileItem );
		if ( item && item->itemAbove() )
			return ((GVFileDetailViewItem*) item->itemAbove())->fileInfo();
		else
			return 0L;
	}
	else
		return firstFileItem();
}

void GVFileDetailView::keyPressEvent( QKeyEvent *e )
{
	KListView::keyPressEvent( e );

	if ( e->key() == Key_Return || e->key() == Key_Enter ) {
		if ( e->state() & ControlButton )
			e->ignore();
		else
			e->accept();
	}
}

//
// mimetype determination on demand
//
void GVFileDetailView::mimeTypeDeterminationFinished()
{
	// anything to do?
}

void GVFileDetailView::determineIcon( GVFileDetailViewItem *item )
{
	(void) item->fileInfo()->determineMimeType();
	updateView( item->fileInfo() );
}

void GVFileDetailView::listingCompleted()
{
	mResolver->start();
}

void GVFileDetailView::startDrag()
{
	// FIXME: Implement multi-selection
	KFileItem* item=KFileView::selectedItems()->getFirst();
	if (!item) {
		kdWarning() << "No item to drag\n";
		return;
	}

	KURL::List urls;
	urls.append(item->url());

	QDragObject* drag = KURLDrag::newDrag( urls, this );
	drag->setPixmap( item->pixmap(0) );

	drag->dragCopy();
}


void GVFileDetailView::setViewedFileItem(const KFileItem* fileItem)
{
	GVFileDetailViewItem* oldViewed=mViewedItem;
	mViewedItem=viewItem(fileItem);
	if (oldViewed) oldViewed->repaint();
	if (mViewedItem) mViewedItem->repaint();
}
