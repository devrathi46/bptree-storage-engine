# B+ Tree Storage Engine

A B+ tree index built from scratch in C++, with a disk-backed storage layer underneath it: fixed-size pages, a write-back LRU buffer pool, and persistence that survives a full close and reopen. Benchmarked against a full table scan to show what the index actually buys you.

## Architecture

Two independent pieces, deliberately kept separate:

```
BPlusTree / Node            <- the tree algorithm: search, insert, splits.
  (in-memory only)             Pure Node* pointers, no awareness of disk.

DiskManager                 <- persistence bridge: walks the Node* graph,
  Page (serialize/deserialize)   assigns each node a page id, and mirrors
  BufferPool (LRU cache)         it onto disk. The tree never knows this
  Pager (raw page I/O)           layer exists.

DiskLookup                  <- query operations that run DIRECTLY against
                                disk pages (via DiskManager::readNodeShallow),
                                without ever reconstructing the tree into
                                memory. This is what the benchmark measures.
```

The tree algorithm and the storage engine don't know about each other. `BPlusTree` builds a tree entirely in memory; `DiskManager` is handed that finished tree's root and mirrors it onto disk one page per node. This means the tree's correctness was validated independently (`tests/test_bplustree.cpp`) before any disk concern was introduced.

## Design decisions

**Fixed 4KB pages, variable-size records.** Each page is exactly `PAGE_SIZE = 4096` bytes. `Page::serialize` stores a node's actual key/value/child *counts* in a header rather than reserving fixed-capacity slots per field — simpler to write, but it means nothing bounds a node's size against the page automatically. `Page::serialize` throws if a node would overflow one page, rather than letting it silently corrupt memory via `memcpy`.

**Lazy page allocation.** A `Node*` doesn't get a page id when it's created — `DiskManager::getPageId` assigns one the first time anyone asks, via a `Node* -> PageId` map. This means the tree can be built entirely in RAM with zero disk awareness, and only pays the "assign a page" cost when actually being persisted.

**Page 0 is metadata.** It holds a magic number (so opening a non-database file fails loudly instead of corrupting silently) and the root's page id — the one fact a reopened database needs to find its way back into the tree.

**Write-back LRU buffer pool.** `BufferPool` sits between `DiskManager` and `Pager`. A read either hits the cache or triggers one real disk read; a write only updates the cached copy and marks it dirty — the actual disk write happens on eviction or on close. The cache is intentionally capped well below a large dataset's total page count (64 pages by default), so eviction actually gets exercised rather than just caching everything.

**Two different "read a node" operations, on purpose:**
- `readNode(pageId)` recursively loads an entire subtree into live `Node*` objects — used once, to reload a persisted tree back into memory (`tests/test_disk_manager.cpp`'s close+reopen checkpoint). It's memoized (`pageToNode`) because a single leaf page is reachable two ways while reconstructing the graph — as a child pointer from its parent, and as the `nextLeaf` pointer from its left sibling — and without that cache you'd get two different `Node*` objects for one logical page.
- `readNodeShallow(pageId, ...)` reads **one page**, no recursion, returning raw child/next-leaf page ids instead of resolved pointers. This is what `DiskLookup`'s point lookup and range scan are built on — a real point lookup should touch O(log n) pages, not reconstruct the whole tree just to answer one query.

**Durability is "flush," not "fsync."** Every `Pager::writePage` flushes the C++ stream buffer to the OS immediately — a write is visible to any other reader of the file right after it returns. It does not `fsync`, so a write surviving an OS-level crash before the kernel itself persists it to physical disk is not guaranteed. That's a real, named gap, not an oversight — see Limitations.

## Benchmark

Three dataset sizes, comparing the indexed path (`DiskLookup`'s point lookup / range scan, descending through pages) against a full scan baseline (walk every leaf, no index). Buffer pool capacity: 64 pages for every run.

| Dataset | Operation | Method | Avg time/op | Avg page reads/op |
|---|---|---|---|---|
| 1,000 | point lookup | indexed | 4.18 µs | 0.025 |
| 1,000 | point lookup | full scan | 4.95 µs | 0.025 |
| 10,000 | point lookup | indexed | 4.52 µs | 0.26 |
| 10,000 | point lookup | full scan | 44.26 µs | 0.265 |
| **100,000** | **point lookup** | **indexed** | **7.86 µs** | **0.91** |
| **100,000** | **point lookup** | **full scan** | **786.87 µs** | **266.69** |
| 1,000 | range scan | indexed | 4.94 µs | 0.25 |
| 1,000 | range scan | full scan | 11.67 µs | 0.25 |
| 10,000 | range scan | indexed | 7.09 µs | 1.3 |
| 10,000 | range scan | full scan | 112.26 µs | 2.65 |
| **100,000** | **range scan** | **indexed** | **22.97 µs** | **5.7** |
| **100,000** | **range scan** | **full scan** | **1,760.63 µs** | **504** |

**At 100,000 keys: the index does ~100x fewer microseconds and ~293x fewer real disk reads per point lookup. Range scan: ~77x faster, ~88x fewer page reads.** Full results in `benchmark_results.csv`.

**Why 1,000 and 10,000 barely show a difference in page reads:** both datasets fit entirely inside the 64-page buffer pool, so after the first query everything is cached — the full scan's extra *disk* cost gets hidden by the cache, same as the indexed path's. The wall-clock time still separates them even there (44µs vs 4.5µs at 10,000), because the full scan still does more *work* per query (iterating every leaf's keys) even with zero disk I/O. The page-read gap only becomes dramatic at 100,000 keys, once the dataset's ~340 pages exceed what 64 pages of cache can hide — which is exactly the realistic case a buffer pool is built for: it helps a lot until the working set outgrows it, and an index matters more and more as that gap widens.

## Running it

```bash
# Tests (each is a standalone checkpoint for one layer)
g++ -std=c++17 -O2 -I. BPlusTree.cpp Node.cpp tests/test_bplustree.cpp -o tests/test_bplustree && ./tests/test_bplustree
g++ -std=c++17 -O2 -I. Pager.cpp BufferPool.cpp tests/test_buffer_pool.cpp -o tests/test_buffer_pool && ./tests/test_buffer_pool
g++ -std=c++17 -O2 -I. BPlusTree.cpp Node.cpp Pager.cpp BufferPool.cpp Page.cpp DiskManager.cpp tests/test_disk_manager.cpp -o tests/test_disk_manager && ./tests/test_disk_manager
g++ -std=c++17 -O2 -I. BPlusTree.cpp Node.cpp Pager.cpp BufferPool.cpp Page.cpp DiskManager.cpp DiskLookup.cpp tests/test_disk_lookup.cpp -o tests/test_disk_lookup && ./tests/test_disk_lookup

# Benchmark
g++ -std=c++17 -O2 -I. BPlusTree.cpp Node.cpp Pager.cpp BufferPool.cpp Page.cpp DiskManager.cpp DiskLookup.cpp benchmark.cpp -o benchmark && ./benchmark
```

## Known limitations

- **No concurrency.** Single-threaded only; no latching, no locking.
- **No WAL / crash recovery.** Writes flush to the OS but aren't `fsync`'d, and a crash mid-split could leave pages inconsistent. No write-ahead log.
- **Fixed-width keys.** Keys and values are `int`. Variable-length keys would need a different page layout (overflow pages, or a byte-budget-based split instead of a count-based one).
- **No delete.** The tree supports insert and search; delete (with merging/borrowing between siblings) isn't implemented.
- **Snapshot persistence, not live disk-native writes.** `BPlusTree` builds a tree entirely in memory; `DiskManager::writeTree` persists it afterward in one pass. Inserting into an already-open disk-backed tree one key at a time (writing just the touched pages) isn't implemented.
- **Pages are never reclaimed.** `Pager::allocatePage` only grows the file; there's no free list for reusing space from deleted/stale pages.
