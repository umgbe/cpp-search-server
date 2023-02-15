#include "string_processing.h"

#include <stdexcept>

using namespace std::string_literals;

bool CheckForSpecialSymbols(const std::string& text) {
    for (const char c : text) {
        if ((c <= 31) && (c >= 0)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    if (CheckForSpecialSymbols(text)) throw std::invalid_argument("Текст содержит недопустимые символы"s);
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

bool CheckForIncorrectMinuses(const std::string& word) {
    if ((word[0] == '-') && ((word.size() < 2) || (word[1] == '-'))) return true;
    return false;
}