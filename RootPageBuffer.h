/*
RootPageBuffer.h
Root Structure of the Page Buffer
*/

#ifndef ROOTPAGEBUFFER_H
#define ROOTPAGEBUFFER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

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

#define ROOT_PAGE_BUFFER_SANITY_CHECK_TAG 0x526F
#define ROOT_PAGE_BUFFER_SANITY_CHECK_TAG_INVALID 0x526F5F

/*
DESCRIPTION
    All statistics associated with the PageBuffer are stored here for ease of
    access. Stores a variety of statistics associated with individual
    datastructures within the RootPageBuffer.

    TODO: Temporary skeleton mock with just arbritrary BucketNode statistics
    for now to represent how future statistics are stored.

STRUCT FIELDS
    [unsigned int] bucket_nodes_allocated: Number of BucketNodes that have been
    dynamically allocated on the heap.

    [unsigned int] bucket_nodes_deleted: Number of BucketNodes that have been
    deleted.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
typedef struct RootPageBufferStatistics {
    signed int page_headers_allocated;
    signed int page_headers_deleted;
} RootPageBufferStatistics;

/*
DESCRIPTION
    An entry in the PageHashTable. Each entry holds its hash_key value for
    indexing, and the entry's PageBucket.

STRUCT FIELDS
    [int] hash_key: Hash of the page, used in PageHashTable. Calculated as a
    function of the page's memory address.

    [*PageBucket] page_bucket: Pointer to the PageBucket of an entry.

CHANGELOG
    First created
    Aijun Hall, 7/23/2024
*/
typedef struct PageHashTableEntry {
    int hash_key;
    struct PageBucket* bucket;
} PageHashTableEntry;

/*
DESCRIPTION
    Root of the PageBuffer system.

    TODO: Temporary skeleton mock to just hold statistics

STRUCT FIELDS
    [int] sanity_check_tag: Struct tag used for error checking.

    [RootPageBufferStatistics*] root_page_buffer_statistics: Pointer to the
    RootPageBuffer statistics.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024

    Now includes PageHashTable
    Aijun Hall, 7/23/2024
*/
typedef struct RootPageBuffer {
    int sanity_check_tag;
    int PAGE_SIZE;
    int PAGE_HASH_TABLE_SIZE;

    RootPageBufferStatistics* stats;
    struct PageHashTableEntry** page_hash_table[];

} RootPageBuffer;

void initializeRootPageBuffer(RootPageBuffer* root_page_buffer, RootPageBufferStatistics* stats);
void initializeRootPageBufferStatistics(RootPageBufferStatistics* stats);
void setupMockRootPageBuffer();
void printPageHeadersAllocated(RootPageBufferStatistics* stats);

bool testAppendPageHeader(RootPageBuffer* root);
bool testAppendPageHeaderEmpty(RootPageBuffer* root);
bool testPrependPageHeader(RootPageBuffer* root);
bool testInsertPageHeader(RootPageBuffer* root);
bool testDeleteHeadPageHeader(RootPageBuffer* root);
bool testDeleteTailPageHeader(RootPageBuffer* root);
bool testRandomBucketLength(int random_seed, RootPageBuffer* root);

bool testMallocAndInitNewPageHeader(RootPageBuffer* root);
void runPageBucketTests();

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

/*
DESCRIPTION
    PageBuckets hold PageHeaders within the RootPageBuffer hash table. A
    PageBucket contains a head and tail pointer to the first and last node in
    the bucket. PageBucket functions will maintain the head and tail pointers as
    necessary.

STRUCT FIELDS
    [PageHeader*] head: Pointer to head PageHeader of the Bucket.

    [PageHeader*] tail: Pointer to the tail PageHeader of the Bucket.

    [signed int] current_page_count: The current number of pages in the Bucket.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024

    Removed Node structure since it introduced unnecessary abstraction.
    Now the Bucket will simply hold PageHeaders, and the PageHeader internal
    pointers will handle linking.
    Aijun Hall, 7/17/2024
*/
typedef struct PageBucket {
    struct PageHeader* head;
    struct PageHeader* tail;
    signed int current_page_count;
} PageBucket;

void prependPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count);
void appendPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count);
void insertPageHeader(PageHeader** head, PageHeader** tail, PageHeader* insert_target_page_header, PageHeader* new_page_header, signed int* current_page_count);
void removePageHeader(PageHeader** head, PageHeader** tail, PageHeader* page_header, signed int* current_page_count);

void printBucket(PageBucket* bucket);
void walkAndAssertBucket(PageHeader** head, PageHeader** tail, signed int* current_page_count, int *expected_values);

#endif