// hash_map.cpp
#include "hash_map.h"

HashMap::HashMap(int initialBuckets) {
    numBuckets = (initialBuckets > 0) ? initialBuckets : 16;
    buckets = new HashNode*[numBuckets];
    for (int i = 0; i < numBuckets; i++) buckets[i] = nullptr;
    count = 0;
}

HashMap::~HashMap() {
    for (int i = 0; i < numBuckets; i++) {
        HashNode* cur = buckets[i];
        while (cur) {
            HashNode* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }
    delete[] buckets;
}

// classic djb2 hash. good distribution for strings.
unsigned int HashMap::hash(const std::string& s) const {
    unsigned int h = 5381;
    for (size_t i = 0; i < s.size(); i++) {
        h = ((h << 5) + h) + (unsigned char)s[i];   // h*33 + c
    }
    return h;
}

void HashMap::put(const std::string& key, void* value) {
    // check load factor - if >0.75, double and rehash to keep O(1)
    if ((double)count / numBuckets > 0.75) rehash(numBuckets * 2);

    unsigned int idx = hash(key) % numBuckets;
    HashNode* cur = buckets[idx];
    while (cur) {
        if (cur->key == key) { cur->value = value; return; }
        cur = cur->next;
    }
    // new entry at head of bucket
    HashNode* n = new HashNode(key, value);
    n->next = buckets[idx];
    buckets[idx] = n;
    count++;
}

void* HashMap::get(const std::string& key) const {
    unsigned int idx = hash(key) % numBuckets;
    HashNode* cur = buckets[idx];
    while (cur) {
        if (cur->key == key) return cur->value;
        cur = cur->next;
    }
    return nullptr;
}

bool HashMap::contains(const std::string& key) const {
    return get(key) != nullptr;
}

bool HashMap::remove(const std::string& key) {
    unsigned int idx = hash(key) % numBuckets;
    HashNode* cur = buckets[idx];
    HashNode* prev = nullptr;
    while (cur) {
        if (cur->key == key) {
            if (prev) prev->next = cur->next;
            else buckets[idx] = cur->next;
            delete cur;
            count--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

void HashMap::rehash(int newBuckets) {
    HashNode** nb = new HashNode*[newBuckets];
    for (int i = 0; i < newBuckets; i++) nb[i] = nullptr;
    for (int i = 0; i < numBuckets; i++) {
        HashNode* cur = buckets[i];
        while (cur) {
            HashNode* nxt = cur->next;
            unsigned int idx = hash(cur->key) % newBuckets;
            cur->next = nb[idx];
            nb[idx] = cur;
            cur = nxt;
        }
    }
    delete[] buckets;
    buckets = nb;
    numBuckets = newBuckets;
}

HashMap::Iter HashMap::begin() const {
    Iter it; it.bucketIdx = 0; it.node = nullptr;
    for (int i = 0; i < numBuckets; i++) {
        if (buckets[i]) { it.bucketIdx = i; it.node = buckets[i]; return it; }
    }
    return it;
}

void HashMap::advance(Iter& it) const {
    if (!it.node) return;
    if (it.node->next) { it.node = it.node->next; return; }
    for (int i = it.bucketIdx + 1; i < numBuckets; i++) {
        if (buckets[i]) { it.bucketIdx = i; it.node = buckets[i]; return; }
    }
    it.node = nullptr;
}
