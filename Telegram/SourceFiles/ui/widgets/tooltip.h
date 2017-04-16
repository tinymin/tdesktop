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

 Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
 Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
 */
#pragma once

namespace style {
struct Tooltip;
} // namespace style

namespace Ui {

class AbstractTooltipShower {
public:
	virtual QString tooltipText() const = 0;
	virtual QPoint tooltipPos() const = 0;
	virtual const style::Tooltip *tooltipSt() const;
	virtual ~AbstractTooltipShower();
};

class Tooltip : public TWidget {
	Q_OBJECT

public:
	static void Show(int32 delay, const AbstractTooltipShower *shower);
	static void Hide();

private slots:
	void onShow();
	void onWndActiveChanged();
	void onHideByLeave();

protected:
	void paintEvent(QPaintEvent *e) override;
	void hideEvent(QHideEvent *e) override;

	bool eventFilter(QObject *o, QEvent *e) override;

private:
	Tooltip();
	~Tooltip();

	void popup(const QPoint &p, const QString &text, const style::Tooltip *st);

	friend class AbstractTooltipShower;
	const AbstractTooltipShower *_shower = nullptr;
	QTimer _showTimer;

	Text _text;
	QPoint _point;

	const style::Tooltip *_st = nullptr;

	QTimer _hideByLeaveTimer;
	bool _useTransparency = true;

};

} // namespace Ui
