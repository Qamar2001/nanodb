// pager.h - the buffer pool / memory layer.
// Owns a fixed-size raw memory region (array of Page* slots).
// Uses LRUCache to pick eviction victims. Persists pages as binary files on disk.
#ifndef PAGER_H
#define PAGER_H

#include "lru_cache.h"
#include <string>

// a single fixed-capacity page. holds raw bytes (used here as dummy storage
// to demonstrate the memory-layer mechanics required by the rubric).
// real rows live in Table for simplicity - this layer simulates page faults.
struct Page {
    int pageId;
    int size;
    unsigned char* data;     // raw bytes, size bytes
    bool dirty;
    Page(int pid, int sz);
    ~Page();
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
};

class Pager {
    int maxPages;        // buffer pool capacity in pages
    int pageSize;        // bytes per page
    std::string storageDir;
    LRUCache* cache;

    void writePageToDisk(Page* p);
    Page* readPageFromDisk(int pageId);

public:
    Pager(int maxPgs, int pgSize, const std::string& dir);
    ~Pager();

    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    // touch / fetch a page by id. on miss, loads from disk (or creates new).
    // May evict; eviction is logged.
    Page* fetchPage(int pageId);

    // mark the most-recently-touched page dirty (caller writes into page->data)
    void markDirty(int pageId);

    // force all cached pages to disk
    void flushAll();

    // counters for testing
    int evictions() const { return cache->evictionsTotal; }
    int capacity() const { return maxPages; }
};

#endif
