#include "DiskManager.h"

#include <cstring>
#include <stdexcept>

using namespace std;

static const int METADATA_PAGE = 0;
static const int MAGIC_NUMBER = 0x42505431;


DiskManager::DiskManager(const string& filename, size_t bufferPoolCapacity)
    : pool(filename, bufferPoolCapacity) {

    /*
        Empty database.

        Page 0 is reserved for metadata.
    */

    if(pool.getPageCount() == 0) {

        rootPageId = -1;

        // Create metadata page.
        pool.allocatePage();

        writeMetadata();
    }
    else {

        readMetadata();
    }
}


void DiskManager::writeMetadata() {

    char buffer[PAGE_SIZE];

    memset(buffer, 0, PAGE_SIZE);

    int offset = 0;

    int magic = MAGIC_NUMBER;

    memcpy(
        buffer + offset,
        &magic,
        sizeof(int)
    );

    offset += sizeof(int);


    memcpy(
        buffer + offset,
        &rootPageId,
        sizeof(PageId)
    );

    offset += sizeof(PageId);


    pool.writePage(
        METADATA_PAGE,
        buffer
    );
}


void DiskManager::readMetadata() {

    char buffer[PAGE_SIZE];

    pool.readPage(
        METADATA_PAGE,
        buffer
    );

    int offset = 0;

    int magic;

    memcpy(
        &magic,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);


    if(magic != MAGIC_NUMBER) {

        throw runtime_error(
            "Invalid database file"
        );
    }


    memcpy(
        &rootPageId,
        buffer + offset,
        sizeof(PageId)
    );
}


PageId DiskManager::allocateNodePage(Node* node) {

    PageId pageId = pool.allocatePage();

    nodeToPage[node] = pageId;

    return pageId;
}


PageId DiskManager::getPageId(Node* node) {

    auto it = nodeToPage.find(node);

    if(it == nodeToPage.end()) {

        return allocateNodePage(node);
    }

    return it->second;
}


void DiskManager::setRootPageId(PageId pageId) {

    rootPageId = pageId;

    writeMetadata();
}


PageId DiskManager::getRootPageId() {

    return rootPageId;
}


void DiskManager::writeNode(Node* node) {

    PageId pageId = getPageId(node);

    vector<int> childPageIds;

    for(Node* child : node->getChildren()) {

        childPageIds.push_back(
            getPageId(child)
        );
    }


    int nextLeafPageId = -1;

    if(node->getNextLeaf() != nullptr) {

        nextLeafPageId =
            getPageId(node->getNextLeaf());
    }


    char buffer[PAGE_SIZE];

    Page::serialize(
        node,
        childPageIds,
        nextLeafPageId,
        buffer
    );


    pool.writePage(
        pageId,
        buffer
    );
}


void DiskManager::writeTree(Node* node) {

    // Persist every descendant before the node itself. Write order
    // doesn't change correctness here (getPageId lazily allocates a
    // page number for any node the first time it's asked for, written
    // or not) -- but writing children first keeps page numbering
    // intuitive: a page is only ever written once we've actually
    // visited that node.
    for(Node* child : node->getChildren()) {

        writeTree(child);
    }

    writeNode(node);
}


Node* DiskManager::readNode(PageId pageId) {

    // Memoization: if this page has already been turned into a Node*
    // during this load (reached earlier via a different pointer -- see
    // the pageToNode comment in DiskManager.h), reuse that same object
    // instead of deserializing it again.
    auto cached = pageToNode.find(pageId);

    if(cached != pageToNode.end()) {

        return cached->second;
    }


    char buffer[PAGE_SIZE];

    pool.readPage(
        pageId,
        buffer
    );


    vector<int> childPageIds;

    int nextLeafPageId;


    Node* node =
        Page::deserialize(
            buffer,
            childPageIds,
            nextLeafPageId
        );


    // Register this node in both maps BEFORE recursing into its
    // children or its next-leaf pointer. That ordering matters: if a
    // child (or the next leaf) ever led back to this same page, the
    // cache lookup above would need to find it already registered.
    nodeToPage[node] = pageId;
    pageToNode[pageId] = node;


    // Rebuild the pointer graph: internal nodes get their children
    // pointers back by recursively loading each child page.
    for(int childPageId : childPageIds) {

        node->getChildren().push_back(
            readNode(childPageId)
        );
    }


    // Leaves get their nextLeaf pointer back the same way. -1 is the
    // sentinel Page::serialize writes when there is no next leaf.
    if(nextLeafPageId != -1) {

        node->setNextLeaf(
            readNode(nextLeafPageId)
        );
    }


    return node;
}


Node* DiskManager::readNodeShallow(PageId pageId, vector<int>& outChildPageIds, int& outNextLeafPageId) {

    char buffer[PAGE_SIZE];

    pool.readPage(
        pageId,
        buffer
    );

    // Page::deserialize already hands back exactly this: one page's
    // keys/values plus its children/next-leaf as raw ids, with no
    // recursion. readNode() is the one that chooses to chase them --
    // this method deliberately doesn't.
    return Page::deserialize(
        buffer,
        outChildPageIds,
        outNextLeafPageId
    );
}


int DiskManager::getReadCount() const {

    return pool.getReadCount();
}


int DiskManager::getCacheHits() const {

    return pool.getCacheHits();
}


int DiskManager::getCacheMisses() const {

    return pool.getCacheMisses();
}