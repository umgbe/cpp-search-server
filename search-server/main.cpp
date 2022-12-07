#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
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
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        if (text.size() > 0) {
            for (const string& word : SplitIntoWords(text)) {
                stop_words_.insert(word);
            }
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        document_count_++;
        double increment = 1.0 / static_cast<double>(words.size());
        
        for (const string& word : words) {
            documents_[word][document_id] += increment;
        }

    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const set<string> query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    map<string, map<int, double>> documents_;
    int document_count_ = 0;
    set<string> stop_words_;

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

    set<string> ParseQuery(const string& text) const {
        set<string> query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            query_words.insert(word);
        }
        return query_words;
    }

    vector<Document> FindAllDocuments(const set<string>& query_words) const {
        
        vector<Document> matched_documents;
        set<string> minus_words;
        map<int, double> results;
        
        for (const string& word : query_words) {
            if (word[0] != '-' && documents_.count(word)) {
                double idf = log(static_cast<double>(document_count_) / static_cast<double>(documents_.at(word).size()));
                for (const auto& [id, tf] : documents_.at(word)) {
                    results[id] += idf * tf;
                }
            }
            if (word[0] == '-') {
                string corrected_word = word.substr(1);
                if (documents_.count(corrected_word)) {
                    minus_words.insert(corrected_word);
                }
            }
        }

        for (const string& word : minus_words) {
            for (const auto& [id, tf] : documents_.at(word)) {
                results.erase(id);
            }
        }

        for (auto& [id, relevance] : results) {
            matched_documents.push_back({id, relevance});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {

    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();

    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}