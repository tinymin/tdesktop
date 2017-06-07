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
#include "mtproto/sender.h"

namespace Ui {
class RadiobuttonGroup;
class Radiobutton;
class FlatLabel;
} // namespace Ui

class SelfDestructionBox : public BoxContent, private MTP::Sender {
	Q_OBJECT

public:
	SelfDestructionBox(QWidget*) {
	}

protected:
	void prepare() override;

private:
	std::vector<int> _ttlValues;
	object_ptr<Ui::FlatLabel> _description = { nullptr };
	std::shared_ptr<Ui::RadiobuttonGroup> _ttlGroup;
	std::vector<object_ptr<Ui::Radiobutton>> _options;

};
