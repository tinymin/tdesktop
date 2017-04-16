/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "stickers/emoji_panel.h"

#include "stickers/emoji_list_widget.h"
#include "stickers/stickers_list_widget.h"
#include "stickers/gifs_list_widget.h"
#include "styles/style_stickers.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/widgets/scroll_area.h"
#include "stickers/stickers.h"
#include "storage/localstorage.h"
#include "lang.h"
#include "mainwindow.h"

namespace ChatHelpers {
namespace {

constexpr auto kSaveChosenTabTimeout = 1000;

} // namespace

class EmojiPanel::SlideAnimation : public Ui::RoundShadowAnimation {
public:
	enum class Direction {
		LeftToRight,
		RightToLeft,
	};
	void setFinalImages(Direction direction, QImage &&left, QImage &&right, QRect inner);

	void start();
	void paintFrame(QPainter &p, float64 dt, float64 opacity);

private:
	Direction _direction = Direction::LeftToRight;
	QPixmap _leftImage, _rightImage;
	int _width = 0;
	int _height = 0;
	int _innerLeft = 0;
	int _innerTop = 0;
	int _innerRight = 0;
	int _innerBottom = 0;
	int _innerWidth = 0;
	int _innerHeight = 0;

	int _painterInnerLeft = 0;
	int _painterInnerTop = 0;
	int _painterInnerWidth = 0;
	int _painterInnerBottom = 0;
	int _painterCategoriesTop = 0;
	int _painterInnerHeight = 0;
	int _painterInnerRight = 0;

	int _frameIntsPerLineAdd = 0;

};

void EmojiPanel::SlideAnimation::setFinalImages(Direction direction, QImage &&left, QImage &&right, QRect inner) {
	Expects(!started());
	_direction = direction;
	_leftImage = QPixmap::fromImage(std::move(left).convertToFormat(QImage::Format_ARGB32_Premultiplied), Qt::ColorOnly);
	_rightImage = QPixmap::fromImage(std::move(right).convertToFormat(QImage::Format_ARGB32_Premultiplied), Qt::ColorOnly);

	t_assert(!_leftImage.isNull());
	t_assert(!_rightImage.isNull());
	_width = _leftImage.width();
	_height = _rightImage.height();
	t_assert(!(_width % cIntRetinaFactor()));
	t_assert(!(_height % cIntRetinaFactor()));
	t_assert(_leftImage.devicePixelRatio() == _rightImage.devicePixelRatio());
	t_assert(_rightImage.width() == _width);
	t_assert(_rightImage.height() == _height);
	t_assert(QRect(0, 0, _width, _height).contains(inner));
	_innerLeft = inner.x();
	_innerTop = inner.y();
	_innerWidth = inner.width();
	_innerHeight = inner.height();
	t_assert(!(_innerLeft % cIntRetinaFactor()));
	t_assert(!(_innerTop % cIntRetinaFactor()));
	t_assert(!(_innerWidth % cIntRetinaFactor()));
	t_assert(!(_innerHeight % cIntRetinaFactor()));
	_innerRight = _innerLeft + _innerWidth;
	_innerBottom = _innerTop + _innerHeight;

	_painterInnerLeft = _innerLeft / cIntRetinaFactor();
	_painterInnerTop = _innerTop / cIntRetinaFactor();
	_painterInnerRight = _innerRight / cIntRetinaFactor();
	_painterInnerBottom = _innerBottom / cIntRetinaFactor();
	_painterInnerWidth = _innerWidth / cIntRetinaFactor();
	_painterInnerHeight = _innerHeight / cIntRetinaFactor();
	_painterCategoriesTop = _painterInnerBottom - st::emojiCategory.height;
}

void EmojiPanel::SlideAnimation::start() {
	t_assert(!_leftImage.isNull());
	t_assert(!_rightImage.isNull());
	RoundShadowAnimation::start(_width, _height, _leftImage.devicePixelRatio());
	auto checkCorner = [this](const Corner &corner) {
		if (!corner.valid()) return;
		t_assert(corner.width <= _innerWidth);
		t_assert(corner.height <= _innerHeight);
	};
	checkCorner(_topLeft);
	checkCorner(_topRight);
	checkCorner(_bottomLeft);
	checkCorner(_bottomRight);
	_frameIntsPerLineAdd = (_width - _innerWidth) + _frameIntsPerLineAdded;
}

void EmojiPanel::SlideAnimation::paintFrame(QPainter &p, float64 dt, float64 opacity) {
	t_assert(started());
	t_assert(dt >= 0.);

	_frameAlpha = anim::interpolate(1, 256, opacity);

	auto frameInts = _frameInts + _innerLeft + _innerTop * _frameIntsPerLine;

	auto leftToRight = (_direction == Direction::LeftToRight);

	auto easeOut = anim::easeOutCirc(1., dt);
	auto easeIn = anim::easeInCirc(1., dt);

	auto arrivingCoord = anim::interpolate(_innerWidth, 0, easeOut);
	auto departingCoord = anim::interpolate(0, _innerWidth, easeIn);
	if (auto decrease = (arrivingCoord % cIntRetinaFactor())) {
		arrivingCoord -= decrease;
	}
	if (auto decrease = (departingCoord % cIntRetinaFactor())) {
		departingCoord -= decrease;
	}
	auto arrivingAlpha = easeIn;
	auto departingAlpha = 1. - easeOut;
	auto leftCoord = (leftToRight ? arrivingCoord : departingCoord) * -1;
	auto leftAlpha = (leftToRight ? arrivingAlpha : departingAlpha);
	auto rightCoord = (leftToRight ? departingCoord : arrivingCoord);
	auto rightAlpha = (leftToRight ? departingAlpha : arrivingAlpha);

	// _innerLeft ..(left).. leftTo ..(both).. bothTo ..(none).. noneTo ..(right).. _innerRight
	auto leftTo = _innerLeft + snap(_innerWidth + leftCoord, 0, _innerWidth);
	auto rightFrom = _innerLeft + snap(rightCoord, 0, _innerWidth);
	auto painterRightFrom = rightFrom / cIntRetinaFactor();
	if (opacity < 1.) {
		_frame.fill(Qt::transparent);
	}
	{
		Painter p(&_frame);
		p.setOpacity(opacity);
		p.fillRect(_painterInnerLeft, _painterInnerTop, _painterInnerWidth, _painterCategoriesTop - _painterInnerTop, st::emojiPanBg);
		p.fillRect(_painterInnerLeft, _painterCategoriesTop, _painterInnerWidth, _painterInnerBottom - _painterCategoriesTop, st::emojiPanCategories);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		if (leftTo > _innerLeft) {
			p.setOpacity(opacity * leftAlpha);
			p.drawPixmap(_painterInnerLeft, _painterInnerTop, _leftImage, _innerLeft - leftCoord, _innerTop, leftTo - _innerLeft, _innerHeight);
		}
		if (rightFrom < _innerRight) {
			p.setOpacity(opacity * rightAlpha);
			p.drawPixmap(painterRightFrom, _painterInnerTop, _rightImage, _innerLeft, _innerTop, _innerRight - rightFrom, _innerHeight);
		}
	}

	// Draw corners
	//paintCorner(_topLeft, _innerLeft, _innerTop);
	//paintCorner(_topRight, _innerRight - _topRight.width, _innerTop);
	paintCorner(_bottomLeft, _innerLeft, _innerBottom - _bottomLeft.height);
	paintCorner(_bottomRight, _innerRight - _bottomRight.width, _innerBottom - _bottomRight.height);

	// Draw shadow upon the transparent
	auto outerLeft = _innerLeft;
	auto outerTop = _innerTop;
	auto outerRight = _innerRight;
	auto outerBottom = _innerBottom;
	if (_shadow.valid()) {
		outerLeft -= _shadow.extend.left();
		outerTop -= _shadow.extend.top();
		outerRight += _shadow.extend.right();
		outerBottom += _shadow.extend.bottom();
	}
	if (cIntRetinaFactor() > 1) {
		if (auto skipLeft = (outerLeft % cIntRetinaFactor())) {
			outerLeft -= skipLeft;
		}
		if (auto skipTop = (outerTop % cIntRetinaFactor())) {
			outerTop -= skipTop;
		}
		if (auto skipRight = (outerRight % cIntRetinaFactor())) {
			outerRight += (cIntRetinaFactor() - skipRight);
		}
		if (auto skipBottom = (outerBottom % cIntRetinaFactor())) {
			outerBottom += (cIntRetinaFactor() - skipBottom);
		}
	}

	if (opacity == 1.) {
		// Fill above the frame top with transparent.
		auto fillTopInts = (_frameInts + outerTop * _frameIntsPerLine + outerLeft);
		auto fillWidth = (outerRight - outerLeft) * sizeof(uint32);
		for (auto fillTop = _innerTop - outerTop; fillTop != 0; --fillTop) {
			memset(fillTopInts, 0, fillWidth);
			fillTopInts += _frameIntsPerLine;
		}

		// Fill to the left and to the right of the frame with transparent.
		auto fillLeft = (_innerLeft - outerLeft) * sizeof(uint32);
		auto fillRight = (outerRight - _innerRight) * sizeof(uint32);
		if (fillLeft || fillRight) {
			auto fillInts = _frameInts + _innerTop * _frameIntsPerLine;
			for (auto y = _innerTop; y != _innerBottom; ++y) {
				memset(fillInts + outerLeft, 0, fillLeft);
				memset(fillInts + _innerRight, 0, fillRight);
				fillInts += _frameIntsPerLine;
			}
		}

		// Fill below the frame bottom with transparent.
		auto fillBottomInts = (_frameInts + _innerBottom * _frameIntsPerLine + outerLeft);
		for (auto fillBottom = outerBottom - _innerBottom; fillBottom != 0; --fillBottom) {
			memset(fillBottomInts, 0, fillWidth);
			fillBottomInts += _frameIntsPerLine;
		}
	}
	if (_shadow.valid()) {
		paintShadow(outerLeft, outerTop, outerRight, outerBottom);
	}

	// Debug
	//frameInts = _frameInts;
	//auto pattern = anim::shifted((static_cast<uint32>(0xFF) << 24) | (static_cast<uint32>(0xFF) << 16) | (static_cast<uint32>(0xFF) << 8) | static_cast<uint32>(0xFF));
	//for (auto y = 0; y != _finalHeight; ++y) {
	//	for (auto x = 0; x != _finalWidth; ++x) {
	//		auto source = *frameInts;
	//		auto sourceAlpha = (source >> 24);
	//		*frameInts = anim::unshifted(anim::shifted(source) * 256 + pattern * (256 - sourceAlpha));
	//		++frameInts;
	//	}
	//	frameInts += _frameIntsPerLineAdded;
	//}

	p.drawImage(outerLeft / cIntRetinaFactor(), outerTop / cIntRetinaFactor(), _frame, outerLeft, outerTop, outerRight - outerLeft, outerBottom - outerTop);
}

EmojiPanel::Tab::Tab(TabType type, object_ptr<Inner> widget)
: _type(type)
, _widget(std::move(widget))
, _weak(_widget)
, _footer(_widget->createFooter()) {
	_footer->setParent(_widget->parentWidget());
}

object_ptr<EmojiPanel::Inner> EmojiPanel::Tab::takeWidget() {
	return std::move(_widget);
}

void EmojiPanel::Tab::returnWidget(object_ptr<Inner> widget) {
	_widget = std::move(widget);
	Ensures(_widget == _weak);
}

void EmojiPanel::Tab::saveScrollTop() {
	_scrollTop = widget()->getVisibleTop();
}

EmojiPanel::EmojiPanel(QWidget *parent) : TWidget(parent)
, _tabsSlider(this, st::emojiTabs)
, _topShadow(this, st::shadowFg)
, _bottomShadow(this, st::shadowFg)
, _scroll(this, st::emojiScroll)
, _tabs { {
	Tab { TabType::Emoji, object_ptr<EmojiListWidget>(this) },
	Tab { TabType::Stickers, object_ptr<StickersListWidget>(this) },
	Tab { TabType::Gifs, object_ptr<GifsListWidget>(this) },
} }
, _currentTabType(AuthSession::Current().data().emojiPanelTab()) {
	resize(QRect(0, 0, st::emojiPanWidth, st::emojiPanMaxHeight).marginsAdded(innerPadding()).size());
	_width = width();
	_height = height();

	createTabsSlider();

	_contentMaxHeight = st::emojiPanMaxHeight - marginTop() - marginBottom();
	_contentHeight = _contentMaxHeight;

	_scroll->resize(st::emojiPanWidth - st::buttonRadius, _contentHeight);
	_scroll->move(verticalRect().x(), verticalRect().y() + marginTop());
	setWidgetToScrollArea();

	_bottomShadow->setGeometry(_tabsSlider->x(), _scroll->y() + _scroll->height() - st::lineWidth, _tabsSlider->width(), st::lineWidth);

	_hideTimer.setSingleShot(true);
	connect(&_hideTimer, SIGNAL(timeout()), this, SLOT(hideByTimerOrLeave()));

	for (auto &tab : _tabs) {
		connect(tab.widget(), &Inner::scrollToY, this, [this, tab = &tab](int y) {
			if (tab == currentTab()) {
				scrollToY(y);
			} else {
				tab->saveScrollTop(y);
			}
		});
		connect(tab.widget(), &Inner::disableScroll, this, [this, tab = &tab](bool disabled) {
			if (tab == currentTab()) {
				_scroll->disableScroll(disabled);
			}
		});
		connect(tab.widget(), SIGNAL(saveConfigDelayed(int)), this, SLOT(onSaveConfigDelayed(int)));
	}

	connect(stickers(), SIGNAL(scrollUpdated()), this, SLOT(onScroll()));
	connect(_scroll, SIGNAL(scrolled()), this, SLOT(onScroll()));
	connect(emoji(), SIGNAL(selected(EmojiPtr)), this, SIGNAL(emojiSelected(EmojiPtr)));
	connect(stickers(), SIGNAL(selected(DocumentData*)), this, SIGNAL(stickerSelected(DocumentData*)));
	connect(stickers(), SIGNAL(checkForHide()), this, SLOT(onCheckForHide()));
	connect(gifs(), SIGNAL(selected(DocumentData*)), this, SIGNAL(stickerSelected(DocumentData*)));
	connect(gifs(), SIGNAL(selected(PhotoData*)), this, SIGNAL(photoSelected(PhotoData*)));
	connect(gifs(), SIGNAL(selected(InlineBots::Result*,UserData*)), this, SIGNAL(inlineResultSelected(InlineBots::Result*,UserData*)));
	connect(gifs(), &GifsListWidget::cancelled, this, [this] {
		hideAnimated();
	});

	_saveConfigTimer.setSingleShot(true);
	connect(&_saveConfigTimer, SIGNAL(timeout()), this, SLOT(onSaveConfig()));

	if (cPlatform() == dbipMac || cPlatform() == dbipMacOld) {
		connect(App::wnd()->windowHandle(), SIGNAL(activeChanged()), this, SLOT(onWndActiveChanged()));
	}

	_topShadow->raise();
	_bottomShadow->raise();
	_tabsSlider->raise();

//	setAttribute(Qt::WA_AcceptTouchEvents);
	setAttribute(Qt::WA_OpaquePaintEvent, false);

	hideChildren();
}

void EmojiPanel::moveBottom(int bottom) {
	_bottom = bottom;
	updateContentHeight();
}

void EmojiPanel::updateContentHeight() {
	auto addedHeight = innerPadding().top() + marginTop() + marginBottom() + innerPadding().bottom();
	auto wantedContentHeight = qRound(st::emojiPanHeightRatio * _bottom) - addedHeight;
	auto contentHeight = snap(wantedContentHeight, st::emojiPanMinHeight, st::emojiPanMaxHeight);
	auto resultTop = _bottom - addedHeight - contentHeight;
	if (contentHeight == _contentHeight) {
		move(x(), resultTop);
		return;
	}

	auto was = _contentHeight;
	_contentHeight = contentHeight;

	resize(QRect(0, 0, innerRect().width(), marginTop() + _contentHeight + marginBottom()).marginsAdded(innerPadding()).size());
	_height = height();
	move(x(), resultTop);

	if (was > _contentHeight) {
		_scroll->resize(_scroll->width(), _contentHeight);
		auto scrollTop = _scroll->scrollTop();
		currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollTop + _contentHeight);
	} else {
		auto scrollTop = _scroll->scrollTop();
		currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollTop + _contentHeight);
		_scroll->resize(_scroll->width(), _contentHeight);
	}
	_bottomShadow->setGeometry(_tabsSlider->x(), _scroll->y() + _scroll->height() - st::lineWidth, _tabsSlider->width(), st::lineWidth);

	_footerTop = innerRect().y() + innerRect().height() - st::emojiCategory.height;
	for (auto &tab : _tabs) {
		tab.footer()->move(_tabsSlider->x(), _footerTop);
	}

	update();
}

void EmojiPanel::onWndActiveChanged() {
	if (!App::wnd()->windowHandle()->isActive() && !isHidden()) {
		leaveEvent(0);
	}
}

void EmojiPanel::onSaveConfig() {
	Local::writeUserSettings();
}

void EmojiPanel::onSaveConfigDelayed(int delay) {
	_saveConfigTimer.start(delay);
}

void EmojiPanel::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();

	// This call can finish _a_show animation and destroy _showAnimation.
	auto opacityAnimating = _a_opacity.animating(ms);

	auto switching = (_slideAnimation != nullptr);
	auto showAnimating = _a_show.animating(ms);
	if (_showAnimation && !showAnimating) {
		_showAnimation.reset();
		if (!switching && !opacityAnimating) {
			showAll();
			currentTab()->widget()->afterShown();
		}
	}

	if (showAnimating) {
		t_assert(_showAnimation != nullptr);
		if (auto opacity = _a_opacity.current(_hiding ? 0. : 1.)) {
			_showAnimation->paintFrame(p, 0, 0, width(), _a_show.current(1.), opacity);
		}
	} else if (opacityAnimating) {
		p.setOpacity(_a_opacity.current(_hiding ? 0. : 1.));
		p.drawPixmap(0, 0, _cache);
	} else if (_hiding || isHidden()) {
		hideFinished();
	} else if (switching) {
		paintSlideFrame(p, ms);
		if (!_a_slide.animating()) {
			_slideAnimation.reset();
			if (!opacityAnimating) {
				showAll();
				currentTab()->widget()->afterShown();
			}
			InvokeQueued(this, [this] {
				if (_hideAfterSlide && !_a_slide.animating()) {
					startOpacityAnimation(true);
				}
			});
		}
	} else {
		if (!_cache.isNull()) _cache = QPixmap();
		if (!_inComplrexGrab) Ui::Shadow::paint(p, innerRect(), width(), st::emojiPanAnimation.shadow);
		paintContent(p);
	}
}

void EmojiPanel::paintSlideFrame(Painter &p, TimeMs ms) {
	Ui::Shadow::paint(p, innerRect(), width(), st::emojiPanAnimation.shadow);

	auto inner = innerRect();
	auto topPart = QRect(inner.x(), inner.y(), inner.width(), _tabsSlider->height() + st::buttonRadius);
	App::roundRect(p, topPart, st::emojiPanBg, ImageRoundRadius::Small, App::RectPart::TopFull | App::RectPart::NoTopBottom);

	auto slideDt = _a_slide.current(ms, 1.);
	_slideAnimation->paintFrame(p, slideDt, _a_opacity.current(_hiding ? 0. : 1.));
}

void EmojiPanel::paintContent(Painter &p) {
	auto inner = innerRect();
	auto topPart = QRect(inner.x(), inner.y(), inner.width(), _tabsSlider->height() + st::buttonRadius);
	App::roundRect(p, topPart, st::emojiPanBg, ImageRoundRadius::Small, App::RectPart::TopFull | App::RectPart::NoTopBottom);

	auto showSectionIcons = (_currentTabType != TabType::Gifs);
	auto bottomPart = QRect(inner.x(), _footerTop - st::buttonRadius, inner.width(), st::emojiCategory.height + st::buttonRadius);
	auto &bottomBg = showSectionIcons ? st::emojiPanCategories : st::emojiPanBg;
	auto bottomParts = App::RectPart::NoTopBottom | App::RectPart::BottomFull;
	App::roundRect(p, bottomPart, bottomBg, ImageRoundRadius::Small, bottomParts);

	auto horizontal = horizontalRect();
	auto sidesTop = horizontal.y();
	auto sidesHeight = _scroll->y() + _scroll->height() - sidesTop;
	p.fillRect(myrtlrect(inner.x() + inner.width() - st::emojiScroll.width, sidesTop, st::emojiScroll.width, sidesHeight), st::emojiPanBg);
	p.fillRect(myrtlrect(inner.x(), sidesTop, st::buttonRadius, sidesHeight), st::emojiPanBg);
}

int EmojiPanel::marginTop() const {
	return _tabsSlider->height() - st::lineWidth;
}

int EmojiPanel::marginBottom() const {
	return st::emojiCategory.height;
}

void EmojiPanel::moveByBottom() {
	moveToRight(0, y());
	updateContentHeight();
}

void EmojiPanel::enterEventHook(QEvent *e) {
	showAnimated();
}

bool EmojiPanel::preventAutoHide() const {
	return stickers()->preventAutoHide();
}

void EmojiPanel::leaveEventHook(QEvent *e) {
	if (preventAutoHide()) {
		return;
	}
	auto ms = getms();
	if (_a_show.animating(ms) || _a_opacity.animating(ms)) {
		hideAnimated();
	} else {
		_hideTimer.start(300);
	}
	return TWidget::leaveEventHook(e);
}

void EmojiPanel::otherEnter() {
	showAnimated();
}

void EmojiPanel::otherLeave() {
	if (preventAutoHide()) {
		return;
	}

	auto ms = getms();
	if (_a_opacity.animating(ms)) {
		hideByTimerOrLeave();
	} else {
		_hideTimer.start(0);
	}
}

void EmojiPanel::hideFast() {
	if (isHidden()) return;

	_hideTimer.stop();
	_hiding = false;
	_a_opacity.finish();
	hideFinished();
}

void EmojiPanel::refreshStickers() {
	stickers()->refreshStickers();
	if (isHidden() || _currentTabType != TabType::Stickers) {
		stickers()->preloadImages();
	}
}

void EmojiPanel::opacityAnimationCallback() {
	update();
	if (!_a_opacity.animating()) {
		if (_hiding) {
			_hiding = false;
			hideFinished();
		} else if (!_a_show.animating() && !_a_slide.animating()) {
			showAll();
			currentTab()->widget()->afterShown();
		}
	}
}

void EmojiPanel::hideByTimerOrLeave() {
	if (isHidden() || preventAutoHide()) return;

	hideAnimated();
}

void EmojiPanel::prepareCache() {
	if (_a_opacity.animating()) return;

	auto showAnimation = base::take(_a_show);
	auto showAnimationData = base::take(_showAnimation);
	auto slideAnimation = base::take(_slideAnimation);
	showAll();
	_cache = myGrab(this);
	_slideAnimation = base::take(slideAnimation);
	_showAnimation = base::take(showAnimationData);
	_a_show = base::take(showAnimation);
	if (_a_show.animating()) {
		hideChildren();
	}
}

void EmojiPanel::startOpacityAnimation(bool hiding) {
	if (!_scroll->isHidden()) {
		currentTab()->widget()->beforeHiding();
	}
	_hiding = false;
	prepareCache();
	_hiding = hiding;
	hideChildren();
	_a_opacity.start([this] { opacityAnimationCallback(); }, _hiding ? 1. : 0., _hiding ? 0. : 1., st::emojiPanDuration);
}

void EmojiPanel::startShowAnimation() {
	if (!_a_show.animating()) {
		auto image = grabForComplexAnimation(GrabType::Panel);

		_showAnimation = std::make_unique<Ui::PanelAnimation>(st::emojiPanAnimation, Ui::PanelAnimation::Origin::BottomRight);
		auto inner = rect().marginsRemoved(st::emojiPanMargins);
		_showAnimation->setFinalImage(std::move(image), QRect(inner.topLeft() * cIntRetinaFactor(), inner.size() * cIntRetinaFactor()));
		auto corners = App::cornersMask(ImageRoundRadius::Small);
		_showAnimation->setCornerMasks(QImage(*corners[0]), QImage(*corners[1]), QImage(*corners[2]), QImage(*corners[3]));
		_showAnimation->start();
	}
	hideChildren();
	_a_show.start([this] { update(); }, 0., 1., st::emojiPanShowDuration);
}

QImage EmojiPanel::grabForComplexAnimation(GrabType type) {
	auto cache = base::take(_cache);
	auto opacityAnimation = base::take(_a_opacity);
	auto slideAnimationData = base::take(_slideAnimation);
	auto slideAnimation = base::take(_a_slide);
	auto showAnimationData = base::take(_showAnimation);
	auto showAnimation = base::take(_a_show);

	showAll();
	if (type == GrabType::Slide) {
		_topShadow->hide();
		_tabsSlider->hide();
	}
	myEnsureResized(this);

	auto result = QImage(size() * cIntRetinaFactor(), QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(cRetinaFactor());
	result.fill(Qt::transparent);
	_inComplrexGrab = true;
	render(&result);
	_inComplrexGrab = false;

	_a_show = base::take(showAnimation);
	_showAnimation = base::take(showAnimationData);
	_a_slide = base::take(slideAnimation);
	_slideAnimation = base::take(slideAnimationData);
	_a_opacity = base::take(opacityAnimation);
	_cache = base::take(_cache);

	return result;
}

void EmojiPanel::hideAnimated() {
	if (isHidden()) return;
	if (_hiding) return;

	_hideTimer.stop();
	if (_a_slide.animating()) {
		_hideAfterSlide = true;
	} else {
		startOpacityAnimation(true);
	}
}

EmojiPanel::~EmojiPanel() = default;

void EmojiPanel::hideFinished() {
	hide();
	for (auto &tab : _tabs) {
		tab.widget()->panelHideFinished();
	}
	_a_show.finish();
	_showAnimation.reset();
	_a_slide.finish();
	_slideAnimation.reset();
	_cache = QPixmap();
	_hiding = false;

	scrollToY(0);
}

void EmojiPanel::showAnimated() {
	_hideTimer.stop();
	_hideAfterSlide = false;
	showStarted();
}

void EmojiPanel::showStarted() {
	if (isHidden()) {
		emit updateStickers();
		currentTab()->widget()->refreshRecent();
		currentTab()->widget()->preloadImages();
		_a_slide.finish();
		_slideAnimation.reset();
		moveByBottom();
		show();
		startShowAnimation();
	} else if (_hiding) {
		startOpacityAnimation(false);
	}
}

bool EmojiPanel::eventFilter(QObject *obj, QEvent *e) {
	if (e->type() == QEvent::Enter) {
		otherEnter();
	} else if (e->type() == QEvent::Leave) {
		otherLeave();
	} else if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton/* && !dynamic_cast<StickerPan*>(obj)*/) {
		if (isHidden() || _hiding || _hideAfterSlide) {
			showAnimated();
		} else {
			hideAnimated();
		}
	}
	return false;
}

void EmojiPanel::stickersInstalled(uint64 setId) {
	_tabsSlider->setActiveSection(static_cast<int>(TabType::Stickers));
	if (isHidden()) {
		moveByBottom();
		startShowAnimation();
		show();
	}
	showAll();
	stickers()->showStickerSet(setId);
	updateContentHeight();
	showAnimated();
}

void EmojiPanel::setInlineQueryPeer(PeerData *peer) {
	gifs()->setInlineQueryPeer(peer);
}

void EmojiPanel::showAll() {
	currentTab()->footer()->show();
	_scroll->show();
	_topShadow->show();
	_bottomShadow->setVisible(_currentTabType == TabType::Gifs);
	_tabsSlider->show();
}

void EmojiPanel::hideForSliding() {
	hideChildren();
	_tabsSlider->show();
	_topShadow->show();
	currentTab()->widget()->clearSelection();
}

void EmojiPanel::onScroll() {
	auto scrollTop = _scroll->scrollTop();
	auto scrollBottom = scrollTop + _scroll->height();
	currentTab()->widget()->setVisibleTopBottom(scrollTop, scrollBottom);
}

style::margins EmojiPanel::innerPadding() const {
	return st::emojiPanMargins;
}

QRect EmojiPanel::innerRect() const {
	return rect().marginsRemoved(innerPadding());
}

QRect EmojiPanel::horizontalRect() const {
	return innerRect().marginsRemoved(style::margins(0, st::buttonRadius, 0, st::buttonRadius));
}

QRect EmojiPanel::verticalRect() const {
	return innerRect().marginsRemoved(style::margins(st::buttonRadius, 0, st::buttonRadius, 0));
}

void EmojiPanel::createTabsSlider() {
	auto sections = QStringList();
	sections.push_back(lang(lng_switch_emoji).toUpper());
	sections.push_back(lang(lng_switch_stickers).toUpper());
	sections.push_back(lang(lng_switch_gifs).toUpper());
	_tabsSlider->setSections(sections);

	_tabsSlider->setActiveSectionFast(static_cast<int>(_currentTabType));
	_tabsSlider->setSectionActivatedCallback([this] {
		switchTab();
	});

	_tabsSlider->resizeToWidth(innerRect().width());
	_tabsSlider->moveToLeft(innerRect().x(), innerRect().y());
	_topShadow->setGeometry(_tabsSlider->x(), _tabsSlider->bottomNoMargins() - st::lineWidth, _tabsSlider->width(), st::lineWidth);
}

void EmojiPanel::switchTab() {
	auto tab = _tabsSlider->activeSection();
	t_assert(tab >= 0 && tab < Tab::kCount);
	auto newTabType = static_cast<TabType>(tab);
	if (_currentTabType == newTabType) {
		return;
	}

	auto wasTab = _currentTabType;
	currentTab()->saveScrollTop();

	if (!_scroll->isHidden()) {
		currentTab()->widget()->beforeHiding();
	}

	auto wasCache = grabForComplexAnimation(GrabType::Slide);

	auto widget = _scroll->takeWidget<Inner>();
	widget->setParent(this);
	widget->hide();
	currentTab()->footer()->hide();
	currentTab()->returnWidget(std::move(widget));

	_currentTabType = newTabType;
	currentTab()->widget()->refreshRecent();
	currentTab()->widget()->preloadImages();
	setWidgetToScrollArea();

	auto nowCache = grabForComplexAnimation(GrabType::Slide);

	auto direction = (wasTab > _currentTabType) ? SlideAnimation::Direction::LeftToRight : SlideAnimation::Direction::RightToLeft;
	if (direction == SlideAnimation::Direction::LeftToRight) {
		std::swap(wasCache, nowCache);
	}
	_slideAnimation = std::make_unique<SlideAnimation>();
	auto inner = rect().marginsRemoved(st::emojiPanMargins);
	auto slidingRect = QRect(_tabsSlider->x() * cIntRetinaFactor(), _scroll->y() * cIntRetinaFactor(), _tabsSlider->width() * cIntRetinaFactor(), (inner.y() + inner.height() - _scroll->y()) * cIntRetinaFactor());
	_slideAnimation->setFinalImages(direction, std::move(wasCache), std::move(nowCache), slidingRect);
	auto corners = App::cornersMask(ImageRoundRadius::Small);
	_slideAnimation->setCornerMasks(QImage(*corners[0]), QImage(*corners[1]), QImage(*corners[2]), QImage(*corners[3]));
	_slideAnimation->start();

	hideForSliding();

	getTab(wasTab)->widget()->hideFinished();

	_a_slide.start([this] { update(); }, 0., 1., st::emojiPanSlideDuration, anim::linear);
	update();

	AuthSession::Current().data().setEmojiPanelTab(_currentTabType);
	onSaveConfigDelayed(kSaveChosenTabTimeout);
}

gsl::not_null<EmojiListWidget*> EmojiPanel::emoji() const {
	return static_cast<EmojiListWidget*>(getTab(TabType::Emoji)->widget().get());
}

gsl::not_null<StickersListWidget*> EmojiPanel::stickers() const {
	return static_cast<StickersListWidget*>(getTab(TabType::Stickers)->widget().get());
}

gsl::not_null<GifsListWidget*> EmojiPanel::gifs() const {
	return static_cast<GifsListWidget*>(getTab(TabType::Gifs)->widget().get());
}

void EmojiPanel::setWidgetToScrollArea() {
	_scroll->setOwnedWidget(currentTab()->takeWidget());
	_scroll->disableScroll(false);
	currentTab()->widget()->moveToLeft(0, 0);
	currentTab()->widget()->show();
	scrollToY(currentTab()->getScrollTop());
	updateContentHeight();
	onScroll();
}

void EmojiPanel::onCheckForHide() {
	if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
		_hideTimer.start(3000);
	}
}

bool EmojiPanel::overlaps(const QRect &globalRect) const {
	if (isHidden() || !_cache.isNull()) return false;

	auto testRect = QRect(mapFromGlobal(globalRect.topLeft()), globalRect.size());
	auto inner = rect().marginsRemoved(st::emojiPanMargins);
	return inner.marginsRemoved(QMargins(st::buttonRadius, 0, st::buttonRadius, 0)).contains(testRect)
		|| inner.marginsRemoved(QMargins(0, st::buttonRadius, 0, st::buttonRadius)).contains(testRect);
}

void EmojiPanel::scrollToY(int y) {
	_scroll->scrollToY(y);

	// Qt render glitch workaround, shadow sometimes disappears if we just scroll to y.
	_topShadow->update();
}

EmojiPanel::Inner::Inner(QWidget *parent) : TWidget(parent) {
}

void EmojiPanel::Inner::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	auto oldVisibleHeight = getVisibleBottom() - getVisibleTop();
	_visibleTop = visibleTop;
	_visibleBottom = visibleBottom;
	auto visibleHeight = getVisibleBottom() - getVisibleTop();
	if (visibleHeight != oldVisibleHeight) {
		resize(st::emojiPanWidth - st::emojiScroll.width - st::buttonRadius, countHeight());
	}
}

void EmojiPanel::Inner::hideFinished() {
	processHideFinished();
	if (auto footer = getFooter()) {
		footer->processHideFinished();
	}
}

void EmojiPanel::Inner::panelHideFinished() {
	hideFinished();
	processPanelHideFinished();
	if (auto footer = getFooter()) {
		footer->processPanelHideFinished();
	}
}

EmojiPanel::InnerFooter::InnerFooter(QWidget *parent) : TWidget(parent) {
	resize(st::emojiPanWidth, st::emojiCategory.height);
}

} // namespace ChatHelpers
