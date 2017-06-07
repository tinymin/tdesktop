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
#pragma once

#include "boxes/abstract_box.h"

class ConfirmBox;

namespace style {
struct RippleAnimation;
} // namespace style

namespace Ui {
class PlainShadow;
class RippleAnimation;
class SettingsSlider;
class SlideAnimation;
} // namespace Ui

class StickersBox : public BoxContent, public RPCSender {
	Q_OBJECT

public:
	enum class Section {
		Installed,
		Featured,
		Archived,
		ArchivedPart,
	};
	StickersBox(QWidget*, Section section);
	StickersBox(QWidget*, const Stickers::Order &archivedIds);

	void closeHook() override;

	~StickersBox();

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private slots:
	void onStickersUpdated();

private:
	class Inner;
	class Tab {
	public:
		Tab() = default;

		template <typename ...Args>
		Tab(int index, Args&&... args);

		object_ptr<Inner> takeWidget();
		void returnWidget(object_ptr<Inner> widget);

		Inner *widget() {
			return _weak;
		}
		int index() const {
			return _index;
		}

		void saveScrollTop();
		int getScrollTop() const {
			return _scrollTop;
		}

	private:
		int _index = 0;
		object_ptr<Inner> _widget = { nullptr };
		QPointer<Inner> _weak;
		int _scrollTop = 0;

	};

	void refreshTabs();
	void rebuildList(Tab *tab = nullptr);
	void updateTabsGeometry();
	void switchTab();
	void installSet(uint64 setId);
	int getTopSkip() const;

	QPixmap grabContentCache();

	void installDone(const MTPmessages_StickerSetInstallResult &result);
	bool installFail(uint64 setId, const RPCError &error);

	void preloadArchivedSets();
	void requestArchivedSets();
	void loadMoreArchived();
	void getArchivedDone(uint64 offsetId, const MTPmessages_ArchivedStickers &result);

	object_ptr<Ui::SettingsSlider> _tabs = { nullptr };
	QList<Section> _tabIndices;

	class CounterWidget;
	object_ptr<CounterWidget> _unreadBadge = { nullptr };

	Section _section;

	Tab _installed;
	Tab _featured;
	Tab _archived;
	Tab *_tab = nullptr;

	std::unique_ptr<Ui::SlideAnimation> _slideAnimation;
	object_ptr<BoxLayerTitleShadow> _titleShadow = { nullptr };

	int _aboutWidth = 0;
	Text _about;
	int _aboutHeight = 0;

	mtpRequestId _archivedRequestId = 0;
	bool _archivedLoaded = false;
	bool _allArchivedLoaded = false;
	bool _someArchivedLoaded = false;

	Stickers::Order _localOrder;
	Stickers::Order _localRemoved;

};

int stickerPacksCount(bool includeArchivedOfficial = false);

// This class is hold in header because it requires Qt preprocessing.
class StickersBox::Inner : public TWidget, private base::Subscriber {
	Q_OBJECT

public:
	using Section = StickersBox::Section;
	Inner(QWidget *parent, Section section);
	Inner(QWidget *parent, const Stickers::Order &archivedIds);

	void rebuild();
	void updateSize();
	void updateRows(); // refresh only pack cover stickers
	bool appendSet(const Stickers::Set &set);

	Stickers::Order getOrder() const;
	Stickers::Order getFullOrder() const;
	Stickers::Order getRemovedSets() const;

	void setFullOrder(const Stickers::Order &order);
	void setRemovedSets(const Stickers::Order &removed);

	void setInstallSetCallback(base::lambda<void(uint64 setId)> callback) {
		_installSetCallback = std::move(callback);
	}
	void setLoadMoreCallback(base::lambda<void()> callback) {
		_loadMoreCallback = std::move(callback);
	}

	void setVisibleTopBottom(int visibleTop, int visibleBottom) override;

	int getVisibleTop() const {
		return _visibleTop;
	}

	~Inner();

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

signals:
	void draggingScrollDelta(int delta);

public slots:
	void onUpdateSelected();

private:
	template <typename Check>
	Stickers::Order collectSets(Check check) const;

	void checkLoadMore();
	void updateScrollbarWidth();
	int getRowIndex(uint64 setId) const;
	void setRowRemoved(int index, bool removed);

	void setActionDown(int newActionDown);
	void setup();
	QRect relativeButtonRect(bool removeButton) const;
	void ensureRipple(const style::RippleAnimation &st, QImage mask, bool removeButton);

	void step_shifting(TimeMs ms, bool timer);
	void paintRow(Painter &p, int index, TimeMs ms);
	void paintFakeButton(Painter &p, int index, TimeMs ms);
	void clear();
	void setActionSel(int32 actionSel);
	float64 aboveShadowOpacity() const;

	void readVisibleSets();

	Section _section;
	Stickers::Order _archivedIds;

	int32 _rowHeight;
	struct Row {
		Row(uint64 id, DocumentData *sticker, int32 count, const QString &title, int titleWidth, bool installed, bool official, bool unread, bool archived, bool removed, int32 pixw, int32 pixh) : id(id)
			, sticker(sticker)
			, count(count)
			, title(title)
			, titleWidth(titleWidth)
			, installed(installed)
			, official(official)
			, unread(unread)
			, archived(archived)
			, removed(removed)
			, pixw(pixw)
			, pixh(pixh)
			, yadd(0, 0) {
		}
		bool isRecentSet() const {
			return (id == Stickers::CloudRecentSetId);
		}
		uint64 id;
		DocumentData *sticker;
		int32 count;
		QString title;
		int titleWidth;
		bool installed, official, unread, archived, removed;
		int32 pixw, pixh;
		anim::value yadd;
		QSharedPointer<Ui::RippleAnimation> ripple;
	};
	using Rows = QList<Row*>;

	void rebuildAppendSet(const Stickers::Set &set, int maxNameWidth);
	void fillSetCover(const Stickers::Set &set, DocumentData **outSticker, int *outWidth, int *outHeight) const;
	int fillSetCount(const Stickers::Set &set) const;
	QString fillSetTitle(const Stickers::Set &set, int maxNameWidth, int *outTitleWidth) const;
	void fillSetFlags(const Stickers::Set &set, bool *outInstalled, bool *outOfficial, bool *outUnread, bool *outArchived);

	int countMaxNameWidth() const;

	Rows _rows;
	QList<TimeMs> _animStartTimes;
	TimeMs _aboveShadowFadeStart = 0;
	anim::value _aboveShadowFadeOpacity;
	BasicAnimation _a_shifting;

	base::lambda<void(uint64 setId)> _installSetCallback;
	base::lambda<void()> _loadMoreCallback;

	int _visibleTop = 0;
	int _visibleBottom = 0;
	int _itemsTop = 0;

	int _actionSel = -1;
	int _actionDown = -1;

	QString _addText;
	int _addWidth = 0;
	QString _undoText;
	int _undoWidth = 0;

	int _buttonHeight = 0;

	QPoint _mouse;
	bool _inDragArea = false;
	int _selected = -1;
	int _pressed = -1;
	QPoint _dragStart;
	int _started = -1;
	int _dragging = -1;
	int _above = -1;

	int _scrollbar = 0;

};
