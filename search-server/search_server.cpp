#include "search_server.h"

#include <stdexcept>
#include <cmath>
#include <numeric>

using namespace std::string_literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("id добавляемого докумета меньше нуля"s);
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("id добавляемого документа уже существует"s);
    }
    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    std::map<std::string, double> words_to_save_with_document;
    std::set<std::string> words_to_save_separate_from_document;
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_to_save_with_document[word] += inv_word_count;
        words_to_save_separate_from_document.insert(word);
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, words_to_save_with_document});
    documents_ids_.insert(document_id);
    words_to_document_ids[words_to_save_separate_from_document].insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const  {
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return tie(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::CheckForSpecialSymbols(const std::string& text) const {
    for (const char c : text) {
        if ((c <= 31) && (c >= 0)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SearchServer::SplitIntoWords(const std::string& text) const {
    std::vector<std::string> words;
    std::string word;
    if (CheckForSpecialSymbols(text)) {
        throw std::invalid_argument("Текст содержит недопустимые символы"s);
    }
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

bool SearchServer::CheckForIncorrectMinuses(const std::string& word) const {
    return ((word[0] == '-') && ((word.size() < 2) || (word[1] == '-')));
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    if (CheckForIncorrectMinuses(text)) {
        throw std::invalid_argument("Поисковый запрос содержит некорректно поставленные минусы"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (count(documents_ids_.begin(), documents_ids_.end(), document_id)) {
        return documents_.at(document_id).words_and_frequencies;
    } 
    else {
        static std::map<std::string, double> empty_result;
        empty_result.clear();
        return empty_result;
    }
}

void SearchServer::RemoveDocument(int document_id) {
    for (auto [word, freq] : documents_.at(document_id).words_and_frequencies) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    std::set<std::string> key_to_find;
    for (auto [words, ids] : words_to_document_ids) {
        if (count(ids.begin(), ids.end(), document_id)) {
            key_to_find = words;
            break;
        }
    }
    words_to_document_ids.at(key_to_find).erase(document_id);
    if (words_to_document_ids.at(key_to_find).empty()) {
        words_to_document_ids.erase(key_to_find);
    }
    documents_.erase(document_id);
    documents_ids_.erase(document_id);
}