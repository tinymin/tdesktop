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
#include "calls/calls_box_controller.h"

#include "styles/style_calls.h"
#include "styles/style_boxes.h"
#include "lang.h"
#include "observer_peer.h"
#include "ui/effects/ripple_animation.h"
#include "calls/calls_instance.h"
#include "history/history_media_types.h"

namespace Calls {
namespace {

constexpr auto kFirstPageCount = 20;
constexpr auto kPerPageCount = 100;

} // namespace

class BoxController::Row : public PeerListBox::Row {
public:
	Row(HistoryItem *item);

	enum class Type {
		Out,
		In,
		Missed,
	};

	bool canAddItem(HistoryItem *item) const {
		return (ComputeType(item) == _type && item->date.date() == _date);
	}
	void addItem(HistoryItem *item) {
		Expects(canAddItem(item));
		_items.push_back(item);
		std::sort(_items.begin(), _items.end(), [](HistoryItem *a, HistoryItem *b) {
			return (a->id > b->id);
		});
		refreshStatus();
	}
	void itemRemoved(HistoryItem *item) {
		if (hasItems() && item->id >= minItemId() && item->id <= maxItemId()) {
			_items.erase(std::remove(_items.begin(), _items.end(), item), _items.end());
			refreshStatus();
		}
	}
	bool hasItems() const {
		return !_items.empty();
	}

	MsgId minItemId() const {
		Expects(hasItems());
		return _items.back()->id;
	}

	MsgId maxItemId() const {
		Expects(hasItems());
		return _items.front()->id;
	}

protected:
	void paintStatusText(Painter &p, int x, int y, int outerWidth, bool selected) override;
	void addActionRipple(QPoint point, base::lambda<void()> updateCallback) override;
	void stopLastActionRipple() override;

private:
	bool needsVerifiedIcon() const override {
		return false;
	}
	QSize actionSize() const override {
		return peer()->isUser() ? QSize(st::callReDial.width, st::callReDial.height) : QSize();
	}
	QMargins actionMargins() const override {
		return QMargins(0, 0, 0, 0);
	}
	void paintAction(Painter &p, TimeMs ms, int x, int y, int outerWidth, bool actionSelected) override;

	void refreshStatus();
	static Type ComputeType(HistoryItem *item);

	std::vector<HistoryItem*> _items;
	QDate _date;
	Type _type;

	std::unique_ptr<Ui::RippleAnimation> _actionRipple;

};

BoxController::Row::Row(HistoryItem *item) : PeerListBox::Row(item->history()->peer, item->id)
, _items(1, item)
, _date(item->date.date())
, _type(ComputeType(item)) {
	refreshStatus();
}

void BoxController::Row::paintStatusText(Painter &p, int x, int y, int outerWidth, bool selected) {
	auto icon = ([this] {
		switch (_type) {
		case Type::In: return &st::callArrowIn;
		case Type::Out: return &st::callArrowOut;
		case Type::Missed: return &st::callArrowMissed;
		}
		Unexpected("_type in Calls::BoxController::Row::paintStatusText().");
	})();
	icon->paint(p, x + st::callArrowPosition.x(), y + st::callArrowPosition.y(), outerWidth);
	x += + st::callArrowPosition.x() + icon->width() + st::callArrowSkip;

	PeerListBox::Row::paintStatusText(p, x, y, outerWidth, selected);
}

void BoxController::Row::paintAction(Painter &p, TimeMs ms, int x, int y, int outerWidth, bool actionSelected) {
	auto size = actionSize();
	if (_actionRipple) {
		_actionRipple->paint(p, x + st::callReDial.rippleAreaPosition.x(), y + st::callReDial.rippleAreaPosition.y(), outerWidth, ms);
		if (_actionRipple->empty()) {
			_actionRipple.reset();
		}
	}
	st::callReDial.icon.paintInCenter(p, rtlrect(x, y, size.width(), size.height(), outerWidth));
}

void BoxController::Row::refreshStatus() {
	if (!hasItems()) {
		return;
	}
	auto text = [this] {
		auto time = _items.front()->date.time().toString(cTimeFormat());
		auto today = QDateTime::currentDateTime().date();
		if (_date == today) {
			return lng_call_box_status_today(lt_time, time);
		} else if (_date.addDays(1) == today) {
			return lng_call_box_status_yesterday(lt_time, time);
		}
		return lng_call_box_status_date(lt_date, langDayOfMonthFull(_date), lt_time, time);
	};
	setCustomStatus((_items.size() > 1) ? lng_call_box_status_group(lt_count, QString::number(_items.size()), lt_status, text()) : text());
}

BoxController::Row::Type BoxController::Row::ComputeType(HistoryItem *item) {
	if (item->out()) {
		return Type::Out;
	} else if (auto media = item->getMedia()) {
		if (media->type() == MediaTypeCall) {
			auto reason = static_cast<HistoryCall*>(media)->reason();
			if (reason == HistoryCall::FinishReason::Busy || reason == HistoryCall::FinishReason::Missed) {
				return Type::Missed;
			}
		}
	}
	return Type::In;
}

void BoxController::Row::addActionRipple(QPoint point, base::lambda<void()> updateCallback) {
	if (!_actionRipple) {
		auto mask = Ui::RippleAnimation::ellipseMask(QSize(st::callReDial.rippleAreaSize, st::callReDial.rippleAreaSize));
		_actionRipple = std::make_unique<Ui::RippleAnimation>(st::callReDial.ripple, std::move(mask), std::move(updateCallback));
	}
	_actionRipple->add(point - st::callReDial.rippleAreaPosition);
}

void BoxController::Row::stopLastActionRipple() {
	if (_actionRipple) {
		_actionRipple->lastStop();
	}
}

void BoxController::prepare() {
	subscribe(Global::RefItemRemoved(), [this](HistoryItem *item) {
		if (auto row = rowForItem(item)) {
			row->itemRemoved(item);
			if (!row->hasItems()) {
				view()->removeRow(row);
				if (!view()->fullRowsCount()) {
					refreshAbout();
				}
			}
			view()->refreshRows();
		}
	});
	subscribe(Current().newServiceMessage(), [this](const FullMsgId &msgId) {
		if (auto item = App::histItemById(msgId)) {
			insertRow(item, InsertWay::Prepend);
		}
	});

	view()->setTitle(lang(lng_call_box_title));
	view()->addButton(lang(lng_close), [this] { view()->closeBox(); });
	view()->setAboutText(lang(lng_contacts_loading));
	view()->refreshRows();

	preloadRows();
}

void BoxController::preloadRows() {
	if (_loadRequestId || _allLoaded) {
		return;
	}

	_loadRequestId = request(MTPmessages_Search(MTP_flags(0), MTP_inputPeerEmpty(), MTP_string(QString()), MTP_inputMessagesFilterPhoneCalls(MTP_flags(0)), MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(_offsetId), MTP_int(_offsetId ? kFirstPageCount : kPerPageCount))).done([this](const MTPmessages_Messages &result) {
		_loadRequestId = 0;

		auto handleResult = [this](auto &data) {
			App::feedUsers(data.vusers);
			App::feedChats(data.vchats);
			receivedCalls(data.vmessages.v);
		};

		switch (result.type()) {
		case mtpc_messages_messages: handleResult(result.c_messages_messages()); _allLoaded = true; break;
		case mtpc_messages_messagesSlice: handleResult(result.c_messages_messagesSlice()); break;
		case mtpc_messages_channelMessages: {
			LOG(("API Error: received messages.channelMessages! (Calls::BoxController::preloadRows)"));
			handleResult(result.c_messages_channelMessages());
		} break;

		default: Unexpected("Type of messages.Messages (Calls::BoxController::preloadRows)");
		}
	}).fail([this](const RPCError &error) {
		_loadRequestId = 0;
	}).send();
}

void BoxController::refreshAbout() {
	view()->setAboutText(view()->fullRowsCount() ? QString() : lang(lng_call_box_about));
}

void BoxController::rowClicked(PeerListBox::Row *row) {
	auto itemsRow = static_cast<Row*>(row);
	auto itemId = itemsRow->maxItemId();
	Ui::showPeerHistoryAsync(row->peer()->id, itemId);
}

void BoxController::rowActionClicked(PeerListBox::Row *row) {
	auto user = row->peer()->asUser();
	t_assert(user != nullptr);

	Current().startOutgoingCall(user);
}

void BoxController::receivedCalls(const QVector<MTPMessage> &result) {
	if (result.empty()) {
		_allLoaded = true;
	}

	for_const (auto &message, result) {
		auto msgId = idFromMessage(message);
		auto peerId = peerFromMessage(message);
		if (auto peer = App::peerLoaded(peerId)) {
			auto item = App::histories().addNewMessage(message, NewMessageExisting);
			insertRow(item, InsertWay::Append);
		} else {
			LOG(("API Error: a search results with not loaded peer %1").arg(peerId));
		}
		_offsetId = msgId;
	}

	refreshAbout();
	view()->refreshRows();
}

bool BoxController::insertRow(HistoryItem *item, InsertWay way) {
	if (auto row = rowForItem(item)) {
		if (row->canAddItem(item)) {
			row->addItem(item);
			return false;
		}
	}
	(way == InsertWay::Append) ? view()->appendRow(createRow(item)) : view()->prependRow(createRow(item));
	view()->reorderRows([](auto &&begin, auto &&end) {
		std::sort(begin, end, [](auto &a, auto &b) {
			return static_cast<Row&>(*a).maxItemId() > static_cast<Row&>(*a).maxItemId();
		});
	});
	return true;
}

BoxController::Row *BoxController::rowForItem(HistoryItem *item) {
	auto v = view();
	if (auto fullRowsCount = v->fullRowsCount()) {
		auto itemId = item->id;
		auto lastRow = static_cast<Row*>(v->rowAt(fullRowsCount - 1));
		if (itemId < lastRow->minItemId()) {
			return lastRow;
		}
		auto firstRow = static_cast<Row*>(v->rowAt(0));
		if (itemId > firstRow->maxItemId()) {
			return firstRow;
		}

		// Binary search. Invariant:
		// 1. rowAt(left)->maxItemId() >= itemId.
		// 2. (left + 1 == fullRowsCount) OR rowAt(left + 1)->maxItemId() < itemId.
		auto left = 0;
		auto right = fullRowsCount;
		while (left + 1 < right) {
			auto middle = (right + left) / 2;
			auto middleRow = static_cast<Row*>(v->rowAt(middle));
			if (middleRow->maxItemId() >= itemId) {
				left = middle;
			} else {
				right = middle;
			}
		}
		auto result = static_cast<Row*>(v->rowAt(left));
		// Check for rowAt(left)->minItemId > itemId > rowAt(left + 1)->maxItemId.
		// In that case we sometimes need to return rowAt(left + 1), not rowAt(left).
		if (result->minItemId() > itemId && left + 1 < fullRowsCount) {
			auto possibleResult = static_cast<Row*>(v->rowAt(left + 1));
			t_assert(possibleResult->maxItemId() < itemId);
			if (possibleResult->canAddItem(item)) {
				return possibleResult;
			}
		}
		return result;
	}
	return nullptr;
}

std::unique_ptr<PeerListBox::Row> BoxController::createRow(HistoryItem *item) const {
	auto row = std::make_unique<Row>(item);
	return std::move(row);
}

} // namespace Calls
