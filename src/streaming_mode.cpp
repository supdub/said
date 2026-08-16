#include "streaming_mode.h"

StreamingRevisionDisposition plan_streaming_revision_delivery(
    const std::string & inserted_text,
    const std::string & revised_text,
    bool owns_target) {
    if (!owns_target) {
        return StreamingRevisionDisposition::Pause;
    }
    if (inserted_text == revised_text) {
        return StreamingRevisionDisposition::NoChange;
    }
    if (inserted_text.empty()) {
        return revised_text.empty()
            ? StreamingRevisionDisposition::NoChange
            : StreamingRevisionDisposition::Insert;
    }
    // Never turn a failed replacement into an append. The exact tracked
    // suffix must be removed before any revised text is inserted.
    return StreamingRevisionDisposition::ReplaceOwnedText;
}

StreamingFinalDisposition plan_streaming_final_delivery(
    const std::string & inserted_text,
    const std::string & final_text,
    bool owns_target) {
    if (final_text.empty()) {
        return StreamingFinalDisposition::NoSpeech;
    }
    if (!owns_target) {
        return StreamingFinalDisposition::CopyFinalText;
    }
    // inserted_text is the exact range SAID typed while it retained ownership
    // of the focused field. Replace that range whenever final recognition or
    // refinement changes it, including when only part of the final phrase
    // had appeared live. Focus changes and user input revoke ownership above.
    if (final_text != inserted_text) {
        return StreamingFinalDisposition::ReplaceLiveText;
    }
    return StreamingFinalDisposition::KeepLiveText;
}
