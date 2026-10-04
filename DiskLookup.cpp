#include "DiskLookup.h"

using namespace std;

// ---------------------------------------------------------------------
// Full scan baseline: pure structural traversal, no key comparisons.
// Implemented directly -- there's no decision-making here to port from
// BPlusTree, just "find the first leaf, then follow the chain."
// ---------------------------------------------------------------------
vector<pair<int, int>> diskFullScan(DiskManager& disk){
    vector<pair<int, int>> results;

    vector<int> childPageIds;
    int nextLeafPageId;

    // Descend the left edge once to find the very first leaf -- this
    // touches a handful of internal pages, not the whole tree.
    PageId pageId = disk.getRootPageId();
    Node* node = disk.readNodeShallow(pageId, childPageIds, nextLeafPageId);

    while(!node->getIsLeaf()){
        PageId nextPageId = childPageIds[0];
        delete node;
        node = disk.readNodeShallow(nextPageId, childPageIds, nextLeafPageId);
    }

    // Walk every leaf via nextLeafPageId, collecting everything. This
    // is the part that touches one page per leaf -- O(n/order) pages,
    // deliberately with no key comparisons to skip any of them.
    while(true){
        vector<int>& keys = node->getKeys();
        vector<int>& values = node->getValues();
        for(size_t i = 0; i < keys.size(); i++){
            results.push_back({keys[i], values[i]});
        }

        int next = nextLeafPageId;
        delete node;

        if(next == -1) break;
        node = disk.readNodeShallow(next, childPageIds, nextLeafPageId);
    }

    return results;
}

// ---------------------------------------------------------------------
// Shared descent: find the page id of the leaf that would contain `key`,
// if it existed. Port of BPlusTree::search's descent loop (BPlusTree.cpp
// lines 18-27) -- same childPos comparison, reading a page via
// readNodeShallow instead of following a Node* child pointer. Both
// diskPointLookup and diskRangeScan need exactly this, so it's factored
// out instead of duplicated.
// ---------------------------------------------------------------------
static PageId findLeafPageId(DiskManager& disk, int key){

    PageId pageId = disk.getRootPageId();

    while(true){
        vector<int> childPageIds;
        int nextLeafPageId;
        Node* node = disk.readNodeShallow(pageId, childPageIds, nextLeafPageId);

        if(node->getIsLeaf()){
            delete node; // throwaway -- caller re-reads this same page next
            return pageId;
        }

        vector<int>& nodeKeys = node->getKeys();
        int childPos = 0;
        while(childPos < (int)nodeKeys.size() && key >= nodeKeys[childPos]){
            childPos++;
        }

        pageId = childPageIds[childPos];
        delete node;
    }
}

bool diskPointLookup(DiskManager& disk, int key){

    PageId leafPageId = findLeafPageId(disk, key);

    vector<int> childPageIds;
    int nextLeafPageId;
    Node* node = disk.readNodeShallow(leafPageId, childPageIds, nextLeafPageId);

    // Port of BPlusTree::search's leaf scan (BPlusTree.cpp lines 29-33).
    bool found = false;
    for(int currentKey : node->getKeys()){
        if(currentKey == key){
            found = true;
            break;
        }
    }

    delete node;
    return found;
}

vector<int> diskRangeScan(DiskManager& disk, int low, int high){

    vector<int> results;
    PageId pageId = findLeafPageId(disk, low);

    while(true){
        vector<int> childPageIds;
        int nextLeafPageId;
        Node* node = disk.readNodeShallow(pageId, childPageIds, nextLeafPageId);

        vector<int>& keys = node->getKeys();
        vector<int>& values = node->getValues();

        bool exceededHigh = false;
        for(size_t i = 0; i < keys.size(); i++){
            if(keys[i] > high){
                // Leaves are sorted, so nothing from here on (in this
                // leaf or any leaf after it) can be in range either.
                exceededHigh = true;
                break;
            }
            if(keys[i] >= low){
                results.push_back(values[i]);
            }
        }

        int next = nextLeafPageId;
        delete node;

        if(exceededHigh || next == -1){
            break;
        }
        pageId = next;
    }

    return results;
}
