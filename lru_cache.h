// lru_cache.h - doubly linked list + hash map for O(1) LRU cache.
// key = page id (int), value = the pointer to the in-memory page object.
// eviction hook returns the evicted page id so the pager can write it to disk.
#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "hash_map.h"
#include <string>

struct LRUNode {
    int pageId;
    void* pageData;       // caller-owned pointer (pager manages lifetime)
    LRUNode* prev;
    LRUNode* next;
    LRUNode(int pid, void* pd) : pageId(pid), pageData(pd), prev(nullptr), next(nullptr) {}
};

class LRUCache {
    LRUNode* head;        // most recently used
    LRUNode* tail;        // least recently used (evict from here)
    int capacity;
    int count;

    // id -> LRUNode*  for O(1) lookup. string key since HashMap uses string keys.
    HashMap* index;

    void detach(LRUNode* n);
    void attachFront(LRUNode* n);
    static std::string intKey(int id);

public:
    LRUCache(int cap);
    ~LRUCache();

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    // Put a page into cache. If full, evicts LRU.
    // Returns evicted pageId (-1 if nothing evicted) via out-param.
    // Returns evicted pageData pointer (nullptr if none) via out-param.
    void put(int pageId, void* pageData, int& evictedIdOut, void*& evictedPtrOut);

    // Get a page. Moves to front. Returns nullptr if miss.
    void* get(int pageId);

    // Peek (no reorder). Returns nullptr if absent.
    void* peek(int pageId) const;

    int size() const { return count; }
    int cap() const { return capacity; }

    // testing / introspection
    int evictionsTotal;
};

#endif
