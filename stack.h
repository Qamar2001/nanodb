// stack.h - custom templated stack backed by a resizable raw array.
// no STL used. grows geometrically like vector would but we own the memory.
#ifndef STACK_H
#define STACK_H

#include <cstddef>

template <typename T>
class Stack {
    T* data;
    int capacity;
    int topIdx;   // index of next free slot (== current size)
public:
    Stack(int initialCap = 16) {
        capacity = (initialCap > 0) ? initialCap : 16;
        data = new T[capacity];
        topIdx = 0;
    }
    ~Stack() { delete[] data; }

    // disallow copy - deep copy is easy to mis-do and we don't need it
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    void push(const T& v) {
        if (topIdx == capacity) grow();
        data[topIdx++] = v;
    }
    T pop() {
        // caller must guard with !empty(); we return default-constructed if empty
        if (topIdx == 0) return T();
        return data[--topIdx];
    }
    T& top() { return data[topIdx - 1]; }
    const T& top() const { return data[topIdx - 1]; }
    bool empty() const { return topIdx == 0; }
    int size() const { return topIdx; }

private:
    void grow() {
        int newCap = capacity * 2;
        T* nd = new T[newCap];
        for (int i = 0; i < topIdx; i++) nd[i] = data[i];
        delete[] data;
        data = nd;
        capacity = newCap;
    }
};

#endif
