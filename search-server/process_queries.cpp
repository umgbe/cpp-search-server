#include "process_queries.h"
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server] (const std::string& query) {
        return search_server.FindTopDocuments(query);
    });
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> all_documents = ProcessQueries(search_server, queries);
    std::list<Document> result;
    std::for_each(all_documents.begin(), all_documents.end(), [&result] (std::vector<Document>& document_vector) {
        std::for_each(document_vector.begin(), document_vector.end(), [&result] (Document& document) {
            result.push_back(document);
        });
    });
    return result;
}