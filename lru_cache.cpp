// lru_cache.cpp
#include "lru_cache.h"

LRUCache::LRUCache(int cap)
    : head(nullptr), tail(nullptr), capacity(cap > 0 ? cap : 1), count(0),
      evictionsTotal(0) {
    index = new HashMap(capacity * 2);
}

LRUCache::~LRUCache() {
    LRUNode* cur = head;
    while (cur) {
        LRUNode* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    delete index;
}

std::string LRUCache::intKey(int id) {
    return std::to_string(id);
}

void LRUCache::detach(LRUNode* n) {
    if (n->prev) n->prev->next = n->next;
    else head = n->next;
    if (n->next) n->next->prev = n->prev;
    else tail = n->prev;
    n->prev = n->next = nullptr;
}

void LRUCache::attachFront(LRUNode* n) {
    n->prev = nullptr;
    n->next = head;
    if (head) head->prev = n;
    head = n;
    if (!tail) tail = n;
}

void LRUCache::put(int pageId, void* pageData, int& evictedIdOut, void*& evictedPtrOut) {
    evictedIdOut = -1;
    evictedPtrOut = nullptr;

    std::string k = intKey(pageId);
    LRUNode* existing = (LRUNode*)index->get(k);
    if (existing) {
        // update and move to front
        existing->pageData = pageData;
        detach(existing);
        attachFront(existing);
        return;
    }

    if (count == capacity) {
        // evict LRU (tail)
        if (tail) {
            LRUNode* victim = tail;
            evictedIdOut = victim->pageId;
            evictedPtrOut = victim->pageData;
            detach(victim);
            index->remove(intKey(victim->pageId));
            delete victim;
            count--;
            evictionsTotal++;
        }
    }

    LRUNode* n = new LRUNode(pageId, pageData);
    attachFront(n);
    index->put(k, (void*)n);
    count++;
}

void* LRUCache::get(int pageId) {
    LRUNode* n = (LRUNode*)index->get(intKey(pageId));
    if (!n) return nullptr;
    detach(n);
    attachFront(n);
    return n->pageData;
}

void* LRUCache::peek(int pageId) const {
    LRUNode* n = (LRUNode*)index->get(intKey(pageId));
    return n ? n->pageData : nullptr;
}
