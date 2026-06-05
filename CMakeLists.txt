cmake_minimum_required(VERSION 3.16)

project(axtp_c_runtime VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

enable_testing()

add_executable(axtp_codec_test tests/codec_test.c)
target_link_libraries(axtp_codec_test PRIVATE axtp_c_runtime)
add_test(NAME axtp_codec_test COMMAND axtp_codec_test)

add_executable(axtp_sdk_test tests/sdk_test.c)
target_link_libraries(axtp_sdk_test PRIVATE axtp_c_runtime)
add_test(NAME axtp_sdk_test COMMAND axtp_sdk_test)
