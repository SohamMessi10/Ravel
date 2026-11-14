cmake_minimum_required(VERSION 3.25)

project(ravel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RAVEL_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

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

enable_testing()
add_test(NAME engine_tests COMMAND ravel-test)
add_test(NAME random_tests COMMAND random-test)

# The demo trace must stay byte-identical to the golden file.
add_test(NAME demo_golden_trace
  COMMAND ${CMAKE_COMMAND}
    -DPROGRAM=$<TARGET_FILE:ravel>
    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/session.txt
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/expected.csv
    -DACTUAL=${CMAKE_CURRENT_BINARY_DIR}/demo_timeline.csv
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compare_output.cmake)
