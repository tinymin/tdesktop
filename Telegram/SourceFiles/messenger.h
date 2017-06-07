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

#include "base/observer.h"

namespace App {
void quit();
} // namespace App

namespace MTP {
class DcOptions;
class Instance;
} // namespace MTP

class AuthSession;
class AuthSessionData;
class MainWidget;
class FileUploader;
class Translator;

namespace Local {
struct StoredAuthSession;
} // namespace Local

namespace Media {
namespace Audio {
class Instance;
} // namespace Audio
} // namespace Media

class Messenger final : public QObject, public RPCSender, private base::Subscriber {
	Q_OBJECT

public:
	Messenger();

	Messenger(const Messenger &other) = delete;
	Messenger &operator=(const Messenger &other) = delete;

	~Messenger();

	MainWindow *mainWindow();
	QPoint getPointForCallPanelCenter() const;
	QImage logo() const {
		return _logo;
	}
	QImage logoNoMargin() const {
		return _logoNoMargin;
	}

	static Messenger *InstancePointer();
	static Messenger &Instance() {
		auto result = InstancePointer();
		t_assert(result != nullptr);
		return *result;
	}

	// MTProto components.
	MTP::DcOptions *dcOptions() {
		return _dcOptions.get();
	}

	// Set from legacy storage.
	void setMtpMainDcId(MTP::DcId mainDcId);
	void setMtpKey(MTP::DcId dcId, const MTP::AuthKey::Data &keyData);
	void setAuthSessionUserId(UserId userId);
	void setAuthSessionFromStorage(std::unique_ptr<Local::StoredAuthSession> data);
	AuthSessionData *getAuthSessionData();

	// Serialization.
	QByteArray serializeMtpAuthorization() const;
	void setMtpAuthorization(const QByteArray &serialized);

	void startMtp();
	MTP::Instance *mtp() {
		return _mtproto.get();
	}
	void suggestMainDcId(MTP::DcId mainDcId);
	void destroyStaleAuthorizationKeys();

	// AuthSession component.
	AuthSession *authSession() {
		return _authSession.get();
	}
	void authSessionCreate(UserId userId);
	void authSessionDestroy();
	base::Observable<void> &authSessionChanged() {
		return _authSessionChanged;
	}

	// Media component.
	Media::Audio::Instance &audio() {
		return *_audio;
	}

	void setInternalLinkDomain(const QString &domain) const;
	QString createInternalLink(const QString &query) const;
	QString createInternalLinkFull(const QString &query) const;

	FileUploader *uploader();
	void uploadProfilePhoto(const QImage &tosend, const PeerId &peerId);
	void regPhotoUpdate(const PeerId &peer, const FullMsgId &msgId);
	bool isPhotoUpdating(const PeerId &peer);
	void cancelPhotoUpdate(const PeerId &peer);

	void selfPhotoCleared(const MTPUserProfilePhoto &result);
	void chatPhotoCleared(PeerId peer, const MTPUpdates &updates);
	void selfPhotoDone(const MTPphotos_Photo &result);
	void chatPhotoDone(PeerId peerId, const MTPUpdates &updates);
	bool peerPhotoFail(PeerId peerId, const RPCError &e);
	void peerClearPhoto(PeerId peer);

	void writeUserConfigIn(TimeMs ms);

	void killDownloadSessionsStart(MTP::DcId dcId);
	void killDownloadSessionsStop(MTP::DcId dcId);

	void checkLocalTime();
	void checkMapVersion();
	void setupPasscode();
	void clearPasscode();
	base::Observable<void> &passcodedChanged() {
		return _passcodedChanged;
	}

	void quitPreventFinished();

	void handleAppActivated();
	void handleAppDeactivated();

	void call_handleHistoryUpdate();
	void call_handleUnreadCounterUpdate();
	void call_handleDelayedPeerUpdates();
	void call_handleObservables();

signals:
	void peerPhotoDone(PeerId peer);
	void peerPhotoFail(PeerId peer);

public slots:
	void onAllKeysDestroyed();

	void photoUpdated(const FullMsgId &msgId, bool silent, const MTPInputFile &file);

	void onSwitchDebugMode();
	void onSwitchWorkMode();
	void onSwitchTestMode();

	void killDownloadSessions();
	void onAppStateChanged(Qt::ApplicationState state);

private:
	void destroyMtpKeys(MTP::AuthKeysList &&keys);
	void startLocalStorage();
	void loadLanguage();

	friend void App::quit();
	static void QuitAttempt();
	void quitDelayed();

	QMap<FullMsgId, PeerId> photoUpdates;

	QMap<MTP::DcId, TimeMs> killDownloadSessionTimes;
	SingleTimer killDownloadSessionsTimer;

	// Some fields are just moved from the declaration.
	struct Private;
	const std::unique_ptr<Private> _private;

	std::unique_ptr<MainWindow> _window;
	FileUploader *_uploader = nullptr;

	std::unique_ptr<Translator> _translator;
	std::unique_ptr<MTP::DcOptions> _dcOptions;
	std::unique_ptr<MTP::Instance> _mtproto;
	std::unique_ptr<MTP::Instance> _mtprotoForKeysDestroy;
	std::unique_ptr<AuthSession> _authSession;
	base::Observable<void> _authSessionChanged;
	base::Observable<void> _passcodedChanged;

	std::unique_ptr<Media::Audio::Instance> _audio;
	QImage _logo;
	QImage _logoNoMargin;

};
