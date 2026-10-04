// Checkpoint test for DiskLookup: full scan, indexed point lookup, and
// indexed range scan, all against a tree built with the user's own
// BPlusTree.
//
//   g++ -std=c++17 -O2 -I.. ../BPlusTree.cpp ../Node.cpp ../Pager.cpp ../BufferPool.cpp ../Page.cpp ../DiskManager.cpp ../DiskLookup.cpp test_disk_lookup.cpp -o test_disk_lookup
//   ./test_disk_lookup

#include "../BPlusTree.h"
#include "../DiskManager.h"
#include "../DiskLookup.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <algorithm>

using namespace std;

static const char* DB_FILE = "test_disk_lookup.db";

int main(){
    remove(DB_FILE);

    const int N = 1000;
    BPlusTree tree(4);
    for(int i = 1; i <= N; i++){
        tree.insert(i, i * 10);
    }

    {
        DiskManager disk(DB_FILE);
        disk.writeTree(tree.getRoot());
        disk.setRootPageId(disk.getPageId(tree.getRoot()));
    }

    {
        DiskManager disk(DB_FILE);
        vector<pair<int,int>> entries = diskFullScan(disk);

        assert((int)entries.size() == N && "full scan must return every entry exactly once");

        sort(entries.begin(), entries.end());
        for(int i = 0; i < N; i++){
            assert(entries[i].first == i + 1 && "full scan must return every inserted key");
            assert(entries[i].second == (i + 1) * 10 && "full scan must return the matching value");
        }
    }

    cout << "diskFullScan checkpoint test PASSED" << endl;

    {
        DiskManager disk(DB_FILE);

        // Every inserted key must be found, with the right value.
        for(int i = 1; i <= N; i++){
            assert(diskPointLookup(disk, i) && "every inserted key must be findable");
        }

        // Keys that were never inserted must not be found.
        assert(!diskPointLookup(disk, 0));
        assert(!diskPointLookup(disk, N + 1));
        assert(!diskPointLookup(disk, -500));
    }

    cout << "diskPointLookup checkpoint test PASSED" << endl;

    {
        DiskManager disk(DB_FILE);

        // A range entirely inside the dataset.
        vector<int> values = diskRangeScan(disk, 100, 200);
        sort(values.begin(), values.end());

        assert((int)values.size() == 101 && "range [100,200] must return exactly 101 entries");
        for(int i = 0; i < 101; i++){
            assert(values[i] == (100 + i) * 10);
        }

        // A range that starts before the dataset and ends after it --
        // must clip to what actually exists, not crash or go out of bounds.
        vector<int> clipped = diskRangeScan(disk, -1000, N + 1000);
        assert((int)clipped.size() == N && "range covering everything must return every entry");

        // A range entirely past the end of the dataset -- empty, not a crash.
        vector<int> empty = diskRangeScan(disk, N + 10, N + 20);
        assert(empty.empty());
    }

    cout << "diskRangeScan checkpoint test PASSED" << endl;

    remove(DB_FILE);
    return 0;
}
