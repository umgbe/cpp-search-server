#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<std::set<std::string>> documents;
    std::vector<int> ids_for_deletion;
    ids_for_deletion.clear();
    for (const int document_id : search_server) {
        const std::map<std::string, double>& document = search_server.GetWordFrequencies(document_id);
        std::set<std::string> words;
        for (auto [key, value] : document) {
            words.insert(std::string(key));            
        }
        if (documents.count(words)) {
            ids_for_deletion.push_back(document_id);
        } else {
            documents.insert(words);
        }
    }

    for (const int document_id : ids_for_deletion) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}