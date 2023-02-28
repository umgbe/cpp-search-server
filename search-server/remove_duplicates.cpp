#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> ids_for_deletion;
    for (const auto [words, ids] : search_server.words_to_document_ids) {
        if (ids.size() > 1) {
            int id_to_save = *std::min_element(ids.begin(), ids.end());
            for (int id_to_delete : ids) {
                if (id_to_delete != id_to_save) {
                    ids_for_deletion.push_back(id_to_delete);
                }
            }
        }
    }
    for (const int document_id : ids_for_deletion) {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}