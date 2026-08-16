#pragma once

#include <optional>
#include <string>

enum class AppProfile {
    Unknown,
    Chat,
    Mail,
    Document,
    DeveloperPrompt,
    CodeEditor,
    Shell,
};

struct AppIdentity {
    std::wstring executable;
    std::wstring title;
};

AppProfile classify_app_identity(const AppIdentity & identity);
std::wstring app_identity_override_key(const AppIdentity & identity);
AppProfile resolve_app_profile(
    const AppIdentity & identity,
    std::optional<AppProfile> user_override);
bool app_profile_allows_adapt(AppProfile profile);
const wchar_t * app_profile_name(AppProfile profile);
