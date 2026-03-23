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

// ── Parser ────────────────────────────────────────────────────────────────────
#include "kissvm/Parser.hpp"
#include "kissvm/Interpreter.hpp"
#include "crypto/Hash.hpp"

TEST_SUITE("KISS VM — Parser") {

    TEST_CASE("Parser: LET node") {
        using namespace minima::kissvm;
        Tokenizer tok("LET x = 5");
        Parser p(tok.tokenize());
        auto ast = p.parse();
        REQUIRE(ast->kind == NodeKind::PROGRAM);
        REQUIRE(ast->children.size() == 1);
        CHECK(ast->children[0]->kind == NodeKind::LET);
        CHECK(ast->children[0]->name == "x");
    }

    TEST_CASE("Parser: IF node") {
        using namespace minima::kissvm;
        Tokenizer tok("IF TRUE THEN RETURN TRUE ENDIF");
        Parser p(tok.tokenize());
        auto ast = p.parse();
        REQUIRE(ast->children.size() == 1);
        CHECK(ast->children[0]->kind == NodeKind::IF);
    }

    TEST_CASE("Parser: WHILE node") {
        using namespace minima::kissvm;
        Tokenizer tok("WHILE TRUE DO ENDWHILE");
        Parser p(tok.tokenize());
        auto ast = p.parse();
        REQUIRE(ast->children.size() == 1);
        CHECK(ast->children[0]->kind == NodeKind::WHILE);
    }

    TEST_CASE("Parser: function call node") {
        using namespace minima::kissvm;
        Tokenizer tok("SHA3(0xABCD)");
        Parser p(tok.tokenize());
        auto ast = p.parse();
        REQUIRE(ast->children.size() == 1);
        CHECK(ast->children[0]->children[0]->kind == NodeKind::CALL);
        CHECK(ast->children[0]->children[0]->name == "SHA3");
    }
}

// ── Built-in functions ────────────────────────────────────────────────────────

TEST_SUITE("KISS VM — Built-in Functions") {

    TEST_CASE("SHA3 returns 32 bytes") {
        CHECK(runScript("LET h = SHA3(0xABCD) RETURN LEN(h) EQ 32"));
    }

    TEST_CASE("SHA2 returns 32 bytes") {
        CHECK(runScript("LET h = SHA2(0xABCD) RETURN LEN(h) EQ 32"));
    }

    TEST_CASE("CONCAT two hex values") {
        CHECK(runScript("LET a = 0xFF LET b = 0x00 RETURN LEN(CONCAT(a,b)) EQ 2"));
    }

    TEST_CASE("LEN of hex") {
        CHECK(runScript("RETURN LEN(0xAABBCC) EQ 3"));
    }

    TEST_CASE("SUBSET extract middle byte") {
        CHECK(runScript("RETURN SUBSET(0xAABBCC, 1, 1) EQ 0xBB"));
    }

    TEST_CASE("REV reverses bytes") {
        CHECK(runScript("RETURN REV(0xAABB) EQ 0xBBAA"));
    }

    TEST_CASE("ABS of negative") {
        CHECK(runScript("RETURN ABS(-5) EQ 5"));
    }

    TEST_CASE("MAX and MIN") {
        CHECK(runScript("RETURN MAX(3, 7) EQ 7"));
        CHECK(runScript("RETURN MIN(3, 7) EQ 3"));
    }

    TEST_CASE("INC and DEC") {
        CHECK(runScript("RETURN INC(9) EQ 10"));
        CHECK(runScript("RETURN DEC(10) EQ 9"));
    }

    TEST_CASE("BOOL conversion") {
        CHECK(runScript("RETURN BOOL(1)"));
        CHECK_FALSE(runScript("RETURN BOOL(0)"));
    }

    TEST_CASE("BITGET and BITSET") {
        CHECK(runScript("LET b = BITSET(0x00, 0, TRUE) RETURN BITGET(b, 0)"));
    }

    TEST_CASE("BITCOUNT popcount") {
        CHECK(runScript("RETURN BITCOUNT(0xFF) EQ 8"));
        CHECK(runScript("RETURN BITCOUNT(0x0F) EQ 4"));
    }

    TEST_CASE("HEXTONUM converts hex to number") {
        CHECK(runScript("RETURN HEXTONUM(0xFF) EQ 255"));
    }
}

// ── MULTISIG ──────────────────────────────────────────────────────────────────

TEST_SUITE("KISS VM — MULTISIG") {

    TEST_CASE("MULTISIG 2-of-3 with 2 present → true") {
        MiniData k1({0x01}), k2({0x02}), k3({0x03});
        Witness w;
        w.addSignature({k1, MiniData({0xAA})});
        w.addSignature({k2, MiniData({0xBB})});
        Contract c("RETURN MULTISIG(2, 0x01, 0x02, 0x03)", Transaction{}, w);
        c.execute();
        CHECK(c.isTrue());
    }

    TEST_CASE("MULTISIG 2-of-3 with 1 present → false") {
        MiniData k1({0x01});
        Witness w;
        w.addSignature({k1, MiniData({0xAA})});
        Contract c("RETURN MULTISIG(2, 0x01, 0x02, 0x03)", Transaction{}, w);
        c.execute();
        CHECK_FALSE(c.isTrue());
    }

    TEST_CASE("MULTISIG 1-of-1 → true") {
        MiniData k1({0xFF});
        Witness w;
        w.addSignature({k1, MiniData({0x01})});
        Contract c("RETURN MULTISIG(1, 0xFF)", Transaction{}, w);
        c.execute();
        CHECK(c.isTrue());
    }
}

// ── ELSEIF chains ─────────────────────────────────────────────────────────────

TEST_SUITE("KISS VM — ELSEIF") {

    TEST_CASE("ELSEIF first branch") {
        CHECK(runScript("LET x=1 IF x EQ 1 THEN RETURN TRUE ELSEIF x EQ 2 THEN RETURN FALSE ELSE RETURN FALSE ENDIF"));
    }

    TEST_CASE("ELSEIF second branch") {
        CHECK(runScript("LET x=2 IF x EQ 1 THEN RETURN FALSE ELSEIF x EQ 2 THEN RETURN TRUE ELSE RETURN FALSE ENDIF"));
    }

    TEST_CASE("ELSEIF else branch") {
        CHECK(runScript("LET x=99 IF x EQ 1 THEN RETURN FALSE ELSEIF x EQ 2 THEN RETURN FALSE ELSE RETURN TRUE ENDIF"));
    }
}

// ── @AMOUNT / @TOTIN env vars ─────────────────────────────────────────────────

TEST_SUITE("KISS VM — Transaction context") {

    TEST_CASE("@AMOUNT matches input coin amount") {
        Transaction txn;
        Coin in; in.setAmount(MiniNumber("42"));
        txn.addInput(in);
        CHECK(runScript("RETURN @AMOUNT EQ 42", txn));
    }

    TEST_CASE("@TOTIN sum of inputs") {
        Transaction txn;
        Coin a; a.setAmount(MiniNumber("30"));
        Coin b; b.setAmount(MiniNumber("70"));
        txn.addInput(a); txn.addInput(b);
        CHECK(runScript("RETURN @TOTIN EQ 100", txn));
    }

    TEST_CASE("GETOUTAMT returns output amount") {
        Transaction txn;
        Coin in; in.setAmount(MiniNumber("50")); txn.addInput(in);
        Coin out; out.setAmount(MiniNumber("50")); txn.addOutput(out);
        CHECK(runScript("RETURN GETOUTAMT(0) EQ 50", txn));
    }
}

// ── Real-world contract patterns ──────────────────────────────────────────────

TEST_SUITE("KISS VM — Real contract patterns") {

    TEST_CASE("Simple payment: SIGNEDBY owner") {
        MiniData owner({0xDE,0xAD,0xBE,0xEF});
        Witness w;
        w.addSignature({owner, MiniData({0x01})});
        Contract c("RETURN SIGNEDBY(0xDEADBEEF)", Transaction{}, w);
        c.execute();
        CHECK(c.isTrue());
    }

    TEST_CASE("Timelock: block >= threshold") {
        Contract c("RETURN @BLOCK GTE 100", Transaction{}, Witness{});
        c.setBlockNumber(MiniNumber("100"));
        c.execute();
        CHECK(c.isTrue());
    }

    TEST_CASE("Timelock: block < threshold → false") {
        Contract c("RETURN @BLOCK GTE 1000", Transaction{}, Witness{});
        c.setBlockNumber(MiniNumber("50"));
        c.execute();
        CHECK_FALSE(c.isTrue());
    }

    TEST_CASE("HTLC: SHA3 preimage check") {
        // SHA3(0xABCD) must equal embedded hash
        auto hashData = crypto::Hash::sha3_256(std::vector<uint8_t>{0xAB, 0xCD});
        std::string hexStr = hashData.toHexString(false);
        std::string script = "LET h = SHA3(0xABCD) RETURN h EQ 0x" + hexStr;
        CHECK(runScript(script));
    }

    TEST_CASE("ASSERT in contract guard") {
        // Guard that prevents execution if wrong key
        MiniData owner({0x01});
        Witness w;
        w.addSignature({owner, MiniData({0x01})});
        Contract c("ASSERT SIGNEDBY(0x01) RETURN TRUE", Transaction{}, w);
        c.execute();
        CHECK(c.isTrue());
    }
}
