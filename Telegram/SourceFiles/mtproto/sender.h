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

#include "base/variant.h"

namespace MTP {

class Instance;
Instance *MainInstance();

class Sender {
	class RequestBuilder {
	public:
		RequestBuilder(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(RequestBuilder &&other) = delete;

	protected:
		using FailPlainHandler = base::lambda_once<void(const RPCError &error)>;
		using FailRequestIdHandler = base::lambda_once<void(const RPCError &error, mtpRequestId requestId)>;
		enum class FailSkipPolicy {
			Simple,
			HandleFlood,
			HandleAll,
		};
		template <typename Response>
		struct DonePlainPolicy {
			using Callback = base::lambda_once<void(const Response &result)>;
			static void handle(Callback &&handler, mtpRequestId requestId, Response &&result) {
				handler(result);
			}

		};
		template <typename Response>
		struct DoneRequestIdPolicy {
			using Callback = base::lambda_once<void(const Response &result, mtpRequestId requestId)>;
			static void handle(Callback &&handler, mtpRequestId requestId, Response &&result) {
				handler(result, requestId);
			}

		};
		template <typename Response, template <typename> typename PolicyTemplate>
		class DoneHandler : public RPCAbstractDoneHandler {
			using Policy = PolicyTemplate<Response>;
			using Callback = typename Policy::Callback;

		public:
			DoneHandler(gsl::not_null<Sender*> sender, Callback handler) : _sender(sender), _handler(std::move(handler)) {
			}

			void operator()(mtpRequestId requestId, const mtpPrime *from, const mtpPrime *end) override {
				auto handler = std::move(_handler);
				_sender->requestHandled(requestId);

				if (handler) {
					auto result = Response();
					result.read(from, end);
					Policy::handle(std::move(handler), requestId, std::move(result));
				}
			}

		private:
			gsl::not_null<Sender*> _sender;
			Callback _handler;

		};

		struct FailPlainPolicy {
			using Callback = base::lambda_once<void(const RPCError &error)>;
			static void handle(Callback &&handler, mtpRequestId requestId, const RPCError &error) {
				handler(error);
			}

		};
		struct FailRequestIdPolicy {
			using Callback = base::lambda_once<void(const RPCError &error, mtpRequestId requestId)>;
			static void handle(Callback &&handler, mtpRequestId requestId, const RPCError &error) {
				handler(error, requestId);
			}

		};
		template <typename Policy>
		class FailHandler : public RPCAbstractFailHandler {
			using Callback = typename Policy::Callback;

		public:
			FailHandler(gsl::not_null<Sender*> sender, Callback handler, FailSkipPolicy skipPolicy)
				: _sender(sender)
				, _handler(std::move(handler))
				, _skipPolicy(skipPolicy) {
			}

			bool operator()(mtpRequestId requestId, const RPCError &error) override {
				if (_skipPolicy == FailSkipPolicy::Simple) {
					if (MTP::isDefaultHandledError(error)) {
						return false;
					}
				} else if (_skipPolicy == FailSkipPolicy::HandleFlood) {
					if (MTP::isDefaultHandledError(error) && !MTP::isFloodError(error)) {
						return false;
					}
				}

				auto handler = std::move(_handler);
				_sender->requestHandled(requestId);

				if (handler) {
					Policy::handle(std::move(handler), requestId, error);
				}
				return true;
			}

		private:
			gsl::not_null<Sender*> _sender;
			Callback _handler;
			FailSkipPolicy _skipPolicy = FailSkipPolicy::Simple;

		};

		explicit RequestBuilder(gsl::not_null<Sender*> sender) noexcept : _sender(sender) {
		}
		RequestBuilder(RequestBuilder &&other) = default;

		void setToDC(ShiftedDcId dcId) noexcept {
			_dcId = dcId;
		}
		void setCanWait(TimeMs ms) noexcept {
			_canWait = ms;
		}
		void setDoneHandler(RPCDoneHandlerPtr &&handler) noexcept {
			_done = std::move(handler);
		}
		void setFailHandler(FailPlainHandler &&handler) noexcept {
			_fail = std::move(handler);
		}
		void setFailHandler(FailRequestIdHandler &&handler) noexcept {
			_fail = std::move(handler);
		}
		void setFailSkipPolicy(FailSkipPolicy policy) noexcept {
			_failSkipPolicy = policy;
		}
		void setAfter(mtpRequestId requestId) noexcept {
			_afterRequestId = requestId;
		}

		ShiftedDcId takeDcId() const noexcept {
			return _dcId;
		}
		TimeMs takeCanWait() const noexcept {
			return _canWait;
		}
		RPCDoneHandlerPtr takeOnDone() noexcept {
			return std::move(_done);
		}
		RPCFailHandlerPtr takeOnFail() {
			if (auto handler = base::get_if<FailPlainHandler>(&_fail)) {
				return MakeShared<FailHandler<FailPlainPolicy>>(_sender, std::move(*handler), _failSkipPolicy);
			} else if (auto handler = base::get_if<FailRequestIdHandler>(&_fail)) {
				return MakeShared<FailHandler<FailRequestIdPolicy>>(_sender, std::move(*handler), _failSkipPolicy);
			}
			return RPCFailHandlerPtr();
		}
		mtpRequestId takeAfter() const noexcept {
			return _afterRequestId;
		}

		gsl::not_null<Sender*> sender() const noexcept {
			return _sender;
		}
		gsl::not_null<Instance*> instance() const noexcept {
			return _sender->_instance;
		}
		void registerRequest(mtpRequestId requestId) {
			_sender->requestRegister(requestId);
		}

	private:
		gsl::not_null<Sender*> _sender;
		ShiftedDcId _dcId = 0;
		TimeMs _canWait = 0;
		RPCDoneHandlerPtr _done;
		base::variant<FailPlainHandler, FailRequestIdHandler> _fail;
		FailSkipPolicy _failSkipPolicy = FailSkipPolicy::Simple;
		mtpRequestId _afterRequestId = 0;

	};

public:
	Sender(gsl::not_null<Instance*> instance = MainInstance()) noexcept : _instance(instance) {
	}

	template <typename Request>
	class SpecificRequestBuilder : public RequestBuilder {
	private:
		friend class Sender;
		SpecificRequestBuilder(gsl::not_null<Sender*> sender, Request &&request) noexcept : RequestBuilder(sender), _request(std::move(request)) {
		}
		SpecificRequestBuilder(SpecificRequestBuilder &&other) = default;

	public:
		SpecificRequestBuilder &toDC(ShiftedDcId dcId) noexcept WARN_UNUSED_RESULT {
			setToDC(dcId);
			return *this;
		}
		SpecificRequestBuilder &canWait(TimeMs ms) noexcept WARN_UNUSED_RESULT {
			setCanWait(ms);
			return *this;
		}
		SpecificRequestBuilder &done(base::lambda_once<void(const typename Request::ResponseType &result)> callback) WARN_UNUSED_RESULT {
			setDoneHandler(MakeShared<DoneHandler<typename Request::ResponseType, DonePlainPolicy>>(sender(), std::move(callback)));
			return *this;
		}
		SpecificRequestBuilder &done(base::lambda_once<void(const typename Request::ResponseType &result, mtpRequestId requestId)> callback) WARN_UNUSED_RESULT {
			setDoneHandler(MakeShared<DoneHandler<typename Request::ResponseType, DoneRequestIdPolicy>>(sender(), std::move(callback)));
			return *this;
		}
		SpecificRequestBuilder &fail(base::lambda_once<void(const RPCError &error)> callback) noexcept WARN_UNUSED_RESULT {
			setFailHandler(std::move(callback));
			return *this;
		}
		SpecificRequestBuilder &fail(base::lambda_once<void(const RPCError &error, mtpRequestId requestId)> callback) noexcept WARN_UNUSED_RESULT {
			setFailHandler(std::move(callback));
			return *this;
		}
		SpecificRequestBuilder &handleFloodErrors() noexcept WARN_UNUSED_RESULT {
			setFailSkipPolicy(FailSkipPolicy::HandleFlood);
			return *this;
		}
		SpecificRequestBuilder &handleAllErrors() noexcept WARN_UNUSED_RESULT {
			setFailSkipPolicy(FailSkipPolicy::HandleAll);
			return *this;
		}
		SpecificRequestBuilder &after(mtpRequestId requestId) noexcept WARN_UNUSED_RESULT {
			setAfter(requestId);
			return *this;
		}

		mtpRequestId send() {
			auto id = instance()->send(_request, takeOnDone(), takeOnFail(), takeDcId(), takeCanWait(), takeAfter());
			registerRequest(id);
			return id;
		}

	private:
		Request _request;

	};

	class SentRequestWrap {
	private:
		friend class Sender;
		SentRequestWrap(gsl::not_null<Sender*> sender, mtpRequestId requestId) : _sender(sender), _requestId(requestId) {
		}

	public:
		void cancel() {
			_sender->requestCancel(_requestId);
		}

	private:
		gsl::not_null<Sender*> _sender;
		mtpRequestId _requestId = 0;

	};

	template <typename Request, typename = std::enable_if_t<std::is_rvalue_reference<Request&&>::value>, typename = typename Request::Unboxed>
	SpecificRequestBuilder<Request> request(Request &&request) noexcept WARN_UNUSED_RESULT;

	SentRequestWrap request(mtpRequestId requestId) noexcept WARN_UNUSED_RESULT;

	void requestSendDelayed() {
		MTP::sendAnything();
	}
	void requestCancellingDiscard() {
		for (auto &request : _requests) {
			request.handled();
		}
	}

private:
	class RequestWrap {
	public:
		RequestWrap(Instance *instance, mtpRequestId requestId) noexcept : _instance(instance), _id(requestId) {
		}

		mtpRequestId id() const noexcept {
			return _id;
		}
		void handled() const noexcept {
			_instance = nullptr;
		}

		~RequestWrap() {
			if (_instance) {
				_instance->cancel(_id);
			}
		}

	private:
		mutable Instance *_instance = nullptr;
		mtpRequestId _id = 0;

	};

	struct RequestWrapComparator {
		using is_transparent = std::true_type;

		struct helper {
			mtpRequestId requestId = 0;

			helper() = default;
			helper(const helper &other) = default;
			helper(mtpRequestId requestId) noexcept : requestId(requestId) {
			}
			helper(const RequestWrap &request) noexcept : requestId(request.id()) {
			}
			bool operator<(helper other) const {
				return requestId < other.requestId;
			}
		};
		bool operator()(const helper &&lhs, const helper &&rhs) const {
			return lhs < rhs;
		}

	};

	template <typename Request>
	friend class SpecialRequestBuilder;
	friend class RequestBuilder;
	friend class RequestWrap;
	friend class SentRequestWrap;

	void requestRegister(mtpRequestId requestId) {
		_requests.emplace(_instance, requestId);
	}
	void requestHandled(mtpRequestId requestId) {
		auto it = _requests.find(requestId);
		if (it != _requests.cend()) {
			it->handled();
			_requests.erase(it);
		}
	}
	void requestCancel(mtpRequestId requestId) {
		auto it = _requests.find(requestId);
		if (it != _requests.cend()) {
			_requests.erase(it);
		}
	}

	gsl::not_null<Instance*> _instance;
	std::set<RequestWrap, RequestWrapComparator> _requests; // Better to use flatmap.

};

template <typename Request, typename, typename>
Sender::SpecificRequestBuilder<Request> Sender::request(Request &&request) noexcept {
	return SpecificRequestBuilder<Request>(this, std::move(request));
}

inline Sender::SentRequestWrap Sender::request(mtpRequestId requestId) noexcept {
	return SentRequestWrap(this, requestId);
}

} // namespace MTP
