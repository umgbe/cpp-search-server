#include "remove_duplicates.h"

bool CompareMapsIgnoringValues(const std::map<std::string, double>& first, const std::map<std::string, double>& second) {
    std::set<std::string> first_set;
    for (auto [key, value] : first) {
            first_set.insert(key);
    }
    std::set<std::string> second_set;
    for (auto [key, value] : second) {
            second_set.insert(key);
    }
    return first_set == second_set;
}

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> ids_for_deletion;
    ids_for_deletion.clear();
    auto i = search_server.begin();
    auto last_element = search_server.end();
    if (i == last_element) {
        return;
    } else {
        --last_element;
    }
    while (i != last_element) {
        int first_document_id = *i;
        std::map<std::string,double> first_document = search_server.GetWordFrequencies(first_document_id);
        ++i;
        int second_document_id = *i;
        std::map<std::string,double> second_document = search_server.GetWordFrequencies(second_document_id);
        if (CompareMapsIgnoringValues(first_document, second_document)) {
            bool end_of_sequence_found = false;
            int minimum_id = std::min(first_document_id, second_document_id);
            ids_for_deletion.push_back(std::max(first_document_id, second_document_id));
            while ((!end_of_sequence_found) && (i != last_element)) {
                first_document_id = *i;
                first_document = search_server.GetWordFrequencies(first_document_id);
                ++i;
                second_document_id = *i;
                second_document = search_server.GetWordFrequencies(second_document_id);
                if (!CompareMapsIgnoringValues(first_document, second_document)) {
                    end_of_sequence_found = true;
                } else {
                    int new_minimum_id = std::min(minimum_id, second_document_id);
                    ids_for_deletion.push_back(std::max(minimum_id, second_document_id));
                    minimum_id = new_minimum_id;
                }
            }
        }
    }

    for (const int document_id : ids_for_deletion) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}