execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --whitespace=nowarn
          --unsafe-paths "--directory=${SOURCE_DIR}" "${PATCH_FILE}"
  WORKING_DIRECTORY "${WORK_DIR}"
  RESULT_VARIABLE already_patched
  ERROR_QUIET
)
if(already_patched EQUAL 0)
  return()
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn
          --unsafe-paths "--directory=${SOURCE_DIR}" "${PATCH_FILE}"
  WORKING_DIRECTORY "${WORK_DIR}"
  RESULT_VARIABLE patch_result
  ERROR_VARIABLE patch_error
)
if(NOT patch_result EQUAL 0)
  message(FATAL_ERROR "Failed to apply ${PATCH_FILE}: ${patch_error}")
endif()
