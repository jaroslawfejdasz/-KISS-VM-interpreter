#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/BIP39.hpp"
#include <vector>
#include <cstdint>

using namespace minima::crypto;

TEST_SUITE("BIP39") {

    TEST_CASE("16 bytes → 12 words") {
        std::vector<uint8_t> e(16, 0x00);
        CHECK(BIP39::wordCount(BIP39::entropyToMnemonic(e)) == 12);
    }
    TEST_CASE("32 bytes → 24 words") {
        std::vector<uint8_t> e(32, 0x00);
        CHECK(BIP39::wordCount(BIP39::entropyToMnemonic(e)) == 24);
    }
    TEST_CASE("deterministic same entropy → same mnemonic") {
        std::vector<uint8_t> e(16, 0xAB);
        CHECK(BIP39::entropyToMnemonic(e) == BIP39::entropyToMnemonic(e));
    }
    TEST_CASE("different entropy → different mnemonic") {
        CHECK(BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x00))
           != BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0xFF)));
    }
    TEST_CASE("roundtrip 12 words — zeros") {
        std::vector<uint8_t> e(16, 0x00);
        CHECK(BIP39::mnemonicToEntropy(BIP39::entropyToMnemonic(e)) == e);
    }
    TEST_CASE("roundtrip 12 words — all 0xFF") {
        std::vector<uint8_t> e(16, 0xFF);
        CHECK(BIP39::mnemonicToEntropy(BIP39::entropyToMnemonic(e)) == e);
    }
    TEST_CASE("roundtrip 12 words — varied") {
        std::vector<uint8_t> e = {0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
                                   0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
        CHECK(BIP39::mnemonicToEntropy(BIP39::entropyToMnemonic(e)) == e);
    }
    TEST_CASE("roundtrip 24 words") {
        std::vector<uint8_t> e(32, 0x33);
        CHECK(BIP39::mnemonicToEntropy(BIP39::entropyToMnemonic(e)) == e);
    }
    TEST_CASE("validateMnemonic valid") {
        std::vector<uint8_t> e(16, 0x42);
        CHECK(BIP39::validateMnemonic(BIP39::entropyToMnemonic(e)));
    }
    TEST_CASE("validateMnemonic wrong word count") {
        CHECK_FALSE(BIP39::validateMnemonic("abandon ability able about above absent"));
    }
    TEST_CASE("validateMnemonic unknown word") {
        std::vector<uint8_t> e(16, 0x00);
        auto mn = BIP39::entropyToMnemonic(e);
        auto pos = mn.find(' ');
        CHECK_FALSE(BIP39::validateMnemonic("XXXXXXX" + mn.substr(pos)));
    }
    TEST_CASE("mnemonicToSeed: 64 bytes") {
        auto mn = BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x01));
        CHECK(BIP39::mnemonicToSeed(mn).size() == 64);
    }
    TEST_CASE("mnemonicToSeed: deterministic") {
        auto mn = BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x01));
        CHECK(BIP39::mnemonicToSeed(mn) == BIP39::mnemonicToSeed(mn));
    }
    TEST_CASE("mnemonicToSeed: different mnemonics → different seeds") {
        auto s1 = BIP39::mnemonicToSeed(BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x01)));
        auto s2 = BIP39::mnemonicToSeed(BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x02)));
        CHECK(s1 != s2);
    }
    TEST_CASE("mnemonicToSeed: passphrase changes seed") {
        auto mn = BIP39::entropyToMnemonic(std::vector<uint8_t>(16, 0x05));
        CHECK(BIP39::mnemonicToSeed(mn, "") != BIP39::mnemonicToSeed(mn, "secret"));
    }
    TEST_CASE("generateMnemonic valid") {
        std::vector<uint8_t> src = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
                                    0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00};
        auto mn = BIP39::generateMnemonic(src);
        CHECK(BIP39::wordCount(mn) == 12);
        CHECK(BIP39::validateMnemonic(mn));
    }
}
