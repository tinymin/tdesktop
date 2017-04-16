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
#include "media/player/media_player_cover.h"

#include "ui/widgets/labels.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/buttons.h"
#include "media/media_audio.h"
#include "media/view/media_clip_playback.h"
#include "media/player/media_player_button.h"
#include "media/player/media_player_instance.h"
#include "media/player/media_player_volume_controller.h"
#include "styles/style_media_player.h"
#include "styles/style_mediaview.h"

namespace Media {
namespace Player {

using ButtonState = PlayButtonLayout::State;

class CoverWidget::PlayButton : public Ui::AbstractButton {
public:
	PlayButton(QWidget *parent);

	void setState(ButtonState state) {
		_layout.setState(state);
	}
	void finishTransform() {
		_layout.finishTransform();
	}

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	PlayButtonLayout _layout;

};

CoverWidget::PlayButton::PlayButton(QWidget *parent) : Ui::AbstractButton(parent)
, _layout(st::mediaPlayerPanelButton, [this] { update(); }) {
	resize(st::mediaPlayerPanelButtonSize);
	setCursor(style::cur_pointer);
}

void CoverWidget::PlayButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	p.translate(st::mediaPlayerPanelButtonPosition.x(), st::mediaPlayerPanelButtonPosition.y());
	_layout.paint(p, st::mediaPlayerActiveFg);
}

CoverWidget::CoverWidget(QWidget *parent) : TWidget(parent)
, _nameLabel(this, st::mediaPlayerName)
, _timeLabel(this, st::mediaPlayerTime)
, _close(this, st::mediaPlayerPanelClose)
, _playback(std::make_unique<Clip::Playback>(new Ui::MediaSlider(this, st::mediaPlayerPanelPlayback)))
, _playPause(this)
, _volumeToggle(this, st::mediaPlayerVolumeToggle)
, _volumeController(this)
, _pinPlayer(this, st::mediaPlayerPanelPinButton)
, _repeatTrack(this, st::mediaPlayerRepeatButton) {
	setAttribute(Qt::WA_OpaquePaintEvent);
	resize(width(), st::mediaPlayerCoverHeight);

	_close->hide();
	_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	_timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	setMouseTracking(true);

	_playback->setChangeProgressCallback([this](float64 value) {
		handleSeekProgress(value);
	});
	_playback->setChangeFinishedCallback([this](float64 value) {
		handleSeekFinished(value);
	});
	_playPause->setClickedCallback([this] {
		instance()->playPauseCancelClicked();
	});

	updateRepeatTrackIcon();
	_repeatTrack->setClickedCallback([this] {
		instance()->toggleRepeat();
	});

	updateVolumeToggleIcon();
	_volumeToggle->setClickedCallback([this]() {
		Global::SetSongVolume((Global::SongVolume() > 0) ? 0. : Global::RememberedSongVolume());
		Global::RefSongVolumeChanged().notify();
	});
	subscribe(Global::RefSongVolumeChanged(), [this] { updateVolumeToggleIcon(); });
	subscribe(instance()->repeatChangedNotifier(), [this] {
		updateRepeatTrackIcon();
	});
	subscribe(instance()->playlistChangedNotifier(), [this] {
		handlePlaylistUpdate();
	});
	subscribe(instance()->updatedNotifier(), [this](const TrackState &state) {
		handleSongUpdate(state);
	});
	subscribe(instance()->songChangedNotifier(), [this] {
		handleSongChange();
	});
	handleSongChange();

	handleSongUpdate(mixer()->currentState(AudioMsgId::Type::Song));
	_playPause->finishTransform();
}

void CoverWidget::setPinCallback(ButtonCallback &&callback) {
	_pinPlayer->setClickedCallback(std::move(callback));
}

void CoverWidget::setCloseCallback(ButtonCallback &&callback) {
	_close->setClickedCallback(std::move(callback));
}

void CoverWidget::handleSeekProgress(float64 progress) {
	if (!_lastDurationMs) return;

	auto positionMs = snap(static_cast<TimeMs>(progress * _lastDurationMs), 0LL, _lastDurationMs);
	if (_seekPositionMs != positionMs) {
		_seekPositionMs = positionMs;
		updateTimeLabel();
		instance()->startSeeking(AudioMsgId::Type::Song);
	}
}

void CoverWidget::handleSeekFinished(float64 progress) {
	if (!_lastDurationMs) return;

	auto positionMs = snap(static_cast<TimeMs>(progress * _lastDurationMs), 0LL, _lastDurationMs);
	_seekPositionMs = -1;

	auto type = AudioMsgId::Type::Song;
	auto state = Media::Player::mixer()->currentState(type);
	if (state.id && state.duration) {
		Media::Player::mixer()->seek(type, qRound(progress * state.duration));
	}

	instance()->stopSeeking(type);
}

void CoverWidget::resizeEvent(QResizeEvent *e) {
	auto widthForName = width() - 2 * (st::mediaPlayerPanelPadding);
	widthForName -= _timeLabel->width() + 2 * st::normalFont->spacew;
	_nameLabel->resizeToWidth(widthForName);
	updateLabelPositions();
	_close->moveToRight(0, 0);

	int skip = (st::mediaPlayerPanelPlayback.seekSize.width() / 2);
	int length = (width() - 2 * st::mediaPlayerPanelPadding + st::mediaPlayerPanelPlayback.seekSize.width());
	_playback->setGeometry(st::mediaPlayerPanelPadding - skip, st::mediaPlayerPanelPlaybackTop, length, 2 * st::mediaPlayerPanelPlaybackPadding + st::mediaPlayerPanelPlayback.width);

	auto top = st::mediaPlayerPanelVolumeToggleTop;
	auto right = st::mediaPlayerPanelPlayLeft;
	_repeatTrack->moveToRight(right, top); right += _repeatTrack->width();
	_pinPlayer->moveToRight(right, top); right += _pinPlayer->width() + st::mediaPlayerPanelVolumeSkip;
	_volumeController->moveToRight(right, st::mediaPlayerPanelVolumeTop); right += _volumeController->width() + st::mediaPlayerPanelVolumeToggleSkip;
	_volumeToggle->moveToRight(right, top);

	updatePlayPrevNextPositions();
}

void CoverWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);
	p.fillRect(e->rect(), st::windowBg);
}

void CoverWidget::mouseMoveEvent(QMouseEvent *e) {
	auto closeAreaLeft = st::mediaPlayerPanelPadding + _nameLabel->width();
	auto closeAreaHeight = _nameLabel->y() + _nameLabel->height();
	auto closeArea = myrtlrect(closeAreaLeft, 0, width() - closeAreaLeft, closeAreaHeight);
	auto closeVisible = closeArea.contains(e->pos());
	setCloseVisible(closeVisible);
}

void CoverWidget::leaveEventHook(QEvent *e) {
	setCloseVisible(false);
}

void CoverWidget::setCloseVisible(bool visible) {
	if (visible == _close->isHidden()) {
		_close->setVisible(visible);
		_timeLabel->setVisible(!visible);
	}
}

void CoverWidget::updatePlayPrevNextPositions() {
	auto left = st::mediaPlayerPanelPlayLeft;
	auto top = st::mediaPlayerPanelPlayTop;
	if (_previousTrack) {
		_previousTrack->moveToLeft(left, top); left += _previousTrack->width() + st::mediaPlayerPanelPlaySkip;
		_playPause->moveToLeft(left, top); left += _playPause->width() + st::mediaPlayerPanelPlaySkip;
		_nextTrack->moveToLeft(left, top);
	} else {
		_playPause->moveToLeft(left, top);
	}
}

void CoverWidget::updateLabelPositions() {
	_nameLabel->moveToLeft(st::mediaPlayerPanelPadding, st::mediaPlayerPanelNameTop - st::mediaPlayerName.style.font->ascent);
	_timeLabel->moveToRight(st::mediaPlayerPanelPadding, st::mediaPlayerPanelNameTop - st::mediaPlayerTime.font->ascent);
}

void CoverWidget::updateRepeatTrackIcon() {
	_repeatTrack->setIconOverride(instance()->repeatEnabled() ? nullptr : &st::mediaPlayerRepeatInactiveIcon);
}

void CoverWidget::handleSongUpdate(const TrackState &state) {
	if (!state.id.audio() || !state.id.audio()->song()) {
		return;
	}

	if (state.id.audio()->loading()) {
		_playback->updateLoadingState(state.id.audio()->progress());
	} else {
		_playback->updateState(state);
	}

	auto stopped = (IsStopped(state.state) || state.state == State::Finishing);
	auto showPause = !stopped && (state.state == State::Playing || state.state == State::Resuming || state.state == State::Starting);
	if (instance()->isSeeking(AudioMsgId::Type::Song)) {
		showPause = true;
	}
	auto buttonState = [audio = state.id.audio(), showPause] {
		if (audio->loading()) {
			return ButtonState::Cancel;
		} else if (showPause) {
			return ButtonState::Pause;
		}
		return ButtonState::Play;
	};
	_playPause->setState(buttonState());

	updateTimeText(state);
}

void CoverWidget::updateTimeText(const TrackState &state) {
	QString time;
	qint64 position = 0, duration = 0, display = 0;
	auto frequency = state.frequency;
	if (!IsStopped(state.state) && state.state != State::Finishing) {
		display = position = state.position;
		duration = state.duration;
	} else {
		display = state.duration ? state.duration : (state.id.audio()->song()->duration * frequency);
	}

	_lastDurationMs = (state.duration * 1000LL) / frequency;

	if (state.id.audio()->loading()) {
		_time = QString::number(qRound(state.id.audio()->progress() * 100)) + '%';
		_playback->setDisabled(true);
	} else {
		display = display / frequency;
		_time = formatDurationText(display);
		_playback->setDisabled(false);
	}
	if (_seekPositionMs < 0) {
		updateTimeLabel();
	}
}

void CoverWidget::updateTimeLabel() {
	auto timeLabelWidth = _timeLabel->width();
	if (_seekPositionMs >= 0) {
		auto playAlready = _seekPositionMs / 1000LL;
		_timeLabel->setText(formatDurationText(playAlready));
	} else {
		_timeLabel->setText(_time);
	}
	if (timeLabelWidth != _timeLabel->width()) {
		_nameLabel->resizeToWidth(width() - 2 * (st::mediaPlayerPanelPadding) - _timeLabel->width() - st::normalFont->spacew);
		updateLabelPositions();
	}
}

void CoverWidget::handleSongChange() {
	auto &current = instance()->current();
	auto song = current.audio()->song();
	if (!song) {
		return;
	}

	TextWithEntities textWithEntities;
	if (song->performer.isEmpty()) {
		textWithEntities.text = song->title.isEmpty() ? (current.audio()->name.isEmpty() ? qsl("Unknown Track") : current.audio()->name) : song->title;
	} else {
		auto title = song->title.isEmpty() ? qsl("Unknown Track") : textClean(song->title);
		textWithEntities.text = song->performer + QString::fromUtf8(" \xe2\x80\x93 ") + title;
		textWithEntities.entities.append({ EntityInTextBold, 0, song->performer.size(), QString() });
	}
	_nameLabel->setMarkedText(textWithEntities);

	handlePlaylistUpdate();
}

void CoverWidget::handlePlaylistUpdate() {
	auto &current = instance()->current();
	auto &playlist = instance()->playlist();
	auto index = playlist.indexOf(current.contextId());
	if (!current || index < 0) {
		destroyPrevNextButtons();
	} else {
		createPrevNextButtons();
		auto previousEnabled = (index > 0);
		auto nextEnabled = (index + 1 < playlist.size());
		_previousTrack->setIconOverride(previousEnabled ? nullptr : &st::mediaPlayerPanelPreviousDisabledIcon);
		_previousTrack->setCursor(previousEnabled ? style::cur_pointer : style::cur_default);
		_nextTrack->setIconOverride(nextEnabled ? nullptr : &st::mediaPlayerPanelNextDisabledIcon);
		_nextTrack->setCursor(nextEnabled ? style::cur_pointer : style::cur_default);
	}
}

void CoverWidget::createPrevNextButtons() {
	if (!_previousTrack) {
		_previousTrack.create(this, st::mediaPlayerPanelPreviousButton);
		_previousTrack->show();
		_previousTrack->setClickedCallback([this]() {
			instance()->previous();
		});
		_nextTrack.create(this, st::mediaPlayerPanelNextButton);
		_nextTrack->show();
		_nextTrack->setClickedCallback([this]() {
			instance()->next();
		});
		updatePlayPrevNextPositions();
	}
}

void CoverWidget::destroyPrevNextButtons() {
	if (_previousTrack) {
		_previousTrack.destroy();
		_nextTrack.destroy();
		updatePlayPrevNextPositions();
	}
}

void CoverWidget::updateVolumeToggleIcon() {
	auto icon = []() -> const style::icon * {
		auto volume = Global::SongVolume();
		if (volume > 0) {
			if (volume < 1 / 3.) {
				return &st::mediaPlayerVolumeIcon1;
			} else if (volume < 2 / 3.) {
				return &st::mediaPlayerVolumeIcon2;
			}
			return &st::mediaPlayerVolumeIcon3;
		}
		return nullptr;
	};
	_volumeToggle->setIconOverride(icon());
}

} // namespace Player
} // namespace Media
