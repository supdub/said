#pragma once

#include <string>

constexpr bool default_streaming_mode_enabled() {
    return false;
}

enum class StreamingFinalDisposition {
    NoSpeech,
    KeepLiveText,
    ReplaceLiveText,
    CopyFinalText,
};

enum class StreamingRevisionDisposition {
    NoChange,
    Insert,
    ReplaceOwnedText,
    Pause,
};

StreamingRevisionDisposition plan_streaming_revision_delivery(
    const std::string & inserted_text,
    const std::string & revised_text,
    bool owns_target);

StreamingFinalDisposition plan_streaming_final_delivery(
    const std::string & inserted_text,
    const std::string & final_text,
    bool owns_target);
