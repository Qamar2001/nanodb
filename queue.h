// queue.h - custom FIFO queue (circular array) + priority queue (binary heap).
// all built from raw arrays. no STL.
#ifndef QUEUE_H
#define QUEUE_H

#include <cstddef>

// ------------- FIFO Queue (circular buffer) -------------
template <typename T>
class Queue {
    T* data;
    int capacity;
    int head;     // index of front
    int tail;     // index where next push goes
    int count;
public:
    Queue(int initialCap = 16) {
        capacity = (initialCap > 0) ? initialCap : 16;
        data = new T[capacity];
        head = tail = count = 0;
    }
    ~Queue() { delete[] data; }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const T& v) {
        if (count == capacity) grow();
        data[tail] = v;
        tail = (tail + 1) % capacity;
        count++;
    }
    T dequeue() {
        if (count == 0) return T();
        T v = data[head];
        head = (head + 1) % capacity;
        count--;
        return v;
    }
    T& front() { return data[head]; }
    bool empty() const { return count == 0; }
    int size() const { return count; }

private:
    void grow() {
        int nc = capacity * 2;
        T* nd = new T[nc];
        // copy in order: head -> tail wrapping
        for (int i = 0; i < count; i++) nd[i] = data[(head + i) % capacity];
        delete[] data;
        data = nd;
        capacity = nc;
        head = 0;
        tail = count;
    }
};

// ------------- Priority Queue (max-heap on int priority) -------------
// higher priority number = dequeued first. admin queries get e.g. priority=10,
// normal queries get priority=1.
template <typename T>
class PriorityQueue {
    struct Node { int priority; T value; };
    Node* heap;
    int capacity;
    int count;
public:
    PriorityQueue(int initialCap = 16) {
        capacity = (initialCap > 0) ? initialCap : 16;
        heap = new Node[capacity];
        count = 0;
    }
    ~PriorityQueue() { delete[] heap; }

    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    void push(int priority, const T& v) {
        if (count == capacity) grow();
        heap[count].priority = priority;
        heap[count].value = v;
        siftUp(count);
        count++;
    }
    T pop() {
        if (count == 0) return T();
        T res = heap[0].value;
        count--;
        if (count > 0) {
            heap[0] = heap[count];
            siftDown(0);
        }
        return res;
    }
    bool empty() const { return count == 0; }
    int size() const { return count; }

private:
    void swap(int i, int j) {
        Node tmp = heap[i]; heap[i] = heap[j]; heap[j] = tmp;
    }
    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (heap[i].priority > heap[p].priority) { swap(i, p); i = p; }
            else break;
        }
    }
    void siftDown(int i) {
        while (true) {
            int l = 2*i + 1, r = 2*i + 2, best = i;
            if (l < count && heap[l].priority > heap[best].priority) best = l;
            if (r < count && heap[r].priority > heap[best].priority) best = r;
            if (best == i) break;
            swap(i, best); i = best;
        }
    }
    void grow() {
        int nc = capacity * 2;
        Node* nd = new Node[nc];
        for (int i = 0; i < count; i++) nd[i] = heap[i];
        delete[] heap;
        heap = nd;
        capacity = nc;
    }
};

#endif
