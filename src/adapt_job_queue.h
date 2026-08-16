#pragma once

#include "app_profile.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>

enum class AdaptJobKind {
    Incremental,
    Final,
    ModelPath,
};

struct AdaptJob {
    AdaptJobKind kind = AdaptJobKind::Incremental;
    uint64_t session_id = 0;
    uint64_t revision = 0;
    std::string committed_prefix;
    std::string clean_text;
    AppProfile profile = AppProfile::Unknown;
    bool streaming = false;
    std::optional<std::filesystem::path> model_path;
};

// A model rewrite is slower than speech recognition, so pending live work is
// intentionally lossy: only the newest revision for a session is useful.
// Final work supersedes all pending live work for the same session.
class AdaptJobQueue {
public:
    void push_incremental(AdaptJob job);
    void push_final(AdaptJob job);
    void push_model_path(AdaptJob job);
    void cancel_session(uint64_t session_id);

    bool empty() const;
    size_t size() const;
    std::optional<AdaptJob> pop_front();

private:
    std::deque<AdaptJob> jobs_;
};
