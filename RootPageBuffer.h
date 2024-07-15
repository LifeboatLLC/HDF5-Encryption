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
    signed int nodes_allocated;
    signed int nodes_deleted;
    signed int page_headers_allocated;
} RootPageBufferStatistics;

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
*/
typedef struct RootPageBuffer {
    int sanity_check_tag;
    int page_size;

    RootPageBufferStatistics* stats;
} RootPageBuffer;

void initializeRootPageBuffer(RootPageBuffer* root_page_buffer, RootPageBufferStatistics* stats);
void initializeRootPageBufferStatistics(RootPageBufferStatistics* stats);
void setupMockRootPageBuffer();
void printNodesAllocated(RootPageBufferStatistics* stats);

bool testMallocAndInitNode(RootPageBuffer* root);
bool testAppendNode(RootPageBuffer* root);
bool testAppendNodeEmpty(RootPageBuffer* root);
bool testPrependNode(RootPageBuffer* root);
bool testInsertNode(RootPageBuffer* root);
bool testDeleteHeadNode(RootPageBuffer* root);
bool testDeleteTailNode(RootPageBuffer* root);
bool testRandomBucketLength(int random_seed, RootPageBuffer* root);

void runPageBucketTests();

#endif