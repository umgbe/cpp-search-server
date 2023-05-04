#pragma once

#include <vector>
#include <map>
#include <mutex>

template <typename Key, typename Value>
class ConcurrentMap {
    public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> l;
        Value& ref_to_value;
    };

    struct Bucket {
        std::map<Key, Value> data;
        std::mutex mut;
    };

    explicit ConcurrentMap(size_t bucket_count) 
    : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        int target_index = static_cast<uint64_t>(key) % buckets_.size();
        return { std::lock_guard(buckets_[target_index].mut), buckets_[target_index].data[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            for (const auto& [key, val] : buckets_[i].data) {
                std::lock_guard l(buckets_[i].mut);
                result.insert({key, val});
            }
        }
        return result;
    }

    void erase(const Key& key) {
        int target_index = static_cast<uint64_t>(key) % buckets_.size();
        buckets_[target_index].data.erase(key);
    }

    private:
    std::vector<Bucket> buckets_;
};