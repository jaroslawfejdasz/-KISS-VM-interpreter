#pragma once
/**
 * RSA — Minima signature scheme: RSA-1024 SHA256withRSA (PKCS#1 v1.5)
 *
 * Java reference: src/org/minima/utils/encrypt/SignVerify.java
 *   SIGN_ALGO = "SHA256withRSA"
 *   KEY_SIZE  = 1024 bits
 *
 * Key encoding:
 *   Public:  X.509 SubjectPublicKeyInfo DER
 *   Private: PKCS#8 DER
 *
 * Compiled with -DMINIMA_OPENSSL → uses OpenSSL EVP.
 * Otherwise → stub that throws (for testing without OpenSSL).
 */

#include "../types/MiniData.hpp"
#include <stdexcept>

namespace minima::crypto {

struct RSAKeyPair {
    MiniData privateKey;  // PKCS#8 DER
    MiniData publicKey;   // X.509 DER
};

class RSA {
public:
    static RSAKeyPair generateKeyPair();

    static MiniData sign(const MiniData& privateKey, const MiniData& message);

    static bool verify(const MiniData& publicKey,
                       const MiniData& message,
                       const MiniData& signature);
};

} // namespace minima::crypto
