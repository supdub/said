set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_VS_PLATFORM_NAME x64 CACHE STRING "Architecture expected by Windows dependencies")

if(NOT DEFINED ZIG_EXECUTABLE)
    find_program(ZIG_EXECUTABLE zig REQUIRED)
endif()
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ZIG_EXECUTABLE SAID_WINDRES)

set(CMAKE_C_COMPILER "${ZIG_EXECUTABLE}" CACHE FILEPATH "")
set(CMAKE_C_COMPILER_ARG1 "cc" CACHE STRING "")
# A few upstream CMake projects key only on WIN32 and emit cl.exe warning
# switches even when the compiler is Clang in GNU-driver mode. Keep the
# workaround inside the cross-toolchain so native MSVC builds remain exact.
set(_SAID_ZIG_CXX_WRAPPER "${CMAKE_BINARY_DIR}/said-zig-cxx")
file(WRITE "${_SAID_ZIG_CXX_WRAPPER}" "#!/bin/bash\nset -eu\nzig=\"${ZIG_EXECUTABLE}\"\nargs=()\nfor arg in \"$@\"; do\n  case \"$arg\" in\n    /wd*|/bigobj) continue ;;\n    /std:c++14) arg=-std=c++14 ;;\n  esac\n  args+=(\"$arg\")\ndone\nexec \"$zig\" c++ \"\${args[@]}\"\n")
execute_process(COMMAND chmod +x "${_SAID_ZIG_CXX_WRAPPER}")

set(CMAKE_CXX_COMPILER "${_SAID_ZIG_CXX_WRAPPER}" CACHE FILEPATH "" FORCE)
if(SAID_WINDRES)
    set(CMAKE_RC_COMPILER "${SAID_WINDRES}" CACHE FILEPATH "" FORCE)
    set(CMAKE_RC_COMPILER_ARG1 "" CACHE STRING "" FORCE)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> -O coff <DEFINES> <INCLUDES> <FLAGS> <SOURCE> <OBJECT>"
        CACHE STRING "" FORCE)
else()
    find_program(SAID_WINDRES NAMES x86_64-w64-mingw32-windres llvm-windres)
    if(NOT SAID_WINDRES)
        message(FATAL_ERROR "The Zig cross-build needs x86_64-w64-mingw32-windres for branded Windows resources.")
    endif()
    set(CMAKE_RC_COMPILER "${SAID_WINDRES}" CACHE FILEPATH "" FORCE)
endif()

set(SAID_ZIG_TARGET "x86_64-windows-gnu")
set(CMAKE_C_FLAGS_INIT "-target ${SAID_ZIG_TARGET}")
# Upstream speech libraries contain a few Windows branches written for MSVC
# rather than Clang/MinGW. Pre-include a tiny compatibility header only for
# this documented Zig cross-build.
set(_SAID_ZIG_COMPAT_HEADER "${CMAKE_CURRENT_LIST_DIR}/zig-windows-compat.h")
set(_SAID_ZIG_COMPAT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/zig-windows-include")
set(CMAKE_CXX_FLAGS_INIT "-target ${SAID_ZIG_TARGET} -I${_SAID_ZIG_COMPAT_INCLUDE} -include ${_SAID_ZIG_COMPAT_HEADER}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target ${SAID_ZIG_TARGET}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-target ${SAID_ZIG_TARGET}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-target ${SAID_ZIG_TARGET}")

# simple-sentencepiece tests /std:c++14 merely because the target OS is
# Windows. Zig already provides a C++17 compiler, so avoid that false negative.
set(SBPE_COMPILER_SUPPORTS_CXX14 ON CACHE BOOL "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
