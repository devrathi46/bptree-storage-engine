// Checkpoint test for the write-back LRU buffer pool. Verifies that
// repeated reads of the same page don't touch disk again, that
// eviction writes back a dirty page before dropping it, and that
// closing the pool flushes whatever's left.
//
//   g++ -std=c++17 -O2 -I.. ../Pager.cpp ../BufferPool.cpp test_buffer_pool.cpp -o test_buffer_pool
//   ./test_buffer_pool

#include "../BufferPool.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <cstring>

using namespace std;

static const char* TEST_FILE = "test_buffer_pool.db";

static void fillBuffer(char* buf, const char* text){
    memset(buf, 0, PAGE_SIZE);
    strcpy(buf, text);
}

static void test_repeated_reads_are_cache_hits(){
    remove(TEST_FILE);
    BufferPool pool(TEST_FILE, 10);

    PageId pid = pool.allocatePage();
    char buf[PAGE_SIZE];
    fillBuffer(buf, "hello");
    pool.writePage(pid, buf);

    // writePage on a page not yet cached loads it first (loadIntoCache
    // needs somewhere to memcpy into), so exactly one real disk read is
    // expected here -- a known, harmless inefficiency for brand-new
    // pages, since the content gets fully overwritten anyway. What
    // matters is that NOTHING after this point touches disk again.
    int readsAfterFirstWrite = pool.getReadCount();
    assert(readsAfterFirstWrite == 1);

    for(int i = 0; i < 5; i++){
        char readBuf[PAGE_SIZE];
        pool.readPage(pid, readBuf);
        assert(string(readBuf) == "hello");
    }

    assert(pool.getReadCount() == readsAfterFirstWrite && "repeated reads of a cached page must not touch disk again");
    assert(pool.getCacheHits() >= 5);

    cout << "[PASS] test_repeated_reads_are_cache_hits" << endl;
    remove(TEST_FILE);
}

static void test_eviction_writes_back_dirty_page(){
    remove(TEST_FILE);
    BufferPool pool(TEST_FILE, 2); // tiny pool: forces eviction on the 3rd distinct page

    PageId p0 = pool.allocatePage();
    PageId p1 = pool.allocatePage();
    PageId p2 = pool.allocatePage();

    char buf[PAGE_SIZE];
    fillBuffer(buf, "page0"); pool.writePage(p0, buf);
    fillBuffer(buf, "page1"); pool.writePage(p1, buf);

    // touch p0 again so p1 becomes the LRU victim instead
    char tmp[PAGE_SIZE];
    pool.readPage(p0, tmp);

    fillBuffer(buf, "page2"); pool.writePage(p2, buf); // capacity=2 exceeded -> evicts p1

    // Confirm p1 reached disk by reading it back through a fresh pool
    // over the same file (nothing is left in any in-memory cache).
    {
        BufferPool verify(TEST_FILE, 10);
        char readBuf[PAGE_SIZE];
        verify.readPage(p1, readBuf);
        assert(string(readBuf) == "page1" && "evicted dirty page must be written back before being dropped");
    }

    cout << "[PASS] test_eviction_writes_back_dirty_page" << endl;
    remove(TEST_FILE);
}

static void test_destructor_flushes_remaining_dirty_pages(){
    remove(TEST_FILE);
    PageId pid;
    {
        BufferPool pool(TEST_FILE, 10);
        pid = pool.allocatePage();
        char buf[PAGE_SIZE];
        fillBuffer(buf, "durable");
        pool.writePage(pid, buf);
    } // pool destructed here -> flushAll() must run

    BufferPool reopened(TEST_FILE, 10);
    char readBuf[PAGE_SIZE];
    reopened.readPage(pid, readBuf);
    assert(string(readBuf) == "durable" && "dirty pages must survive pool destruction");

    cout << "[PASS] test_destructor_flushes_remaining_dirty_pages" << endl;
    remove(TEST_FILE);
}

int main(){
    test_repeated_reads_are_cache_hits();
    test_eviction_writes_back_dirty_page();
    test_destructor_flushes_remaining_dirty_pages();

    cout << "\nAll buffer pool checkpoint tests passed." << endl;
    return 0;
}
