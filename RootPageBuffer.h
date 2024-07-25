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

#include "PageBucket.h"
#include "PageHeader.h"

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

#endif