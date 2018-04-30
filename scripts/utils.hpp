#pragma once

#include <array>
#include <bsd/stdlib.h>
#include <cassert>
#include <iostream>
#include <openssl/sha.h>
#include <sstream>
#include <vector>

#include "../native/secp256k1/include/secp256k1.h"
#include "hexxer.hpp"
#include "json.hpp"

typedef std::array<uint8_t, 32> uint8_t_32;
typedef std::array<uint8_t, 33> uint8_t_33;
typedef std::array<uint8_t, 64> uint8_t_64;
typedef std::array<uint8_t, 65> uint8_t_65;
typedef std::vector<uint8_t> uint8_t_vec;

template <typename A>
auto vectorify (const A a) {
	return uint8_t_vec(a.begin(), a.end());
}

auto randomUInt8 () {
	uint8_t x;
	arc4random_buf(&x, sizeof(x));
	return x;
}

template <typename A>
auto random () {
	A x;
	arc4random_buf(x.data(), x.size());
	return x;
}

template <typename A>
auto randomHigh () {
	A x;
	x.fill(0xff);
	arc4random_buf(x.data(), x.size() / 2);
	return x;
}

template <typename A>
auto randomLow () {
	A x;
	x.fill(0);
	arc4random_buf(x.data() + x.size() / 2, x.size() / 2);
	return x;
}

template <typename A>
auto fromUInt32 (const uint32_t i) {
	A x;
	x.fill(0);
	const auto s = x.size();
	x.at(s - 4) = i >> 24;
	x.at(s - 3) = i >> 16;
	x.at(s - 2) = i >> 8;
	x.at(s - 1) = i & 0xff;
	return x;
}

auto randomScalar () { return random<uint8_t_32>(); }
auto randomScalarHigh () { return randomHigh<uint8_t_32>(); }
auto randomScalarLow () { return randomLow<uint8_t_32>(); }
auto scalarFromUInt32 (const uint32_t i) { return fromUInt32<uint8_t_32>(i); }

template <typename A>
auto fromHex (const std::string& s) {
	assert(s.size() == sizeof(A) * 2);
	A x;
	auto i = 0;
	for (auto& y : x) {
		const auto a = s.at(i++);
		const auto b = s.at(i++);
		y = hexxer::decode(a, b);
	}
	return x;
}

auto scalarFromHex (const std::string& s) { return fromHex<uint8_t_32>(s); }
auto signatureFromHex (const std::string& s) { return fromHex<uint8_t_64>(s); }
auto point33FromHex (const std::string& s) { return fromHex<uint8_t_33>(s); }
auto point65FromHex (const std::string& s) { return fromHex<uint8_t_65>(s); }

secp256k1_context* ctx = nullptr;

auto randomPrivate () {
	while (true) {
		const auto key = randomScalar();
		if (secp256k1_ec_seckey_verify(ctx, key.data())) return key;
	}
}

auto randomPrivateHigh () {
	while (true) {
		const auto key = randomScalarHigh();
		if (secp256k1_ec_seckey_verify(ctx, key.data())) return key;
	}
}

auto randomPrivateLow () {
	while (true) {
		const auto key = randomScalarLow();
		if (secp256k1_ec_seckey_verify(ctx, key.data())) return key;
	}
}

// utility functions
auto _isPriv (const uint8_t_32& key) {
	return secp256k1_ec_seckey_verify(ctx, key.data());
}

auto _privAdd (uint8_t_32 key, const uint8_t_32 tweak, bool& ok) {
	ok &= secp256k1_ec_privkey_tweak_add(ctx, key.data(), tweak.data());
	return key;
}

void _ec_init () {
	ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
}

template <typename A>
auto _ec_pubkey_to_array (const secp256k1_pubkey& public_key, bool& ok) {
	if (!ok) return A{};
	A out;
	size_t outlen = sizeof(A);
	ok &= secp256k1_ec_pubkey_serialize(ctx, out.data(), &outlen, &public_key,
		sizeof(A) == 33 ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED);
	return out;
}

template <typename A>
auto _isPoint (const A p) {
	secp256k1_pubkey a;
	return secp256k1_ec_pubkey_parse(ctx, &a, p.data(), sizeof(A));
}

template <typename A>
auto _pointAdd (const A p, const A q, bool& ok) {
	secp256k1_pubkey a, b;
	ok &= secp256k1_ec_pubkey_parse(ctx, &a, p.data(), sizeof(A));
	ok &= secp256k1_ec_pubkey_parse(ctx, &b, q.data(), sizeof(A));

	const secp256k1_pubkey* points[] = { &a, &b };
	secp256k1_pubkey public_key;
	ok &= secp256k1_ec_pubkey_combine(ctx, &public_key, points, 2);

	return _ec_pubkey_to_array<A>(public_key, ok);
}

template <typename A>
uint8_t_vec _pointCompress (const uint8_t_vec p, bool& ok) {
	assert(!p.empty());
	secp256k1_pubkey public_key;
	ok &= secp256k1_ec_pubkey_parse(ctx, &public_key, p.data(), p.size());
	return vectorify(_ec_pubkey_to_array<A>(public_key, ok));
}

template <typename A>
A _pointAddScalar (const A p, const uint8_t_32 d, bool& ok) {
	secp256k1_pubkey public_key;
	ok &= secp256k1_ec_pubkey_parse(ctx, &public_key, p.data(), sizeof(A));
	ok &= secp256k1_ec_pubkey_tweak_add(ctx, &public_key, d.data());
	return _ec_pubkey_to_array<A>(public_key, ok);
}

template <typename A>
auto _pointFromScalar (const uint8_t_32 s, bool& ok) {
	secp256k1_pubkey public_key;
	ok &= secp256k1_ec_pubkey_create(ctx, &public_key, s.data());
	return _ec_pubkey_to_array<A>(public_key, ok);
}

template <typename A>
auto _pointFromUInt32 (const uint32_t i, bool& ok) {
	const auto s = scalarFromUInt32(i);

	secp256k1_pubkey public_key;
	ok &= secp256k1_ec_pubkey_create(ctx, &public_key, s.data());
	return _ec_pubkey_to_array<A>(public_key, ok);
}

auto _signatureFromRS (const uint8_t_32 r, const uint8_t_32 s) {
	uint8_t_64 rs;
	std::copy(r.begin(), r.end(), rs.begin());
	return rs;
}

auto _eccSign (const uint8_t_32 d, const uint8_t_32 message, bool& ok) {
	uint8_t_64 output;
	secp256k1_ecdsa_signature signature;
	ok &= secp256k1_ecdsa_sign(ctx, &signature, message.data(), d.data(), nullptr, nullptr);
	ok &= secp256k1_ecdsa_signature_serialize_compact(ctx, output.data(), &signature);
	return output;
}

template <typename A>
auto _eccVerify (const A& p, const uint8_t_32 message, const uint8_t_64 signature) {
	secp256k1_pubkey public_key;
	bool ok = true;
	ok &= secp256k1_ec_pubkey_parse(ctx, &public_key, p.data(), sizeof(A));
	if (!ok) return false;

	secp256k1_ecdsa_signature _signature;
	ok &= secp256k1_ecdsa_signature_parse_compact(ctx, &_signature, signature.data());
	if (!ok) return false;

	ok &= secp256k1_ecdsa_verify(ctx, &_signature, message.data(), &public_key);
	return ok;
}

template <typename A>
auto sha256 (const A& m) {
	uint8_t_32 h;
	SHA256_CTX hctx;
	SHA256_Init(&hctx);
	SHA256_Update(&hctx, m.data(), m.size());
	SHA256_Final(h.data(), &hctx);
	return h;
}

// we use 0xfefefefefefefe.... as a null placeholder
template <typename A>
auto Null () {
	A a;
	a.fill(0xfe);
	return a;
}
template <typename A>
auto isNull (const A& a) { return a == Null<A>(); }

const auto ZERO = scalarFromHex("0000000000000000000000000000000000000000000000000000000000000000");
const auto ONE = scalarFromHex("0000000000000000000000000000000000000000000000000000000000000001");
const auto TWO = scalarFromHex("0000000000000000000000000000000000000000000000000000000000000002");
const auto THREE = scalarFromHex("0000000000000000000000000000000000000000000000000000000000000003");
const auto GROUP_ORDER = scalarFromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
const auto GROUP_ORDER_LESS_3 = scalarFromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd036413e");
const auto GROUP_ORDER_LESS_2 = scalarFromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd036413f");
const auto GROUP_ORDER_LESS_1 = scalarFromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140");
const auto GROUP_ORDER_OVER_1 = scalarFromHex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364142");
const auto UINT256_MAX = scalarFromHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
const auto GENERATOR = point65FromHex("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");
const auto GENERATORC = point33FromHex("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
