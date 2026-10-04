#include<bits/stdc++.h>
#include "BPlusTree.h"
using namespace std;


int main(){
    //order 4-->there will be 3 keys
    BPlusTree tree(4);
    

    tree.insert(20,200);
    tree.insert(40,400);
    tree.insert(30,300);
    tree.insert(10,100);
    tree.insert(50,400);

    cout<<tree.getRoot()->getKeys().size()<<endl;


    return 0;

}

