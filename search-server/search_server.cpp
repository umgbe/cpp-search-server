#include "search_server.h"

#include <stdexcept>
#include <cmath>
#include <numeric>

using namespace std::string_literals;

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(std::string_view(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("id добавляемого докумета меньше нуля"s);
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("id добавляемого документа уже существует"s);
    }
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
    std::map<std::string, double> words_to_save_with_document;
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words) {
        word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
        words_to_save_with_document[std::string(word)] += inv_word_count;
    }
    std::vector<std::string> words_without_duplicates(words.begin(), words.end());
    std::sort(words_without_duplicates.begin(), words_without_duplicates.end());
    auto last = std::unique(words_without_duplicates.begin(), words_without_duplicates.end());
    words_without_duplicates.erase(last, words_without_duplicates.end());
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, words_to_save_with_document, words_without_duplicates});
    documents_ids_.insert(document_id);

}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
}

std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, std::string_view raw_query) const {
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const  {
    if (documents_ids_.count(document_id) == 0) {
        throw std::out_of_range("document_id не существует"s);
    }

    const Query query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    if (!(std::any_of(query.minus_words.begin(), query.minus_words.end(),
            [this, &matched_words, document_id] (std::string_view word) {
                return word_to_document_freqs_.at(std::string(word)).count(document_id);
            })))
    {
        matched_words.reserve(query.plus_words.size());
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(std::string(word)) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
                matched_words.push_back(word);
            }
        }
    }
    return tie(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const {
    if (documents_ids_.count(document_id) == 0) {
        throw std::out_of_range("document_id не существует"s);
    }

    const Query query = ParseQuery(raw_query, false);

    std::vector<std::string_view> matched_words;
    if (!(std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [this, &matched_words, document_id] (std::string_view word) {
                if (word_to_document_freqs_.count(std::string(word)) == 0) {
                    return false;
                } else {
                    return word_to_document_freqs_.at(std::string(word)).count(document_id) > 0;
                }
                
            }))) 
    {
        matched_words.resize(query.plus_words.size());
        auto it = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), 
        [this, document_id] (std::string_view word) {
            if (word_to_document_freqs_.count(std::string(word)) == 0) {
                return false;
            }
            if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
                return true;
            }
            return false;
        });
        matched_words.resize(std::distance(matched_words.begin(), it));
    }

    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    return tie(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(std::string(word)) > 0;
}

bool SearchServer::CheckForSpecialSymbols(std::string_view text) const {
    for (const char c : text) {
        if ((c <= 31) && (c >= 0)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string_view> SearchServer::SplitIntoWords(std::string_view text) const {
    if (CheckForSpecialSymbols(text)) {
        throw std::invalid_argument("Текст содержит недопустимые символы"s);
    }

    std::vector<std::string_view> words;
    bool word_found = false;
    auto word_start = text.begin();
    for (auto it = text.begin(); it != text.end(); ++it) {
        if (*it == ' ') {
            if (word_found) {
                words.push_back(text.substr(std::distance(text.begin(), word_start), std::distance(word_start, it)));
                word_found = false;
            }
        } else {
            if (!word_found) {
                word_start = it;
                word_found = true;
            }
        }
    }
    if (word_found) {
        words.push_back(text.substr(std::distance(text.begin(), word_start), std::distance(word_start, text.end())));
    }
    return words;
}

bool SearchServer::CheckForIncorrectMinuses(std::string_view word) const {
    return ((word[0] == '-') && ((word.size() < 2) || (word[1] == '-')));
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool remove_duplicates) const {
    Query query;
    const std::vector<std::string_view> words = SplitIntoWords(text);
    query.minus_words.reserve(words.size());
    query.plus_words.reserve(words.size());
    for (std::string_view word : words) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    if (remove_duplicates) {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        auto last = std::unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(last, query.minus_words.end());
        std::sort(query.plus_words.begin(), query.plus_words.end());
        last = std::unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(last, query.plus_words.end());
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
}

std::map<std::string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    std::map<std::string_view, double> result;
    if (count(documents_ids_.begin(), documents_ids_.end(), document_id)) {
        for (auto it = result.begin(); it != result.end(); ++it) {
            result.insert({std::string_view(it->first), it->second});
        }
    }
    return result;

}

void SearchServer::RemoveDocument(int document_id) {
    for (auto [word, freq] : documents_.at(document_id).words_and_frequencies) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    documents_.erase(document_id);
    documents_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    std::for_each(std::execution::par, documents_.at(document_id).words.begin(), documents_.at(document_id).words.end(), 
    [this, document_id] (const std::string& word) {
        word_to_document_freqs_.at(word).erase(document_id);
    });
    documents_.erase(document_id);
    documents_ids_.erase(document_id);
}