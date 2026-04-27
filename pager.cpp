// pager.cpp
#include "pager.h"
#include "logger.h"
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#endif

Page::Page(int pid, int sz) : pageId(pid), size(sz), dirty(false) {
    data = new unsigned char[sz];
    std::memset(data, 0, sz);
}
Page::~Page() { delete[] data; }

static void ensureDir(const std::string& d) {
#ifdef _WIN32
    _mkdir(d.c_str());
#else
    mkdir(d.c_str(), 0755);
#endif
}

Pager::Pager(int maxPgs, int pgSize, const std::string& dir)
    : maxPages(maxPgs), pageSize(pgSize), storageDir(dir) {
    ensureDir(storageDir);
    cache = new LRUCache(maxPages);
}

Pager::~Pager() {
    flushAll();
    LRUNode* cur = cache->firstNode();
    while (cur) {
        Page* p = (Page*)cur->pageData;
        delete p;
        cur = cur->next;
    }
    delete cache;
}

static std::string pagePath(const std::string& dir, int pid) {
    return dir + "/page_" + std::to_string(pid) + ".bin";
}

void Pager::writePageToDisk(Page* p) {
    std::ofstream out(pagePath(storageDir, p->pageId), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;
    out.write((char*)&p->pageId, sizeof(int));
    out.write((char*)&p->size, sizeof(int));
    out.write((char*)p->data, p->size);
    out.close();
    p->dirty = false;
}

Page* Pager::readPageFromDisk(int pageId) {
    std::ifstream in(pagePath(storageDir, pageId), std::ios::binary);
    if (!in.is_open()) {
        // cold page - allocate blank
        return new Page(pageId, pageSize);
    }
    int pid = 0, sz = 0;
    in.read((char*)&pid, sizeof(int));
    in.read((char*)&sz, sizeof(int));
    if (!in || sz <= 0 || sz > (1 << 24)) {
        in.close();
        return new Page(pageId, pageSize);
    }
    Page* p = new Page(pid, sz);
    in.read((char*)p->data, sz);
    in.close();
    return p;
}

Page* Pager::fetchPage(int pageId) {
    // check cache
    Page* p = (Page*)cache->get(pageId);
    if (p) return p;

    // miss - load from disk
    p = readPageFromDisk(pageId);

    int evictedId = -1;
    void* evictedPtr = nullptr;
    cache->put(pageId, p, evictedId, evictedPtr);
    if (evictedId != -1 && evictedPtr) {
        Page* victim = (Page*)evictedPtr;
        if (victim->dirty) {
            writePageToDisk(victim);
            if (g_logger) g_logger->log("Page " + std::to_string(victim->pageId)
                + " evicted via LRU, written to disk");
        } else {
            if (g_logger) g_logger->log("Page " + std::to_string(victim->pageId)
                + " evicted via LRU (clean)");
        }
        delete victim;
    }
    return p;
}

void Pager::markDirty(int pageId) {
    Page* p = (Page*)cache->peek(pageId);
    if (p) p->dirty = true;
}

void Pager::flushAll() {
    LRUNode* cur = cache->firstNode();
    while (cur) {
        Page* p = (Page*)cur->pageData;
        if (p && p->dirty) {
            writePageToDisk(p);
            if (g_logger) {
                g_logger->log("Page " + std::to_string(p->pageId) +
                              " flushed from buffer pool to disk");
            }
        }
        cur = cur->next;
    }
}
