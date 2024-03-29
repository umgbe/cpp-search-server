#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(std::string_view stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);
       
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    using MatchedDocument = std::tuple<std::vector<std::string_view>, DocumentStatus>;

    MatchedDocument MatchDocument(std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    auto begin() const {
        return documents_ids_.begin();
    }
    
    auto end() const {
        return documents_ids_.end();
    }

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::map<std::string, double> words_and_frequencies;
        std::vector<std::string> words;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> documents_ids_; 

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text, bool remove_duplicates = true) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const;

    bool CheckForSpecialSymbols(std::string_view text) const;

    std::vector<std::string_view> SplitIntoWords(std::string_view text) const;

    bool CheckForIncorrectMinuses(std::string_view word) const;
};


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    for (std::string_view word : stop_words) {
        if (CheckForSpecialSymbols(word)) {
            throw std::invalid_argument("Стоп-слово содержит недопустимые символы"s);
        }
    }
}

constexpr double ALLOWABLE_ERROR = 1e-6;

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(), 
                [](const Document& lhs, const Document& rhs) {
                 if (std::abs(lhs.relevance - rhs.relevance) < ALLOWABLE_ERROR) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
                });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status;});
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

constexpr size_t BUCKETS_COUNT = 100;

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(BUCKETS_COUNT);
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), 
    [this, &document_to_relevance, document_predicate] (std::string_view word) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    });
    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
    [this, &document_to_relevance] (std::string_view word) {
        if (word_to_document_freqs_.count(std::string(word)) == 0) {
            return;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.erase(document_id);
        }
    });
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}