#pragma once

#include "app_profile.h"
#include "grammar_correction.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

class LocalGrammarCorrector final : public GrammarCorrectionBackend {
public:
    explicit LocalGrammarCorrector(
        std::filesystem::path model_path,
        int requested_threads = 0,
        const std::atomic<bool> * cancel_requested = nullptr);
    ~LocalGrammarCorrector() override;

    LocalGrammarCorrector(const LocalGrammarCorrector &) = delete;
    LocalGrammarCorrector & operator=(const LocalGrammarCorrector &) = delete;

    std::string correct(const std::string & text) override;
    std::string rewrite_for_app(
        const std::string & text,
        AppProfile profile,
        bool final_structure);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
