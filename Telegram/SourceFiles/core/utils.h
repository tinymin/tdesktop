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

#include "core/basic_types.h"
#include <array>
#include <vector>
#include <algorithm>
#include <set>
#include <gsl/gsl>

#ifdef OS_MAC_OLD
namespace gsl {

inline span<char> make_span(QByteArray &container) {
	return span<char>(container.begin(), container.end());
}

inline span<const char> make_span(const QByteArray &container) {
	return span<const char>(container.begin(), container.end());
}

} // namespace gsl
#endif // OS_MAC_OLD

// Release build assertions.
inline void t_noop() {
}
[[noreturn]] inline void t_assert_fail(const char *message, const char *file, int32 line) {
	auto info = qsl("%1 %2:%3").arg(message).arg(file).arg(line);
	LOG(("Assertion Failed! ") + info);
	SignalHandlers::setCrashAnnotation("Assertion", info);

	// Crash with access violation and generate crash report.
	volatile int *t_assert_nullptr = nullptr;
	*t_assert_nullptr = 0;

	// Silent the possible failure to comply noreturn warning.
	std::abort();
}
#define t_assert_full(condition, message, file, line) ((GSL_UNLIKELY(!(condition))) ? t_assert_fail(message, file, line) : t_noop())
#define t_assert_c(condition, comment) t_assert_full(condition, "\"" #condition "\" (" comment ")", __FILE__, __LINE__)
#define t_assert(condition) t_assert_full(condition, "\"" #condition "\"", __FILE__, __LINE__)

// Declare our own versions of Expects() and Ensures().
// Let them crash with reports and logging.
#ifdef Expects
#undef Expects
#endif // Expects
#define Expects(condition) t_assert_full(condition, "\"" #condition "\"", __FILE__, __LINE__)

#ifdef Ensures
#undef Ensures
#endif // Ensures
#define Ensures(condition) t_assert_full(condition, "\"" #condition "\"", __FILE__, __LINE__)

#ifdef Unexpected
#undef Unexpected
#endif // Unexpected
#define Unexpected(message) t_assert_fail("Unexpected: " message, __FILE__, __LINE__)

// Define specializations for QByteArray for Qt 5.3.2, because
// QByteArray in Qt 5.3.2 doesn't declare "pointer" subtype.
#ifdef OS_MAC_OLD
namespace gsl {

template <>
inline span<char> make_span<QByteArray>(QByteArray &cont) {
	return span<char>(cont.data(), cont.size());
}

template <>
inline span<const char> make_span(const QByteArray &cont) {
	return span<const char>(cont.constData(), cont.size());
}

} // namespace gsl
#endif // OS_MAC_OLD

namespace base {

template <typename T, size_t N>
inline constexpr size_t array_size(const T(&)[N]) {
	return N;
}

template <typename T>
inline T take(T &source) {
	return std::exchange(source, T());
}

namespace internal {

template <typename D, typename T>
inline constexpr D up_cast_helper(std::true_type, T object) {
	return object;
}

template <typename D, typename T>
inline constexpr D up_cast_helper(std::false_type, T object) {
	return nullptr;
}

} // namespace internal

template <typename D, typename T>
inline constexpr D up_cast(T object) {
	using DV = std::decay_t<decltype(*D())>;
	using TV = std::decay_t<decltype(*T())>;
	return internal::up_cast_helper<D>(std::integral_constant<bool, std::is_base_of<DV, TV>::value || std::is_same<DV, TV>::value>(), object);
}

template <typename Lambda>
class scope_guard_helper {
public:
	scope_guard_helper(Lambda on_scope_exit) : _handler(std::move(on_scope_exit)) {
	}
	void dismiss() {
		_dismissed = true;
	}
	~scope_guard_helper() {
		if (!_dismissed) {
			_handler();
		}
	}

private:
	Lambda _handler;
	bool _dismissed = false;

};

template <typename Lambda>
scope_guard_helper<Lambda> scope_guard(Lambda on_scope_exit) {
	return scope_guard_helper<Lambda>(std::move(on_scope_exit));
}

template <typename Container, typename T>
inline bool contains(const Container &container, const T &value) {
	auto end = std::end(container);
	return std::find(std::begin(container), end, value) != end;
}

// We need a custom comparator for std::set<std::unique_ptr<T>>::find to work with pointers.
// thanks to http://stackoverflow.com/questions/18939882/raw-pointer-lookup-for-sets-of-unique-ptrs
template <typename T>
struct pointer_comparator {
	using is_transparent = std::true_type;

	// helper does some magic in order to reduce the number of
	// pairs of types we need to know how to compare: it turns
	// everything into a pointer, and then uses `std::less<T*>`
	// to do the comparison:
	struct helper {
		T *ptr = nullptr;
		helper() = default;
		helper(const helper &other) = default;
		helper(T *p) : ptr(p) {
		}
		template <typename ...Ts>
		helper(const std::shared_ptr<Ts...> &other) : ptr(other.get()) {
		}
		template <typename ...Ts>
		helper(const std::unique_ptr<Ts...> &other) : ptr(other.get()) {
		}
		bool operator<(helper other) const {
			return std::less<T*>()(ptr, other.ptr);
		}
	};

	// without helper, we'd need 2^n different overloads, where
	// n is the number of types we want to support (so, 8 with
	// raw pointers, unique pointers, and shared pointers).  That
	// seems silly.
	// && helps enforce rvalue use only
	bool operator()(const helper &&lhs, const helper &&rhs) const {
		return lhs < rhs;
	}

};

template <typename T>
using set_of_unique_ptr = std::set<std::unique_ptr<T>, base::pointer_comparator<T>>;

template <typename T>
using set_of_shared_ptr = std::set<std::shared_ptr<T>, base::pointer_comparator<T>>;

using byte_span = gsl::span<gsl::byte>;
using const_byte_span = gsl::span<const gsl::byte>;
using byte_vector = std::vector<gsl::byte>;
template <size_t N>
using byte_array = std::array<gsl::byte, N>;

inline void copy_bytes(byte_span destination, const_byte_span source) {
	Expects(destination.size() >= source.size());
	memcpy(destination.data(), source.data(), source.size());
}

inline void set_bytes(byte_span destination, gsl::byte value) {
	memset(destination.data(), gsl::to_integer<unsigned char>(value), destination.size());
}

inline int compare_bytes(const_byte_span a, const_byte_span b) {
	auto aSize = a.size(), bSize = b.size();
	return (aSize > bSize) ? 1 : (aSize < bSize) ? -1 : memcmp(a.data(), b.data(), aSize);
}

} // namespace base

// using for_const instead of plain range-based for loop to ensure usage of const_iterator
// it is important for the copy-on-write Qt containers
// if you have "QVector<T*> v" then "for (T * const p : v)" will still call QVector::detach(),
// while "for_const (T *p, v)" won't and "for_const (T *&p, v)" won't compile
#define for_const(range_declaration, range_expression) for (range_declaration : std::as_const(range_expression))

template <typename Enum>
inline constexpr QFlags<Enum> qFlags(Enum v) {
	return QFlags<Enum>(v);
}

template <typename Lambda>
inline void InvokeQueued(QObject *context, Lambda &&lambda) {
	QObject proxy;
	QObject::connect(&proxy, &QObject::destroyed, context, std::forward<Lambda>(lambda), Qt::QueuedConnection);
}

static const int32 ScrollMax = INT_MAX;

extern uint64 _SharedMemoryLocation[];
template <typename T, unsigned int N>
T *SharedMemoryLocation() {
	static_assert(N < 4, "Only 4 shared memory locations!");
	return reinterpret_cast<T*>(_SharedMemoryLocation + N);
}

// see https://github.com/boostcon/cppnow_presentations_2012/blob/master/wed/schurr_cpp11_tools_for_class_authors.pdf
class str_const { // constexpr string
public:
	template<std::size_t N>
	constexpr str_const(const char(&a)[N]) : _str(a), _size(N - 1) {
	}
	constexpr char operator[](std::size_t n) const {
		return (n < _size) ? _str[n] :
#ifndef OS_MAC_OLD
			throw std::out_of_range("");
#else // OS_MAC_OLD
			throw std::exception();
#endif // OS_MAC_OLD
	}
	constexpr std::size_t size() const { return _size; }
	const char *c_str() const { return _str; }

private:
	const char* const _str;
	const std::size_t _size;

};

inline QString str_const_toString(const str_const &str) {
	return QString::fromUtf8(str.c_str(), str.size());
}

inline QByteArray str_const_toByteArray(const str_const &str) {
	return QByteArray::fromRawData(str.c_str(), str.size());
}

template <typename T>
inline void accumulate_max(T &a, const T &b) { if (a < b) a = b; }

template <typename T>
inline void accumulate_min(T &a, const T &b) { if (a > b) a = b; }

class Exception : public std::exception {
public:
	Exception(const QString &msg, bool isFatal = true) : _fatal(isFatal), _msg(msg.toUtf8()) {
		LOG(("Exception: %1").arg(msg));
	}
	bool fatal() const {
		return _fatal;
	}

	virtual const char *what() const throw() {
		return _msg.constData();
	}
	virtual ~Exception() throw() {
	}

private:
	bool _fatal;
	QByteArray _msg;

};

class MTPint;
using TimeId = int32;
TimeId myunixtime();
void unixtimeInit();
void unixtimeSet(TimeId servertime, bool force = false);
TimeId unixtime();
TimeId fromServerTime(const MTPint &serverTime);
void toServerTime(const TimeId &clientTime, MTPint &outServerTime);
uint64 msgid();
int32 reqid();

inline QDateTime date(int32 time = -1) {
	QDateTime result;
	if (time >= 0) result.setTime_t(time);
	return result;
}

inline QDateTime dateFromServerTime(const MTPint &time) {
	return date(fromServerTime(time));
}

inline QDateTime date(const MTPint &time) {
	return dateFromServerTime(time);
}

QDateTime dateFromServerTime(TimeId time);

inline void mylocaltime(struct tm * _Tm, const time_t * _Time) {
#ifdef Q_OS_WIN
	localtime_s(_Tm, _Time);
#else
	localtime_r(_Time, _Tm);
#endif
}

namespace ThirdParty {

void start();
void finish();

}

using TimeMs = int64;
bool checkms(); // returns true if time has changed
TimeMs getms(bool checked = false);

const static uint32 _md5_block_size = 64;
class HashMd5 {
public:

	HashMd5(const void *input = 0, uint32 length = 0);
	void feed(const void *input, uint32 length);
	int32 *result();

private:

	void init();
	void finalize();
	void transform(const uchar *block);

	bool _finalized;
	uchar _buffer[_md5_block_size];
	uint32 _count[2];
	uint32 _state[4];
	uchar _digest[16];

};

int32 hashCrc32(const void *data, uint32 len);

int32 *hashSha1(const void *data, uint32 len, void *dest); // dest - ptr to 20 bytes, returns (int32*)dest
inline std::array<char, 20> hashSha1(const void *data, int len) {
	auto result = std::array<char, 20>();
	hashSha1(data, len, result.data());
	return result;
}

int32 *hashSha256(const void *data, uint32 len, void *dest); // dest - ptr to 32 bytes, returns (int32*)dest
inline std::array<char, 32> hashSha256(const void *data, int size) {
	auto result = std::array<char, 32>();
	hashSha1(data, size, result.data());
	return result;
}

int32 *hashMd5(const void *data, uint32 len, void *dest); // dest = ptr to 16 bytes, returns (int32*)dest
inline std::array<char, 16> hashMd5(const void *data, int size) {
	auto result = std::array<char, 16>();
	hashMd5(data, size, result.data());
	return result;
}

char *hashMd5Hex(const int32 *hashmd5, void *dest); // dest = ptr to 32 bytes, returns (char*)dest
inline char *hashMd5Hex(const void *data, uint32 len, void *dest) { // dest = ptr to 32 bytes, returns (char*)dest
	return hashMd5Hex(HashMd5(data, len).result(), dest);
}
inline std::array<char, 32> hashMd5Hex(const void *data, int size) {
	auto result = std::array<char, 32>();
	hashMd5Hex(data, size, result.data());
	return result;
}

// good random (using openssl implementation)
void memset_rand(void *data, uint32 len);
template <typename T>
T rand_value() {
	T result;
	memset_rand(&result, sizeof(result));
	return result;
}

inline void memset_rand_bad(void *data, uint32 len) {
	for (uchar *i = reinterpret_cast<uchar*>(data), *e = i + len; i != e; ++i) {
		*i = uchar(rand() & 0xFF);
	}
}

template <typename T>
inline void memsetrnd_bad(T &value) {
	memset_rand_bad(&value, sizeof(value));
}

class ReadLockerAttempt {
public:

	ReadLockerAttempt(QReadWriteLock *_lock) : success(_lock->tryLockForRead()), lock(_lock) {
	}
	~ReadLockerAttempt() {
		if (success) {
			lock->unlock();
		}
	}

	operator bool() const {
		return success;
	}

private:

	bool success;
	QReadWriteLock *lock;

};

inline QString fromUtf8Safe(const char *str, int32 size = -1) {
	if (!str || !size) return QString();
	if (size < 0) size = int32(strlen(str));
	QString result(QString::fromUtf8(str, size));
	QByteArray back = result.toUtf8();
	if (back.size() != size || memcmp(back.constData(), str, size)) return QString::fromLocal8Bit(str, size);
	return result;
}

inline QString fromUtf8Safe(const QByteArray &str) {
	return fromUtf8Safe(str.constData(), str.size());
}

static const QRegularExpression::PatternOptions reMultiline(QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption);

template <typename T>
inline T snap(const T &v, const T &_min, const T &_max) {
	return (v < _min) ? _min : ((v > _max) ? _max : v);
}

template <typename T>
class ManagedPtr {
public:
	ManagedPtr() = default;
	ManagedPtr(T *p) : _data(p) {
	}
	T *operator->() const {
		return _data;
	}
	T *v() const {
		return _data;
	}

	explicit operator bool() const {
		return _data != nullptr;
	}

protected:
	using Parent = ManagedPtr<T>;
	T *_data = nullptr;

};

QString translitRusEng(const QString &rus);
QString rusKeyboardLayoutSwitch(const QString &from);

enum DBISendKey {
	dbiskEnter = 0,
	dbiskCtrlEnter = 1,
};

enum DBINotifyView {
	dbinvShowPreview = 0,
	dbinvShowName = 1,
	dbinvShowNothing = 2,
};

enum DBIWorkMode {
	dbiwmWindowAndTray = 0,
	dbiwmTrayOnly = 1,
	dbiwmWindowOnly = 2,
};

enum DBIConnectionType {
	dbictAuto = 0,
	dbictHttpAuto = 1, // not used
	dbictHttpProxy = 2,
	dbictTcpProxy = 3,
};

struct ProxyData {
	QString host;
	uint32 port = 0;
	QString user, password;
};

enum DBIScale {
	dbisAuto = 0,
	dbisOne = 1,
	dbisOneAndQuarter = 2,
	dbisOneAndHalf = 3,
	dbisTwo = 4,

	dbisScaleCount = 5,
};

static const int MatrixRowShift = 40000;

enum DBIPlatform {
	dbipWindows = 0,
	dbipMac = 1,
	dbipLinux64 = 2,
	dbipLinux32 = 3,
	dbipMacOld = 4,
};

enum DBIPeerReportSpamStatus {
	dbiprsNoButton = 0, // hidden, but not in the cloud settings yet
	dbiprsUnknown = 1, // contacts not loaded yet
	dbiprsShowButton = 2, // show report spam button, each show peer request setting from cloud
	dbiprsReportSent = 3, // report sent, but the report spam panel is not hidden yet
	dbiprsHidden = 4, // hidden in the cloud or not needed (bots, contacts, etc), no more requests
	dbiprsRequesting = 5, // requesting the cloud setting right now
};

template <int Size>
inline QString strMakeFromLetters(const uint32 (&letters)[Size]) {
	QString result;
	result.reserve(Size);
	for (int32 i = 0; i < Size; ++i) {
		result.push_back(QChar((((letters[i] >> 16) & 0xFF) << 8) | (letters[i] & 0xFF)));
	}
	return result;
}

class MimeType {
public:
	enum class Known {
		Unknown,
		TDesktopTheme,
		TDesktopPalette,
		WebP,
	};

	MimeType(const QMimeType &type) : _typeStruct(type) {
	}
	MimeType(Known type) : _type(type) {
	}
	QStringList globPatterns() const;
	QString filterString() const;
	QString name() const;

private:
	QMimeType _typeStruct;
	Known _type = Known::Unknown;

};

MimeType mimeTypeForName(const QString &mime);
MimeType mimeTypeForFile(const QFileInfo &file);
MimeType mimeTypeForData(const QByteArray &data);

#include <cmath>

inline int rowscount(int fullCount, int countPerRow) {
	return (fullCount + countPerRow - 1) / countPerRow;
}
inline int floorclamp(int value, int step, int lowest, int highest) {
	return qMin(qMax(value / step, lowest), highest);
}
inline int floorclamp(float64 value, int step, int lowest, int highest) {
	return qMin(qMax(static_cast<int>(std::floor(value / step)), lowest), highest);
}
inline int ceilclamp(int value, int step, int lowest, int highest) {
	return qMax(qMin((value + step - 1) / step, highest), lowest);
}
inline int ceilclamp(float64 value, int32 step, int32 lowest, int32 highest) {
	return qMax(qMin(static_cast<int>(std::ceil(value / step)), highest), lowest);
}

enum ForwardWhatMessages {
	ForwardSelectedMessages,
	ForwardContextMessage,
	ForwardPressedMessage,
	ForwardPressedLinkMessage
};

enum ShowLayerOption {
	CloseOtherLayers = 0x00,
	KeepOtherLayers = 0x01,
	ShowAfterOtherLayers = 0x03,

	AnimatedShowLayer = 0x00,
	ForceFastShowLayer = 0x04,
};
Q_DECLARE_FLAGS(ShowLayerOptions, ShowLayerOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(ShowLayerOptions);

static int32 FullArcLength = 360 * 16;
static int32 QuarterArcLength = (FullArcLength / 4);
static int32 MinArcLength = (FullArcLength / 360);
static int32 AlmostFullArcLength = (FullArcLength - MinArcLength);

template <typename T, typename... Args>
inline QSharedPointer<T> MakeShared(Args&&... args) {
	return QSharedPointer<T>(new T(std::forward<Args>(args)...));
}

// This pointer is used for global non-POD variables that are allocated
// on demand by createIfNull(lambda) and are never automatically freed.
template <typename T>
class NeverFreedPointer {
public:
	NeverFreedPointer() = default;
	NeverFreedPointer(const NeverFreedPointer<T> &other) = delete;
	NeverFreedPointer &operator=(const NeverFreedPointer<T> &other) = delete;

	template <typename... Args>
	void createIfNull(Args&&... args) {
		if (isNull()) {
			reset(new T(std::forward<Args>(args)...));
		}
	};

	T *data() const {
		return _p;
	}
	T *release() {
		return base::take(_p);
	}
	void reset(T *p = nullptr) {
		delete _p;
		_p = p;
	}
	bool isNull() const {
		return data() == nullptr;
	}

	void clear() {
		reset();
	}
	T *operator->() const {
		return data();
	}
	T &operator*() const {
		t_assert(!isNull());
		return *data();
	}
	explicit operator bool() const {
		return !isNull();
	}

private:
	T *_p;

};

// This pointer is used for static non-POD variables that are allocated
// on first use by constructor and are never automatically freed.
template <typename T>
class StaticNeverFreedPointer {
public:
	explicit StaticNeverFreedPointer(T *p) : _p(p) {
	}
	StaticNeverFreedPointer(const StaticNeverFreedPointer<T> &other) = delete;
	StaticNeverFreedPointer &operator=(const StaticNeverFreedPointer<T> &other) = delete;

	T *data() const {
		return _p;
	}
	T *release() {
		return base::take(_p);
	}
	void reset(T *p = nullptr) {
		delete _p;
		_p = p;
	}
	bool isNull() const {
		return data() == nullptr;
	}

	void clear() {
		reset();
	}
	T *operator->() const {
		return data();
	}
	T &operator*() const {
		t_assert(!isNull());
		return *data();
	}
	explicit operator bool() const {
		return !isNull();
	}

private:
	T *_p = nullptr;

};
