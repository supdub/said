#pragma once

#include "app_profile.h"

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

std::wstring utf8_to_wide(const std::string & value);
std::string wide_to_utf8(const std::wstring & value);
std::filesystem::path executable_directory();
std::optional<std::filesystem::path> resolve_model_path(const std::vector<std::wstring> & arguments);
std::optional<std::filesystem::path> resolve_grammar_model_path(
    const std::vector<std::wstring> & arguments,
    const std::optional<std::filesystem::path> & speech_model_path);
std::filesystem::path expected_model_path();
std::filesystem::path expected_grammar_model_path();
std::vector<std::wstring> command_line_arguments();
HWND focused_control(HWND foreground_window);
bool foreground_focus_matches(HWND foreground_window, HWND control);
AppIdentity app_identity_for_window(HWND window);
