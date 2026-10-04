# include "BPlusTree.h"
#include<bits/stdc++.h>
using namespace std;

BPlusTree::BPlusTree(int order){
    this->order=order;
    root=new Node(true);
}
Node* BPlusTree::getRoot(){
    return root;

}
bool BPlusTree::search(int key){

    Node* node=getRoot();

    // descend to the correct leaf, same pattern as insert()
    while(!node->getIsLeaf()){
        vector<int>& nodeKeys=node->getKeys();
        vector<Node*>& children=node->getChildren();

        int childPos=0;
        while(childPos<nodeKeys.size() && key>=nodeKeys[childPos]){
            childPos++;
        }
        node=children[childPos];
    }

    for(int currentKey:node->getKeys()){
        if(currentKey==key){
            return true;
        }
    }
    return false;
}

void BPlusTree::insertIntoParent(Node* left, Node* right, int separator,int pathIndex,vector<Node*>& path){
    //left was the root;
    if(pathIndex<0){
        Node* newRoot=new Node(false);
        newRoot->getKeys().push_back(separator);
        newRoot->getChildren().push_back(left);
        newRoot->getChildren().push_back(right);
        this->root=newRoot;
        return;
    }
    //find parent
    Node* parent=path[pathIndex];

    //insert separator in the parent
    int parentPos=0;
    while(parentPos<parent->getKeys().size() && parent->getKeys()[parentPos]<separator){
        parentPos++;
    }
    parent->getKeys().insert(parent->getKeys().begin()+parentPos,separator);

    int childPos=0;
    while(childPos<parent->getChildren().size() && parent->getChildren()[childPos]!=left){
        childPos++;
    }
    parent->getChildren().insert(parent->getChildren().begin()+childPos+1,right);

    //parent did not overflow
    if(parent->getKeys().size()<=order){
        return;
    }

    //parent overflowed
    Node* newInternal=new Node(false);
    vector<int>& parentKeys=parent->getKeys();
    vector<Node*>& parentChildren=parent->getChildren();

    int mid=parentKeys.size()/2;

    int newSeparator=parentKeys[mid];

    for(int i=mid+1;i<parentKeys.size();i++){
        newInternal->getKeys().push_back(parentKeys[i]);
    }
    for(int i=mid+1;i<parentChildren.size();i++){
        newInternal->getChildren().push_back(parentChildren[i]);
        
    }
    parentKeys.resize(mid);
    parentChildren.resize(mid+1);

    insertIntoParent(parent,newInternal,newSeparator,pathIndex-1,path);

}
void BPlusTree:: insert(int key,int value){

    Node* root=getRoot();
    //Node* parent=nullptr;
    vector<Node*>path;

    while(!root->getIsLeaf()){
        path.push_back(root);
        //parent=root;
        vector<int>& rootKeys=root->getKeys();
        vector<Node*>& children=root->getChildren();

        int childPos=0;
        while(childPos<rootKeys.size() && key>=rootKeys[childPos]){
            childPos++;
        }
        root=children[childPos];
    }
    vector<int>& keys=root->getKeys();
    vector<int>& values=root->getValues();

    int pos=0;
    while(pos<keys.size() && keys[pos]<key){
        pos++;
    }
    keys.insert(keys.begin()+pos,key);
    values.insert(values.begin()+pos,value);


    if(keys.size()>order){
        Node *newLeaf=new Node(true);
        int mid=keys.size()/2;
        
        //split the node from middle position and copy the elements to the next leaf
        for(int i=mid;i<keys.size();i++){
            newLeaf->getKeys().push_back(keys[i]);
            newLeaf->getValues().push_back(values[i]);
        }

        //remove the keys,values from the old leaf;
        keys.resize(mid);
        values.resize(mid);

        //change the connectivity
        //a-->b---->.  a--c--b
        Node* oldNext=root->getNextLeaf();
        root->setNextLeaf(newLeaf);
        newLeaf->setNextLeaf(oldNext);

        //Now creating internal roots, and splitting the tree;
        int separator=newLeaf->getKeys()[0];
        if(path.empty()){
            insertIntoParent(root,newLeaf,separator,-1,path);
        }
        else{
            //find separator position in parent.
            insertIntoParent(root,newLeaf,separator,path.size()-1,path);
        }
    }

}