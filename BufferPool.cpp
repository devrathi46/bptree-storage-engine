#include "BufferPool.h"
#include <cstring>

BufferPool::BufferPool(const std::string& filename, size_t capacity)
    : pager(filename), capacity(capacity), cacheHits(0), cacheMisses(0) {}

BufferPool::~BufferPool(){
    flushAll();
}

PageId BufferPool::allocatePage(){
    return pager.allocatePage();
}

void BufferPool::touch(PageId pageId){
    auto it = lruPos.find(pageId);
    if(it != lruPos.end()){
        lruOrder.erase(it->second);
    }
    lruOrder.push_front(pageId);
    lruPos[pageId] = lruOrder.begin();
}

void BufferPool::evictOneIfOverCapacity(){
    if(cache.size() <= capacity) return;

    PageId victim = lruOrder.back();
    lruOrder.pop_back();
    lruPos.erase(victim);

    auto it = cache.find(victim);
    if(it->second.dirty){
        pager.writePage(victim, it->second.buffer); // the deferred write-back actually happens here
    }
    cache.erase(it);
}

char* BufferPool::loadIntoCache(PageId pageId){
    auto it = cache.find(pageId);
    if(it != cache.end()){
        cacheHits++;
        touch(pageId);
        return it->second.buffer;
    }

    cacheMisses++;
    CacheEntry entry;
    entry.dirty = false;
    pager.readPage(pageId, entry.buffer); // the real disk read this whole layer exists to avoid repeating

    auto inserted = cache.emplace(pageId, entry).first;
    touch(pageId);
    evictOneIfOverCapacity();
    return inserted->second.buffer;
}

void BufferPool::readPage(PageId pageId, char* outBuffer){
    char* cached = loadIntoCache(pageId);
    std::memcpy(outBuffer, cached, PAGE_SIZE);
}

void BufferPool::writePage(PageId pageId, const char* buffer){
    char* cached = loadIntoCache(pageId); // bring it in first so eviction order is correct
    std::memcpy(cached, buffer, PAGE_SIZE);
    cache[pageId].dirty = true;
}

void BufferPool::flushAll(){
    for(auto& [pageId, entry] : cache){
        if(entry.dirty){
            pager.writePage(pageId, entry.buffer);
            entry.dirty = false;
        }
    }
}

int BufferPool::getPageCount() const{
    return pager.getPageCount();
}

int BufferPool::getReadCount() const{
    return pager.getReadCount();
}

int BufferPool::getCacheHits() const{
    return cacheHits;
}

int BufferPool::getCacheMisses() const{
    return cacheMisses;
}
