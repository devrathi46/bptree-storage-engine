#include "Page.h"
#include <cstring>
#include <stdexcept>

using namespace std;

void Page::serialize(
    Node* node,
    const vector<int>& childPageIds,
    int nextLeafPageId,
    char* buffer
) {
    memset(buffer, 0, PAGE_SIZE);

    int offset = 0;

    int isLeaf = node->getIsLeaf() ? 1 : 0;
    int keyCount = node->getKeys().size();
    int valueCount = node->getValues().size();
    int childCount = childPageIds.size();

    // Fixed PAGE_SIZE buffer, variable-size record: nothing else here
    // stops a node from needing more than one page's worth of bytes.
    // Guard it explicitly instead of letting memcpy write past the end
    // of buffer -- that would silently corrupt whatever memory follows it.
    size_t requiredBytes = 5 * sizeof(int) // header: isLeaf, keyCount, valueCount, childCount, nextLeafPageId
                          + (size_t)keyCount * sizeof(int)
                          + (size_t)valueCount * sizeof(int)
                          + (size_t)childCount * sizeof(int);

    if(requiredBytes > (size_t)PAGE_SIZE){
        throw runtime_error(
            "Page::serialize: node does not fit in one page -- reduce order or increase PAGE_SIZE"
        );
    }

    // -------------------------
    // Header
    // -------------------------

    memcpy(buffer + offset, &isLeaf, sizeof(int));
    offset += sizeof(int);

    memcpy(buffer + offset, &keyCount, sizeof(int));
    offset += sizeof(int);

    memcpy(buffer + offset, &valueCount, sizeof(int));
    offset += sizeof(int);

    memcpy(buffer + offset, &childCount, sizeof(int));
    offset += sizeof(int);

    memcpy(buffer + offset, &nextLeafPageId, sizeof(int));
    offset += sizeof(int);



    for(int key : node->getKeys()) {

        memcpy(
            buffer + offset,
            &key,
            sizeof(int)
        );

        offset += sizeof(int);
    }

    for(int value : node->getValues()) {

        memcpy(
            buffer + offset,
            &value,
            sizeof(int)
        );

        offset += sizeof(int);
    }

    for(int pageId : childPageIds) {

        memcpy(
            buffer + offset,
            &pageId,
            sizeof(int)
        );

        offset += sizeof(int);
    }
}


Node* Page::deserialize(
    const char* buffer,
    vector<int>& childPageIds,
    int& nextLeafPageId
) {

    int offset = 0;

    int isLeaf;
    int keyCount;
    int valueCount;
    int childCount;

    // -------------------------
    // Header
    // -------------------------

    memcpy(
        &isLeaf,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);

    memcpy(
        &keyCount,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);

    memcpy(
        &valueCount,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);

    memcpy(
        &childCount,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);

    memcpy(
        &nextLeafPageId,
        buffer + offset,
        sizeof(int)
    );

    offset += sizeof(int);


    // -------------------------
    // Create Node
    // -------------------------

    Node* node = new Node(isLeaf);


    // -------------------------
    // Keys
    // -------------------------

    for(int i = 0; i < keyCount; i++) {

        int key;

        memcpy(
            &key,
            buffer + offset,
            sizeof(int)
        );

        offset += sizeof(int);

        node->getKeys().push_back(key);
    }


    // -------------------------
    // Values
    // -------------------------

    for(int i = 0; i < valueCount; i++) {

        int value;

        memcpy(
            &value,
            buffer + offset,
            sizeof(int)
        );

        offset += sizeof(int);

        node->getValues().push_back(value);
    }


    // -------------------------
    // Children
    // -------------------------

    childPageIds.clear();

    for(int i = 0; i < childCount; i++) {

        int pageId;

        memcpy(
            &pageId,
            buffer + offset,
            sizeof(int)
        );

        offset += sizeof(int);

        childPageIds.push_back(pageId);
    }

    return node;
}