# Tests — doctest single-header (vendored in tests/vendor/doctest/)
# No network access needed — header is local

# Helper macro to register a test executable
macro(add_minima_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE minima-core)
    target_include_directories(${name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# Test suites
add_minima_test(test_mini_number  types/test_mini_number.cpp)
add_minima_test(test_mini_data    types/test_mini_data.cpp)
add_minima_test(test_kissvm       kissvm/test_kissvm.cpp)
add_minima_test(test_txpow        objects/test_txpow.cpp)
add_minima_test(test_witness_wire  objects/test_witness_wire.cpp)
add_minima_test(test_genesis        objects/test_genesis.cpp)
add_minima_test(test_validation   validation/test_validation.cpp)
add_minima_test(test_mmr          mmr/test_mmr.cpp)
add_minima_test(test_megammr       mmr/test_megammr.cpp)
add_minima_test(test_chain        chain/test_chain.cpp)
add_minima_test(test_token        objects/test_token.cpp)
add_minima_test(test_mining       mining/test_mining.cpp)
add_minima_test(test_difficulty   chain/test_difficulty.cpp)
add_minima_test(test_network      network/test_network.cpp)
add_minima_test(test_p2p_sync     network/test_p2p_sync.cpp)
add_minima_test(test_ibd          objects/test_ibd.cpp)
add_minima_test(test_persistence  persistence/test_persistence.cpp)
add_minima_test(test_cascade      cascade/test_cascade.cpp)
add_minima_test(test_bip39        crypto/test_bip39.cpp)
add_minima_test(test_wots         crypto/test_wots.cpp)
add_minima_test(test_treekey      crypto/test_treekey.cpp)
add_minima_test(test_database   database/test_database.cpp)
add_minima_test(test_processor system/test_processor.cpp)

add_minima_test(test_integration  integration/test_integration.cpp)

# ── TCP loopback wire-format tests ───────────────────────────────────────
# Uses POSIX fork()+sockets on 127.0.0.1 — no external node required.
# Runs in standard ctest alongside unit tests.
add_minima_test(test_live_node    network/test_live_node.cpp)
