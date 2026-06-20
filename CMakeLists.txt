cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AXTP_C_RUNTIME_VERSION LIMIT_COUNT 1)
project(axtp_c_runtime VERSION ${AXTP_C_RUNTIME_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(AXTP_C_RUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(AXTP_C_RUNTIME_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(AXTP_C_RUNTIME_IS_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(AXTP_C_RUNTIME_IS_TOP_LEVEL ON)
endif()

option(AXTP_C_RUNTIME_BUILD_TESTS "Build AXTP C runtime tests" ${AXTP_C_RUNTIME_IS_TOP_LEVEL})
option(AXTP_C_RUNTIME_BUILD_CONFORMANCE "Build AXTP conformance runner" OFF)

add_library(axtp_c_runtime
  src/broker.c
  src/codec.c
  src/core.c
  src/endpoint.c
  src/io.c
  src/model.c
  src/sdk.c
  src/testing_mock_transport.c
  src/transport.c
)

target_include_directories(axtp_c_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(axtp_c_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AXTP_C_RUNTIME_BUILD_TESTS OR AXTP_C_RUNTIME_BUILD_CONFORMANCE)
  enable_testing()
  include(cmake/targets/tests.cmake)
endif()
