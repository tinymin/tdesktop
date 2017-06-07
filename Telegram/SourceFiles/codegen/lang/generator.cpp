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
#include "codegen/lang/generator.h"

#include <memory>
#include <functional>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QPainter>

namespace codegen {
namespace lang {
namespace {

constexpr auto kMaxPluralVariants = 6;

char hexChar(uchar ch) {
	if (ch < 10) {
		return '0' + ch;
	} else if (ch < 16) {
		return 'a' + (ch - 10);
	}
	return '0';
}

char hexSecondChar(char ch) {
	return hexChar((*reinterpret_cast<uchar*>(&ch)) & 0x0F);
}

char hexFirstChar(char ch) {
	return hexChar((*reinterpret_cast<uchar*>(&ch)) >> 4);
}

QString stringToEncodedString(const QString &str) {
	QString result, lineBreak = "\\\n";
	result.reserve(str.size() * 8);
	bool writingHexEscapedCharacters = false, startOnNewLine = false;
	int lastCutSize = 0;
	auto utf = str.toUtf8();
	for (auto ch : utf) {
		if (result.size() - lastCutSize > 80) {
			startOnNewLine = true;
			result.append(lineBreak);
			lastCutSize = result.size();
		}
		if (ch == '\n') {
			writingHexEscapedCharacters = false;
			result.append("\\n");
		} else if (ch == '\t') {
			writingHexEscapedCharacters = false;
			result.append("\\t");
		} else if (ch == '"' || ch == '\\') {
			writingHexEscapedCharacters = false;
			result.append('\\').append(ch);
		} else if (ch < 32 || static_cast<uchar>(ch) > 127) {
			writingHexEscapedCharacters = true;
			result.append("\\x").append(hexFirstChar(ch)).append(hexSecondChar(ch));
		} else {
			if (writingHexEscapedCharacters) {
				writingHexEscapedCharacters = false;
				result.append("\"\"");
			}
			result.append(ch);
		}
	}
	return '"' + (startOnNewLine ? lineBreak : QString()) + result + '"';
}

QString stringToEncodedString(const std::string &str) {
	return stringToEncodedString(QString::fromStdString(str));
}

QString stringToBinaryArray(const std::string &str) {
	QStringList rows, chars;
	chars.reserve(13);
	rows.reserve(1 + (str.size() / 13));
	for (uchar ch : str) {
		if (chars.size() > 12) {
			rows.push_back(chars.join(", "));
			chars.clear();
		}
		chars.push_back(QString("0x") + hexFirstChar(ch) + hexSecondChar(ch));
	}
	if (!chars.isEmpty()) {
		rows.push_back(chars.join(", "));
	}
	return QString("{") + ((rows.size() > 1) ? '\n' : ' ') + rows.join(",\n") + " }";
}

} // namespace

Generator::Generator(const Langpack &langpack, const QString &destBasePath, const common::ProjectInfo &project)
: langpack_(langpack)
, basePath_(destBasePath)
, baseName_(QFileInfo(basePath_).baseName())
, project_(project) {
}

bool Generator::writeHeader() {
	header_ = std::make_unique<common::CppFile>(basePath_ + ".h", project_);
	header_->stream() << "\
class LangString : public QString {\n\
public:\n\
	LangString() = default;\n\
	LangString(const QString &str) : QString(str) {\n\
	}\n\
	LangString &operator=(const QString &str) {\n\
		QString::operator=(str);\n\
		return *this;\n\
	}\n\
\n\
	LangString tag(ushort tag, const QString &replacement);\n\
\n\
};\n\
\n\
LangString langCounted(ushort key0, ushort tag, float64 value);\n\
\n";
	auto index = 0;
	for (auto &tag : langpack_.tags) {
		header_->stream() << "enum lngtag_" << tag.tag << " { lt_" << tag.tag << " = " << index++ << " };\n";
	}
	header_->stream() << "\
\n\
constexpr auto lngtags_cnt = " << langpack_.tags.size() << ";\n\
constexpr auto lngtags_max_counted_values = " << kMaxPluralVariants << ";\n\
\n\
enum LangKey {\n";
	for (auto &entry : langpack_.entries) {
		header_->stream() << "\t" << getFullKey(entry) << ",\n";
	}
	header_->stream() << "\
\n\
	lngkeys_cnt,\n\
};\n\
\n\
LangString lang(LangKey key);\n\
\n\
LangString langOriginal(LangKey key);\n\
\n";
	for (auto &entry : langpack_.entries) {
		if (!entry.tags.empty()) {
			auto &key = entry.key;
			auto params = QStringList();
			auto invokations = QStringList();
			for (auto &tagData : entry.tags) {
				auto &tag = tagData.tag;
				auto isPlural = isTagPlural(key, tag);
				params.push_back("lngtag_" + tag + ", " + (isPlural ? "float64 " : "const QString &") + tag + "__val");
				invokations.push_back("tag(lt_" + tag + ", " + (isPlural ? ("langCounted(" + key + "__" + tag + "0, lt_" + tag + ", " + tag + "__val)") : (tag + "__val")) + ")");
			}
			header_->stream() << "\
inline LangString " << entry.key << "(" << params.join(QString(", ")) << ") {\n\
	return lang(" << entry.key << "__tagged)." << invokations.join('.') << ";\n\
}\n\
\n";
		}
	}
	return header_->finalize();
}

bool Generator::writeSource() {
	source_ = std::make_unique<common::CppFile>(basePath_ + ".cpp", project_);

	source_->include("lang.h").pushNamespace().stream() << "\
const char *_langKeyNames[lngkeys_cnt] = {\n\
\n";
	for (auto &entry : langpack_.entries) {
		source_->stream() << "\"" << entry.key << "\",\n";
	}
	source_->stream() << "\
\n\
};\n\
\n\
LangString _langValues[lngkeys_cnt], _langValuesOriginal[lngkeys_cnt];\n\
\n\
void set(LangKey key, const QString &val) {\n\
	_langValues[key] = val;\n\
}\n\
\n\
class LangInit {\n\
public:\n\
	LangInit() {\n";
	for (auto &entry : langpack_.entries) {
		source_->stream() << "\t\tset(" << getFullKey(entry) << ", QString::fromUtf8(" << stringToEncodedString(entry.value) << "));\n";
	}
	source_->stream() << "\
	}\n\
\n\
};\n\
\n\
LangInit _langInit;\n\
\n";

	source_->popNamespace().stream() << "\
\n\
LangString lang(LangKey key) {\n\
	return (key < 0 || key > lngkeys_cnt) ? QString() : _langValues[key];\n\
}\n\
\n\
LangString langOriginal(LangKey key) {\n\
	return (key < 0 || key > lngkeys_cnt || _langValuesOriginal[key] == qsl(\"{}\")) ? QString() : (_langValuesOriginal[key].isEmpty() ? _langValues[key] : _langValuesOriginal[key]);\n\
}\n\
\n\
const char *langKeyName(LangKey key) {\n\
	return (key < 0 || key > lngkeys_cnt) ? \"\" : _langKeyNames[key];\n\
}\n\
\n\
ushort LangLoader::tagIndex(QLatin1String tag) const {\n\
	auto size = tag.size();\n\
	auto data = tag.data();\n";

	auto tagsSet = std::set<QString, std::greater<QString>>();
	for (auto &tag : langpack_.tags) {
		tagsSet.insert(tag.tag);
	}

	writeSetSearch(tagsSet, [](const QString &tag) {
		return "lt_" + tag;
	}, "lngtags_cnt");

	source_->stream() << "\
}\n\
\n\
LangKey LangLoader::keyIndex(QLatin1String key) const {\n\
	auto size = key.size();\n\
	auto data = key.data();\n";

	auto taggedKeys = std::map<QString, QString>();
	auto keysSet = std::set<QString, std::greater<QString>>();
	for (auto &entry : langpack_.entries) {
		if (entry.key.mid(0, entry.key.size() - 1).endsWith("__count")) {
			continue;
		}

		auto full = getFullKey(entry);
		if (full != entry.key) {
			taggedKeys.emplace(entry.key, full);
		}
		keysSet.insert(entry.key);
	}

	writeSetSearch(keysSet, [&taggedKeys](const QString &key) {
		auto it = taggedKeys.find(key);
		return (it != taggedKeys.end()) ? it->second : key;
	}, "lngkeys_cnt");

	source_->stream() << "\
}\n\
\n\
bool LangLoader::tagReplaced(LangKey key, ushort tag) const {\n\
	switch (key) {\n";

	for (auto &entry : langpack_.entries) {
		if (entry.tags.empty()) {
			continue;
		}
		source_->stream() << "\
	case " << entry.key << "__tagged: {\n\
		switch (tag) {\n";
		for (auto &tag : entry.tags) {
			source_->stream() << "\
		case lt_" << tag.tag << ":\n";
		}
		source_->stream() << "\
			return true;\n\
		}\n\
	} break;\n";
	}

	source_->stream() << "\
	}\
\n\
	return false;\n\
}\n\
\n\
LangKey LangLoader::subkeyIndex(LangKey key, ushort tag, ushort index) const {\n\
	if (index >= lngtags_max_counted_values) return lngkeys_cnt;\n\
\n\
	switch (key) {\n";

	for (auto &entry : langpack_.entries) {
		auto cases = QString();
		for (auto &tag : entry.tags) {
			if (isTagPlural(entry.key, tag.tag)) {
				cases += "\t\t\tcase lt_" + tag.tag + ": return LangKey(" + entry.key + "__" + tag.tag + "0 + index);\n";
			}
		}
		if (cases.isEmpty()) {
			continue;
		}
		source_->stream() << "\
	case " << entry.key << "__tagged: {\n\
		switch (tag) {\n\
" << cases << "\
		}\n\
	} break;\n";
	}

	source_->stream() << "\
	}\n\
\n\
	return lngkeys_cnt;\n\
}\n\
\n\
bool LangLoader::feedKeyValue(LangKey key, const QString &value) {\n\
	if (key < lngkeys_cnt) {\n\
		_found[key] = 1;\n\
		if (_langValuesOriginal[key].isEmpty()) {\n\
			_langValuesOriginal[key] = _langValues[key].isEmpty() ? qsl(\"{}\") : _langValues[key];\n\
		}\n\
		_langValues[key] = value;\n\
		return true;\n\
	}\n\
	return false;\n\
}\n";

	return source_->finalize();
}

template <typename ComputeResult>
void Generator::writeSetSearch(const std::set<QString, std::greater<QString>> &set, ComputeResult computeResult, const QString &invalidResult) {
	auto tabs = [](int size) {
		return QString(size, '\t');
	};

	enum class UsedCheckType {
		Switch,
		If,
		UpcomingIf,
	};
	auto checkTypes = QVector<UsedCheckType>();
	auto checkLengthHistory = QVector<int>(1, 0);
	auto chars = QString();
	auto tabsUsed = 1;

	// Returns true if at least one check was finished.
	auto finishChecksTillKey = [this, &chars, &checkTypes, &checkLengthHistory, &tabsUsed, tabs](const QString &key) {
		auto result = false;
		while (!chars.isEmpty() && key.midRef(0, chars.size()) != chars) {
			result = true;

			auto wasType = checkTypes.back();
			chars.resize(chars.size() - 1);
			checkTypes.pop_back();
			checkLengthHistory.pop_back();
			if (wasType == UsedCheckType::Switch || wasType == UsedCheckType::If) {
				--tabsUsed;
				if (wasType == UsedCheckType::Switch) {
					source_->stream() << tabs(tabsUsed) << "break;\n";
				}
				if ((!chars.isEmpty() && key.midRef(0, chars.size()) != chars) || key == chars) {
					source_->stream() << tabs(tabsUsed) << "}\n";
				}
			}
		}
		return result;
	};

	// Check if we can use "if" for a check on "charIndex" in "it" (otherwise only "switch")
	auto canUseIfForCheck = [](auto it, auto end, int charIndex) {
		auto key = *it;
		auto i = it;
		auto keyStart = key.mid(0, charIndex);
		for (++i; i != end; ++i) {
			auto nextKey = *i;
			if (nextKey.mid(0, charIndex) != keyStart) {
				return true;
			} else if (nextKey.size() > charIndex && nextKey[charIndex] != key[charIndex]) {
				return false;
			}
		}
		return true;
	};

	auto countMinimalLength = [](auto it, auto end, int charIndex) {
		auto key = *it;
		auto i = it;
		auto keyStart = key.mid(0, charIndex);
		auto result = key.size();
		for (++i; i != end; ++i) {
			auto nextKey = *i;
			if (nextKey.mid(0, charIndex) != keyStart) {
				break;
			} else if (nextKey.size() > charIndex && result > nextKey.size()) {
				result = nextKey.size();
			}
		}
		return result;
	};

	for (auto i = set.begin(), e = set.end(); i != e; ++i) {
		// If we use just "auto" here and "name" becomes mutable,
		// the operator[] will return QCharRef instead of QChar,
		// and "auto ch = name[index]" will behave like "auto &ch =",
		// if you assign something to "ch" after that you'll change "name" (!)
		const auto name = *i;

		auto weContinueOldSwitch = finishChecksTillKey(name);
		while (chars.size() != name.size()) {
			auto checking = chars.size();
			auto partialKey = name.mid(0, checking);

			auto keyChar = name[checking];
			auto usedIfForCheckCount = 0;
			auto minimalLengthCheck = countMinimalLength(i, e, checking);
			for (; checking + usedIfForCheckCount != name.size(); ++usedIfForCheckCount) {
				if (!canUseIfForCheck(i, e, checking + usedIfForCheckCount)
					|| countMinimalLength(i, e, checking + usedIfForCheckCount) != minimalLengthCheck) {
					break;
				}
			}
			auto usedIfForCheck = !weContinueOldSwitch && (usedIfForCheckCount > 0);
			auto checkLengthCondition = QString();
			if (weContinueOldSwitch) {
				weContinueOldSwitch = false;
			} else {
				checkLengthCondition = (minimalLengthCheck > checkLengthHistory.back()) ? ("size >= " + QString::number(minimalLengthCheck)) : QString();
				if (!usedIfForCheck) {
					source_->stream() << tabs(tabsUsed) << (checkLengthCondition.isEmpty() ? QString() : ("if (" + checkLengthCondition + ") ")) << "switch (data[" << checking << "]) {\n";
				}
			}
			if (usedIfForCheck) {
				auto conditions = QStringList();
				if (usedIfForCheckCount > 1) {
					conditions.push_back("!memcmp(data + " + QString::number(checking) + ", \"" + name.mid(checking, usedIfForCheckCount) + "\", " + QString::number(usedIfForCheckCount) + ")");
				} else {
					conditions.push_back("data[" + QString::number(checking) + "] == '" + keyChar + "'");
				}
				if (!checkLengthCondition.isEmpty()) {
					conditions.push_front(checkLengthCondition);
				}
				source_->stream() << tabs(tabsUsed) << "if (" << conditions.join(" && ") << ") {\n";
				checkTypes.push_back(UsedCheckType::If);
				for (auto i = 1; i != usedIfForCheckCount; ++i) {
					checkTypes.push_back(UsedCheckType::UpcomingIf);
					chars.push_back(keyChar);
					checkLengthHistory.push_back(qMax(minimalLengthCheck, checkLengthHistory.back()));
					keyChar = name[checking + i];
				}
			} else {
				source_->stream() << tabs(tabsUsed) << "case '" << keyChar << "':\n";
				checkTypes.push_back(UsedCheckType::Switch);
			}
			++tabsUsed;
			chars.push_back(keyChar);
			checkLengthHistory.push_back(qMax(minimalLengthCheck, checkLengthHistory.back()));
		}
		source_->stream() << tabs(tabsUsed) << "return (size == " << chars.size() << ") ? " << computeResult(name) << " : " << invalidResult << ";\n";
	}
	finishChecksTillKey(QString());

	source_->stream() << "\
\n\
	return " << invalidResult << ";\n";
}

QString Generator::getFullKey(const Langpack::Entry &entry) {
	if (entry.tags.empty()) {
		return entry.key;
	}
	return entry.key + "__tagged";
}

bool Generator::isTagPlural(const QString &key, const QString &tag) const {
	auto searchForKey = key + "__" + tag + "0";
	for (auto &entry : langpack_.entries) {
		if (entry.key == searchForKey) {
			return true;
		}
	}
	return false;
}

} // namespace lang
} // namespace codegen
