/**
 * KISS VM unit tests
 * Tests the tokenizer, environment, and contract execution.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "kissvm/Contract.hpp"
#include "kissvm/Tokenizer.hpp"
#include "objects/Transaction.hpp"
#include "objects/Witness.hpp"

using namespace minima;
using namespace minima::kissvm;

// Helper: run a script with given context, return result
static bool runScript(
    const std::string& script,
    const Transaction& txn = Transaction{},
    const Witness&     wit = Witness{},
    size_t inputIdx = 0
) {
    Contract c(script, txn, wit, inputIdx);
    Value result = c.execute();
    return result.isTrue();
}

TEST_SUITE("KISS VM — Basic") {

    TEST_CASE("RETURN TRUE") {
        CHECK(runScript("RETURN TRUE"));
    }

    TEST_CASE("RETURN FALSE") {
        CHECK_FALSE(runScript("RETURN FALSE"));
    }

    TEST_CASE("LET and RETURN variable") {
        CHECK(runScript("LET x = TRUE\nRETURN x"));
    }

    TEST_CASE("arithmetic: 2 + 2 = 4") {
        CHECK(runScript("LET x = 2 + 2\nRETURN x EQ 4"));
    }

    TEST_CASE("comparison: GT") {
        CHECK(runScript("RETURN 10 GT 5"));
        CHECK_FALSE(runScript("RETURN 5 GT 10"));
    }

    TEST_CASE("IF/THEN/ENDIF") {
        CHECK(runScript("IF 1 EQ 1 THEN\nRETURN TRUE\nENDIF\nRETURN FALSE"));
    }

    TEST_CASE("IF/THEN/ELSE/ENDIF — else branch") {
        CHECK_FALSE(runScript("IF 1 EQ 2 THEN\nRETURN TRUE\nELSE\nRETURN FALSE\nENDIF"));
    }

    TEST_CASE("WHILE loop") {
        // sum 1..5 = 15
        CHECK(runScript(
            "LET s = 0\nLET i = 1\n"
            "WHILE i LTE 5 DO\nLET s = s + i\nLET i = i + 1\nENDWHILE\n"
            "RETURN s EQ 15"
        ));
    }

    TEST_CASE("instruction limit 1024 — should not throw for simple scripts") {
        CHECK_NOTHROW(runScript("RETURN TRUE"));
    }
}

TEST_SUITE("KISS VM — SIGNEDBY") {

    TEST_CASE("SIGNEDBY returns TRUE when signature present") {
        const std::string pubKey = "0xABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCD";

        Witness wit;
        SignatureProof sp;
        sp.pubKey    = MiniData::fromHex(pubKey);
        sp.signature = MiniData::fromHex("0x" + std::string(128, 'A')); // mock sig
        wit.addSignature(sp);

        CHECK(runScript("RETURN SIGNEDBY(" + pubKey + ")", Transaction{}, wit));
    }

    TEST_CASE("SIGNEDBY returns FALSE when signature absent") {
        const std::string pubKey = "0xABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCDABCD";
        // No witness — no signatures
        CHECK_FALSE(runScript("RETURN SIGNEDBY(" + pubKey + ")"));
    }
}

TEST_SUITE("KISS VM — Tokenizer") {

    TEST_CASE("tokenizes simple script") {
        Tokenizer t("RETURN TRUE");
        auto tokens = t.tokenize();
        CHECK(tokens.size() >= 2);
        CHECK(tokens[0].type == TokenType::KEYWORD);
        CHECK(tokens[0].value == "RETURN");
    }

    TEST_CASE("0x prefix hex literal") {
        Tokenizer t("LET x = 0xDEAD");
        auto tokens = t.tokenize();
        bool found = false;
        for (auto& tok : tokens) {
            if (tok.type == TokenType::HEX_LITERAL) { found = true; break; }
        }
        CHECK(found);
    }

    TEST_CASE("empty hex 0x is invalid") {
        Tokenizer t("LET x = 0x");
        CHECK_THROWS_AS(t.tokenize(), TokenizerError);
    }
}
