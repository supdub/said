#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

std::wstring utf8_to_wide(const std::string & value);
std::string wide_to_utf8(const std::wstring & value);
std::filesystem::path executable_directory();
std::optional<std::filesystem::path> resolve_model_path(const std::vector<std::wstring> & arguments);
std::filesystem::path expected_model_path();
std::vector<std::wstring> command_line_arguments();
