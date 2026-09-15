# AddressSanitizer wiring, kept in one place. Include this early: it declares
# the option and the _asan generator expression. The function is an ordered
# hook point the root calls at the right moment:
#   ja2_asan_instrument_first_party() — after dependency targets (which keep
#     default flags), before our own targets are defined.

option(ADDRESS_SANITIZER "Enable AddressSanitizer for Debug and RelWithDebInfo" OFF)

# asan only on the debuggable configs
set(_asan "$<AND:$<BOOL:${ADDRESS_SANITIZER}>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>")

if(ADDRESS_SANITIZER)
  message(STATUS "AddressSanitizer ENABLED for Debug and RelWithDebInfo (first-party code only)")
  # Third-party C++ targets are not instrumented, so disable MSVC STL container
  # annotations globally to keep their annotate_vector setting consistent with
  # first-party objects. Re-enable annotations when all C++ targets use ASan.
  add_compile_definitions("$<${_asan}:_DISABLE_STL_ANNOTATION>")
endif()

function(ja2_asan_instrument_first_party)
  if(MSVC)
    # /bigobj for the TUs ASan inflates past the COFF section cap
    # (LanguageStrings.cpp). The ignorelist opts individual functions with
    # 32-bit inline __asm out of instrumentation.
    add_compile_options(
      "$<${_asan}:-fsanitize=address;-fsanitize-ignorelist=${CMAKE_SOURCE_DIR}/cmake/asan-ignorelist.txt;/bigobj>")

    # clang-cl/lld-link do not infer the ASan runtime; /WHOLEARCHIVE is invalid for llvm-lib.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      add_link_options(
        "$<$<AND:${_asan},$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>>:clang_rt.asan_dynamic-i386.lib;/WHOLEARCHIVE:clang_rt.asan_static_runtime_thunk-i386.lib>")
    endif()
  else()
    add_compile_options("$<${_asan}:-fsanitize=address;-fno-omit-frame-pointer>")
    add_link_options("$<${_asan}:-fsanitize=address>")
  endif()
endfunction()
