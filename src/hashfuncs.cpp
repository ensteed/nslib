#include "logging.h"
#include "hashfuncs.h"
#include <cstring>

namespace nslib
{

u64 siphash(const u8 *in, const sizet inlen, u64 seed0, u64 seed1)
{
#define U8TO64_LE(p)                                                                                                                       \
    {                                                                                                                                      \
        (((u64)((p)[0])) | ((u64)((p)[1]) << 8) | ((u64)((p)[2]) << 16) | ((u64)((p)[3]) << 24) | ((u64)((p)[4]) << 32) |                  \
         ((u64)((p)[5]) << 40) | ((u64)((p)[6]) << 48) | ((u64)((p)[7]) << 56))                                                            \
    }
#define U64TO8_LE(p, v)                                                                                                                    \
    {                                                                                                                                      \
        U32TO8_LE((p), (u32)((v)));                                                                                                        \
        U32TO8_LE((p) + 4, (u32)((v) >> 32));                                                                                              \
    }
#define U32TO8_LE(p, v)                                                                                                                    \
    {                                                                                                                                      \
        (p)[0] = (u8)((v));                                                                                                                \
        (p)[1] = (u8)((v) >> 8);                                                                                                           \
        (p)[2] = (u8)((v) >> 16);                                                                                                          \
        (p)[3] = (u8)((v) >> 24);                                                                                                          \
    }
#define ROTL(x, b) (u64)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND                                                                                                                           \
    {                                                                                                                                      \
        v0 += v1;                                                                                                                          \
        v1 = ROTL(v1, 13);                                                                                                                 \
        v1 ^= v0;                                                                                                                          \
        v0 = ROTL(v0, 32);                                                                                                                 \
        v2 += v3;                                                                                                                          \
        v3 = ROTL(v3, 16);                                                                                                                 \
        v3 ^= v2;                                                                                                                          \
        v0 += v3;                                                                                                                          \
        v3 = ROTL(v3, 21);                                                                                                                 \
        v3 ^= v0;                                                                                                                          \
        v2 += v1;                                                                                                                          \
        v1 = ROTL(v1, 17);                                                                                                                 \
        v1 ^= v2;                                                                                                                          \
        v2 = ROTL(v2, 32);                                                                                                                 \
    }
    u64 k0 = U8TO64_LE((u8 *)&seed0);
    u64 k1 = U8TO64_LE((u8 *)&seed1);
    u64 v3 = UINT64_C(0x7465646279746573) ^ k1;
    u64 v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    u64 v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    u64 v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    const u8 *end = in + inlen - (inlen % sizeof(u64));
    for (; in != end; in += 8) {
        u64 m = U8TO64_LE(in);
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
    }
    const sizet left = inlen & 7;
    u64 b = ((u64)inlen) << 56;
    switch (left) {
    case 7:
        b |= ((u64)in[6]) << 48; /* fall through */
    case 6:
        b |= ((u64)in[5]) << 40; /* fall through */
    case 5:
        b |= ((u64)in[4]) << 32; /* fall through */
    case 4:
        b |= ((u64)in[3]) << 24; /* fall through */
    case 3:
        b |= ((u64)in[2]) << 16; /* fall through */
    case 2:
        b |= ((u64)in[1]) << 8; /* fall through */
    case 1:
        b |= ((u64)in[0]);
        break;
    case 0:
        break;
    }
    v3 ^= b;
    SIPROUND;
    SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    b = v0 ^ v1 ^ v2 ^ v3;
    u64 out = 0;
    U64TO8_LE((u8 *)&out, b);
    return out;
}

u32 murmurhash2(const void *key, sizet len, u32 seed)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const u32 m = 0x5bd1e995;
    const sizet r = 24;

    // Initialize the hash to a 'random' value

    u32 h = (u32)(seed ^ len);

    // Mix 4 bytes at a time into the hash

    const unsigned char *data = (const unsigned char *)key;

    while (len >= 4) {
        u32 k = *(u32 *)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array

    switch (len) {
    case 3:
        h ^= data[2] << 16;
    case 2:
        h ^= data[1] << 8;
    case 1:
        h ^= data[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

u64 murmurhash3(const void *key, sizet len, u32 seed)
{
#define ROTL32(x, r) ((x << r) | (x >> (32 - r)))
#define FMIX32(h)                                                                                                                          \
    h ^= h >> 16;                                                                                                                          \
    h *= 0x85ebca6b;                                                                                                                       \
    h ^= h >> 13;                                                                                                                          \
    h *= 0xc2b2ae35;                                                                                                                       \
    h ^= h >> 16;
    const u8 *data = (const u8 *)key;
    const sizet nblocks = len / 16;
    u32 h1 = seed;
    u32 h2 = seed;
    u32 h3 = seed;
    u32 h4 = seed;
    u32 c1 = 0x239b961b;
    u32 c2 = 0xab0e9789;
    u32 c3 = 0x38b34ae5;
    u32 c4 = 0xa1e38b93;
    const u32 *blocks = (const u32 *)(data + nblocks * 16);
    for (sizet i = -1 * nblocks; i; i++) {
        u32 k1 = blocks[i * 4 + 0];
        u32 k2 = blocks[i * 4 + 1];
        u32 k3 = blocks[i * 4 + 2];
        u32 k4 = blocks[i * 4 + 3];
        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = ROTL32(h1, 19);
        h1 += h2;
        h1 = h1 * 5 + 0x561ccd1b;
        k2 *= c2;
        k2 = ROTL32(k2, 16);
        k2 *= c3;
        h2 ^= k2;
        h2 = ROTL32(h2, 17);
        h2 += h3;
        h2 = h2 * 5 + 0x0bcaa747;
        k3 *= c3;
        k3 = ROTL32(k3, 17);
        k3 *= c4;
        h3 ^= k3;
        h3 = ROTL32(h3, 15);
        h3 += h4;
        h3 = h3 * 5 + 0x96cd1c35;
        k4 *= c4;
        k4 = ROTL32(k4, 18);
        k4 *= c1;
        h4 ^= k4;
        h4 = ROTL32(h4, 13);
        h4 += h1;
        h4 = h4 * 5 + 0x32ac3b17;
    }
    const u8 *tail = (const u8 *)(data + nblocks * 16);
    u32 k1 = 0;
    u32 k2 = 0;
    u32 k3 = 0;
    u32 k4 = 0;
    switch (len & 15) {
    case 15:
        k4 ^= tail[14] << 16; /* fall through */
    case 14:
        k4 ^= tail[13] << 8; /* fall through */
    case 13:
        k4 ^= tail[12] << 0;
        k4 *= c4;
        k4 = ROTL32(k4, 18);
        k4 *= c1;
        h4 ^= k4;
        /* fall through */
    case 12:
        k3 ^= tail[11] << 24; /* fall through */
    case 11:
        k3 ^= tail[10] << 16; /* fall through */
    case 10:
        k3 ^= tail[9] << 8; /* fall through */
    case 9:
        k3 ^= tail[8] << 0;
        k3 *= c3;
        k3 = ROTL32(k3, 17);
        k3 *= c4;
        h3 ^= k3;
        /* fall through */
    case 8:
        k2 ^= tail[7] << 24; /* fall through */
    case 7:
        k2 ^= tail[6] << 16; /* fall through */
    case 6:
        k2 ^= tail[5] << 8; /* fall through */
    case 5:
        k2 ^= tail[4] << 0;
        k2 *= c2;
        k2 = ROTL32(k2, 16);
        k2 *= c3;
        h2 ^= k2;
        /* fall through */
    case 4:
        k1 ^= tail[3] << 24; /* fall through */
    case 3:
        k1 ^= tail[2] << 16; /* fall through */
    case 2:
        k1 ^= tail[1] << 8; /* fall through */
    case 1:
        k1 ^= tail[0] << 0;
        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        /* fall through */
    };
    h1 ^= len;
    h2 ^= len;
    h3 ^= len;
    h4 ^= len;
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;
    FMIX32(h1);
    FMIX32(h2);
    FMIX32(h3);
    FMIX32(h4);
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;
    return (((u64)h2) << 32) | h1;
}

#define XXH_PRIME64_1 11400714785074694791ULL
#define XXH_PRIME64_2 14029467366897019727ULL
#define XXH_PRIME64_3 1609587929392839161ULL
#define XXH_PRIME64_4 9650029242287828579ULL
#define XXH_PRIME64_5 2870177450012600261ULL

intern u64 XXH_read64(const void *memptr)
{
    u64 val;
    memcpy(&val, memptr, sizeof(val));
    return val;
}

intern u32 XXH_read32(const void *memptr)
{
    u32 val;
    memcpy(&val, memptr, sizeof(val));
    return val;
}

intern u64 XXH_rotl64(u64 x, int r)
{
    return (x << r) | (x >> (64 - r));
}

u64 xxh64(const void *data, size_t len, u64 seed)
{
    const u8 *p = (const u8 *)data;
    const u8 *const end = p + len;
    u64 h64;

    if (len >= 32) {
        const u8 *const limit = end - 32;
        u64 v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
        u64 v2 = seed + XXH_PRIME64_2;
        u64 v3 = seed + 0;
        u64 v4 = seed - XXH_PRIME64_1;

        do {
            v1 += XXH_read64(p) * XXH_PRIME64_2;
            v1 = XXH_rotl64(v1, 31) * XXH_PRIME64_1;
            p += 8;

            v2 += XXH_read64(p) * XXH_PRIME64_2;
            v2 = XXH_rotl64(v2, 31) * XXH_PRIME64_1;
            p += 8;

            v3 += XXH_read64(p) * XXH_PRIME64_2;
            v3 = XXH_rotl64(v3, 31) * XXH_PRIME64_1;
            p += 8;

            v4 += XXH_read64(p) * XXH_PRIME64_2;
            v4 = XXH_rotl64(v4, 31) * XXH_PRIME64_1;
            p += 8;
        } while (p <= limit);

        // This is the specific convergence logic for the 4 lanes
        h64 = XXH_rotl64(v1, 1) + XXH_rotl64(v2, 7) + XXH_rotl64(v3, 12) + XXH_rotl64(v4, 18);

        // Merge v1
        v1 *= XXH_PRIME64_2;
        v1 = XXH_rotl64(v1, 31);
        v1 *= XXH_PRIME64_1;
        h64 ^= v1;
        h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        // Merge v2
        v2 *= XXH_PRIME64_2;
        v2 = XXH_rotl64(v2, 31);
        v2 *= XXH_PRIME64_1;
        h64 ^= v2;
        h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        // Merge v3
        v3 *= XXH_PRIME64_2;
        v3 = XXH_rotl64(v3, 31);
        v3 *= XXH_PRIME64_1;
        h64 ^= v3;
        h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        // Merge v4
        v4 *= XXH_PRIME64_2;
        v4 = XXH_rotl64(v4, 31);
        v4 *= XXH_PRIME64_1;
        h64 ^= v4;
        h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;
    }
    else {
        h64 = seed + XXH_PRIME64_5;
    }

    h64 += (u64)len;

    // Process remaining 8-byte chunks
    while (p + 8 <= end) {
        u64 k1 = XXH_read64(p);
        k1 *= XXH_PRIME64_2;
        k1 = XXH_rotl64(k1, 31);
        k1 *= XXH_PRIME64_1;
        h64 ^= k1;
        h64 = XXH_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        p += 8;
    }

    // Process remaining 4-byte chunk
    if (p + 4 <= end) {
        h64 ^= (u64)(XXH_read32(p))*XXH_PRIME64_1;
        h64 = XXH_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        p += 4;
    }

    // Process remaining bytes
    while (p < end) {
        h64 ^= (*p) * XXH_PRIME64_5;
        h64 = XXH_rotl64(h64, 11) * XXH_PRIME64_1;
        p++;
    }

    // The Avalanche
    h64 ^= h64 >> 33;
    h64 *= XXH_PRIME64_2;
    h64 ^= h64 >> 29;
    h64 *= XXH_PRIME64_3;
    h64 ^= h64 >> 32;

    return h64;
}

// XXH3 Constants
static const u64 XXH3_PRIME64_1 = 0x9E3779B185EBCA87ULL;
static const u64 XXH3_PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
static const u64 XXH3_PRIME64_3 = 0x165667B19E3779F9ULL;

// A simplified 64-bit "secret" key (standard XXH3 uses a 192-byte secret,
// but we can use a simplified version for a basic internal asset hash)
static const u64 XXH3_SECRET[4] = {0xbe45453e28e3291dULL, 0x66b92c22c159a442ULL, 0x3af49fa2e9f0415cULL, 0x47e583c27633526cULL};

inline u64 xxh3_mul128_fold64(u64 lhs, u64 rhs)
{
#if defined(_MSC_VER) && defined(_M_X64)
    u64 high;
    u64 low = _umul128(lhs, rhs, &high);
    return low ^ high;
#else
    __uint128_t product = (__uint128_t)lhs * (__uint128_t)rhs;
    return (u64)product ^ (u64)(product >> 64);
#endif
}

u64 xxh3(const char *input)
{
    size_t len = strlen(input);
    u64 acc = len * XXH3_PRIME64_1;

    // Process input in 16-byte chunks (Scalar version)
    const char *ptr = input;
    size_t remaining = len;

    while (remaining >= 16) {
        // Read 16 bytes and normalize path (simplified normalization here)
        u64 lane1, lane2;
        memcpy(&lane1, ptr, 8);
        memcpy(&lane2, ptr + 8, 8);

        acc ^= xxh3_mul128_fold64(lane1 ^ XXH3_SECRET[0], lane2 ^ XXH3_SECRET[1]);
        acc *= XXH3_PRIME64_2;

        ptr += 16;
        remaining -= 16;
    }

    // Handle tail bytes
    if (remaining > 0) {
        u64 tail = 0;
        memcpy(&tail, ptr, remaining);
        acc ^= xxh3_mul128_fold64(tail ^ XXH3_SECRET[2], XXH3_PRIME64_3);
    }

    // Final avalanche
    acc ^= acc >> 33;
    acc *= XXH3_PRIME64_2;
    acc ^= acc >> 29;
    acc *= XXH3_PRIME64_3;
    acc ^= acc >> 32;

    return acc;
}

/*
 * This file is derived from crc32.c from the zlib-1.1.3 distribution
 * by Jean-loup Gailly and Mark Adler.
 */

/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static const u32 crc_table[256] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL};

#define DO1(buf) crc = crc_table[((sizet)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)                                                                                                                           \
    DO1(buf);                                                                                                                              \
    DO1(buf);
#define DO4(buf)                                                                                                                           \
    DO2(buf);                                                                                                                              \
    DO2(buf);
#define DO8(buf)                                                                                                                           \
    DO4(buf);                                                                                                                              \
    DO4(buf);

void crc32(const void *key, sizet len, u32 seed, void *out)
{
    uint8_t *buf = (uint8_t *)key;
    u32 crc = seed ^ 0xffffffffL;

    while (len >= 8) {
        DO8(buf);
        len -= 8;
    }

    while (len--) {
        DO1(buf);
    }

    crc ^= 0xffffffffL;

    *(u32 *)out = crc;
}

u64 hash_ptr_sip(const void *data, sizet len, u64 seed0, u64 seed1)
{
    return siphash((u8 *)data, len, seed0, seed1);
}

u64 hash_ptr_murmur(const void *data, sizet len, u64 seed0, u64)
{
    return murmurhash3(data, len, (u32)seed0);
}

u64 hash_ptr_xxh64(const void *data, sizet len, u64 seed0, u64 seed1)
{
    return xxh64(data, len, seed0);
}

u64 hash_ptr_xxh3(const void *data, sizet, u64, u64)
{
    return xxh3((const char*)data);
}

u64 hash_type(const cstr &key, u64 seed0, u64 seed1)
{
    u64 ret = hash_ptr_xxh64(key, strlen(key), seed0, seed1);
    return ret;
}

} // namespace nslib
