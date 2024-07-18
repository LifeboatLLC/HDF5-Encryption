/*
PageBucket.h
Linked List Utility Functions for Bucket and LRU
*/

#ifndef PAGEBUCKET_H
#define PAGEBUCKET_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "PageHeader.h"
#include "RootPageBuffer.h"

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
    PageHeader* head;
    PageHeader* tail;
    signed int current_page_count;
} PageBucket;

void prependPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count);
void appendPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count);
void insertPageHeader(PageHeader** head, PageHeader** tail, PageHeader* insert_target_page_header, PageHeader* new_page_header, signed int* current_page_count);
void removePageHeader(PageHeader** head, PageHeader** tail, PageHeader* page_header, signed int* current_page_count);

void printBucket(PageBucket* bucket);
void walkAndAssertBucket(PageHeader** head, PageHeader** tail, signed int* current_page_count, int *expected_values);

#endif
