#pragma once

#include <iostream>
#include <vector>
#include <stdexcept>

template <typename It>
class IteratorRange {
    public:

        IteratorRange(It start, It finish)
        :   range_start_(start),
            range_finish_(finish)
        {
            next_iterator_range_ = nullptr;
            i_am_pointing_to_nothing_ = false;
        }

        It GetStart() const {
            return range_start_;
        }

        It GetFinish() const {
            return range_finish_;
        }

        void SetStart(It it) {
            range_start_ = it;
        }

        void SetFinish(It it) {
            range_finish_ = it;
        }

        IteratorRange* GetNextIteratorRange() const {
            return next_iterator_range_;
        }

        void SetNextIteratorRange(IteratorRange* it) {
            next_iterator_range_ = it;
        }

        bool IsLast() const {
            return i_am_pointing_to_nothing_;
        }

        IteratorRange& operator++() {
            if (i_am_pointing_to_nothing_ == true) {
                throw std::out_of_range(""s);
            }
            if (next_iterator_range_ == nullptr) {
                i_am_pointing_to_nothing_ = true;
                return *this;
            }
            range_start_ = next_iterator_range_->GetStart();
            range_finish_ = next_iterator_range_->GetFinish();
            next_iterator_range_ = next_iterator_range_->GetNextIteratorRange();
            return *this;
        }

        bool operator==(const IteratorRange& right) {
            return ((range_start_ == right.GetStart()) && (range_finish_ == right.GetFinish()) && (next_iterator_range_ == right.GetNextIteratorRange()) && (i_am_pointing_to_nothing_ == right.IsLast()));
        }

        bool operator!=(const IteratorRange& right) {
            return !(*this == right);
        }

        IteratorRange& operator*() {
            return *this;
        }

    private:
        It range_start_;
        It range_finish_;
        IteratorRange* next_iterator_range_;
        bool i_am_pointing_to_nothing_;
};

template <typename It>
class Paginator {

    public:

    Paginator(const It container_start, const It container_finish, const size_t page_size) {
        int dist = distance(container_start, container_finish);
        int number_of_full_pages = dist / page_size;
        It element = container_start;
        for (int i = 0; i < number_of_full_pages; i++) {
            It start = element;
            advance(element, page_size);
            It finish = element;
            IteratorRange range(start, finish);
            pages_.push_back(range);
        }
        if (dist % page_size != 0) {
            It start = element;
            advance(element, (dist % page_size));
            It finish = element;
            IteratorRange range(start, finish);
            pages_.push_back(range);
        }
        for (int i = 0; i < pages_.size() - 1; i++) {
            pages_[i].SetNextIteratorRange(&pages_[i + 1]);
        }
    }

    IteratorRange<It> begin() const {
        return pages_.front();
    }

    IteratorRange<It> end() const {
        IteratorRange result = pages_.back();
        ++result;
        return result;
    }

    private:

    std::vector<IteratorRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template<typename It>
std::ostream& operator<<(std::ostream& out, IteratorRange<It>& ir) {
    int dist = distance(ir.GetStart(), ir.GetFinish());
    It element = ir.GetStart();
    for (int i = 0; i < dist; i++) {
        out << *element;
        advance(element, 1);
    }
    return out;
}