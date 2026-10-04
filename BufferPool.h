#pragma once

#include <list>
#include <unordered_map>
#include "Pager.h"

// BufferPool: a fixed-capacity, write-back LRU cache of raw pages,
// sitting between DiskManager and Pager. DiskManager calls readPage/
// writePage here exactly like it used to call them on Pager directly --
// the cache decides whether that actually needs to touch disk.
//
// Write-back, not write-through: writePage() only updates the cached
// copy and marks it dirty. The write reaches disk when that page is
// evicted to make room for another, or when flushAll() runs (also
// called from the destructor).
class BufferPool {
    private:
        struct CacheEntry {
            char buffer[PAGE_SIZE];
            bool dirty;
        };

        Pager pager;
        size_t capacity;

        std::unordered_map<PageId, CacheEntry> cache;
        std::list<PageId> lruOrder; // front = most recently used, back = least recently used
        std::unordered_map<PageId, std::list<PageId>::iterator> lruPos;

        int cacheHits;
        int cacheMisses;

        void touch(PageId pageId);          // move pageId to the front of lruOrder
        void evictOneIfOverCapacity();        // evict the LRU entry, writing it back if dirty
        char* loadIntoCache(PageId pageId);   // ensures pageId is cached, returns its buffer

    public:
        BufferPool(const std::string& filename, size_t capacity);
        ~BufferPool();

        PageId allocatePage();
        void readPage(PageId pageId, char* outBuffer);
        void writePage(PageId pageId, const char* buffer);

        int getPageCount() const;

        // Writes back every dirty cached page. Called automatically from
        // the destructor; call it explicitly if you need durability
        // guarantees before then.
        void flushAll();

        // Real disk reads since this pool's underlying Pager was opened --
        // a cache hit never reaches this counter.
        int getReadCount() const;

        int getCacheHits() const;
        int getCacheMisses() const;
};
