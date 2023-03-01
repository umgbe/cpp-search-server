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

    std::vector<int> ids_for_deletion;
    int i = 0;
    while (i < (all_documents.size() - 1)) {
        if (all_documents[i].second == all_documents[i+1].second) {
            bool end_of_sequence_found = false;
            int minimum_id = std::min(all_documents[i].first, all_documents[i+1].first);
            ids_for_deletion.push_back(std::max(all_documents[i].first, all_documents[i+1].first));
            ++i;
            while (!end_of_sequence_found) {
                if (all_documents[i].second != all_documents[i+1].second) {
                    end_of_sequence_found = true;
                } else {
                    int new_minimum_id = std::min(minimum_id, all_documents[i+1].first);
                    ids_for_deletion.push_back(std::max(minimum_id, new_minimum_id));
                    minimum_id = new_minimum_id;
                    ++i;
                }
            }
        }
        ++i;
    }

    for (const int document_id : ids_for_deletion) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}