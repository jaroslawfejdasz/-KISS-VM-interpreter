cmake_minimum_required(VERSION 3.16)
project(minima-core VERSION 0.1.0 LANGUAGES CXX)

# ── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ───────────────────────────────────────────────────────────────────
option(MINIMA_BUILD_TESTS  "Build unit tests"           ON)
option(MINIMA_CRYPTO_OPENSSL "Use OpenSSL crypto backend" OFF)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(minima-core STATIC
    src/types/MiniNumber.cpp
    src/types/MiniData.cpp
    src/types/MiniString.cpp

    src/objects/Address.cpp
    src/objects/StateVariable.cpp
    src/objects/Coin.cpp
    src/objects/Transaction.cpp
    src/objects/Witness.cpp
    src/objects/TxHeader.cpp
    src/objects/TxBody.cpp
    src/objects/TxPoW.cpp

    src/kissvm/Value.cpp
    src/kissvm/Token.cpp
    src/kissvm/Tokenizer.cpp
    src/kissvm/Environment.cpp
    src/kissvm/Contract.cpp
    src/kissvm/Parser.cpp
    src/kissvm/Interpreter.cpp
    src/kissvm/functions/Functions.cpp

    src/crypto/Hash.cpp
    src/crypto/RSA.cpp
    src/objects/Magic.cpp

    src/serialization/DataStream.cpp

    src/validation/TxPoWValidator.cpp
    src/mmr/MMRData.cpp
    src/mmr/MMREntry.cpp
    src/mmr/MMRProof.cpp
    src/mmr/MMRSet.cpp
)

target_include_directories(minima-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# ── Crypto backend ────────────────────────────────────────────────────────────
if(MINIMA_CRYPTO_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(minima-core PRIVATE OpenSSL::Crypto)
    target_compile_definitions(minima-core PRIVATE MINIMA_CRYPTO_OPENSSL)
    message(STATUS "Minima crypto backend: OpenSSL ${OPENSSL_VERSION}")
else()
    message(STATUS "Minima crypto backend: reference (pure C++)")
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(minima-core PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(MINIMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS minima-core ARCHIVE DESTINATION lib)
install(DIRECTORY src/ DESTINATION include/minima-core
    FILES_MATCHING PATTERN "*.hpp")
