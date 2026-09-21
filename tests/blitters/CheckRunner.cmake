execute_process(COMMAND ${EMULATOR} "${BENCHMARK}" ${ARGUMENTS}
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 50)
if(NOT "${result}" STREQUAL "${EXPECTED_EXIT}")
  message(FATAL_ERROR "Expected exit ${EXPECTED_EXIT}, got ${result}\n${output}\n${diagnostics}")
endif()
if(EXPECTED_EXIT STREQUAL "1" AND NOT output MATCHES ",REGRESSION")
  message(FATAL_ERROR "Timing gate failed without a regression result\n${output}\n${diagnostics}")
endif()
