#include "Pager.h"
#include <stdexcept>

Pager::Pager(const std::string& filename) : filename(filename), pageCount(0), readCount(0){
    // fstream with in|out won't create a missing file, so touch it first
    // if it doesn't exist, then reopen in read+write binary mode.
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if(!file.is_open()){
        std::ofstream create(filename, std::ios::binary);
        create.close();
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }
    if(!file.is_open()){
        throw std::runtime_error("Pager: could not open or create file " + filename);
    }

    // Reopening an existing file: recover how many pages it already
    // holds from its size, so page ids stay stable across a close/reopen.
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    pageCount = static_cast<int>(size / PAGE_SIZE);
}

Pager::~Pager(){
    if(file.is_open()){
        file.flush();
        file.close();
    }
}

PageId Pager::allocatePage(){
    PageId newPageId = pageCount;

    // Zero-fill the new page on disk so reads of unwritten pages are
    // well-defined instead of returning garbage.
    char zeroBuf[PAGE_SIZE] = {0};
    file.seekp(static_cast<std::streamoff>(newPageId) * PAGE_SIZE);
    file.write(zeroBuf, PAGE_SIZE);
    file.flush();

    pageCount++;
    return newPageId;
}

void Pager::readPage(PageId pageId, char* buffer){
    if(pageId < 0 || pageId >= pageCount){
        throw std::out_of_range("Pager::readPage: invalid page id " + std::to_string(pageId));
    }
    file.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE);
    file.read(buffer, PAGE_SIZE);
    readCount++;
}

void Pager::writePage(PageId pageId, const char* buffer){
    if(pageId < 0 || pageId >= pageCount){
        throw std::out_of_range("Pager::writePage: invalid page id " + std::to_string(pageId));
    }
    file.seekp(static_cast<std::streamoff>(pageId) * PAGE_SIZE);
    file.write(buffer, PAGE_SIZE);

    // Without this, the write sits in the C++ stream's internal buffer --
    // not yet visible to another file handle on the same path, and not
    // guaranteed to survive if the process dies before the buffer happens
    // to flush on its own. flush() pushes it to the OS immediately.
    // (This is not fsync: the OS can still lose it on a power loss before
    // the OS itself persists it to physical disk -- the same "no WAL,
    // no durability guarantee beyond a crashed process" limitation this
    // project already documents, just one level more precise about where
    // the line actually is.)
    file.flush();
}

int Pager::getPageCount() const{
    return pageCount;
}

int Pager::getReadCount() const{
    return readCount;
}
