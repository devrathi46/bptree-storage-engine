// Round-trip test for the persistence layer (Pager + Page + DiskManager).
// The tree itself is built entirely with the user's own BPlusTree/Node
// code -- nothing here reimplements insert, search, or splitting.
//
//   g++ -std=c++17 -O2 -I.. ../BPlusTree.cpp ../Node.cpp ../Pager.cpp ../Page.cpp ../DiskManager.cpp test_disk_manager.cpp -o test_disk_manager
//   ./test_disk_manager

#include "../BPlusTree.h"
#include "../DiskManager.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <vector>

using namespace std;

static const char* DB_FILE = "test_disk_manager.db";

// Test-only helper: walks a bare Node* the same way BPlusTree::search()
// does. Needed because the tree reloaded from disk is a Node*, not a
// BPlusTree instance (BPlusTree's constructor only ever builds its own
// fresh root -- it has no way to adopt an externally loaded one).
static bool searchInTree(Node* root, int key){
    Node* node = root;
    while(!node->getIsLeaf()){
        vector<int>& keys = node->getKeys();
        vector<Node*>& children = node->getChildren();
        int childPos = 0;
        while(childPos < (int)keys.size() && key >= keys[childPos]) childPos++;
        node = children[childPos];
    }
    for(int k : node->getKeys()){
        if(k == key) return true;
    }
    return false;
}

int main(){
    remove(DB_FILE);

    // 1. Build a tree with the user's own BPlusTree.
    BPlusTree tree(4);
    for(int i = 1; i <= 1000; i++){
        tree.insert(i, i * 10);
    }

    // 2. Persist the whole tree to disk, then close the file completely.
    {
        DiskManager disk(DB_FILE);
        disk.writeTree(tree.getRoot());
        disk.setRootPageId(disk.getPageId(tree.getRoot()));
    } // disk (and its Pager) destructed here -> file closed

    // 3. Reopen with a brand-new DiskManager and reload purely from disk.
    {
        DiskManager disk(DB_FILE);
        Node* loadedRoot = disk.readNode(disk.getRootPageId());

        for(int i = 1; i <= 1000; i++){
            assert(searchInTree(loadedRoot, i) && "key must survive the round trip");
        }
        assert(!searchInTree(loadedRoot, 0));
        assert(!searchInTree(loadedRoot, 1001));
    }

    cout << "Disk persistence round-trip test PASSED" << endl;
    remove(DB_FILE);
    return 0;
}
