if(AXTP_C_RUNTIME_BUILD_TESTS)
  add_executable(axtp_codec_test
    ${AXTP_C_RUNTIME_ROOT}/tests/codec_test.c
  )
  target_link_libraries(axtp_codec_test PRIVATE axtp_c_runtime)
  add_test(NAME axtp_codec_test COMMAND axtp_codec_test)

  add_executable(axtp_sdk_test
    ${AXTP_C_RUNTIME_ROOT}/tests/sdk_test.c
  )
  target_link_libraries(axtp_sdk_test PRIVATE axtp_c_runtime)
  add_test(NAME axtp_sdk_test COMMAND axtp_sdk_test)

  add_executable(axtp_stream_test
    ${AXTP_C_RUNTIME_ROOT}/tests/stream_test.c
  )
  target_link_libraries(axtp_stream_test PRIVATE axtp_c_runtime)
  add_test(NAME axtp_stream_test COMMAND axtp_stream_test)

  add_executable(axtp_media_profile_test
    ${AXTP_C_RUNTIME_ROOT}/tests/media_profile_test.c
  )
  target_link_libraries(axtp_media_profile_test PRIVATE axtp_c_runtime)
  add_test(NAME axtp_media_profile_test COMMAND axtp_media_profile_test)
endif()

if(AXTP_C_RUNTIME_BUILD_CONFORMANCE)
  add_executable(axtp_conformance_runner
    ${AXTP_C_RUNTIME_ROOT}/devtools/conformance/conformance_runner.c
  )
  target_link_libraries(axtp_conformance_runner PRIVATE axtp_c_runtime)
endif()
