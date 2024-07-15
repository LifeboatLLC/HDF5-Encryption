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

#include "RootPageBuffer.h"

/*
Sanity Check tags are created by running the struct name through ascii
conversion. Only use the first "word" of the struct, and the first 2 chars.
Invalid tag is the same
process, but with an appended "_".

For example:
Node -> No -> 0x4E6F (struct tag)
Node -> No_ -> 0x4E6F5F (invalid struct tag)
*/

#define NODE_SANITY_CHECK_TAG 0x4E6F
#define NODE_SANITY_CHECK_TAG_INVALID 0x4E6F5F
#define PRINT_NODE(node) do {                                       \
    printf("[NODE]\n");                                             \
    printf("STRUCT TAG: %d | ", node->sanity_check_tag);          \
    printf("PageHeader Data: %d | ", node->page_header->data[0]);    \
    printf("Next: %p | ", node->next);                            \
    printf("Prev: %p\n", node->prev);                             \
} while(0);

/*
DESCRIPTION
    A node in a doubly linked list representing the PageBucket.

STRUCT FIELDS
    [int] sanity_check_tag: Struct tag used for error checking.

    [PageHeader*] page_header: Pointer to PageHeader within Node.

    [Node*] next: Pointer to the next node in the doubly linked list.

    [Node*] prev: Pointer to the previous node in the doubly linked list.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
typedef struct Node {
    int sanity_check_tag;
    struct PageHeader* page_header;
    struct Node* next;
    struct Node* prev;
} Node;

/*
DESCRIPTION
    PageBucket containing a head and tail pointer to the first and last node in
    the bucket. PageBucket functions will maintain the head and tail pointers as
    necessary.

STRUCT FIELDS
    [Node*] head: Pointer to head node of the Bucket.

    [Node*] tail: Pointer to the tail node of the Bucket.

    [signed int] current_page_count: The current number of pages in the Bucket.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
typedef struct PageBucket {
    Node* head;
    Node* tail;
    signed int current_page_count;
} PageBucket;

Node* allocateNode(RootPageBufferStatistics* stats);
void initializeNode(Node* target_node, struct PageHeader* page_header);

void prependNode(Node** head, Node** tail, Node* new_node, signed int* current_page_count);
void appendNode(Node** head, Node** tail, Node* new_node, signed int* current_page_count);
void insertNode(Node** head, Node** tail, Node* insert_target_node, Node* new_node, signed int* current_page_count);
void deleteNode(Node** head, Node** tail, Node* node, signed int* current_page_count);

void printBucket(PageBucket* bucket);
void walkAndAssertBucket(Node** head, Node** tail, signed int* current_page_count, int *expected_values);

#endif
