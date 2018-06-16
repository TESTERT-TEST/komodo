// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include "crypto/ripemd160.h"
#include "crypto/sha256.h"
#include "crypto/verus_hash.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

#include "sodium.h"

#include <vector>

typedef uint256 ChainCode;

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256 {
private:
    CSHA256 sha;
public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE]) {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash256& Write(const unsigned char *data, size_t len) {
        sha.Write(data, len);
        return *this;
    }

    CHash256& Reset() {
        sha.Reset();
        return *this;
    }
};

/** A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160). */
class CHash160 {
private:
    CSHA256 sha;
public:
    static const size_t OUTPUT_SIZE = CRIPEMD160::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE]) {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        CRIPEMD160().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash160& Write(const unsigned char *data, size_t len) {
        sha.Write(data, len);
        return *this;
    }

    CHash160& Reset() {
        sha.Reset();
        return *this;
    }
};

/** Compute the 256-bit hash of an object. */
template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of two objects. */
template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end) {
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of three objects. */
template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end) {
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .Write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 160-bit hash an object. */
template<typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1] = {};
    uint160 result;
    CHash160().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 160-bit hash of a vector. */
inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    return Hash160(vch.begin(), vch.end());
}

/** A writer stream (for serialization) that computes a 256-bit hash. */
class CHashWriter
{
private:
    CHash256 ctx;

public:
    int nType;
    int nVersion;

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

    CHashWriter& write(const char *pch, size_t size) {
        ctx.Write((const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 result;
        ctx.Finalize((unsigned char*)&result);
        return result;
    }

    template<typename T>
    CHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};


/** A writer stream (for serialization) that computes a 256-bit BLAKE2b hash. */
class CBLAKE2bWriter
{
private:
    crypto_generichash_blake2b_state state;

public:
    int nType;
    int nVersion;

    CBLAKE2bWriter(int nTypeIn, int nVersionIn, const unsigned char* personal) : nType(nTypeIn), nVersion(nVersionIn) {
        assert(crypto_generichash_blake2b_init_salt_personal(
            &state,
            NULL, 0, // No key.
            32,
            NULL,    // No salt.
            personal) == 0);
    }

    CBLAKE2bWriter& write(const char *pch, size_t size) {
        crypto_generichash_blake2b_update(&state, (const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 result;
        crypto_generichash_blake2b_final(&state, (unsigned char*)&result, 32);
        return result;
    }

    template<typename T>
    CBLAKE2bWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

/** A writer stream (for serialization) that computes a 256-bit Verus hash. */
class CVerusHashWriter
{
private:
    CVerusHash state;

public:
    int nType;
    int nVersion;

    CVerusHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn), state() {}

    CVerusHashWriter& write(const char *pch, size_t size) {
        state.Write((const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object for further writing
    uint256 GetHash() {
        uint256 result;
        state.Finalize((unsigned char*)&result);
        return result;
    }

    template<typename T>
    CVerusHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

/** A writer stream (for serialization) that computes a 256-bit Verus hash. */
class CVerusHashPortableWriter
{
private:
    CVerusHashPortable state;

public:
    int nType;
    int nVersion;

    CVerusHashPortableWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn), state() {}

    CVerusHashPortableWriter& write(const char *pch, size_t size) {
        state.Write((const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object for further writing
    uint256 GetHash() {
        uint256 result;
        state.Finalize((unsigned char*)&result);
        return result;
    }

    template<typename T>
    CVerusHashPortableWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

/** An optimized and dangerous writer stream (for serialization) that computes a 256-bit Verus hash without the normal
 * safety checks. Do not try to write more than 1488 bytes to this hash writer. */
class CVerusMiningHashWriter
{
public:
    union hwBuf {
        unsigned char charBuf[1488];
        int32_t i32a[522];
        hwBuf()
        {
            memset(charBuf, 0, sizeof(charBuf));
        }
    };
    hwBuf buf;
    int nPos;
    int nType;
    int nVersion;

    CVerusMiningHashWriter(int nTypeIn, int nVersionIn, int pos = 0) : buf()
    {
        nPos = pos;
        nType = nTypeIn;
        nVersion = nVersionIn;
    }

    CVerusMiningHashWriter& write(const char *pch, size_t size) {
        if ((nPos + size) <= sizeof(buf.charBuf))
        {
            memcpy(&(buf.charBuf[nPos]), pch, size);
            nPos += size;
        }
        return (*this);
    }

    // does not invalidate the object for modification and further hashing
    uint256 GetHash() {
        uint256 result;
        CVerusHash::Hash((unsigned char*)&result, buf.charBuf, nPos);
        return result;
    }

    template<typename T>
    CVerusMiningHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

/** Compute the 256-bit hash of an object's serialization. */
template<typename T>
uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

/** Compute the 256-bit Verus hash of an object's serialization. */
template<typename T>
uint256 SerializeVerusHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CVerusHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

/** Compute the 256-bit Verus hash of an object's serialization. */
template<typename T>
uint256 SerializeVerusHashPortable(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CVerusHashPortableWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

/** Compute the 256-bit Verus hash of an object's serialization. */
template<typename T>
uint256 SerializeVerusMiningHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CVerusMiningHashWriter ss = CVerusMiningHashWriter(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

void BIP32Hash(const ChainCode &chainCode, unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64]);

#endif // BITCOIN_HASH_H
