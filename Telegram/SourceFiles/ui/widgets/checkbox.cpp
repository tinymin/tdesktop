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
#include "ui/widgets/checkbox.h"

#include "lang.h"
#include "ui/effects/ripple_animation.h"

namespace Ui {
namespace {

TextParseOptions _checkboxOptions = {
	TextParseMultiline, // flags
	0, // maxw
	0, // maxh
	Qt::LayoutDirectionAuto, // dir
};

} // namespace

Checkbox::Checkbox(QWidget *parent, const QString &text, bool checked, const style::Checkbox &st) : RippleButton(parent, st.ripple)
, _st(st)
, _text(_st.style, text, _checkboxOptions)
, _checked(checked) {
	if (_st.width <= 0) {
		resizeToWidth(_text.maxWidth() - _st.width);
	} else {
		resizeToWidth(_st.width);
	}
	_checkRect = myrtlrect(_st.margin.left(), _st.margin.top(), _st.diameter, _st.diameter);

	connect(this, SIGNAL(clicked()), this, SLOT(onClicked()));

	setCursor(style::cur_pointer);
}

bool Checkbox::checked() const {
	return _checked;
}

void Checkbox::setChecked(bool checked, NotifyAboutChange notify) {
	if (_checked != checked) {
		_checked = checked;
		_a_checked.start([this] { update(_checkRect); }, _checked ? 0. : 1., _checked ? 1. : 0., _st.duration);
		if (notify == NotifyAboutChange::Notify) {
			emit changed();
		}
	}
}

void Checkbox::finishAnimations() {
	_a_checked.finish();
}

int Checkbox::naturalWidth() const {
	return _st.textPosition.x() + _text.maxWidth();
}

void Checkbox::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();
	auto active = _a_checked.current(ms, _checked ? 1. : 0.);
	auto color = anim::color(_st.rippleBg, _st.rippleBgActive, active);
	paintRipple(p, _st.rippleAreaPosition.x(), _st.rippleAreaPosition.y(), ms, &color);

	if (_checkRect.intersects(e->rect())) {
		auto pen = anim::pen(_st.checkFg, _st.checkFgActive, active);
		pen.setWidth(_st.thickness);
		p.setPen(pen);
		p.setBrush(anim::brush(_st.checkBg, anim::color(_st.checkFg, _st.checkFgActive, active), active));

		{
			PainterHighQualityEnabler hq(p);
			p.drawRoundedRect(QRectF(_checkRect).marginsRemoved(QMarginsF(_st.thickness / 2., _st.thickness / 2., _st.thickness / 2., _st.thickness / 2.)), st::buttonRadius - (_st.thickness / 2.), st::buttonRadius - (_st.thickness / 2.));
		}

		if (active > 0) {
			_st.checkIcon.paint(p, QPoint(_st.margin.left(), _st.margin.top()), width());
		}
	}
	if (_checkRect.contains(e->rect())) return;

	auto textWidth = qMax(width() - (_st.textPosition.x() + (_st.textPosition.x() - _st.diameter)), 1);

	p.setPen(_st.textFg);
	_text.drawLeftElided(p, _st.margin.left() + _st.textPosition.x(), _st.margin.top() + _st.textPosition.y(), textWidth, width());
}

void Checkbox::onClicked() {
	if (isDisabled()) return;
	setChecked(!checked());
}

void Checkbox::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	if (isDisabled() && !(was & StateFlag::Disabled)) {
		setCursor(style::cur_default);
	} else if (!isDisabled() && (was & StateFlag::Disabled)) {
		setCursor(style::cur_pointer);
	}
}

int Checkbox::resizeGetHeight(int newWidth) {
	return _st.height;
}

QImage Checkbox::prepareRippleMask() const {
	return RippleAnimation::ellipseMask(QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

QPoint Checkbox::prepareRippleStartPosition() const {
	auto position = mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
	if (QRect(0, 0, _st.rippleAreaSize, _st.rippleAreaSize).contains(position)) {
		return position;
	}
	return disabledRippleStartPosition();
}

void RadiobuttonGroup::setValue(int value) {
	if (_hasValue && _value == value) {
		return;
	}
	_hasValue = true;
	_value = value;
	for (auto button : _buttons) {
		button->handleNewGroupValue(_value);
	}
	if (_changedCallback) {
		_changedCallback(_value);
	}
}

Radiobutton::Radiobutton(QWidget *parent, const std::shared_ptr<RadiobuttonGroup> &group, int value, const QString &text, const style::Checkbox &st) : RippleButton(parent, st.ripple)
, _group(group)
, _value(value)
, _st(st)
, _text(_st.style, text, _checkboxOptions)
, _checked(_group->hasValue() && _group->value() == _value) {
	_group->registerButton(this);

	if (_st.width <= 0) {
		resizeToWidth(_text.maxWidth() - _st.width);
	} else {
		resizeToWidth(_st.width);
	}
	_checkRect = myrtlrect(_st.margin.left(), _st.margin.top(), _st.diameter, _st.diameter);
}

void Radiobutton::handleNewGroupValue(int value) {
	auto checked = (value == _value);
	if (_checked != checked) {
		_checked = checked;
		_a_checked.start([this] { update(_checkRect); }, _checked ? 0. : 1., _checked ? 1. : 0., _st.duration);
	}
}

int Radiobutton::naturalWidth() const {
	return _st.textPosition.x() + _text.maxWidth();
}

void Radiobutton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto ms = getms();
	auto active = _a_checked.current(ms, _checked ? 1. : 0.);
	auto color = anim::color(_st.rippleBg, _st.rippleBgActive, active);
	paintRipple(p, _st.rippleAreaPosition.x(), _st.rippleAreaPosition.y(), ms, &color);

	if (_checkRect.intersects(e->rect())) {
		PainterHighQualityEnabler hq(p);

		auto pen = anim::pen(_st.checkFg, _st.checkFgActive, active);
		pen.setWidth(_st.thickness);
		p.setPen(pen);
		p.setBrush(_st.checkBg);
		//int32 skip = qCeil(_st.thickness / 2.);
		//p.drawEllipse(_checkRect.marginsRemoved(QMargins(skip, skip, skip, skip)));
		p.drawEllipse(QRectF(_checkRect).marginsRemoved(QMarginsF(_st.thickness / 2., _st.thickness / 2., _st.thickness / 2., _st.thickness / 2.)));

		if (active > 0) {
			p.setPen(Qt::NoPen);
			p.setBrush(anim::brush(_st.checkFg, _st.checkFgActive, active));

			auto skip0 = _checkRect.width() / 2., skip1 = _st.radioSkip / 10., checkSkip = skip0 * (1. - active) + skip1 * active;
			p.drawEllipse(QRectF(_checkRect).marginsRemoved(QMarginsF(checkSkip, checkSkip, checkSkip, checkSkip)));
			//int32 fskip = qFloor(checkSkip), cskip = qCeil(checkSkip);
			//if (2 * fskip < _checkRect.width()) {
			//	if (fskip != cskip) {
			//		p.setOpacity(float64(cskip) - checkSkip);
			//		p.drawEllipse(_checkRect.marginsRemoved(QMargins(fskip, fskip, fskip, fskip)));
			//		p.setOpacity(1.);
			//	}
			//	if (2 * cskip < _checkRect.width()) {
			//		p.drawEllipse(_checkRect.marginsRemoved(QMargins(cskip, cskip, cskip, cskip)));
			//	}
			//}
		}
	}
	if (_checkRect.contains(e->rect())) return;

	auto textWidth = qMax(width() - (_st.textPosition.x() + (_st.textPosition.x() - _st.diameter)), 1);

	p.setPen(_st.textFg);
	_text.drawLeftElided(p, _st.margin.left() + _st.textPosition.x(), _st.margin.top() + _st.textPosition.y(), textWidth, width());
}

void Radiobutton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	if (isDisabled() && !(was & StateFlag::Disabled)) {
		setCursor(style::cur_default);
	} else if (!isDisabled() && (was & StateFlag::Disabled)) {
		setCursor(style::cur_pointer);
	}

	auto now = state();
	if (!isDisabled() && (was & StateFlag::Over) && (now & StateFlag::Over)) {
		if ((was & StateFlag::Down) && !(now & StateFlag::Down)) {
			_group->setValue(_value);
		}
	}
}

int Radiobutton::resizeGetHeight(int newWidth) {
	return _st.height;
}

QImage Radiobutton::prepareRippleMask() const {
	return RippleAnimation::ellipseMask(QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

QPoint Radiobutton::prepareRippleStartPosition() const {
	auto position = mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
	if (QRect(0, 0, _st.rippleAreaSize, _st.rippleAreaSize).contains(position)) {
		return position;
	}
	return disabledRippleStartPosition();
}

Radiobutton::~Radiobutton() {
	_group->unregisterButton(this);
}

} // namespace Ui
