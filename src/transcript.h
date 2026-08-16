#pragma once

#include <string>
#include <vector>

std::string join_recognizer_segments(const std::vector<std::string> & segments);
std::string capitalize_spelled_initialisms(const std::string & text);
std::string normalize_to_simplified_chinese(const std::string & text);
bool audio_has_signal(const std::vector<float> & samples);
