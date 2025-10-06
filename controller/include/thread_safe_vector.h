#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

template <typename T>
class ThreadSafeVector {
   public:
    void add(std::shared_ptr<T> item) {
        std::lock_guard<std::mutex> lk(mx_);
        data_.push_back(std::move(item));
    }

    void remove_if(std::function<bool(const std::shared_ptr<T>&)> pred) {
        std::lock_guard<std::mutex> lk(mx_);
        data_.erase(std::remove_if(data_.begin(), data_.end(), pred),
                    data_.end());
    }

    template <typename F>
    void for_each(F&& f) {
        std::lock_guard<std::mutex> lk(mx_);
        for (auto& item : data_) f(item);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mx_);
        return data_.size();
    }

   private:
    mutable std::mutex mx_;
    std::vector<std::shared_ptr<T>> data_;
};
