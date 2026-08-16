#include "app_profile.h"

#include <algorithm>
#include <array>
#include <cwctype>

namespace {
std::wstring lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

template <size_t Size>
bool contains_any(const std::wstring & value, const std::array<const wchar_t *, Size> & needles) {
    return std::any_of(needles.begin(), needles.end(), [&](const wchar_t * needle) {
        return value.find(needle) != std::wstring::npos;
    });
}
}

AppProfile classify_app_identity(const AppIdentity & identity) {
    const std::wstring executable = lowercase(identity.executable);
    const std::wstring title = lowercase(identity.title);

    static constexpr std::array kShells{
        L"windowsterminal.exe", L"wt.exe", L"cmd.exe", L"powershell.exe",
        L"pwsh.exe", L"conhost.exe", L"mintty.exe", L"wezterm-gui.exe",
        L"alacritty.exe",
    };
    if (contains_any(executable, kShells)) {
        // Codex and tmux commonly share the same host process as a real shell.
        // A title heuristic is not strong enough to enable generative rewriting.
        return AppProfile::Shell;
    }

    static constexpr std::array kEditors{
        L"code.exe", L"cursor.exe", L"devenv.exe", L"notepad++.exe",
        L"sublime_text.exe", L"zed.exe", L"idea64.exe", L"pycharm64.exe",
    };
    if (contains_any(executable, kEditors)) {
        return AppProfile::CodeEditor;
    }

    static constexpr std::array kMail{
        L"outlook.exe", L"olk.exe", L"thunderbird.exe", L"mailbird.exe",
    };
    if (contains_any(executable, kMail) ||
        title.find(L"gmail") != std::wstring::npos ||
        title.find(L"outlook") != std::wstring::npos) {
        return AppProfile::Mail;
    }

    static constexpr std::array kChat{
        L"slack.exe", L"teams.exe", L"ms-teams.exe", L"discord.exe",
        L"wechat.exe", L"whatsapp.exe", L"telegram.exe",
    };
    if (contains_any(executable, kChat)) {
        return AppProfile::Chat;
    }
    static constexpr std::array kChatTitles{
        L"slack", L"discord", L"microsoft teams", L"whatsapp", L"telegram",
    };
    if (contains_any(title, kChatTitles)) {
        return AppProfile::Chat;
    }

    static constexpr std::array kDocuments{
        L"winword.exe", L"onenote.exe", L"obsidian.exe", L"notion.exe",
        L"libreoffice.exe", L"soffice.bin",
    };
    if (contains_any(executable, kDocuments)) {
        return AppProfile::Document;
    }
    static constexpr std::array kDocumentTitles{
        L"google docs", L"notion", L"office online", L"word online",
    };
    if (contains_any(title, kDocumentTitles)) {
        return AppProfile::Document;
    }

    static constexpr std::array kDeveloperPrompts{
        L"codex", L"github copilot", L"claude code", L"developer prompt",
    };
    if (contains_any(title, kDeveloperPrompts)) {
        return AppProfile::DeveloperPrompt;
    }
    return AppProfile::Unknown;
}

std::wstring app_identity_override_key(const AppIdentity & identity) {
    std::wstring executable = lowercase(identity.executable);
    std::wstring title = lowercase(identity.title);
    if (executable.empty()) {
        executable = L"unknown";
    }

    std::wstring context;
    static constexpr std::array kContextTokens{
        L"codex", L"claude", L"tmux", L"gmail", L"outlook", L"slack",
        L"discord", L"google docs", L"notion",
    };
    for (const wchar_t * token : kContextTokens) {
        if (title.find(token) != std::wstring::npos) {
            context = token;
            break;
        }
    }
    std::wstring key = executable + (context.empty() ? L"|default" : L"|" + context);
    for (wchar_t & character : key) {
        if (character == L'\\' || character == L'/' || character == L':' ||
            character == L'*' || character == L'?' || character == L'"' ||
            character == L'<' || character == L'>' || character == L'|') {
            character = L'_';
        }
    }
    if (key.size() > 160U) {
        key.resize(160U);
    }
    return key;
}

AppProfile resolve_app_profile(
    const AppIdentity & identity,
    std::optional<AppProfile> user_override) {
    return user_override.value_or(classify_app_identity(identity));
}

bool app_profile_allows_adapt(AppProfile profile) {
    return profile != AppProfile::Unknown && profile != AppProfile::Shell;
}

const wchar_t * app_profile_name(AppProfile profile) {
    switch (profile) {
    case AppProfile::Unknown:
        return L"Unknown app";
    case AppProfile::Chat:
        return L"Chat";
    case AppProfile::Mail:
        return L"Mail";
    case AppProfile::Document:
        return L"Document";
    case AppProfile::DeveloperPrompt:
        return L"Developer prompt";
    case AppProfile::CodeEditor:
        return L"Code editor";
    case AppProfile::Shell:
        return L"Shell";
    }
    return L"Unknown app";
}
