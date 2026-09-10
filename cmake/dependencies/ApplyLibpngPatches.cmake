set(apng_patch "${PATCH_DIR}/libpng-1.2.50-apng.patch")
set(ja2_patch "${PATCH_DIR}/libpng-1.2.50-ja2-fixes.patch")

execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --whitespace=nowarn "${ja2_patch}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE fully_patched
  ERROR_QUIET
)
if(fully_patched EQUAL 0)
  return()
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --whitespace=nowarn "${apng_patch}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE apng_patched
  ERROR_QUIET
)

set(patches "${ja2_patch}")
if(NOT apng_patched EQUAL 0)
  list(PREPEND patches "${apng_patch}")
endif()

foreach(patch IN LISTS patches)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${patch}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
  )
  if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to apply ${patch}: ${patch_error}")
  endif()
endforeach()
