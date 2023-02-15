#include "document.h"

using namespace std::string_literals;

std::ostream& operator<<(std::ostream& out, const Document& doc) {
    out << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
    return out;
}