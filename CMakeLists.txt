cmake_minimum_required(VERSION 3.22)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(OB_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)
if(OB_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(ob INTERFACE)
target_include_directories(ob INTERFACE include)

add_executable(ob_tests tests/test_main.cpp tests/book_tests.cpp tests/differential_tests.cpp)
target_link_libraries(ob_tests PRIVATE ob)

enable_testing()
add_test(NAME ob_tests COMMAND ob_tests)
