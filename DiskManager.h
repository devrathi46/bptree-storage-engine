#pragma once

#include "BufferPool.h"
#include "Page.h"

#include <unordered_map>
#include <string>

class DiskManager {

private:

    BufferPool pool;

    std::unordered_map<Node*, PageId> nodeToPage;

    // Reverse of nodeToPage: pageId -> the Node* already materialized
    // for it. Needed because a single leaf page is reachable two ways
    // while reconstructing the tree -- as a child pointer from its
    // parent, and as the nextLeaf pointer from its left sibling. Without
    // this cache, readNode would deserialize that page twice and hand
    // back two different Node* objects for what should be one node.
    std::unordered_map<PageId, Node*> pageToNode;

    PageId rootPageId;

    void writeMetadata();

    void readMetadata();

public:

    // bufferPoolCapacity: how many pages the LRU cache holds at once.
    // Deliberately small by default so eviction actually exercises the
    // LRU logic on a modest tree, instead of just caching everything.
    DiskManager(const std::string& filename, size_t bufferPoolCapacity = 64);

    PageId allocateNodePage(Node* node);

    void writeNode(Node* node);

    // Recursively writes node and every descendant of it (its whole
    // subtree). writeNode alone only persists one node's own page --
    // this is what actually gets a full tree onto disk.
    void writeTree(Node* node);

    Node* readNode(PageId pageId);

    // Reads ONE page's node without recursing into its children or
    // resolving its next-leaf pointer -- unlike readNode(), which
    // eagerly loads the whole subtree. The returned Node's children
    // vector is empty and nextLeaf is null regardless of what's on
    // disk; outChildPageIds/outNextLeafPageId are the raw page ids for
    // the caller to decide where to go next.
    //
    // This is a throwaway peek, not registered in nodeToPage/pageToNode
    // and not part of any tracked tree -- the caller owns the returned
    // Node* and should delete it once done with it. It exists so a
    // traversal (point lookup, range scan) only touches the pages
    // actually on its path, instead of the whole tree.
    Node* readNodeShallow(PageId pageId, std::vector<int>& outChildPageIds, int& outNextLeafPageId);

    PageId getPageId(Node* node);

    PageId getRootPageId();

    void setRootPageId(PageId pageId);

    // Passthroughs to the buffer pool, for the benchmark harness to
    // report real disk reads vs cache hits per operation.
    int getReadCount() const;
    int getCacheHits() const;
    int getCacheMisses() const;
};