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

constexpr const str_const LanguageCodes[] = {
	"en",
	"it",
	"es",
	"de",
	"nl",
<<<<<<< HEAD
	"ko",
=======
	"pt_BR",
<<<<<<< HEAD
>>>>>>> 9e43f1b5de3d44041264ad670ec7456cd419c268
=======
	"ko",
>>>>>>> 7b7df23e3b21b782bfc6fe0001ae5a6b08a006da
};
constexpr const int languageTest = -1, languageDefault = 0, languageCount = base::array_size(LanguageCodes);

class LangString : public QString {
public:
	LangString() {
	}
	LangString(const QString &str) : QString(str) {
	}

	LangString tag(ushort tag, const QString &replacement) {
		for (const QChar *s = constData(), *ch = s, *e = ch + size(); ch != e;) {
			if (*ch == TextCommand) {
				if (ch + 3 < e && (ch + 1)->unicode() == TextCommandLangTag && *(ch + 3) == TextCommand) {
					if ((ch + 2)->unicode() == 0x0020 + tag) {
						LangString result;
						result.reserve(size() + replacement.size() - 4);
						if (ch > s) result.append(midRef(0, ch - s));
						result.append(replacement);
						if (ch + 4 < e) result.append(midRef(ch - s + 4));
						return result;
					} else {
						ch += 4;
					}
				} else {
					const QChar *next = textSkipCommand(ch, e);
					if (next == ch) {
						++ch;
					} else {
						ch = next;
					}
				}
			} else {
				++ch;
			}
		}
		return *this;
	}

	LangString &operator=(const QString &str) {
		QString::operator=(str);
		return (*this);
	}

};

LangString langCounted(ushort key0, ushort tag, float64 value);

#include "lang_auto.h"

const char *langKeyName(LangKey key);

template <typename WithYear, typename WithoutYear>
inline LangString langDateMaybeWithYear(QDate date, WithYear withYear, WithoutYear withoutYear) {
	auto month = date.month();
	if (month <= 0 || month > 12) {
		return qsl("MONTH_ERR");
	};
	auto year = date.year();
	auto current = QDate::currentDate();
	auto currentYear = current.year();
	auto currentMonth = current.month();
	if (year != currentYear) {
		auto yearIsMuchGreater = [](int year, int otherYear) {
			return (year > otherYear + 1);
		};
		auto monthIsMuchGreater = [](int year, int month, int otherYear, int otherMonth) {
			return (year == otherYear + 1) && (month + 12 > otherMonth + 3);
		};
		if (false
			|| yearIsMuchGreater(year, currentYear)
			|| yearIsMuchGreater(currentYear, year)
			|| monthIsMuchGreater(year, month, currentYear, currentMonth)
			|| monthIsMuchGreater(currentYear, currentMonth, year, month)) {
			return withYear(month, year);
		}
	}
	return withoutYear(month, year);
}

inline LangString langDayOfMonth(const QDate &date) {
	auto day = date.day();
	return langDateMaybeWithYear(date, [day](int month, int year) {
		return lng_month_day_year(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_day, QString::number(day), lt_year, QString::number(year));
	}, [day](int month, int year) {
		return lng_month_day(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_day, QString::number(day));
	});
}

inline LangString langDayOfMonthFull(const QDate &date) {
	auto day = date.day();
	return langDateMaybeWithYear(date, [day](int month, int year) {
		return lng_month_day_year(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_day, QString::number(day), lt_year, QString::number(year));
	}, [day](int month, int year) {
		return lng_month_day(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_day, QString::number(day));
	});
}

inline LangString langMonthOfYear(int month, int year) {
	return (month > 0 && month <= 12) ? lng_month_year(lt_month, lang(LangKey(lng_month1_small + month - 1)), lt_year, QString::number(year)) : qsl("MONTH_ERR");
}

inline LangString langMonth(const QDate &date) {
	return langDateMaybeWithYear(date, [](int month, int year) {
		return langMonthOfYear(month, year);
	}, [](int month, int year) {
		return lang(LangKey(lng_month1_small + month - 1));
	});
}

inline LangString langMonthOfYearFull(int month, int year) {
	return (month > 0 && month <= 12) ? lng_month_year(lt_month, lang(LangKey(lng_month1 + month - 1)), lt_year, QString::number(year)) : qsl("MONTH_ERR");
}

inline LangString langMonthFull(const QDate &date) {
	return langDateMaybeWithYear(date, [](int month, int year) {
		return langMonthOfYearFull(month, year);
	}, [](int month, int year) {
		return lang(LangKey(lng_month1 + month - 1));
	});
}

inline LangString langDayOfWeek(int index) {
	return (index > 0 && index <= 7) ? lang(LangKey(lng_weekday1 + index - 1)) : qsl("DAY_ERR");
}

inline LangString langDayOfWeek(const QDate &date) {
	return langDayOfWeek(date.dayOfWeek());
}

inline LangString langDayOfWeekFull(int index) {
	return (index > 0 && index <= 7) ? lang(LangKey(lng_weekday1_full + index - 1)) : qsl("DAY_ERR");
}

inline LangString langDayOfWeekFull(const QDate &date) {
	return langDayOfWeekFull(date.dayOfWeek());
}

inline LangString langDateTime(const QDateTime &date) {
	return lng_mediaview_date_time(lt_date, langDayOfMonth(date.date()), lt_time, date.time().toString(cTimeFormat()));
}

inline LangString langDateTimeFull(const QDateTime &date) {
	return lng_mediaview_date_time(lt_date, langDayOfMonthFull(date.date()), lt_time, date.time().toString(cTimeFormat()));
}

QString langNewVersionText();
QString langNewVersionTextForLang(int langId);

class LangLoader {
public:
	const QString &errors() const;
	const QString &warnings() const;

protected:
	LangLoader() : _checked(false) {
		memset(_found, 0, sizeof(_found));
	}

	ushort tagIndex(const QByteArray &tag) const;
	LangKey keyIndex(const QByteArray &key) const;
	bool tagReplaced(LangKey key, ushort tag) const;
	LangKey subkeyIndex(LangKey key, ushort tag, ushort index) const;

	bool feedKeyValue(LangKey key, const QString &value);
	void foundKeyValue(LangKey key);

	void error(const QString &text) {
		_err.push_back(text);
	}
	void warning(const QString &text) {
		_warn.push_back(text);
	}

private:
	mutable QStringList _err, _warn;
	mutable QString _errors, _warnings;
	mutable bool _checked;
	bool _found[lngkeys_cnt];

	LangLoader(const LangLoader &);
	LangLoader &operator=(const LangLoader &);

};

class Translator : public QTranslator {
public:
	QString translate(const char *context, const char *sourceText, const char *disambiguation = 0, int n = -1) const override;

};

inline bool langFirstNameGoesSecond() {
	QString fullname = lang(lng_full_name__tagged);
	for (const QChar *s = fullname.constData(), *ch = s, *e = ch + fullname.size(); ch != e;) {
		if (*ch == TextCommand) {
			if (ch + 3 < e && (ch + 1)->unicode() == TextCommandLangTag && *(ch + 3) == TextCommand) {
				if ((ch + 2)->unicode() == 0x0020 + lt_last_name) {
					return true;
				} else if ((ch + 2)->unicode() == 0x0020 + lt_first_name) {
					break;
				}
			}
		}
	}
	return false;
}
