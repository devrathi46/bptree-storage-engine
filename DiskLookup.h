#pragma once

#include <vector>
#include "DiskManager.h"

// Query operations through the disk-backed tree, all built on
// DiskManager::readNodeShallow so each one only touches the pages its
// own access pattern actually needs -- that's the thing the benchmark
// in benchmark.cpp is measuring.

// Indexed point lookup: descends from the root, reading one page at a
// time, comparing `key` against that page's keys to pick the next
// child -- the same decision BPlusTree::search() makes against an
// in-memory Node*, just reading a page instead of following a pointer.
// Touches O(log n) pages.
bool diskPointLookup(DiskManager& disk, int key);

// Indexed range scan: find the leaf that would contain `low` (same
// descent as above), then walk the leaf chain via nextLeafPageId,
// collecting values for keys in [low, high], stopping once a key
// exceeds high. Touches O(log n) + (number of matching leaves) pages.
std::vector<int> diskRangeScan(DiskManager& disk, int low, int high);

// Full scan baseline: walks the ENTIRE leaf chain from the very first
// leaf, collecting every (key, value) pair with no comparisons to skip
// anything. This is deliberately NOT index-aware -- it's the thing the
// index above is supposed to beat. Touches every leaf page.
std::vector<std::pair<int, int>> diskFullScan(DiskManager& disk);
