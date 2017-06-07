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
#include "boxes/about_box.h"

#include "lang.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "autoupdater.h"
#include "boxes/confirm_box.h"
#include "application.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "styles/style_boxes.h"
#include "platform/platform_file_utilities.h"

AboutBox::AboutBox(QWidget *parent)
: _version(this, lng_about_version(lt_version, QString::fromLatin1(AppVersionStr.c_str()) + (cAlphaVersion() ? " alpha" : "") + (cBetaVersion() ? qsl(" beta %1").arg(cBetaVersion()) : QString())), st::aboutVersionLink)
, _text1(this, lang(lng_about_text_1), Ui::FlatLabel::InitType::Rich, st::aboutLabel)
, _text2(this, lang(lng_about_text_2), Ui::FlatLabel::InitType::Rich, st::aboutLabel)
, _text3(this, st::aboutLabel) {
}

void AboutBox::prepare() {
	setTitle(qsl("Telegram Desktop"));

	addButton(lang(lng_close), [this] { closeBox(); });

	_text3->setRichText(lng_about_text_3(lt_faq_open, qsl("[a href=\"%1\"]").arg(telegramFaqLink()), lt_faq_close, qsl("[/a]")));

	_version->setClickedCallback([this] { showVersionHistory(); });

	setDimensions(st::aboutWidth, st::aboutTextTop + _text1->height() + st::aboutSkip + _text2->height() + st::aboutSkip + _text3->height());
}

void AboutBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	_version->moveToLeft(st::boxPadding.left(), st::aboutVersionTop);
	_text1->moveToLeft(st::boxPadding.left(), st::aboutTextTop);
	_text2->moveToLeft(st::boxPadding.left(), _text1->y() + _text1->height() + st::aboutSkip);
	_text3->moveToLeft(st::boxPadding.left(), _text2->y() + _text2->height() + st::aboutSkip);
}

void AboutBox::showVersionHistory() {
	if (cRealBetaVersion()) {
		auto url = qsl("https://tdesktop.com/");
		switch (cPlatform()) {
		case dbipWindows: url += qsl("win/%1.zip"); break;
		case dbipMac: url += qsl("mac/%1.zip"); break;
		case dbipMacOld: url += qsl("mac32/%1.zip"); break;
		case dbipLinux32: url += qsl("linux32/%1.tar.xz"); break;
		case dbipLinux64: url += qsl("linux/%1.tar.xz"); break;
		}
		url = url.arg(qsl("tbeta%1_%2").arg(cRealBetaVersion()).arg(countBetaVersionSignature(cRealBetaVersion())));

		Application::clipboard()->setText(url);

		Ui::show(Box<InformBox>("The link to the current private beta version of Telegram Desktop was copied to the clipboard."));
	} else {
		QDesktopServices::openUrl(qsl("https://desktop.telegram.org/changelog"));
	}
}

void AboutBox::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
		closeBox();
	} else {
		BoxContent::keyPressEvent(e);
	}
}

QString telegramFaqLink() {
	QString result = qsl("https://telegram.org/faq");
	if (cLang() > languageDefault && cLang() < languageCount) {
		const char *code = LanguageCodes[cLang()].c_str();
		if (qstr("de") == code || qstr("es") == code || qstr("it") == code || qstr("ko") == code) {
			result += qsl("/") + code;
		} else if (qstr("pt_BR") == code) {
			result += qsl("/br");
		}
	}
	return result;
}

QString currentVersionText() {
	auto result = QString::fromLatin1(AppVersionStr.c_str());
	if (cAlphaVersion()) {
		result += " alpha";
	}
	if (cBetaVersion()) {
		result += qsl(" beta %1").arg(cBetaVersion() % 1000);
	}
	return result;
}
