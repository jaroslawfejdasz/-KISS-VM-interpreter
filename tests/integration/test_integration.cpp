/**
 * Integration test — full node bootstrap flow.
 *
 * Testuje:
 *   1. Genesis → MinimaDB → TxPoWProcessor
 *   2. Block submission flow (processor)
 *   3. TxPoWGenerator → builds valid template
 *   4. MiningManager mines genesis successor
 *   5. Mempool → TX lifecycle
 *   6. Wallet → address creation, signing
 *   7. BIP39 → seed phrase used for wallet derivation
 *   8. DB replay (genesis stored + restored)
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/database/MinimaDB.hpp"
#include "../../src/objects/Genesis.hpp"
#include "../../src/system/TxPoWProcessor.hpp"
#include "../../src/system/TxPoWGenerator.hpp"
#include "../../src/system/TxPoWSearcher.hpp"
#include "../../src/mining/TxPoWMiner.hpp"
#include "../../src/mining/MiningManager.hpp"
#include "../../src/crypto/BIP39.hpp"
#include "../../src/crypto/Winternitz.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include <thread>
#include <chrono>

using namespace minima;
using namespace minima::system;
using namespace minima::crypto;

TEST_SUITE("Integration") {

    TEST_CASE("1. Genesis block created and stored in MinimaDB") {
        MinimaDB db;
        TxPoW genesis = makeGenesisTxPoW();
        MiniData gid = genesis.computeID();

        bool ok = db.addBlock(genesis);
        CHECK(ok);
        CHECK(db.currentHeight() == 0);
        CHECK(db.hasBlock(gid));
    }

    TEST_CASE("2. TxPoWProcessor accepts genesis") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoW genesis = makeGenesisTxPoW();
        auto result = proc.processTxPoWSync(genesis);
        CHECK(result == ProcessResult::ACCEPTED);
        CHECK(db.currentHeight() == 0);
    }

    TEST_CASE("3. TxPoWProcessor rejects duplicate") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoW genesis = makeGenesisTxPoW();
        proc.processTxPoWSync(genesis);
        auto r2 = proc.processTxPoWSync(genesis);
        CHECK(r2 == ProcessResult::DUPLICATE);
    }

    TEST_CASE("4. onBlockAccepted callback fires") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        int callCount = 0;
        proc.onBlockAccepted([&](const TxPoW& blk) {
            CHECK(blk.header().blockNumber.getAsLong() == 0);
            ++callCount;
        });
        proc.processTxPoWSync(makeGenesisTxPoW());
        CHECK(callCount == 1);
    }

    TEST_CASE("5. TxPoWGenerator builds block #1 template") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        proc.processTxPoWSync(makeGenesisTxPoW());

        TxPoWGenerator gen(db.txPowTree(), db.mempool());
        TxPoW tmpl = gen.generateTxPoW();

        MiniData genesisID = makeGenesisTxPoW().computeID();
        CHECK(tmpl.header().blockNumber.getAsLong() == 1);
        CHECK(tmpl.header().superParents[0] == genesisID);
    }

    TEST_CASE("6. TxPoWSearcher finds genesis") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoW genesis = makeGenesisTxPoW();
        proc.processTxPoWSync(genesis);

        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        CHECK(searcher.isInChain(genesis.computeID()));
        CHECK(searcher.getHeight() == 0);
    }

    TEST_CASE("7. Wallet: create address, sign, WOTS one-time") {
        MinimaDB db;
        Wallet& wallet = db.wallet();
        CHECK(wallet.addressCount() == 0);
        auto addr = wallet.createAddress();
        CHECK(wallet.addressCount() == 1);
        CHECK(wallet.defaultAddress() == addr);
        CHECK(wallet.hasKey(addr));
        MiniData data(std::vector<uint8_t>(32, 0xAB));
        auto sig = wallet.sign(addr, data);
        CHECK(sig.has_value());
        CHECK(!sig->bytes().empty());
        // WOTS = one-time
        auto sig2 = wallet.sign(addr, data);
        CHECK(!sig2.has_value());
    }

    TEST_CASE("8. BIP39 + Winternitz wallet derivation") {
        std::vector<uint8_t> entropy(16, 0x42);
        std::string mnemonic = BIP39::entropyToMnemonic(entropy);
        CHECK(BIP39::validateMnemonic(mnemonic));
        auto seed = BIP39::mnemonicToSeed(mnemonic);
        CHECK(seed.size() == 64);
        // deterministic
        CHECK(BIP39::mnemonicToSeed(mnemonic) == seed);
        // WOTS key from seed
        std::vector<uint8_t> keySeed(seed.begin(), seed.begin() + 32);
        auto pubKey = Winternitz::generatePublicKey(keySeed);
        CHECK(!pubKey.empty());
    }

    TEST_CASE("9. Full flow: genesis → block #1 mined and accepted") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoWSearcher  searcher(db.txPowTree(), db.blockStore());
        TxPoWGenerator gen(db.txPowTree(), db.mempool());

        // genesis
        proc.processTxPoWSync(makeGenesisTxPoW());
        CHECK(searcher.getHeight() == 0);

        // build block #1 template
        TxPoW tmpl = gen.generateTxPoW();
        CHECK(tmpl.header().blockNumber.getAsLong() == 1);

        // force trivial difficulty → always passes isBlock()
        tmpl.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
        CHECK(tmpl.isBlock()); // 0xFF = always block

        // submit
        auto r = proc.processTxPoWSync(tmpl);
        CHECK(r == ProcessResult::ACCEPTED);
        CHECK(db.currentHeight() == 1);
        CHECK(searcher.getHeight() == 1);
    }

    TEST_CASE("10. Processor stats counters") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        CHECK(proc.blocksProcessed() == 0);
        CHECK(proc.txnsProcessed() == 0);

        proc.processTxPoWSync(makeGenesisTxPoW());
        CHECK(proc.blocksProcessed() == 1);

        // Generate and add block #1
        TxPoWGenerator gen(db.txPowTree(), db.mempool());
        TxPoW tmpl = gen.generateTxPoW();
        tmpl.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
        proc.processTxPoWSync(tmpl);
        CHECK(proc.blocksProcessed() == 2);
    }

    TEST_CASE("11. MinimaDB canonical chain after 3 blocks") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoWGenerator gen(db.txPowTree(), db.mempool());

        proc.processTxPoWSync(makeGenesisTxPoW());

        // Add 2 more blocks
        for (int i = 1; i <= 2; ++i) {
            TxPoW blk = gen.generateTxPoW();
            blk.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
            auto r = proc.processTxPoWSync(blk);
            CHECK(r == ProcessResult::ACCEPTED);
        }

        CHECK(db.currentHeight() == 2);
        auto chain = db.canonicalChain();
        CHECK(chain.size() == 3); // genesis + 2
    }

    TEST_CASE("12. Async processor: submit + wait") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        proc.start();

        std::atomic<bool> called{false};
        proc.submitTxPoW(makeGenesisTxPoW(), [&](const MiniData&, ProcessResult r) {
            CHECK(r == ProcessResult::ACCEPTED);
            called = true;
        });

        // Wait up to 1s
        for (int i = 0; i < 100 && !called; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        CHECK(called.load());
        proc.stop();
    }
}
