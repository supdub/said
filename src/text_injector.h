#pragma once

#include <string>

bool inject_utf8_text(const std::string & text, std::wstring & error);
bool replace_recent_utf8_text(
    const std::string & existing_text,
    const std::string & replacement_text,
    std::wstring & error);
bool copy_utf8_text(const std::string & text, std::wstring & error);
