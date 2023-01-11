#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }
    
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status) const {
        return FindTopDocuments(raw_query, [document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; });
    }
    
    template <typename FilterFunction>
    vector<Document> FindTopDocuments(const string& raw_query, FilterFunction filter_function) const {
        const Query query = ParseQuery(raw_query);
        
        vector<Document> matched_documents = FindAllDocuments(query, filter_function);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename FilterFunction>
    vector<Document> FindAllDocuments(const Query& query, FilterFunction filter_function) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (filter_function(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

template <typename T>
ostream& operator<<(ostream& os, const vector<T>& v) {   
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
ostream& operator<<(ostream& os, const set<T>& s) {   
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
ostream& operator<<(ostream& os, const map<T,U> m) {
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
/*
ostream& operator<<(ostream& os, const DocumentStatus status) {
    switch (status) {
        case DocumentStatus::ACTUAL:
        os << "DocumentStatus::ACTUAL"s;
        break;
        case DocumentStatus::BANNED:
        os << "DocumentStatus::BANNED"s;
        break;
        case DocumentStatus::IRRELEVANT:
        os << "DocumentStatus::IRRELEVANT"s;
        break;
        case DocumentStatus::REMOVED:
        os << "DocumentStatus::REMOVED"s;
        break;
    }
    return os;
}*/

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& function, const string& function_name) {
    function();
    cerr << function_name << " OK"s << endl;
}
#define RUN_TEST(func) RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/

void TestAddDocument() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {7, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 1};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("ioy"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Добавленный документ не найден"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 65, "Найден неправильный документ"s);
    }
}

void TestStopWords() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {7, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("jj"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Найден документ, содержащий стоп-слово"s);
    }
}

void TestMinusWords() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {7, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 1};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("jigj -ioy"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Минус-слово не исключило документ"s);
    }
}

void TestMatchDocument() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {7, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 1};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const string raw_query = "ioy tpp fr"s;
        const auto [matched_words, status] = server.MatchDocument(raw_query, 65);
        const vector<string> expected_result = {"fr"s, "ioy"s};
        ASSERT_EQUAL_HINT(matched_words, expected_result, "Матчинг документов вернул некорректный набор слов"s);
        //ASSERT_EQUAL_HINT(status, DocumentStatus::ACTUAL, "Матчинг документов вернул некорректный статус"s);
        ASSERT_HINT((status == DocumentStatus::ACTUAL), "Матчинг документов вернул некорректный статус"s);
    }
    {
        const string raw_query = "ioy tpp -fr"s;
        const auto [matched_words, status] = server.MatchDocument(raw_query, 65);
        ASSERT_HINT(matched_words.empty(), "Матчинг документов некорректно обработал минус-слово"s);
    }
}

void TestSortRelevance() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {7, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 1};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("ioy jigj"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "Добавленные документы не найдены"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 65, "Документы не отсортированы по релевантности"s);
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL_HINT(doc1.id, 85, "Документы не отсортированы по релевантности"s);
    }
}

void TestRating() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {6, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 8};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("ioy jigj"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "Добавленные документы не найдены"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 4, "Рейтинг документа посчитан некорректно"s);
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL_HINT(doc1.rating, 6, "Рейтинг документа посчитан некорректно"s);
    }
}

void TestCustomPredicate() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {6, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 8};
        server.AddDocument(doc_id, content, DocumentStatus::IRRELEVANT, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("ioy jigj"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Поиск документа по статусу работает некорректно"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 85, "Поиск документа по статусу работает некорректно"s);
    }
    {
        const auto found_docs = server.FindTopDocuments("jigj"s, [](int document_id, DocumentStatus status, int rating) {
            return (rating < 5);
        });
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Поиск документа с использованием произвольной функции работает некорректно"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 65, "Поиск документа с использованием произвольной функции работает некорректно"s);
    }
}

void TestRelevance() {
    SearchServer server;
    server.SetStopWords("a the jj");
    {
        const int doc_id = 85;
        const string content = "jigj a jj opr"s;
        const vector<int> ratings = {6, 3, 9};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const int doc_id = 65;
        const string content = "ioy jiijiji fr a jigj"s;
        const vector<int> ratings = {2, 2, 8};
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    }
    {
        const auto found_docs = server.FindTopDocuments("opr"s);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT((abs(doc0.relevance - 0.34) < 0.01), "Расчет релевантности работает некорректно"s);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    // Не забудьте вызывать остальные тесты здесь
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRating);
    RUN_TEST(TestCustomPredicate);
    RUN_TEST(TestRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}