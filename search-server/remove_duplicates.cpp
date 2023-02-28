#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<std::pair<int, std::set<std::string>>> all_documents;

    for (const int document_id : search_server) {
        std::map document_data = search_server.GetWordFrequencies(document_id);
        std::set<std::string> document_words;
        for (auto [key, value] : document_data) {
            document_words.insert(key);
        }
        all_documents.push_back({document_id, document_words});
    }

    std::sort(all_documents.begin(), all_documents.end(), 
    [](std::pair<int, std::set<std::string>>& left, std::pair<int, std::set<std::string>>& right) {
        if (left.second == right.second) return left.first > right.first;
        return left.second < right.second;
    });

    std::vector<int> ids_for_deletion;
    for (int i = 0; i < (all_documents.size() - 1); ++i) {
        if (all_documents[i].second == all_documents[i+1].second) {
            ids_for_deletion.push_back(all_documents[i].first);
        }
    }

    for (const int document_id : ids_for_deletion) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}