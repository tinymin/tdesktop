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

class AuthSession;

namespace Platform {
namespace Notifications {
class Manager;
} // namespace Notifications
} // namespace Platform

namespace Window {
namespace Notifications {

enum class ChangeType {
	SoundEnabled,
	IncludeMuted,
	DesktopEnabled,
	ViewParams,
	MaxCount,
	Corner,
	DemoIsShown,
};

} // namespace Notifications
} // namespace Window

namespace base {

template <>
struct custom_is_fast_copy_type<Window::Notifications::ChangeType> : public std::true_type {
};

} // namespace base

namespace Window {
namespace Notifications {

class Manager;

class System final : private base::Subscriber {
public:
	System(AuthSession *session);

	void createManager();

	void checkDelayed();
	void schedule(History *history, HistoryItem *item);
	void clearFromHistory(History *history);
	void clearFromItem(HistoryItem *item);
	void clearAll();
	void clearAllFast();
	void updateAll();

	base::Observable<ChangeType> &settingsChanged() {
		return _settingsChanged;
	}

	AuthSession *authSession() {
		return _authSession;
	}

private:
	void showNext();

	AuthSession *_authSession = nullptr;

	QMap<History*, QMap<MsgId, TimeMs>> _whenMaps;

	struct Waiter {
		Waiter(MsgId msg, TimeMs when, PeerData *notifyByFrom)
			: msg(msg)
			, when(when)
			, notifyByFrom(notifyByFrom) {
		}
		MsgId msg;
		TimeMs when;
		PeerData *notifyByFrom;
	};
	using Waiters = QMap<History*, Waiter>;
	Waiters _waiters;
	Waiters _settingWaiters;
	SingleTimer _waitTimer;

	QMap<History*, QMap<TimeMs, PeerData*>> _whenAlerts;

	std::unique_ptr<Manager> _manager;

	base::Observable<ChangeType> _settingsChanged;

};

class Manager {
public:
	Manager(System *system) : _system(system) {
	}

	void showNotification(HistoryItem *item, int forwardedCount) {
		doShowNotification(item, forwardedCount);
	}
	void updateAll() {
		doUpdateAll();
	}
	void clearAll() {
		doClearAll();
	}
	void clearAllFast() {
		doClearAllFast();
	}
	void clearFromItem(HistoryItem *item) {
		doClearFromItem(item);
	}
	void clearFromHistory(History *history) {
		doClearFromHistory(history);
	}

	void notificationActivated(PeerId peerId, MsgId msgId);
	void notificationReplied(PeerId peerId, MsgId msgId, const QString &reply);

	struct DisplayOptions {
		bool hideNameAndPhoto;
		bool hideMessageText;
		bool hideReplyButton;
	};
	static DisplayOptions getNotificationOptions(HistoryItem *item);

	virtual ~Manager() = default;

protected:
	System *system() const {
		return _system;
	}

	virtual void doUpdateAll() = 0;
	virtual void doShowNotification(HistoryItem *item, int forwardedCount) = 0;
	virtual void doClearAll() = 0;
	virtual void doClearAllFast() = 0;
	virtual void doClearFromItem(HistoryItem *item) = 0;
	virtual void doClearFromHistory(History *history) = 0;
	virtual void onBeforeNotificationActivated(PeerId peerId, MsgId msgId) {
	}
	virtual void onAfterNotificationActivated(PeerId peerId, MsgId msgId) {
	}

private:
	System *_system = nullptr;

};

class NativeManager : public Manager {
protected:
	using Manager::Manager;

	void doUpdateAll() override {
		doClearAllFast();
	}
	void doClearAll() override {
		doClearAllFast();
	}
	void doClearFromItem(HistoryItem *item) override {
	}
	void doShowNotification(HistoryItem *item, int forwardedCount) override;

	virtual void doShowNativeNotification(PeerData *peer, MsgId msgId, const QString &title, const QString &subtitle, const QString &msg, bool hideNameAndPhoto, bool hideReplyButton) = 0;

};

} // namespace Notifications
} // namespace Window
