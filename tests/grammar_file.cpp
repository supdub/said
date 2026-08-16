#include "grammar_correction.h"
#include "grammar_corrector.h"
#include "app_profile.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <chrono>
#include <iostream>
#include <string>

namespace {
#ifdef _WIN32
std::string utf8(const std::wstring & value) {
    if (value.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}
#endif

int run_grammar_file(
    const std::filesystem::path & model_path,
    const std::string & mode,
    const std::string & text) {
    const bool advanced = mode == "--enabled" || mode == "--advanced" ||
                          mode == "--advanced-safe";
    const bool standard = mode == "--standard";
    const bool safe_advanced = mode == "--advanced-safe";
    LocalGrammarCorrector corrector{model_path};
    if (mode == "--benchmark-developer") {
        std::string output;
        std::chrono::milliseconds slowest_warm{0};
        for (int attempt = 0; attempt < 4; ++attempt) {
            const auto start = std::chrono::steady_clock::now();
            output = corrector.rewrite_for_app(
                text, AppProfile::DeveloperPrompt, true);
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (attempt > 0 && elapsed > slowest_warm) {
                slowest_warm = elapsed;
            }
        }
        std::cout << output << "\n";
        std::cerr << "slowest_warm_ms=" << slowest_warm.count() << "\n";
        return slowest_warm <= std::chrono::seconds(3) ? 0 : 6;
    }
    AppProfile adapt_profile = AppProfile::Unknown;
    bool final_structure = true;
    const bool safe_adapt = mode == "--adapt-developer-final-safe" ||
                            mode == "--adapt-mail-final-safe" ||
                            mode == "--adapt-document-final-safe" ||
                            mode == "--adapt-chat-live-safe";
    if (mode == "--adapt-developer-final" ||
        mode == "--adapt-developer-final-safe") {
        adapt_profile = AppProfile::DeveloperPrompt;
    } else if (mode == "--adapt-mail-final" || mode == "--adapt-mail-final-safe") {
        adapt_profile = AppProfile::Mail;
    } else if (mode == "--adapt-document-final" ||
               mode == "--adapt-document-final-safe") {
        adapt_profile = AppProfile::Document;
    } else if (mode == "--adapt-chat-live" || mode == "--adapt-chat-live-safe") {
        adapt_profile = AppProfile::Chat;
        final_structure = false;
    }
    if (adapt_profile != AppProfile::Unknown) {
        try {
            std::cout << corrector.rewrite_for_app(
                text, adapt_profile, final_structure) << "\n";
            return 0;
        } catch (const std::exception & error) {
            if (safe_adapt) {
                std::cout << text << "\n";
                return 0;
            }
            std::cerr << "Adapt rewrite did not complete: " << error.what() << "\n";
            return 5;
        }
    }
    const GrammarCorrectionResult result = process_grammar_correction(
        text,
        advanced ? GrammarCorrectionMode::Advanced
                 : (standard ? GrammarCorrectionMode::Standard : GrammarCorrectionMode::Off),
        &corrector);
    std::cout << result.text << "\n";

    if (advanced && result.status != GrammarCorrectionStatus::AdvancedApplied &&
        !(safe_advanced && result.status == GrammarCorrectionStatus::AdvancedFallback)) {
        std::cerr << "grammar correction did not complete\n";
        return 3;
    }
    if (!advanced && !standard && result.status != GrammarCorrectionStatus::Disabled) {
        std::cerr << "disabled grammar correction did not bypass the model\n";
        return 4;
    }
    return 0;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t ** argv) {
    if (argc != 4) {
        std::cerr << "usage: said_grammar_file MODEL MODE TEXT\n";
        return 2;
    }

    return run_grammar_file(
        std::filesystem::path(argv[1]),
        utf8(argv[2]),
        utf8(argv[3]));
}
#else
int main(int argc, char ** argv) {
    if (argc != 4) {
        std::cerr << "usage: said_grammar_file MODEL MODE TEXT\n";
        return 2;
    }
    return run_grammar_file(argv[1], argv[2], argv[3]);
}
#endif
