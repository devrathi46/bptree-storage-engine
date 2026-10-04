// Checkpoint tests for the in-memory B+ tree.
// Dependency-free: plain asserts, compiled and run directly (no pytest/Catch2).
//   g++ -std=c++17 -O2 -I.. ../BPlusTree.cpp ../Node.cpp test_bplustree.cpp -o test_bplustree
//   ./test_bplustree

#include <bits/stdc++.h>
#include "../BPlusTree.h"
using namespace std;

// Walk from the root down the leftmost child at every level to reach the
// leftmost leaf, then follow next-leaf pointers across the whole bottom
// level. This is the one path that proves the leaf chain is real: if a
// split ever forgot to relink next_leaf, or a key landed in the wrong
// leaf, this is where it shows up.
static vector<int> collectLeafChain(BPlusTree& tree){
    Node* node = tree.getRoot();
    while(!node->getIsLeaf()){
        node = node->getChildren()[0];
    }

    vector<int> allKeys;
    while(node != nullptr){
        for(int k : node->getKeys()){
            allKeys.push_back(k);
        }
        node = node->getNextLeaf();
    }
    return allKeys;
}

static void test_insert_and_search_1000_keys(){
    BPlusTree tree(4);

    vector<int> keys(1000);
    for(int i = 0; i < 1000; i++) keys[i] = i;

    // insert out of order so splits happen on both sides of the tree,
    // not just by always appending at the right edge
    mt19937 rng(42);
    shuffle(keys.begin(), keys.end(), rng);

    for(int k : keys){
        tree.insert(k, k * 10);
    }

    for(int k = 0; k < 1000; k++){
        assert(tree.search(k) && "every inserted key must be findable");
    }

    assert(!tree.search(-1) && "key never inserted must not be found");
    assert(!tree.search(1000) && "key never inserted must not be found");

    cout << "[PASS] test_insert_and_search_1000_keys" << endl;
}

static void test_leaf_chain_is_sorted_and_complete(){
    BPlusTree tree(4);

    vector<int> keys(1000);
    for(int i = 0; i < 1000; i++) keys[i] = i;
    mt19937 rng(7);
    shuffle(keys.begin(), keys.end(), rng);
    for(int k : keys) tree.insert(k, k);

    vector<int> chain = collectLeafChain(tree);

    assert(chain.size() == 1000 && "leaf chain must contain every key exactly once");
    for(size_t i = 1; i < chain.size(); i++){
        assert(chain[i] > chain[i-1] && "leaf chain must be strictly increasing");
    }
    assert(chain.front() == 0 && chain.back() == 999);

    cout << "[PASS] test_leaf_chain_is_sorted_and_complete" << endl;
}

static void test_all_leaves_at_same_depth(){
    // B+ tree invariant: every leaf is at the same depth. Walk every root-to-leaf
    // path via BFS and assert the depth never varies.
    BPlusTree tree(4);
    for(int k = 0; k < 500; k++) tree.insert(k, k);

    int leafDepth = -1;
    queue<pair<Node*,int>> q;
    q.push({tree.getRoot(), 0});

    while(!q.empty()){
        auto [node, depth] = q.front(); q.pop();
        if(node->getIsLeaf()){
            if(leafDepth == -1) leafDepth = depth;
            assert(depth == leafDepth && "all leaves must be at the same depth");
        } else {
            for(Node* child : node->getChildren()){
                q.push({child, depth + 1});
            }
        }
    }

    cout << "[PASS] test_all_leaves_at_same_depth" << endl;
}

static void test_duplicate_insert_still_searchable(){
    // not a full duplicate-key policy test, just guarding against a crash/regression
    BPlusTree tree(4);
    for(int k = 0; k < 20; k++) tree.insert(k, k);
    tree.insert(10, 999); // re-insert an existing key

    assert(tree.search(10));
    cout << "[PASS] test_duplicate_insert_still_searchable" << endl;
}

int main(){
    test_insert_and_search_1000_keys();
    test_leaf_chain_is_sorted_and_complete();
    test_all_leaves_at_same_depth();
    test_duplicate_insert_still_searchable();

    cout << "\nAll checkpoint tests passed." << endl;
    return 0;
}
