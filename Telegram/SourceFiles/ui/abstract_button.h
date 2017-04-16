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

#include "ui/twidget.h"

namespace Ui {

class AbstractButton : public TWidget {
	Q_OBJECT

public:
	AbstractButton(QWidget *parent) : TWidget(parent) {
		setMouseTracking(true);
	}

	Qt::KeyboardModifiers clickModifiers() const {
		return _modifiers;
	}

	void setDisabled(bool disabled = true);
	void clearState();
	bool isOver() const {
		return _state & StateFlag::Over;
	}
	bool isDown() const {
		return _state & StateFlag::Down;
	}
	bool isDisabled() {
		return _state & StateFlag::Disabled;
	}

	void setPointerCursor(bool enablePointerCursor);

	void setAcceptBoth(bool acceptBoth = true);

	void setClickedCallback(base::lambda<void()> callback) {
		_clickedCallback = std::move(callback);
	}

	void setVisible(bool visible) override {
		TWidget::setVisible(visible);
		if (!visible) {
			clearState();
		}
	}

protected:
	void enterEventHook(QEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

signals:
	void clicked();

protected:
	enum class StateFlag {
		None = 0x00,
		Over = 0x01,
		Down = 0x02,
		Disabled = 0x04,
	};
	Q_DECLARE_FLAGS(State, StateFlag);
	Q_DECLARE_FRIEND_OPERATORS_FOR_FLAGS(State);

	State state() const {
		return _state;
	}

	enum class StateChangeSource {
		ByUser = 0x00,
		ByPress = 0x01,
		ByHover = 0x02,
	};
	void setOver(bool over, StateChangeSource source = StateChangeSource::ByUser);

	virtual void onStateChanged(State was, StateChangeSource source) {
	}

private:
	void updateCursor();
	void checkIfOver(QPoint localPos);

	State _state = StateFlag::None;

	bool _acceptBoth = false;
	Qt::KeyboardModifiers _modifiers;
	bool _enablePointerCursor = true;

	base::lambda<void()> _clickedCallback;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractButton::State);

} // namespace Ui
