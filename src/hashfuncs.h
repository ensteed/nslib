#pragma once

#include "basic_types.h"
#include "basic_type_traits.h"

namespace nslib
{
//-----------------------------------------------------------------------------
// SipHash reference C implementation
//
// Copyright (c) 2012-2016 Jean-Philippe Aumasson
// <jeanphilippe.aumasson@gmail.com>
// Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// default: SipHash-2-4
//-----------------------------------------------------------------------------
u64 siphash(const u8 *in, const sizet inlen, u64 seed0, u64 seed1);

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Murmur3_86_128
//-----------------------------------------------------------------------------
u32 murmurhash2(const void *key, sizet len, u32 seed);
u64 murmurhash3(const void *key, sizet len, u32 seed);


//-----------------------------------------------------------------------------
// xxHash Library
// Copyright (c) 2012-2021 Yann Collet
// All rights reserved.
//
// BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
//
// xxHash3
//-----------------------------------------------------------------------------
u64 xxh64(const void *data, sizet len, u64 seed);

u64 xxh3(const char *input, sizet len);

void crc32(const void *key, int len, u32 seed, void *out);

// hashmap_sip returns a hash value for `data` using SipHash-2-4.
u64 hash_ptr_sip(const void *data, sizet len, u64 seed0, u64 seed1);

// hashmap_murmur returns a hash value for `data` using Murmur3_86_128.
u64 hash_ptr_murmur(const void *data, sizet len, u64 seed0, u64 seed1);

u64 hash_ptr_xxh64(const void *data, sizet len, u64 seed0, u64 seed1);

u64 hash_ptr_xxh3(const void *data, sizet len);

inline void hash_combine(u64 *h, u64 v)
{
    *h ^= v + 0x9e3779b97f4a7c15ull + (*h << 6) + (*h >> 2);
} 

// Hash strings

// Simply 1 for 1 hash - the key has been computed already and we just make use of the hash buckets
template<integral T>
inline u64 hash_type(const T &key, u64, u64) {
    return (T)key;
}

template<integral T>
inline u64 hash_type(const T &key) {
    return (T)key;
}

u64 hash_type(const void*, sizet sz, u64, u64);

u64 hash_type(const void*, sizet sz);

u64 hash_type(const cstr&, u64, u64);

u64 hash_type(const cstr&);

// Default hash policy for hash tables. Calling hash_type through a template defers overload resolution to the point
// of instantiation, so hash_type overloads declared after the container headers (rid, user types) are still found
template<class Key>
inline u64 hash_type_default(const Key &key, u64 seed0, u64 seed1)
{
    return hash_type(key, seed0, seed1);
}



} // namespace nslib
