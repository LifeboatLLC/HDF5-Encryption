/*
PageHeader.h
Root Structure of the Page Buffer
*/

#ifndef PAGEHEADER_H
#define PAGEHEADER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "RootPageBuffer.h"
#include "PageBucket.h"

#define PAGE_HEADER_SANITY_CHECK_TAG 0x6865
#define PAGE_HEADER_SANITY_CHECK_TAG_INVALID 0x68655F
#define PRINT_PAGE_HEADER(page_header) do {                                      \
    printf("[pageHeader]\n");                                                   \
    printf("STRUCT TAG: %d \n", (page_header)->sanity_check_tag);               \
    printf("Data: %u \n", (page_header)->data);                                 \
    printf("Hash Key: %d \n", (page_header)->hash_key);                         \
    printf("Page Offset Address: %d \n", (page_header)->page_offset_address);   \
    printf("HashTable Next Pointer: %p\n", (page_header)->hash_next_ptr);       \
    printf("HashTable Prev Pointer: %p\n", (page_header)->hash_prev_ptr);       \
    printf("RP Next Pointer: %p\n", (page_header)->rp_next_ptr);                \
    printf("RP Prev Pointer: %p\n", (page_header)->rp_prev_ptr);                \
    printf("Dirty Flag: %p\n", (page_header)->is_dirty);                        \
    printf("Busy Flag: %p\n", (page_header)->is_busy);                          \
    printf("READ Flag: %p\n", (page_header)->is_read);                          \
    printf("WRITE Flag: %p\n", (page_header)->is_write);                        \
} while(0);

/*
DESCRIPTION
    Store a Page's Metadata and maintain points to actual contents in the page.
    Maintain pointers to other PageHeaders in the hash table, as well as the
    replacement policy

STRUCT FIELDS
    [int] sanity_check_tag: Struct tag used for error checking.

    [int] page_offset_address: Memory Offset of Page in File.

    [int] hash_key: Hash of the page, used in PageHashTable. Calculated as a
    function of the page's memory address.

    [PageHeader*] hash_next_ptr: Pointer to the next pageHeader in a HashTable
    Bucket.

    [PageHeader*] hash_prev_ptr: Pointer to the previous pageHeader in a
    HashTable Bucket.

    [PageHeader*] rp_prev_ptr: Pointer to the next pageHeader in the Replacement
    Policy

    [PageHeader*] rp_next_ptr: Pointer to the prev pageHeader in the Replacement
    Policy

    [bool] is_dirty: Bool indicating whether the page has been modified since it
    was last written to disk

    [bool] is_busy: Bool indicating whether a page is in a 'busy state'. A page
    in a busy state should not be used for read/write operations or have its
    data accessed or altered while remaining busy. Used as a safety in a
    multithread context.

    [bool] is_read: Bool indicating whether the page is queued up to be read

    [bool] is_write: Bool indicating whether the page is queued up to be written

    # TODO SupportReplacementPolicy?

    [uint8_t] data: raw data of the page.

CHANGELOG
    First created
    Aijun Hall, 6/20/2024
*/
typedef struct PageHeader {
    int sanity_check_tag;

    int page_offset_address;
    int hash_key;

    struct PageHeader* hash_next_ptr;
    struct PageHeader* hash_prev_ptr;
    struct PageHeader* rp_next_ptr;
    struct PageHeader* rp_prev_ptr;

    bool is_dirty;
    bool is_busy;
    bool is_read;
    bool is_write;

    uint8_t* data;
} PageHeader;

PageHeader* allocatePageHeader(int page_size, RootPageBufferStatistics* stats);
void initializePageHeader(PageHeader* target_page_header, int page_offset_address, int page_size, uint8_t* data);
int calculatePageHeaderHashKey(int page_offset_address, int page_size);

#endif