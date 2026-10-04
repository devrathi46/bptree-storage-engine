#include<bits/stdc++.h>
#include "Node.h"
#include<vector>

using namespace std;

Node ::Node(bool isLeaf){
    this->isLeaf=isLeaf;
    this->nextLeaf=nullptr;

}
bool Node::getIsLeaf() const{
    return isLeaf;
}

vector<int>& Node::getKeys(){
    return keys;
}
vector<int>& Node::getValues(){
    return values;
}

void Node::addKey(int key){
    keys.push_back(key);
}

Node* Node::getNextLeaf(){
    return nextLeaf;
}

void Node::setNextLeaf(Node* next){
    nextLeaf=next;
}

vector<Node*>& Node::getChildren(){
    return children;

}
