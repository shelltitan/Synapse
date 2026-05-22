#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <utility>

template <typename T, typename size_type = std::uint32_t>
class SparseSet {
public:
    template <typename U>
    void Insert(size_type index, U&& value) {
        if(index >= sparse.size()) {
            sparse.resize(index + 1, 0);
        }

        if(Contains(index)) {
            auto dense_index = sparse[index];
            dense_data[dense_index] = std::forward<U>(value);
            return;
        }

        if(size >= dense_data.size()) {
            dense_data.emplace_back(std::forward<U>(value));
            dense_index_to_sparse.push_back(index);
        } else {
            auto dense_index = size;
            dense_data[dense_index] = std::forward<U>(value);
            dense_index_to_sparse[dense_index] = index;
        }

        sparse[index] = size;
        ++size;
    }

    template <typename U>
    size_type Add(U&& item) {
        return Emplace(std::forward<U>(item));
    }

    template <typename... Args>
    size_type Emplace(Args&&... args) {
        auto dense_index = size++;

        // try to reuse last freed index
        if(dense_index < dense_index_to_sparse.size()) {
            auto sparse_idx = dense_index_to_sparse[dense_index];
            sparse[sparse_idx] = dense_index;
            dense_data[dense_index] = T(std::forward<Args>(args)...);
            return sparse_idx;
        }

        // allocate new index
        auto sparse_idx = static_cast<size_type>(sparse.size());

        dense_index_to_sparse.push_back(sparse_idx);
        sparse.push_back(dense_index);
        dense_data.emplace_back(std::forward<Args>(args)...);

        return sparse_idx;
    }

    bool Remove(size_type index) {
        if(!Contains(index)) {
            return false;
        }

        auto dense_index = sparse[index];
        auto end_dense_index = --size;
        auto end_sparse_index = dense_index_to_sparse[end_dense_index];

        if (dense_index != end_dense_index) {
            dense_data[dense_index] = std::move(dense_data[end_dense_index]);
            dense_index_to_sparse[dense_index] = end_sparse_index;
            sparse[end_sparse_index] = dense_index;
        }

        dense_index_to_sparse[end_dense_index] = index;
        sparse[index] = end_dense_index;

        return true;
    }

    bool Contains(size_type index) {
        if(index >= sparse.size()) {
            return false;
        }
        auto dense_index = sparse[index];

        if (dense_index >= size) {
            return false;
        }

        return dense_index_to_sparse[dense_index] == index;
    }

    T& operator[](size_type index) {
        auto dense_index = sparse[index];
        return dense_data[dense_index];
    }

    std::optional<T*const> Get(size_type index) {
        if(!Contains(index)) {
            return std::nullopt;
        }
        auto dense_index = sparse[index];
        return {&dense_data[dense_index]};
    }

    std::pair<typename std::vector<T>::const_iterator, 
        typename std::vector<T>::const_iterator>
    iterator() {
        auto start = dense_data.begin();
        auto end = dense_data.begin() + size;
        return { start, end };
    }

private:
    size_type size = 0;
    std::vector<size_type> sparse;
    std::vector<size_type> dense_index_to_sparse;
    std::vector<T> dense_data;
};
