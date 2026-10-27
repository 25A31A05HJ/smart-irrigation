#ifndef MINHEAP_H
#define MINHEAP_H

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

// Generic binary Min-Heap.
// Compare(a, b) == true means "a must come out before b".
// With the default std::less<T>, the smallest element is on top.
template <typename T, typename Compare = std::less<T>>
class MinHeap {
private:
    std::vector<T> data;
    Compare comp;

    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (comp(data[i], data[parent])) {
                std::swap(data[i], data[parent]);
                i = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(std::size_t i) {
        std::size_t n = data.size();
        while (true) {
            std::size_t left = 2 * i + 1;
            std::size_t right = 2 * i + 2;
            std::size_t best = i;
            if (left < n && comp(data[left], data[best])) best = left;
            if (right < n && comp(data[right], data[best])) best = right;
            if (best == i) break;
            std::swap(data[i], data[best]);
            i = best;
        }
    }

public:
    MinHeap() = default;

    bool empty() const { return data.empty(); }
    std::size_t size() const { return data.size(); }

    void push(const T& value) {
        data.push_back(value);
        siftUp(data.size() - 1);
    }

    const T& top() const {
        if (data.empty()) throw std::runtime_error("MinHeap::top on empty heap");
        return data.front();
    }

    // Removes and returns the highest-priority element.
    T pop() {
        if (data.empty()) throw std::runtime_error("MinHeap::pop on empty heap");
        T result = data.front();
        data.front() = data.back();
        data.pop_back();
        if (!data.empty()) siftDown(0);
        return result;
    }
};

#endif
