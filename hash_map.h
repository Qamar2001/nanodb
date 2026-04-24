// hash_map.h - custom hash table for the system catalog.
// string -> void* (generic). collision resolution by chaining with singly-linked list.
// O(1) average lookup; O(N) worst case if everything collides.
#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <string>

struct HashNode {
    std::string key;
    void* value;
    HashNode* next;
    HashNode(const std::string& k, void* v) : key(k), value(v), next(nullptr) {}
};

class HashMap {
    HashNode** buckets;   // array of bucket heads
    int numBuckets;
    int count;

    unsigned int hash(const std::string& s) const;
    void rehash(int newBuckets);

public:
    HashMap(int initialBuckets = 16);
    ~HashMap();

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    void put(const std::string& key, void* value);  // replaces if exists
    void* get(const std::string& key) const;        // returns nullptr if missing
    bool contains(const std::string& key) const;
    bool remove(const std::string& key);
    int size() const { return count; }

    // iterate support - give caller a way to visit all keys
    // we expose a simple "next" walk via indices.
    struct Iter {
        int bucketIdx;
        HashNode* node;
    };
    Iter begin() const;
    bool valid(const Iter& it) const { return it.node != nullptr; }
    void advance(Iter& it) const;
    const std::string& keyAt(const Iter& it) const { return it.node->key; }
    void* valueAt(const Iter& it) const { return it.node->value; }
};

#endif
