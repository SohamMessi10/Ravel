cmake_minimum_required(VERSION 3.25)

project(ravel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAVEL_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# nlohmann/json, pinned to the v3.12.0 release archive and its published SHA-256.
# SYSTEM keeps the library's headers out of our warning flags. It is declared before
# add_compile_options so our flags do not apply to its targets.
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  SYSTEM)
FetchContent_MakeAvailable(json)

# Directory-wide options apply only to targets created after this point.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -pedantic)
  if(RAVEL_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
elseif(RAVEL_SANITIZE)
  message(FATAL_ERROR "RAVEL_SANITIZE requires Clang, AppleClang or GCC")
endif()

add_executable(ravel src/main.cpp)
add_executable(ravel-test tests/test.cpp)
add_executable(random-test tests/random_test.cpp)
add_executable(scheduler-test tests/scheduler_test.cpp)

add_library(ravel-scenario STATIC src/scenario.cpp)
target_link_libraries(ravel-scenario PRIVATE nlohmann_json::nlohmann_json)
add_executable(scenario-test tests/scenario_test.cpp)
target_link_libraries(scenario-test PRIVATE ravel-scenario nlohmann_json::nlohmann_json)

enable_testing()
add_test(NAME engine_tests COMMAND ravel-test)
add_test(NAME random_tests COMMAND random-test)
add_test(NAME scheduler_tests COMMAND scheduler-test)
add_test(NAME scenario_tests COMMAND scenario-test)

# The demo trace must stay byte-identical to the golden file.
add_test(NAME demo_golden_trace
  COMMAND ${CMAKE_COMMAND}
    -DPROGRAM=$<TARGET_FILE:ravel>
    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/session.txt
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/expected.csv
    -DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/demo_timeline.csv
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compare_output.cmake)
