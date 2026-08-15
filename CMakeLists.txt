cmake_minimum_required(VERSION 3.21)

project(VoiceKey VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VOICEKEY_BUILD_TESTS "Build the small platform-independent test suite" ON)
option(VOICEKEY_BUILD_INTEGRATION_TEST_TOOL "Build a Windows-only file transcription probe" OFF)
set(VOICEKEY_WHISPER_SOURCE_DIR "" CACHE PATH "Use an existing whisper.cpp checkout")

if(WIN32)
    enable_language(RC)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
    set(GGML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    # The cross-compiler cannot execute its feature probe, so these otherwise
    # fall back to scalar code. AVX2 is the Windows 10/11 release baseline.
    set(GGML_SSE42 ON CACHE BOOL "" FORCE)
    set(GGML_AVX ON CACHE BOOL "" FORCE)
    set(GGML_AVX2 ON CACHE BOOL "" FORCE)
    set(GGML_BMI2 ON CACHE BOOL "" FORCE)
    set(GGML_FMA ON CACHE BOOL "" FORCE)
    set(GGML_F16C ON CACHE BOOL "" FORCE)
    set(GGML_CPU_AARCH64 OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    set(GGML_BLAS OFF CACHE BOOL "" FORCE)
    set(GGML_STATIC ON CACHE BOOL "" FORCE)

    if(VOICEKEY_WHISPER_SOURCE_DIR)
        add_subdirectory("${VOICEKEY_WHISPER_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/whisper")
    else()
        FetchContent_Declare(
            whisper
            GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
            GIT_TAG v1.7.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(whisper)
    endif()
    target_compile_definitions(whisper PRIVATE _USE_MATH_DEFINES)

    FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.25
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(miniaudio)
    if(NOT miniaudio_POPULATED)
        FetchContent_Populate(miniaudio)
    endif()

    add_executable(voicekey WIN32
        resources/voicekey.rc
        src/app_settings.cpp
        src/audio_capture.cpp
        src/main.cpp
        src/overlay.cpp
        src/setup_window.cpp
        src/text_injector.cpp
        src/transcriber.cpp
        src/transcript.cpp
        src/win_util.cpp
    )

    target_include_directories(voicekey PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${miniaudio_SOURCE_DIR}"
    )

    target_compile_definitions(voicekey PRIVATE
        NOMINMAX
        WINVER=0x0A00
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        VOICEKEY_VERSION="${PROJECT_VERSION}"
    )

    if(MSVC)
        target_compile_options(voicekey PRIVATE /W4 /permissive- /utf-8)
        set_property(TARGET voicekey PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(voicekey PRIVATE -Wall -Wextra -Wpedantic)
        target_link_options(voicekey PRIVATE -municode -Wl,--subsystem,windows)
    endif()

    target_link_libraries(voicekey PRIVATE
        whisper
        advapi32
        comctl32
        dwmapi
        gdi32
        gdiplus
        ole32
        shell32
        user32
        winmm
    )

    if(VOICEKEY_BUILD_INTEGRATION_TEST_TOOL)
        add_executable(voicekey_transcribe_file
            tests/transcribe_file.cpp
            src/audio_capture.cpp
            src/transcriber.cpp
            src/transcript.cpp
        )
        target_include_directories(voicekey_transcribe_file PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${miniaudio_SOURCE_DIR}"
        )
        target_compile_definitions(voicekey_transcribe_file PRIVATE
            NOMINMAX
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(voicekey_transcribe_file PRIVATE whisper ole32 user32 winmm)

        add_executable(voicekey_audio_probe
            tests/audio_probe.cpp
            src/audio_capture.cpp
        )
        target_include_directories(voicekey_audio_probe PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${miniaudio_SOURCE_DIR}"
        )
        target_compile_definitions(voicekey_audio_probe PRIVATE
            NOMINMAX
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(voicekey_audio_probe PRIVATE ole32 user32 winmm)

        add_executable(voicekey_text_injector_probe
            tests/text_injector_probe.cpp
            src/text_injector.cpp
            src/win_util.cpp
        )
        target_include_directories(voicekey_text_injector_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(voicekey_text_injector_probe PRIVATE
            NOMINMAX
            WINVER=0x0A00
            UNICODE
            _UNICODE
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(voicekey_text_injector_probe PRIVATE shell32 user32)
    endif()
endif()

if(VOICEKEY_BUILD_TESTS)
    enable_testing()
    add_executable(voicekey_core_tests
        tests/transcript_tests.cpp
        src/transcript.cpp
    )
    target_include_directories(voicekey_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME voicekey_core_tests COMMAND voicekey_core_tests)
endif()
