#pragma once

#include <vector>
#include <string>
#include <deque>

#include "search_server.h"

class RequestQueue {
public:

    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
        requests_.push_back({result, result.empty()});
        if (requests_.size() > min_in_day_) requests_.pop_front();
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:

    struct QueryResult {
        std::vector<Document> result;
        bool isEmpty;
    };

    std::deque<QueryResult> requests_;

    const static int min_in_day_ = 1440;
    
    const SearchServer& search_server_;
};