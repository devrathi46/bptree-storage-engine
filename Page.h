#pragma once

#include "Pager.h"
#include "Node.h"

class Page {

public:

    static void serialize(
        Node* node,
        const std::vector<int>& childPageIds,
        int nextLeafPageId,
        char* buffer
    );

    static Node* deserialize(
        const char* buffer,
        std::vector<int>& childPageIds,
        int& nextLeafPageId
    );
};