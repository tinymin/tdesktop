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

#include "styles/style_widgets.h"

namespace Ui {

class InputField;
class CrossButton;
class ScrollArea;

class MultiSelect : public TWidget {
public:
	MultiSelect(QWidget *parent, const style::MultiSelect &st, const QString &placeholder = QString());

	QString getQuery() const;
	void setInnerFocus();
	void clearQuery();

	void setQueryChangedCallback(base::lambda<void(const QString &query)> callback);
	void setSubmittedCallback(base::lambda<void(bool ctrlShiftEnter)> callback);
	void setResizedCallback(base::lambda<void()> callback);

	enum class AddItemWay {
		Default,
		SkipAnimation,
	};
	using PaintRoundImage = base::lambda<void(Painter &p, int x, int y, int outerWidth, int size)>;
	void addItem(uint64 itemId, const QString &text, style::color color, PaintRoundImage paintRoundImage, AddItemWay way = AddItemWay::Default);
	void addItemInBunch(uint64 itemId, const QString &text, style::color color, PaintRoundImage paintRoundImage);
	void finishItemsBunch();
	void setItemText(uint64 itemId, const QString &text);

	void setItemRemovedCallback(base::lambda<void(uint64 itemId)> callback);
	void removeItem(uint64 itemId);

	int getItemsCount() const;
	QVector<uint64> getItems() const;
	bool hasItem(uint64 itemId) const;

protected:
	int resizeGetHeight(int newWidth) override;
	bool eventFilter(QObject *o, QEvent *e) override;

private:
	void scrollTo(int activeTop, int activeBottom);

	const style::MultiSelect &_st;

	object_ptr<Ui::ScrollArea> _scroll;

	class Inner;
	QPointer<Inner> _inner;

	base::lambda<void()> _resizedCallback;
	base::lambda<void(const QString &query)> _queryChangedCallback;

};

// This class is hold in header because it requires Qt preprocessing.
class MultiSelect::Inner : public TWidget {
	Q_OBJECT

public:
	using ScrollCallback = base::lambda<void(int activeTop, int activeBottom)>;
	Inner(QWidget *parent, const style::MultiSelect &st, const QString &placeholder, ScrollCallback callback);

	QString getQuery() const;
	bool setInnerFocus();
	void clearQuery();

	void setQueryChangedCallback(base::lambda<void(const QString &query)> callback);
	void setSubmittedCallback(base::lambda<void(bool ctrlShiftEnter)> callback);

	class Item;
	void addItemInBunch(std::unique_ptr<Item> item);
	void finishItemsBunch(AddItemWay way);
	void setItemText(uint64 itemId, const QString &text);

	void setItemRemovedCallback(base::lambda<void(uint64 itemId)> callback);
	void removeItem(uint64 itemId);

	int getItemsCount() const;
	QVector<uint64> getItems() const;
	bool hasItem(uint64 itemId) const;

	void setResizedCallback(base::lambda<void(int heightDelta)> callback);

	~Inner();

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private slots:
	void onQueryChanged();
	void onSubmitted(bool ctrlShiftEnter) {
		if (_submittedCallback) {
			_submittedCallback(ctrlShiftEnter);
		}
	}
	void onFieldFocused();

private:
	void computeItemsGeometry(int newWidth);
	void updateItemsGeometry();
	void updateFieldGeometry();
	void updateHasAnyItems(bool hasAnyItems);
	void updateSelection(QPoint mousePosition);
	void clearSelection() {
		updateSelection(QPoint(-1, -1));
	}
	void updateCursor();
	void updateHeightStep();
	void finishHeightAnimation();
	enum class ChangeActiveWay {
		Default,
		SkipSetFocus,
	};
	void setActiveItem(int active, ChangeActiveWay skipSetFocus = ChangeActiveWay::Default);
	void setActiveItemPrevious();
	void setActiveItemNext();

	QMargins itemPaintMargins() const;

	const style::MultiSelect &_st;
	Animation _iconOpacity;

	ScrollCallback _scrollCallback;

	std::set<uint64> _idsMap;
	std::vector<std::unique_ptr<Item>> _items;
	std::set<std::unique_ptr<Item>> _removingItems;

	int _selected = -1;
	int _active = -1;
	bool _overDelete = false;

	int _fieldLeft = 0;
	int _fieldTop = 0;
	int _fieldWidth = 0;
	object_ptr<Ui::InputField> _field;
	object_ptr<Ui::CrossButton> _cancel;

	int _newHeight = 0;
	Animation _height;

	base::lambda<void(const QString &query)> _queryChangedCallback;
	base::lambda<void(bool ctrlShiftEnter)> _submittedCallback;
	base::lambda<void(uint64 itemId)> _itemRemovedCallback;
	base::lambda<void(int heightDelta)> _resizedCallback;

};

} // namespace Ui
