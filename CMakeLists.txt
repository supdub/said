cmake_minimum_required(VERSION 3.21)

project(SAID VERSION 0.3.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SAID_BUILD_TESTS "Build the small platform-independent test suite" ON)
option(SAID_BUILD_INTEGRATION_TEST_TOOLS "Build Windows-only integration probes" OFF)
option(SAID_BUILD_MODEL_PROBE "Build the portable local-model integration probe" OFF)
set(SAID_SHERPA_ONNX_SOURCE_DIR "" CACHE PATH "Use an existing sherpa-onnx checkout")
set(SAID_ZIG_ONNXRUNTIME_SOURCE_DIR "" CACHE PATH "Use an unpacked ONNX Runtime DLL package for the Zig cross-build")

if(WIN32)
    enable_language(RC)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_CHECK OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_C_API ON CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_GPU OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_DIRECTML OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_BINARY OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TTS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_BUILD_C_API_EXAMPLES OFF CACHE BOOL "" FORCE)

    # Microsoft's static ONNX Runtime archives require the MSVC C++ runtime.
    # The Zig/MinGW verification build therefore uses the ABI-stable DLL and
    # import library from the matching pinned ONNX Runtime release.
    if(SAID_ZIG_TARGET)
        if(SAID_ZIG_ONNXRUNTIME_SOURCE_DIR)
            set(said_onnxruntime_dynamic_SOURCE_DIR "${SAID_ZIG_ONNXRUNTIME_SOURCE_DIR}")
        else()
            FetchContent_Declare(
                said_onnxruntime_dynamic
                URL https://github.com/csukuangfj/onnxruntime-libs/releases/download/v1.27.1/onnxruntime-win-x64-MT-Release-1.27.1.tar.bz2
                URL_HASH SHA256=25122f517cb114f74009cef7482468f8537eceb7153431f1a2755d062eea0bd7
            )
            FetchContent_MakeAvailable(said_onnxruntime_dynamic)
        endif()
        set(ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR} "${said_onnxruntime_dynamic_SOURCE_DIR}/include")
        set(ENV{SHERPA_ONNXRUNTIME_LIB_DIR} "${said_onnxruntime_dynamic_SOURCE_DIR}/lib")
    endif()

    # sherpa-onnx 1.13.5 leaves the Windows import-library variable unset
    # when callers provide a preinstalled ONNX Runtime via environment paths.
    # Supplying it here keeps that supported path usable (and lets the Zig
    # validation build use the DLL ABI instead of Microsoft static archives).
    if(DEFINED ENV{SHERPA_ONNXRUNTIME_LIB_DIR})
        set(location_onnxruntime_lib2 "$ENV{SHERPA_ONNXRUNTIME_LIB_DIR}/onnxruntime.lib")
    endif()

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if(SAID_SHERPA_ONNX_SOURCE_DIR)
        add_subdirectory("${SAID_SHERPA_ONNX_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sherpa-onnx")
    else()
        FetchContent_Declare(
            sherpa_onnx
            GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
            GIT_TAG v1.13.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(sherpa_onnx)
    endif()

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

    # Adapt can use an optional local GGUF model. Keep llama.cpp
    # to its CPU library only; SAID supplies the UI, model path, and probes.
    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    set(GGML_BACKEND_DL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
        GIT_TAG b6114
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(llama_cpp)

    add_executable(said WIN32
        resources/said.rc
        src/adapt_job_queue.cpp
        src/advanced_model_download.cpp
        src/app_profile.cpp
        src/app_settings.cpp
        src/audio_capture.cpp
        src/grammar_correction.cpp
        src/grammar_corrector.cpp
        src/grammar_wait_ui.cpp
        src/incremental_refinement.cpp
        src/local_refinement.cpp
        src/main.cpp
        src/output_mode.cpp
        src/overlay.cpp
        src/rewrite_safety.cpp
        src/setup_window.cpp
        src/shortcut_binding.cpp
        src/speech_language.cpp
        src/streaming_mode.cpp
        src/text_injector.cpp
        src/transcriber.cpp
        src/transcript.cpp
        src/win_util.cpp
    )

    target_include_directories(said PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${miniaudio_SOURCE_DIR}"
    )

    target_compile_definitions(said PRIVATE
        NOMINMAX
        WINVER=0x0A00
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        SAID_VERSION="${PROJECT_VERSION}"
    )

    if(MSVC)
        target_compile_options(said PRIVATE /W4 /permissive- /utf-8)
        set_property(TARGET said PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(said PRIVATE -Wall -Wextra -Wpedantic)
        target_link_options(said PRIVATE -municode -Wl,--subsystem,windows)
    endif()

    target_link_libraries(said PRIVATE
        llama
        sherpa-onnx-cxx-api
        advapi32
        bcrypt
        bits
        comctl32
        dwmapi
        gdi32
        gdiplus
        ole32
        shell32
        user32
        winmm
    )

    if(SAID_ZIG_TARGET)
        add_custom_command(TARGET said POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${said_onnxruntime_dynamic_SOURCE_DIR}/lib/onnxruntime.dll"
                "$<TARGET_FILE_DIR:said>/onnxruntime.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${said_onnxruntime_dynamic_SOURCE_DIR}/lib/onnxruntime_providers_shared.dll"
                "$<TARGET_FILE_DIR:said>/onnxruntime_providers_shared.dll"
        )
    endif()

    if(SAID_BUILD_INTEGRATION_TEST_TOOLS)
        add_executable(said_transcribe_file
            tests/transcribe_file.cpp
            src/audio_capture.cpp
            src/speech_language.cpp
            src/transcriber.cpp
            src/transcript.cpp
        )
        target_include_directories(said_transcribe_file PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${miniaudio_SOURCE_DIR}"
        )
        target_compile_definitions(said_transcribe_file PRIVATE
            NOMINMAX
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(said_transcribe_file PRIVATE sherpa-onnx-cxx-api ole32 user32 winmm)

        add_executable(said_grammar_file
            tests/grammar_file.cpp
            src/app_profile.cpp
            src/grammar_correction.cpp
            src/grammar_corrector.cpp
            src/rewrite_safety.cpp
            src/transcript.cpp
        )
        target_include_directories(said_grammar_file PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(said_grammar_file PRIVATE
            NOMINMAX
            WINVER=0x0A00
            UNICODE
            _UNICODE
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        if(NOT MSVC)
            target_link_options(said_grammar_file PRIVATE -municode)
        endif()
        target_link_libraries(said_grammar_file PRIVATE llama)

        add_executable(said_audio_probe
            tests/audio_probe.cpp
            src/audio_capture.cpp
        )
        target_include_directories(said_audio_probe PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${miniaudio_SOURCE_DIR}"
        )
        target_compile_definitions(said_audio_probe PRIVATE
            NOMINMAX
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(said_audio_probe PRIVATE ole32 user32 winmm)

        add_executable(said_text_injector_probe
            tests/text_injector_probe.cpp
            src/text_injector.cpp
            src/win_util.cpp
        )
        target_include_directories(said_text_injector_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(said_text_injector_probe PRIVATE
            NOMINMAX
            WINVER=0x0A00
            UNICODE
            _UNICODE
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(said_text_injector_probe PRIVATE shell32 user32)
    endif()
endif()

if(SAID_BUILD_MODEL_PROBE AND NOT WIN32)
    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
        GIT_TAG b6114
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(llama_cpp)

    add_executable(said_grammar_file
        tests/grammar_file.cpp
        src/app_profile.cpp
        src/grammar_correction.cpp
        src/grammar_corrector.cpp
        src/rewrite_safety.cpp
        src/transcript.cpp
    )
    target_include_directories(said_grammar_file PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(said_grammar_file PRIVATE llama)
endif()

if(SAID_BUILD_TESTS)
    enable_testing()
    add_executable(said_core_tests
        tests/transcript_tests.cpp
        src/grammar_correction.cpp
        src/grammar_wait_ui.cpp
        src/streaming_mode.cpp
        src/transcript.cpp
    )
    target_include_directories(said_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(said_core_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(said_core_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME said_core_tests COMMAND said_core_tests)

    add_executable(said_speech_language_tests
        tests/speech_language_tests.cpp
        src/speech_language.cpp
    )
    target_include_directories(
        said_speech_language_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(said_speech_language_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(said_speech_language_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME said_speech_language_tests COMMAND said_speech_language_tests)

    add_executable(said_shortcut_tests
        tests/shortcut_tests.cpp
        src/shortcut_binding.cpp
    )
    target_include_directories(
        said_shortcut_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(said_shortcut_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(said_shortcut_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME said_shortcut_tests COMMAND said_shortcut_tests)

    add_executable(said_refinement_tests
        tests/refinement_tests.cpp
        src/adapt_job_queue.cpp
        src/app_profile.cpp
        src/grammar_correction.cpp
        src/incremental_refinement.cpp
        src/local_refinement.cpp
        src/output_mode.cpp
        src/rewrite_safety.cpp
        src/speech_language.cpp
        src/transcript.cpp
    )
    target_include_directories(said_refinement_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(said_refinement_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(said_refinement_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME said_refinement_tests COMMAND said_refinement_tests)

    add_executable(said_refinement_performance_tests
        tests/refinement_performance_tests.cpp
        src/app_profile.cpp
        src/grammar_correction.cpp
        src/incremental_refinement.cpp
        src/local_refinement.cpp
        src/output_mode.cpp
        src/rewrite_safety.cpp
        src/speech_language.cpp
        src/transcript.cpp
    )
    target_include_directories(
        said_refinement_performance_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(said_refinement_performance_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(said_refinement_performance_tests PRIVATE -UNDEBUG)
    endif()
    add_test(
        NAME said_refinement_performance_tests
        COMMAND said_refinement_performance_tests)
endif()
