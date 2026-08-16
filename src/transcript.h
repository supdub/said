#pragma once

#include <string>
#include <vector>

std::string join_recognizer_segments(const std::vector<std::string> & segments);
std::string append_recognizer_segment(std::string & transcript, const std::string & segment);
std::string capitalize_spelled_initialisms(const std::string & text);
// Canonicalize punctuation without changing words or technical tokens. Chinese
// clauses use full-width punctuation; English clauses, numbers, URLs, paths,
// and versions keep ASCII punctuation.
std::string normalize_bilingual_punctuation(const std::string & text);
std::string normalize_to_simplified_chinese(const std::string & text);
bool audio_has_signal(const std::vector<float> & samples);
