# Tests — lightweight single-header framework (doctest)
# Uses vendored copy in tests/vendor/doctest/ — no network required in CI

add_library(doctest_interface INTERFACE)
target_include_directories(doctest_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# Helper macro to register a test executable
macro(add_minima_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE minima-core doctest_interface)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# Core test suites
find_package(Threads REQUIRED)
add_minima_test(test_mini_number  types/test_mini_number.cpp)
add_minima_test(test_mini_data    types/test_mini_data.cpp)
add_minima_test(test_kissvm       kissvm/test_kissvm.cpp)
add_minima_test(test_txpow        objects/test_txpow.cpp)
add_minima_test(test_validation   validation/test_validation.cpp)
add_minima_test(test_mmr          mmr/test_mmr.cpp)
add_minima_test(test_chain        chain/test_chain.cpp)
add_minima_test(test_token        objects/test_token.cpp)
add_minima_test(test_mining       mining/test_mining.cpp)
add_minima_test(test_difficulty   chain/test_difficulty.cpp)
add_minima_test(test_network      network/test_network.cpp)
add_minima_test(test_loopback     network/test_loopback_handshake.cpp)
add_minima_test(test_ibd          objects/test_ibd.cpp)
add_minima_test(test_mempool      mempool/test_mempool.cpp)

# Crypto suites (use real RSA when OpenSSL is enabled)
add_minima_test(test_rsa          crypto/test_rsa.cpp)
add_minima_test(test_checksig     crypto/test_checksig_integration.cpp)
add_minima_test(test_multisig     crypto/test_multisig.cpp)

# When OpenSSL is enabled, explicitly propagate MINIMA_OPENSSL to test targets
# that use RSA directly (minima-core already links OpenSSL, but test executables
# may need explicit linkage on some platforms / linker configurations)
target_link_libraries(test_loopback PRIVATE Threads::Threads)

if(MINIMA_CRYPTO_OPENSSL)
    find_package(OpenSSL REQUIRED)
    foreach(tgt
        test_rsa test_checksig test_multisig
        test_validation test_kissvm)
        target_link_libraries(${tgt} PRIVATE OpenSSL::Crypto OpenSSL::SSL)
        target_compile_definitions(${tgt} PRIVATE MINIMA_OPENSSL)
    endforeach()
endif()
