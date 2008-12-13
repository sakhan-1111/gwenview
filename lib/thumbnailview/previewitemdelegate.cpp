// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <aurelien.gateau@free.fr>

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
#include "previewitemdelegate.moc"
#include <config-gwenview.h>

// Qt
#include <QHash>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QStylePainter>
#include <QToolButton>
#include <QToolTip>

// KDE
#include <kdebug.h>
#include <kdirmodel.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kurl.h>
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
#include <nepomuk/kratingpainter.h>
#endif

// Local
#include "archiveutils.h"
#include "paintutils.h"
#include "thumbnailview.h"
#include "timeutils.h"
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
#include "../semanticinfo/semanticinfodirmodel.h"
#endif

// Define this to be able to fine tune the rendering of the selection
// background through a config file
//#define FINETUNE_SELECTION_BACKGROUND
#ifdef FINETUNE_SELECTION_BACKGROUND
#include <QDir>
#include <QSettings>
#endif

namespace Gwenview {

/**
 * Space between the item outer rect and the content, and between the
 * thumbnail and the caption
 */
const int ITEM_MARGIN = 5;

/** How darker is the border line around selection */
const int SELECTION_BORDER_DARKNESS = 140;

/** Radius of the selection rounded corners, in pixels */
const int SELECTION_RADIUS = 5;

/** Border around gadget icons */
const int GADGET_MARGIN = 1;

/** Radius of the gadget frame, in pixels */
const int GADGET_RADIUS = 12;

/** How lighter is the border line around gadgets */
const int GADGET_BORDER_LIGHTNESS = 300;

/** How dark is the shadow, 0 is invisible, 255 is as dark as possible */
const int SHADOW_STRENGTH = 128;

/** How many pixels around the thumbnail are shadowed */
const int SHADOW_SIZE = 4;


static KFileItem fileItemForIndex(const QModelIndex& index) {
	Q_ASSERT(index.isValid());
	QVariant data = index.data(KDirModel::FileItemRole);
	return qvariant_cast<KFileItem>(data);
}


static KUrl urlForIndex(const QModelIndex& index) {
	KFileItem item = fileItemForIndex(index);
	return item.url();
}


class RoundButton : public QToolButton {
public:
protected:
	virtual void paintEvent(QPaintEvent*) {
		QStylePainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		QStyleOptionToolButton opt;
		initStyleOption(&opt);
		if (opt.state & QStyle::State_MouseOver && parentWidget()) {
			QColor color = parentWidget()->palette().color(parentWidget()->backgroundRole());
			QColor borderColor = color.light(GADGET_BORDER_LIGHTNESS);

			const QRectF rectF = QRectF(opt.rect).adjusted(0.5, 0.5, -0.5, -0.5);
			const QPainterPath path = PaintUtils::roundedRectangle(rectF, height() / 2);

			if (opt.state & QStyle::State_Sunken) {
				painter.fillPath(path, color);
			}

			painter.setPen(borderColor);
			painter.drawPath(path);
		}
		painter.drawControl(QStyle::CE_ToolButtonLabel, opt);
	}
};


/**
 * A frame with a rounded semi-opaque background. Since it's not possible (yet)
 * to define non-opaque colors in Qt stylesheets, we do it the old way: by
 * reimplementing paintEvent().
 * FIXME: Found out it is possible in fact, code should be updated.
 */
class GlossyFrame : public QFrame {
public:
	GlossyFrame(QWidget* parent = 0)
	: QFrame(parent)
	{}

	void setBackgroundColor(const QColor& color) {
		QPalette pal = palette();
		pal.setColor(backgroundRole(), color);
		setPalette(pal);
	}

protected:
	virtual void paintEvent(QPaintEvent* /*event*/) {
		QColor color = palette().color(backgroundRole());
		QColor borderColor = color.light(GADGET_BORDER_LIGHTNESS);
		QRectF rectF = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		QPainterPath path = PaintUtils::roundedRectangle(rectF, height() / 2);

		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);

		painter.fillPath(path, PaintUtils::alphaAdjustedF(color, 0.8));

		painter.setPen(borderColor);
		painter.drawPath(path);
	}

private:
	bool mOpaque;
};


static QToolButton* createFrameButton(const char* iconName) {
	int size = KIconLoader::global()->currentSize(KIconLoader::Small);
	QToolButton* button = new RoundButton;
	button->setIcon(SmallIcon(iconName));
	button->setIconSize(QSize(size, size));
	button->setAutoRaise(true);

	return button;
}


struct PreviewItemDelegatePrivate {
	/**
	 * Maps full text to elided text.
	 */
	mutable QHash<QString, QString> mElidedTextCache;

	// Key is height * 1000 + width
	typedef QHash<int, QPixmap> ShadowCache;
	mutable ShadowCache mShadowCache;

	PreviewItemDelegate* that;
	ThumbnailView* mView;
	GlossyFrame* mButtonFrame;
	GlossyFrame* mSaveButtonFrame;
	QPixmap mSaveButtonFramePixmap;

	QToolButton* mToggleSelectionButton;
	QToolButton* mFullScreenButton;
	QToolButton* mRotateLeftButton;
	QToolButton* mRotateRightButton;
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
	KRatingPainter mRatingPainter;
#endif

	QModelIndex mIndexUnderCursor;
	int mThumbnailSize;
	PreviewItemDelegate::ThumbnailDetails mDetails;

	QLabel* mTipLabel;

	void initSaveButtonFramePixmap() {
		// Necessary otherwise we won't see the save button itself
		mSaveButtonFrame->adjustSize();

		// This is hackish.
		// Show/hide the frame to make sure mSaveButtonFrame->render produces
		// something coherent.
		mSaveButtonFrame->show();
		mSaveButtonFrame->repaint();
		mSaveButtonFrame->hide();

		mSaveButtonFramePixmap = QPixmap(mSaveButtonFrame->size());
		mSaveButtonFramePixmap.fill(Qt::transparent);
		mSaveButtonFrame->render(&mSaveButtonFramePixmap, QPoint(), QRegion(), QWidget::DrawChildren);
	}


	void initTipLabel() {
		mTipLabel = new QLabel(mView);
		mTipLabel->setAutoFillBackground(true);
		mTipLabel->setFrameStyle(QFrame::Box | QFrame::Plain);
		mTipLabel->setPalette(QToolTip::palette());
		mTipLabel->hide();
	}


	bool hoverEventFilter(QHoverEvent* event) {
		QModelIndex index = mView->indexAt(event->pos());
		if (mIndexUnderCursor.isValid()) {
			mView->update(mIndexUnderCursor);
		}
		if (index == mIndexUnderCursor) {
			// Same index, nothing to do
			return false;
		}
		mIndexUnderCursor = index;

		if (mIndexUnderCursor.isValid()) {
			that->updateButtonFrameOpacity();
			updateToggleSelectionButton();
			updateImageButtons();

			mButtonFrame->adjustSize();
			QRect rect = mView->visualRect(mIndexUnderCursor);
			int posX = rect.x() + (rect.width() - mButtonFrame->width()) / 2;
			int posY = rect.y() + GADGET_MARGIN;
			mButtonFrame->move(posX, posY);

			mButtonFrame->show();

			if (mView->isModified(mIndexUnderCursor)) {
				showSaveButtonFrame(rect);
			} else {
				mSaveButtonFrame->hide();
			}

			showToolTip(index);

		} else {
			mButtonFrame->hide();
			mSaveButtonFrame->hide();
			mTipLabel->hide();
		}
		return false;
	}

	QRect ratingRectFromIndexRect(const QRect& rect) const {
		return QRect(
			rect.left(),
			rect.bottom() - ratingRowHeight() - ITEM_MARGIN,
			rect.width(),
			ratingRowHeight());
	}

#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
	int ratingFromCursorPosition(const QRect& ratingRect) const {
		const QPoint pos = mView->viewport()->mapFromGlobal(QCursor::pos());
		return mRatingPainter.ratingFromPosition(ratingRect, pos);
	}
#endif

	bool mouseReleaseEventFilter() {
	#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		const QRect rect = ratingRectFromIndexRect(mView->visualRect(mIndexUnderCursor));
		const int rating = ratingFromCursorPosition(rect);
		if (rating == -1) {
			return false;
		}
		that->setDocumentRatingRequested(urlForIndex(mIndexUnderCursor) , rating);
		return true;
	#else
		return false;
	#endif
	}


	QPoint saveButtonFramePosition(const QRect& itemRect) const {
		QSize frameSize = mSaveButtonFrame->sizeHint();
		int textHeight = mView->fontMetrics().height();
		int posX = itemRect.right() - GADGET_MARGIN - frameSize.width();
		int posY = itemRect.bottom() - GADGET_MARGIN - textHeight - frameSize.height();

		return QPoint(posX, posY);
	}


	void showSaveButtonFrame(const QRect& itemRect) const {
		mSaveButtonFrame->move(saveButtonFramePosition(itemRect));
		mSaveButtonFrame->show();
	}


	void drawBackground(QPainter* painter, const QRect& rect, const QColor& bgColor, const QColor& borderColor) const {
		int bgH, bgS, bgV;
		int borderH, borderS, borderV, borderMargin;
	#ifdef FINETUNE_SELECTION_BACKGROUND
		QSettings settings(QDir::homePath() + "/colors.ini", QSettings::IniFormat);
		bgH = settings.value("bg/h").toInt();
		bgS = settings.value("bg/s").toInt();
		bgV = settings.value("bg/v").toInt();
		borderH = settings.value("border/h").toInt();
		borderS = settings.value("border/s").toInt();
		borderV = settings.value("border/v").toInt();
		borderMargin = settings.value("border/margin").toInt();
	#else
		bgH = 0;
		bgS = -20;
		bgV = 43;
		borderH = 0;
		borderS = -100;
		borderV = 60;
		borderMargin = 1;
	#endif
		painter->setRenderHint(QPainter::Antialiasing);

		QRectF rectF = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);

		QPainterPath path = PaintUtils::roundedRectangle(rectF, SELECTION_RADIUS);

		QLinearGradient gradient(rectF.topLeft(), rectF.bottomLeft());
		gradient.setColorAt(0, PaintUtils::adjustedHsv(bgColor, bgH, bgS, bgV));
		gradient.setColorAt(1, bgColor);
		painter->fillPath(path, gradient);

		painter->setPen(borderColor);
		painter->drawPath(path);

		painter->setPen(PaintUtils::adjustedHsv(borderColor, borderH, borderS, borderV));
		rectF = rectF.adjusted(borderMargin, borderMargin, -borderMargin, -borderMargin);
		path = PaintUtils::roundedRectangle(rectF, SELECTION_RADIUS);
		painter->drawPath(path);
	}


	void drawShadow(QPainter* painter, const QRect& rect) const {
		const QPoint shadowOffset(-SHADOW_SIZE, -SHADOW_SIZE + 1);

		int key = rect.height() * 1000 + rect.width();

		ShadowCache::Iterator it = mShadowCache.find(key);
		if (it == mShadowCache.end()) {
			QSize size = QSize(rect.width() + 2*SHADOW_SIZE, rect.height() + 2*SHADOW_SIZE);
			QColor color(0, 0, 0, SHADOW_STRENGTH);
			QPixmap shadow = PaintUtils::generateFuzzyRect(size, color, SHADOW_SIZE);
			it = mShadowCache.insert(key, shadow);
		}
		painter->drawPixmap(rect.topLeft() + shadowOffset, it.value());
	}


	void drawText(QPainter* painter, const QRect& rect, const QColor& fgColor, const QString& fullText) const {
		QFontMetrics fm = mView->fontMetrics();

		// Elide text
		QString text;
		QHash<QString, QString>::const_iterator it = mElidedTextCache.constFind(fullText);
		if (it == mElidedTextCache.constEnd()) {
			text = fm.elidedText(fullText, Qt::ElideRight, rect.width());
			mElidedTextCache[fullText] = text;
		} else {
			text = it.value();
		}

		// Compute x pos
		int posX;
		if (text.length() == fullText.length()) {
			// Not elided, center text
			posX = (rect.width() - fm.width(text)) / 2;
		} else {
			// Elided, left align
			posX = 0;
		}

		// Draw text
		painter->setPen(fgColor);
		painter->drawText(rect.left() + posX, rect.top() + fm.ascent(), text);
	}


	void drawRating(QPainter* painter, const QRect& rect, const QVariant& value) {
	#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		const int rating = value.toInt();
		const QRect ratingRect = ratingRectFromIndexRect(rect);
		const int hoverRating = ratingFromCursorPosition(ratingRect);
		mRatingPainter.paint(painter, ratingRect, rating, hoverRating);
	#endif
	}


	bool isTextElided(const QString& text) const {
		QHash<QString, QString>::const_iterator it = mElidedTextCache.constFind(text);
		if (it == mElidedTextCache.constEnd()) {
			return false;
		}
		return it.value().length() < text.length();
	}


	/**
	 * Show a tooltip only if the item has been elided.
	 * This function places the tooltip over the item text.
	 */
	void showToolTip(const QModelIndex& index) {
		if (mDetails == 0 || mDetails == PreviewItemDelegate::RatingDetail) {
			// No text to display
			return;
		}

		// Gather tip text
		QStringList textList;
		bool elided = false;
		if (mDetails & PreviewItemDelegate::FileNameDetail) {
			const QString text = index.data().toString();
			elided |= isTextElided(text);
			textList << text;
		}
		if (mDetails & PreviewItemDelegate::DateDetail) {
			// FIXME: Duplicated from drawText
			const KFileItem fileItem = fileItemForIndex(index);
			const KDateTime dt = TimeUtils::dateTimeForFileItem(fileItem);
			const QString text = KGlobal::locale()->formatDateTime(dt);
			elided |= isTextElided(text);
			textList << text;
		}
		if (!elided) {
			return;
		}
		mTipLabel->setText(textList.join("\n"));

		// Compute tip position
		QRect rect = mView->visualRect(index);
		const int textX = ITEM_MARGIN;
		const int textY = ITEM_MARGIN + mThumbnailSize + ITEM_MARGIN;
		const int margin = mTipLabel->frameWidth();
		const QPoint tipPosition = rect.topLeft() + QPoint(textX - margin, textY - margin);

		// Show tip
		mTipLabel->adjustSize();
		mTipLabel->move(tipPosition);
		mTipLabel->show();
	}

	int itemWidth() const {
		return mThumbnailSize + 2 * ITEM_MARGIN;
	}

	int ratingRowHeight() const {
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		return mView->fontMetrics().ascent();
#endif
		return 0;
	}

	int itemHeight() const {
		const int lineHeight = mView->fontMetrics().height();
		int textHeight = 0;
		if (mDetails & PreviewItemDelegate::FileNameDetail) {
			textHeight += lineHeight;
		}
		if (mDetails & PreviewItemDelegate::DateDetail) {
			textHeight += lineHeight;
		}
		if (mDetails & PreviewItemDelegate::RatingDetail) {
			textHeight += ratingRowHeight();
		}
		if (textHeight == 0) {
			// Keep at least one row of text, so that we can show folder names
			textHeight = lineHeight;
		}
		return mThumbnailSize + textHeight + 3*ITEM_MARGIN;
	}

	void selectIndexUnderCursorIfNoMultiSelection() {
		if (mView->selectionModel()->selectedIndexes().size() <= 1) {
			mView->setCurrentIndex(mIndexUnderCursor);
		}
	}

	void updateToggleSelectionButton() {
		mToggleSelectionButton->setIcon(SmallIcon(
			mView->selectionModel()->isSelected(mIndexUnderCursor) ? "list-remove" : "list-add"
			));
	}

	void updateImageButtons() {
		const KFileItem item = fileItemForIndex(mIndexUnderCursor);
		const bool isImage = !ArchiveUtils::fileItemIsDirOrArchive(item);
		mFullScreenButton->setEnabled(isImage);
		mRotateLeftButton->setEnabled(isImage);
		mRotateRightButton->setEnabled(isImage);
	}
};


PreviewItemDelegate::PreviewItemDelegate(ThumbnailView* view)
: QAbstractItemDelegate(view)
, d(new PreviewItemDelegatePrivate) {
	d->that = this;
	d->mView = view;
	view->viewport()->installEventFilter(this);
	d->mThumbnailSize = view->thumbnailSize();
	d->mDetails = FileNameDetail;

#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
	d->mRatingPainter.setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
	d->mRatingPainter.setLayoutDirection(view->layoutDirection());
	d->mRatingPainter.setMaxRating(10);
#endif

	connect(view, SIGNAL(thumbnailSizeChanged(int)),
		SLOT(setThumbnailSize(int)) );
	connect(view, SIGNAL(selectionChangedSignal(const QItemSelection&, const QItemSelection&)),
		SLOT(updateButtonFrameOpacity()) );

	QColor bgColor = d->mView->palette().highlight().color();
	QColor borderColor = bgColor.dark(SELECTION_BORDER_DARKNESS);

	QString styleSheet =
		"QFrame {"
		"	padding: 1px;"
		"}"

		"QToolButton {"
		"	padding: 2px;"
		"	border-radius: 4px;"
		"}"

		"QToolButton:hover {"
		"	border: 1px solid %2;"
		"}"

		"QToolButton:pressed {"
		"	background-color:"
		"		qlineargradient(x1:0, y1:0, x2:0, y2:1,"
		"		stop:0 %2, stop:1 %1);"
		"	border: 1px solid %2;"
		"}";
	styleSheet = styleSheet.arg(bgColor.name()).arg(borderColor.name());

	// Button frame
	d->mButtonFrame = new GlossyFrame(d->mView->viewport());
	//d->mButtonFrame->setStyleSheet(styleSheet);
	//d->mButtonFrame->setBackgroundColor(bgColor);
	d->mButtonFrame->setBackgroundColor(QColor(20, 20, 20));
	d->mButtonFrame->hide();

	d->mToggleSelectionButton = createFrameButton("list-add");
	connect(d->mToggleSelectionButton, SIGNAL(clicked()),
		SLOT(slotToggleSelectionClicked()));

	d->mFullScreenButton = createFrameButton("view-fullscreen");
	connect(d->mFullScreenButton, SIGNAL(clicked()),
		SLOT(slotFullScreenClicked()) );

	d->mRotateLeftButton = createFrameButton("object-rotate-left");
	connect(d->mRotateLeftButton, SIGNAL(clicked()),
		SLOT(slotRotateLeftClicked()) );

	d->mRotateRightButton = createFrameButton("object-rotate-right");
	connect(d->mRotateRightButton, SIGNAL(clicked()),
		SLOT(slotRotateRightClicked()) );

	QHBoxLayout* layout = new QHBoxLayout(d->mButtonFrame);
	layout->setMargin(2);
	layout->setSpacing(2);
	layout->addWidget(d->mToggleSelectionButton);
	layout->addWidget(d->mFullScreenButton);
	layout->addWidget(d->mRotateLeftButton);
	layout->addWidget(d->mRotateRightButton);

	// Save button frame
	d->mSaveButtonFrame = new GlossyFrame(d->mView->viewport());
	d->mSaveButtonFrame->setStyleSheet(styleSheet);
	d->mSaveButtonFrame->setBackgroundColor(bgColor);
	d->mSaveButtonFrame->hide();

	QToolButton* saveButton = createFrameButton("document-save");
	connect(saveButton, SIGNAL(clicked()),
		SLOT(slotSaveClicked()) );

	layout = new QHBoxLayout(d->mSaveButtonFrame);
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(saveButton);

	d->initSaveButtonFramePixmap();
	d->initTipLabel();
}


PreviewItemDelegate::~PreviewItemDelegate() {
	delete d;
}


QSize PreviewItemDelegate::sizeHint( const QStyleOptionViewItem & /*option*/, const QModelIndex & /*index*/ ) const {
	return QSize( d->itemWidth(), d->itemHeight() );
}


bool PreviewItemDelegate::eventFilter(QObject*, QEvent* event) {
	switch (event->type()) {
	case QEvent::ToolTip:
		return true;

	case QEvent::HoverMove:
		return d->hoverEventFilter(static_cast<QHoverEvent*>(event));

	case QEvent::MouseButtonRelease:
		return d->mouseReleaseEventFilter();

	default:
		return false;
	}
}


void PreviewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const {
	int thumbnailSize = d->mThumbnailSize;
	QPixmap thumbnailPix = d->mView->thumbnailForIndex(index);
	const KFileItem fileItem = fileItemForIndex(index);
	const bool opaque = !thumbnailPix.hasAlphaChannel();
	const bool isDirOrArchive = ArchiveUtils::fileItemIsDirOrArchive(fileItemForIndex(index));
	QRect rect = option.rect;

#ifdef DEBUG_RECT
	painter->setPen(Qt::red);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(rect);
#endif

	// Select color group
	QPalette::ColorGroup cg;

	if ( (option.state & QStyle::State_Enabled) && (option.state & QStyle::State_Active) ) {
		cg = QPalette::Normal;
	} else if ( (option.state & QStyle::State_Enabled)) {
		cg = QPalette::Inactive;
	} else {
		cg = QPalette::Disabled;
	}

	// Select colors
	QColor bgColor, borderColor, fgColor;
	if (option.state & QStyle::State_Selected) {
		bgColor = option.palette.color(cg, QPalette::Highlight);
		borderColor = bgColor.dark(SELECTION_BORDER_DARKNESS);
		fgColor = option.palette.color(cg, QPalette::HighlightedText);
	} else {
		QWidget* viewport = d->mView->viewport();
		bgColor = viewport->palette().color(viewport->backgroundRole());
		fgColor = viewport->palette().color(viewport->foregroundRole());

		if (bgColor.value() < 128) {
			borderColor = bgColor.dark(200);
		} else {
			borderColor = bgColor.light(200);
		}
	}

	// Compute thumbnailRect
	QRect thumbnailRect = QRect(
		rect.left() + (rect.width() - thumbnailPix.width())/2,
		rect.top() + (thumbnailSize - thumbnailPix.height()) + ITEM_MARGIN,
		thumbnailPix.width(),
		thumbnailPix.height());

	// Draw background
	if (option.state & QStyle::State_Selected) {
		const int radius = ITEM_MARGIN;
		d->drawBackground(painter, thumbnailRect.adjusted(-radius, -radius, radius, radius), bgColor, borderColor);
	}

	// Draw thumbnail
	if (!thumbnailPix.isNull()) {
		if (!(option.state & QStyle::State_Selected) && opaque) {
			d->drawShadow(painter, thumbnailRect);
		}

		if (opaque) {
			painter->setPen(borderColor);
			painter->setRenderHint(QPainter::Antialiasing, false);
			QRect borderRect = thumbnailRect.adjusted(-1, -1, 0, 0);
			painter->drawRect(borderRect);
		}
		painter->drawPixmap(thumbnailRect.left(), thumbnailRect.top(), thumbnailPix);
	}

	// Draw modified indicator
	bool isModified = d->mView->isModified(index);
	if (isModified) {
		// Draws a pixmap of the save button frame, as an indicator that
		// the image has been modified
		QPoint framePosition = d->saveButtonFramePosition(rect);
		painter->drawPixmap(framePosition, d->mSaveButtonFramePixmap);
	}

	if (index == d->mIndexUnderCursor) {
		if (isModified) {
			// If we just rotated the image with the buttons from the
			// button frame, we need to show the save button frame right now.
			d->showSaveButtonFrame(rect);
		} else {
			d->mSaveButtonFrame->hide();
		}
	}

	QRect textRect(
		rect.left() + ITEM_MARGIN,
		rect.top() + 2 * ITEM_MARGIN + thumbnailSize,
		rect.width() - 2 * ITEM_MARGIN,
		d->mView->fontMetrics().height());
	if (isDirOrArchive || (d->mDetails & PreviewItemDelegate::FileNameDetail)) {
		d->drawText(painter, textRect, fgColor, index.data().toString());
		textRect.moveTop(textRect.bottom());
	}

	if (!isDirOrArchive && (d->mDetails & PreviewItemDelegate::DateDetail)) {
		const KDateTime dt = TimeUtils::dateTimeForFileItem(fileItem);
		d->drawText(painter, textRect, fgColor, KGlobal::locale()->formatDateTime(dt));
	}

	if (!isDirOrArchive && (d->mDetails & PreviewItemDelegate::RatingDetail)) {
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		d->drawRating(painter, rect, index.data(SemanticInfoDirModel::RatingRole));
#endif
	}
}


void PreviewItemDelegate::updateButtonFrameOpacity() {
	//bool isSelected = d->mView->selectionModel()->isSelected(d->mIndexUnderCursor);
}


void PreviewItemDelegate::setThumbnailSize(int value) {
	d->mThumbnailSize = value;

	const int width = d->itemWidth();
	const int buttonWidth = d->mRotateRightButton->sizeHint().width();
	d->mRotateLeftButton->setVisible(width >= 3 * buttonWidth);
	d->mRotateRightButton->setVisible(width >= 4 * buttonWidth);
	d->mButtonFrame->adjustSize();

	d->mElidedTextCache.clear();
}


void PreviewItemDelegate::slotSaveClicked() {
	saveDocumentRequested(urlForIndex(d->mIndexUnderCursor));
}


void PreviewItemDelegate::slotRotateLeftClicked() {
	d->selectIndexUnderCursorIfNoMultiSelection();
	rotateDocumentLeftRequested(urlForIndex(d->mIndexUnderCursor));
}


void PreviewItemDelegate::slotRotateRightClicked() {
	d->selectIndexUnderCursorIfNoMultiSelection();
	rotateDocumentRightRequested(urlForIndex(d->mIndexUnderCursor));
}


void PreviewItemDelegate::slotFullScreenClicked() {
	showDocumentInFullScreenRequested(urlForIndex(d->mIndexUnderCursor));
}


void PreviewItemDelegate::slotToggleSelectionClicked() {
	d->mView->selectionModel()->select(d->mIndexUnderCursor, QItemSelectionModel::Toggle);
	d->updateToggleSelectionButton();
}


PreviewItemDelegate::ThumbnailDetails PreviewItemDelegate::thumbnailDetails() const {
	return d->mDetails;
}


void PreviewItemDelegate::setThumbnailDetails(PreviewItemDelegate::ThumbnailDetails details) {
	d->mDetails = details;
	d->mView->scheduleDelayedItemsLayout();
}


} // namespace
