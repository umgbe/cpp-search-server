#include <iostream>

using namespace std::string_literals;

#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"
#include "remove_duplicates.h"

using namespace std;

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {   
    if (!v.empty()) { 
        os << "["s;
        int i = 0;
        for (const T& element : v) {
            os << element;
            if (i < (v.size() - 1)) os << ", "s;
            i++;
        }     
        os << "]"s;
    }
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& s) {   
    if (!s.empty()) { 
        os << "{"s;
        int i = 0;
        for (const T& element : s) {
            os << element;
            if (i < (s.size() - 1)) os << ", "s;
            i++;
        }     
        os << "}"s;
    }
    return os;
}

template <typename T, typename U>
std::ostream& operator<<(std::ostream& os, const std::map<T,U> m) {
    if (!m.empty()) {
        os << "{"s;
        int i = 0;
        for (const auto [key, value] : m) {
           os << key << ": "s << value;
           if (i < (m.size() - 1)) os << ", "s;
           i++;
        }
        os << "}";
    }
    return os;
}

int main() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    search_server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
    
    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
}