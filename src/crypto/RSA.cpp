/**
 * RSA.cpp — RSA-1024 SHA256withRSA
 *
 * Two modes:
 *   MINIMA_OPENSSL defined → real OpenSSL EVP implementation
 *   otherwise             → stub (throws — safe for CI without OpenSSL)
 */
#include "RSA.hpp"

#ifdef MINIMA_OPENSSL
// ────────────────────────────────────────────────────────────────────────────
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

namespace minima::crypto {

static EVP_PKEY* loadPrivKey(const MiniData& d) {
    const uint8_t* p = d.bytes().data();
    return d2i_PrivateKey(EVP_PKEY_RSA, nullptr, &p, (long)d.bytes().size());
}
static EVP_PKEY* loadPubKey(const MiniData& d) {
    const uint8_t* p = d.bytes().data();
    return d2i_PUBKEY(nullptr, &p, (long)d.bytes().size());
}

RSAKeyPair RSA::generateKeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024);
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);
    if (!pkey) throw std::runtime_error("RSA keygen failed");

    int privLen = i2d_PrivateKey(pkey, nullptr);
    std::vector<uint8_t> privB(privLen);
    uint8_t* p = privB.data(); i2d_PrivateKey(pkey, &p);

    int pubLen = i2d_PUBKEY(pkey, nullptr);
    std::vector<uint8_t> pubB(pubLen);
    uint8_t* q = pubB.data(); i2d_PUBKEY(pkey, &q);

    EVP_PKEY_free(pkey);
    return {MiniData(privB), MiniData(pubB)};
}

MiniData RSA::sign(const MiniData& privateKey, const MiniData& message) {
    EVP_PKEY* pkey = loadPrivKey(privateKey);
    if (!pkey) throw std::runtime_error("RSA: bad private key");
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
    EVP_DigestSignUpdate(ctx, message.bytes().data(), message.bytes().size());
    size_t sigLen = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sigLen);
    std::vector<uint8_t> sig(sigLen);
    EVP_DigestSignFinal(ctx, sig.data(), &sigLen);
    sig.resize(sigLen);
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey);
    return MiniData(sig);
}

bool RSA::verify(const MiniData& publicKey,
                 const MiniData& message,
                 const MiniData& signature) {
    EVP_PKEY* pkey = loadPubKey(publicKey);
    if (!pkey) return false;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
    EVP_DigestVerifyUpdate(ctx, message.bytes().data(), message.bytes().size());
    int r = EVP_DigestVerifyFinal(ctx,
        signature.bytes().data(), signature.bytes().size());
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey);
    return r == 1;
}

} // namespace minima::crypto

#else
// ── Stub (no OpenSSL) ────────────────────────────────────────────────────────
namespace minima::crypto {

RSAKeyPair RSA::generateKeyPair() {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}
MiniData RSA::sign(const MiniData&, const MiniData&) {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}
bool RSA::verify(const MiniData&, const MiniData&, const MiniData&) {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}

} // namespace minima::crypto
#endif
