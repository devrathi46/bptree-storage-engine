// Benchmark harness: for three dataset sizes, measures point lookup and
// range scan both "indexed" (through the disk-backed B+ tree via
// DiskLookup) and as a full scan (no index, walk every leaf), recording
// wall-clock time and real disk page reads for each. Results go to
// benchmark_results.csv.
//
// NOTE: diskPointLookup/diskRangeScan in DiskLookup.cpp are stubs until
// their TODOs are filled in -- until then, "indexed" numbers here are
// not meaningful (a stub returning immediately will look impossibly
// fast). The harness itself is complete and correct; it's measuring
// what you point it at.
//
//   g++ -std=c++17 -O2 -I. BPlusTree.cpp Node.cpp Pager.cpp BufferPool.cpp Page.cpp DiskManager.cpp DiskLookup.cpp benchmark.cpp -o benchmark
//   ./benchmark

#include "BPlusTree.h"
#include "DiskManager.h"
#include "DiskLookup.h"
#include <bits/stdc++.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

static const size_t BUFFER_POOL_CAPACITY = 64; // deliberately smaller than any dataset's page count
static const int TREE_ORDER = 300;             // well within Page::serialize's PAGE_SIZE budget (see Page.cpp)
static const int NUM_POINT_QUERIES = 200;
static const int NUM_RANGE_QUERIES = 20;
static const int RANGE_SPAN_FRACTION = 100;    // each range covers ~1/100th of the dataset

struct BenchResult {
    int datasetSize;
    string operation; // "point_lookup" or "range_scan"
    string method;    // "indexed" or "full_scan"
    int numQueries;
    double totalWallClockMs;
    long long totalPageReads;
};

// Mechanical full-scan point lookup: walk every leaf via readNodeShallow
// checking keys directly, no routing decisions. This is the "no index"
// baseline a point lookup would cost without a B+ tree at all.
static bool fullScanPointLookup(DiskManager& disk, int key){
    vector<int> childPageIds;
    int nextLeafPageId;

    PageId pageId = disk.getRootPageId();
    Node* node = disk.readNodeShallow(pageId, childPageIds, nextLeafPageId);
    while(!node->getIsLeaf()){
        PageId nextPageId = childPageIds[0];
        delete node;
        node = disk.readNodeShallow(nextPageId, childPageIds, nextLeafPageId);
    }

    while(true){
        for(int k : node->getKeys()){
            if(k == key){
                delete node;
                return true;
            }
        }
        int next = nextLeafPageId;
        delete node;
        if(next == -1) return false;
        node = disk.readNodeShallow(next, childPageIds, nextLeafPageId);
    }
}

// Builds a tree of N keys (1..N) with the user's own BPlusTree, persists
// it to dbFile via DiskManager, and returns the file ready to be opened
// fresh for each measurement block below.
static void buildDataset(const string& dbFile, int n){
    remove(dbFile.c_str());

    BPlusTree tree(TREE_ORDER);
    vector<int> keys(n);
    for(int i = 0; i < n; i++) keys[i] = i + 1;
    mt19937 rng(42);
    shuffle(keys.begin(), keys.end(), rng);
    for(int k : keys) tree.insert(k, k * 10);

    DiskManager disk(dbFile, BUFFER_POOL_CAPACITY);
    disk.writeTree(tree.getRoot());
    disk.setRootPageId(disk.getPageId(tree.getRoot()));
}

static vector<int> randomKeysInRange(int n, int count, mt19937& rng){
    uniform_int_distribution<int> dist(1, n);
    vector<int> result(count);
    for(int i = 0; i < count; i++) result[i] = dist(rng);
    return result;
}

int main(){
    vector<int> datasetSizes = {1000, 10000, 100000}; // the "three datasets"
    vector<BenchResult> results;
    mt19937 rng(123);

    for(int n : datasetSizes){
        string dbFile = "bench_" + to_string(n) + ".db";
        cout << "=== Building dataset: " << n << " keys ===" << endl;
        buildDataset(dbFile, n);

        vector<int> queryKeys = randomKeysInRange(n, NUM_POINT_QUERIES, rng);

        // --- Point lookup: indexed ---
        {
            DiskManager disk(dbFile, BUFFER_POOL_CAPACITY);
            long long readsBefore = disk.getReadCount();
            auto t0 = high_resolution_clock::now();
            for(int k : queryKeys) diskPointLookup(disk, k);
            auto t1 = high_resolution_clock::now();
            long long readsAfter = disk.getReadCount();

            results.push_back({
                n, "point_lookup", "indexed", NUM_POINT_QUERIES,
                duration<double, milli>(t1 - t0).count(),
                readsAfter - readsBefore
            });
        }

        // --- Point lookup: full scan ---
        {
            DiskManager disk(dbFile, BUFFER_POOL_CAPACITY);
            long long readsBefore = disk.getReadCount();
            auto t0 = high_resolution_clock::now();
            for(int k : queryKeys) fullScanPointLookup(disk, k);
            auto t1 = high_resolution_clock::now();
            long long readsAfter = disk.getReadCount();

            results.push_back({
                n, "point_lookup", "full_scan", NUM_POINT_QUERIES,
                duration<double, milli>(t1 - t0).count(),
                readsAfter - readsBefore
            });
        }

        // --- Range scan: indexed vs full scan ---
        int span = max(1, n / RANGE_SPAN_FRACTION);
        {
            DiskManager disk(dbFile, BUFFER_POOL_CAPACITY);
            long long readsBefore = disk.getReadCount();
            auto t0 = high_resolution_clock::now();
            for(int i = 0; i < NUM_RANGE_QUERIES; i++){
                int low = uniform_int_distribution<int>(1, max(1, n - span))(rng);
                diskRangeScan(disk, low, low + span);
            }
            auto t1 = high_resolution_clock::now();
            long long readsAfter = disk.getReadCount();

            results.push_back({
                n, "range_scan", "indexed", NUM_RANGE_QUERIES,
                duration<double, milli>(t1 - t0).count(),
                readsAfter - readsBefore
            });
        }
        {
            DiskManager disk(dbFile, BUFFER_POOL_CAPACITY);
            long long readsBefore = disk.getReadCount();
            auto t0 = high_resolution_clock::now();
            for(int i = 0; i < NUM_RANGE_QUERIES; i++){
                int low = uniform_int_distribution<int>(1, max(1, n - span))(rng);
                int high = low + span;
                auto all = diskFullScan(disk);
                vector<pair<int,int>> inRange;
                for(auto& kv : all){
                    if(kv.first >= low && kv.first <= high) inRange.push_back(kv);
                }
            }
            auto t1 = high_resolution_clock::now();
            long long readsAfter = disk.getReadCount();

            results.push_back({
                n, "range_scan", "full_scan", NUM_RANGE_QUERIES,
                duration<double, milli>(t1 - t0).count(),
                readsAfter - readsBefore
            });
        }

        remove(dbFile.c_str());
    }

    ofstream csv("benchmark_results.csv");
    csv << "dataset_size,operation,method,num_queries,total_wall_clock_ms,avg_wall_clock_us,total_page_reads,avg_page_reads\n";
    for(auto& r : results){
        double avgUs = (r.totalWallClockMs * 1000.0) / r.numQueries;
        double avgReads = (double)r.totalPageReads / r.numQueries;
        csv << r.datasetSize << "," << r.operation << "," << r.method << ","
            << r.numQueries << "," << r.totalWallClockMs << "," << avgUs << ","
            << r.totalPageReads << "," << avgReads << "\n";

        cout << r.datasetSize << " | " << r.operation << " | " << r.method
             << " | " << r.totalWallClockMs << "ms total, " << avgUs << "us/op"
             << " | " << r.totalPageReads << " page reads total, " << avgReads << "/op"
             << endl;
    }

    cout << "\nWrote benchmark_results.csv" << endl;
    return 0;
}
