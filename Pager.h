#pragma once

#include <string>
#include <fstream>

// A page id is just an integer index into the page file: page i lives
// at byte offset i * PAGE_SIZE. A named type (instead of a bare int)
// makes DiskManager.h's signatures self-documenting.
using PageId = int;

// 4KB matches the common OS/filesystem block size -- the traditional
// default for database pages, since a read smaller than this doesn't
// save anything (the OS fetches a full block regardless).
constexpr int PAGE_SIZE = 4096;

// Pager: the lowest layer of the storage engine. It knows nothing about
// B+ trees or nodes -- it only maps a page_id to a fixed PAGE_SIZE block
// of bytes in a single on-disk file. Everything above this (Page's
// serialize/deserialize, DiskManager's node<->page mapping) is built on
// top of just these operations.
class Pager {
    private:
        std::fstream file;
        std::string filename;
        int pageCount; // number of pages currently allocated in the file
        int readCount; // number of readPage() calls that actually touched disk

    public:
        explicit Pager(const std::string& filename);
        ~Pager();

        // Allocates a new page at the end of the file (zero-filled) and
        // returns its id.
        PageId allocatePage();

        // Reads exactly PAGE_SIZE bytes for pageId into buffer.
        void readPage(PageId pageId, char* buffer);

        // Writes exactly PAGE_SIZE bytes from buffer to pageId's slot on disk.
        void writePage(PageId pageId, const char* buffer);

        int getPageCount() const;

        // Real disk reads since this Pager was opened -- the metric the
        // benchmark compares against a buffer-pool cache hit (which never
        // reaches this counter) and against a full table scan.
        int getReadCount() const;
};
