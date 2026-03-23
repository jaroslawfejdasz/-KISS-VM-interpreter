# Tests — using a lightweight single-header framework (doctest)
# Fetch doctest via CMake FetchContent (no external install needed)

include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Helper macro to register a test executable
macro(add_minima_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE minima-core doctest::doctest)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# Test suites
add_minima_test(test_mini_number  types/test_mini_number.cpp)
add_minima_test(test_mini_data    types/test_mini_data.cpp)
add_minima_test(test_kissvm       kissvm/test_kissvm.cpp)
add_minima_test(test_txpow        objects/test_txpow.cpp)
add_minima_test(test_validation   validation/test_validation.cpp)
add_minima_test(test_mmr          mmr/test_mmr.cpp)
add_minima_test(test_chain        chain/test_chain.cpp)

# New modules
add_minima_test(test_token   objects/test_token.cpp)
add_minima_test(test_mining  mining/test_mining.cpp)

add_minima_test(test_difficulty chain/test_difficulty.cpp)

add_minima_test(test_network network/test_network.cpp)
add_minima_test(test_ibd objects/test_ibd.cpp)
