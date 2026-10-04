#pragma once

#include "Node.h"

class BPlusTree{
    private:
        Node* root;
        int order;
    
    public:
        BPlusTree(int order);
        Node* getRoot();
        bool search(int key);
        void insert(int key,int value);
        void insertIntoParent(Node* left,Node* right,int separator,int pathIndex,vector<Node*>& path);
};
