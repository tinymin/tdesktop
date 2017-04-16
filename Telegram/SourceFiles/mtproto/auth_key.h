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

#include <array>
#include <memory>

namespace MTP {

class AuthKey {
public:
	static constexpr auto kSize = 256; // 2048 bits.
	using Data = std::array<char, kSize>;
	using KeyId = uint64;

	enum class Type {
		Generated,
		ReadFromFile,
		Local,
	};
	AuthKey(Type type, DcId dcId, const Data &data) : _type(type), _dcId(dcId), _key(data) {
		countKeyId();
	}
	AuthKey(const Data &data) : _type(Type::Local), _key(data) {
		countKeyId();
	}

	AuthKey(const AuthKey &other) = delete;
	AuthKey &operator=(const AuthKey &other) = delete;

	Type type() const {
		return _type;
	}

	int dcId() const {
		return _dcId;
	}

	KeyId keyId() const {
		return _keyId;
	}

	void prepareAES(const MTPint128 &msgKey, MTPint256 &aesKey, MTPint256 &aesIV, bool send = true) const {
		uint32 x = send ? 0 : 8;

		uchar data_a[16 + 32], sha1_a[20];
		memcpy(data_a, &msgKey, 16);
		memcpy(data_a + 16, _key.data() + x, 32);
		hashSha1(data_a, 16 + 32, sha1_a);

		uchar data_b[16 + 16 + 16], sha1_b[20];
		memcpy(data_b, _key.data() + 32 + x, 16);
		memcpy(data_b + 16, &msgKey, 16);
		memcpy(data_b + 32, _key.data() + 48 + x, 16);
		hashSha1(data_b, 16 + 16 + 16, sha1_b);

		uchar data_c[32 + 16], sha1_c[20];
		memcpy(data_c, _key.data() + 64 + x, 32);
		memcpy(data_c + 32, &msgKey, 16);
		hashSha1(data_c, 32 + 16, sha1_c);

		uchar data_d[16 + 32], sha1_d[20];
		memcpy(data_d, &msgKey, 16);
		memcpy(data_d + 16, _key.data() + 96 + x, 32);
		hashSha1(data_d, 16 + 32, sha1_d);

		auto key = reinterpret_cast<uchar*>(&aesKey);
		auto iv = reinterpret_cast<uchar*>(&aesIV);
		memcpy(key, sha1_a, 8);
		memcpy(key + 8, sha1_b + 8, 12);
		memcpy(key + 8 + 12, sha1_c + 4, 12);
		memcpy(iv, sha1_a + 8, 12);
		memcpy(iv + 12, sha1_b, 8);
		memcpy(iv + 12 + 8, sha1_c + 16, 4);
		memcpy(iv + 12 + 8 + 4, sha1_d, 8);
	}

	void write(QDataStream &to) const {
		to.writeRawData(_key.data(), _key.size());
	}

	bool equals(const std::shared_ptr<AuthKey> &other) const {
		return other ? (_key == other->_key) : false;
	}

private:
	void countKeyId() {
		auto sha1 = hashSha1(_key.data(), _key.size());

		// Lower 64 bits = 8 bytes of 20 byte SHA1 hash.
		_keyId = *reinterpret_cast<KeyId*>(sha1.data() + 12);
	}

	Type _type = Type::Generated;
	DcId _dcId = 0;
	Data _key = { { 0 } };
	KeyId _keyId = 0;

};

using AuthKeyPtr = std::shared_ptr<AuthKey>;
using AuthKeysList = std::vector<AuthKeyPtr>;

void aesIgeEncrypt(const void *src, void *dst, uint32 len, const void *key, const void *iv);
void aesIgeDecrypt(const void *src, void *dst, uint32 len, const void *key, const void *iv);

inline void aesIgeEncrypt(const void *src, void *dst, uint32 len, const AuthKeyPtr &authKey, const MTPint128 &msgKey) {
	MTPint256 aesKey, aesIV;
	authKey->prepareAES(msgKey, aesKey, aesIV);

	return aesIgeEncrypt(src, dst, len, static_cast<const void*>(&aesKey), static_cast<const void*>(&aesIV));
}

inline void aesEncryptLocal(const void *src, void *dst, uint32 len, const AuthKeyPtr &authKey, const void *key128) {
	MTPint256 aesKey, aesIV;
	authKey->prepareAES(*(const MTPint128*)key128, aesKey, aesIV, false);

	return aesIgeEncrypt(src, dst, len, static_cast<const void*>(&aesKey), static_cast<const void*>(&aesIV));
}

inline void aesIgeDecrypt(const void *src, void *dst, uint32 len, const AuthKeyPtr &authKey, const MTPint128 &msgKey) {
	MTPint256 aesKey, aesIV;
	authKey->prepareAES(msgKey, aesKey, aesIV, false);

	return aesIgeDecrypt(src, dst, len, static_cast<const void*>(&aesKey), static_cast<const void*>(&aesIV));
}

inline void aesDecryptLocal(const void *src, void *dst, uint32 len, const AuthKeyPtr &authKey, const void *key128) {
	MTPint256 aesKey, aesIV;
	authKey->prepareAES(*(const MTPint128*)key128, aesKey, aesIV, false);

	return aesIgeDecrypt(src, dst, len, static_cast<const void*>(&aesKey), static_cast<const void*>(&aesIV));
}

// ctr used inplace, encrypt the data and leave it at the same place
struct CTRState {
	static constexpr int KeySize = 32;
	static constexpr int IvecSize = 16;
	static constexpr int EcountSize = 16;

	uchar ivec[IvecSize] = { 0 };
	uint32 num = 0;
	uchar ecount[EcountSize] = { 0 };
};
void aesCtrEncrypt(void *data, uint32 len, const void *key, CTRState *state);

} // namespace MTP
