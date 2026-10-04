#include "../BPlusTree.h"
#include <cassert>
#include <iostream>

using namespace std;

int main() {

    BPlusTree tree(4);

    // Insert 1000 keys
    for(int i = 1; i <= 1000; i++) {
        tree.insert(i, i * 10);
    }

    // Search all 1000 keys
    for(int i = 1; i <= 1000; i++) {

        bool found = tree.search(i);

        assert(found);
    }

    cout << "1000-key insert/search test PASSED" << endl;


    // Walk the leaf chain and assert it is fully sorted and complete.
    // This is the real proof it's a B+ tree and not just a tree that
    // happens to answer search() correctly: the leaves must form one
    // sorted, unbroken sequence via next_leaf.

    Node* node = tree.getRoot();

    while(!node->getIsLeaf()) {

        node = node->getChildren()[0];
    }

    vector<int> leafChain;

    while(node != nullptr) {

        for(int key : node->getKeys()) {

            leafChain.push_back(key);
        }

        node = node->getNextLeaf();
    }

    assert(leafChain.size() == 1000 && "leaf chain must contain every key exactly once");

    for(size_t i = 1; i < leafChain.size(); i++) {

        assert(leafChain[i] > leafChain[i - 1] && "leaf chain must be strictly increasing");
    }

    cout << "Leaf chain sortedness/completeness test PASSED" << endl;

    return 0;
}