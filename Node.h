#pragma once
#include<vector>
using namespace std;

class Node{
    private:
        bool isLeaf;
        vector<int>keys;
        vector<int>values;
        vector<Node*> children;
        Node* nextLeaf;

    public:
        //constructor
        Node(bool isLeaf);

        //Methods

        bool getIsLeaf() const;
        vector<int>& getKeys();
        vector<int>& getValues();
        vector<Node*>& getChildren();

        Node* getNextLeaf();
        void setNextLeaf(Node* next);

        void addKey(int key);

      
        

};