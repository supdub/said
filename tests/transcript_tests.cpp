#include "transcript.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    assert(join_recognizer_segments({" Hello", " world."}) == "Hello world.");
    assert(join_recognizer_segments({"  \xE4\xBD\xA0\xE5\xA5\xBD", "\xEF\xBC\x8Cworld!  "}) ==
           "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8Cworld!");
    assert(join_recognizer_segments({"  ", "\n"}).empty());
    assert(capitalize_spelled_initialisms("use n l p and a i") == "use NLP and AI");
    assert(capitalize_spelled_initialisms("\xE6\x88\x91\xE5\x81\x9A n l p \xE5\x92\x8C a i") ==
           "\xE6\x88\x91\xE5\x81\x9A NLP \xE5\x92\x8C AI");
    assert(capitalize_spelled_initialisms("I am a developer") == "I am a developer");
    assert(capitalize_spelled_initialisms("version x") == "version x");
    assert(normalize_to_simplified_chinese("SAID 123").compare("SAID 123") == 0);
#ifdef _WIN32
    assert(normalize_to_simplified_chinese(
               "\xE9\x80\x99\xE6\x98\xAF\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87,\xE4\xB8\x8B\xE4\xB8\x80\xE5\x8F\xA5!") ==
           "\xE8\xBF\x99\xE6\x98\xAF\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87\xEF\xBC\x8C\xE4\xB8\x8B\xE4\xB8\x80\xE5\x8F\xA5\xEF\xBC\x81");
    assert(normalize_to_simplified_chinese("SAID, English 3.14!") ==
           "SAID, English 3.14!");
#endif

    assert(!audio_has_signal(std::vector<float>(1000, 0.5F)));
    assert(!audio_has_signal(std::vector<float>(16000, 0.0F)));
    assert(audio_has_signal(std::vector<float>(16000, 0.01F)));
    return 0;
}
