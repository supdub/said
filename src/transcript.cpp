#include "transcript.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
bool is_ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

#ifdef _WIN32
bool is_cjk_character(wchar_t value) {
    return (value >= 0x3400 && value <= 0x4DBF) ||
           (value >= 0x4E00 && value <= 0x9FFF) ||
           (value >= 0xF900 && value <= 0xFAFF);
}

wchar_t chinese_punctuation_for(wchar_t value) {
    switch (value) {
    case L',': return L'\uFF0C';
    case L'.': return L'\u3002';
    case L'?': return L'\uFF1F';
    case L'!': return L'\uFF01';
    case L':': return L'\uFF1A';
    case L';': return L'\uFF1B';
    default: return value;
    }
}
#endif
}

std::string join_recognizer_segments(const std::vector<std::string> & segments) {
    std::string result;
    for (const auto & segment : segments) {
        result += segment;
    }

    const auto first = std::find_if_not(result.begin(), result.end(), [](unsigned char value) {
        return is_ascii_space(value);
    });
    if (first == result.end()) {
        return {};
    }
    const auto last = std::find_if_not(result.rbegin(), result.rend(), [](unsigned char value) {
        return is_ascii_space(value);
    }).base();
    return std::string(first, last);
}

std::string normalize_to_simplified_chinese(const std::string & text) {
#ifdef _WIN32
    if (text.empty()) {
        return {};
    }

    const int wide_count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_count <= 0) {
        return text;
    }

    std::wstring wide(static_cast<size_t>(wide_count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), wide.data(), wide_count) <= 0) {
        return text;
    }

    const int normalized_count = LCMapStringEx(
        L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), wide_count,
        nullptr, 0, nullptr, nullptr, 0);
    if (normalized_count <= 0) {
        return text;
    }

    std::wstring normalized(static_cast<size_t>(normalized_count), L'\0');
    if (LCMapStringEx(L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), wide_count,
                      normalized.data(), normalized_count, nullptr, nullptr, 0) <= 0) {
        return text;
    }

    for (size_t index = 0; index < normalized.size(); ++index) {
        const wchar_t mapped = chinese_punctuation_for(normalized[index]);
        if (mapped == normalized[index]) {
            continue;
        }
        const bool follows_chinese = index > 0 && is_cjk_character(normalized[index - 1]);
        const bool precedes_chinese = index + 1 < normalized.size() &&
                                      is_cjk_character(normalized[index + 1]);
        if (follows_chinese || precedes_chinese) {
            normalized[index] = mapped;
        }
    }

    const int utf8_count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), normalized_count,
        nullptr, 0, nullptr, nullptr);
    if (utf8_count <= 0) {
        return text;
    }

    std::string result(static_cast<size_t>(utf8_count), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            normalized.data(), normalized_count,
                            result.data(), utf8_count, nullptr, nullptr) <= 0) {
        return text;
    }
    return result;
#else
    return text;
#endif
}

bool audio_has_signal(const std::vector<float> & samples) {
    constexpr size_t kMinimumSamples = 16000U / 4U;
    if (samples.size() < kMinimumSamples) {
        return false;
    }

    double square_sum = 0.0;
    float peak = 0.0F;
    for (const float sample : samples) {
        if (!std::isfinite(sample)) {
            continue;
        }
        const float absolute = std::abs(sample);
        peak = std::max(peak, absolute);
        square_sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const double rms = std::sqrt(square_sum / static_cast<double>(samples.size()));
    return peak >= 0.003F || rms >= 0.0005;
}
