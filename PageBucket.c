/*
PageBucket.c
Linked List Utility Functions for Bucket and LRU
*/
#include "PageBucket.h"
#include "PageHeader.h"

/*
DESCRIPTION
    Prepend a PageHeader to a given Bucket. Pointer operations to handle new
    head are handled within this function.

FUNCTION FIELDS
    [PageHeader**] head: Double pointer to the head PageHeader of a Bucket.

    [PageHeader**] tail: Double pointer to the tail PageHeader of a Bucket.

    [PageHeader*] new_page_header: Pointer to the PageHeader to prepend.

    [signed int*] current_page_count: Pointer to the current count of
    PageHeaders in the Bucket

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024

    Adapted to remove Node struct and use PageHeaders directly
    Aijun Hall, 7/17/2024
*/
void prependPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count) {
    assert(new_page_header != NULL);
    assert(new_page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);

    if (*head == NULL) {
        assert(*tail == NULL);

        new_page_header->hash_prev_ptr = NULL;
        new_page_header->hash_next_ptr = NULL;

        *head = new_page_header;
        *tail = new_page_header;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        new_page_header->hash_prev_ptr = NULL;
        new_page_header->hash_next_ptr = *head;

        assert((*head)->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
        (*head)->hash_prev_ptr = new_page_header;
        *head = new_page_header;

        assert(*head != NULL);
        assert((*head)->hash_prev_ptr == NULL);
    }

    assert(*head != NULL);
    assert(*tail != NULL);
    (*current_page_count)++;
}

/*
DESCRIPTION
    Append a PageHeader to a given Bucket. Pointer operations are handled within
    this function.

FUNCTION FIELDS
    [PageHeader**] head: Double pointer to the head PageHeader of a Bucket.

    [PageHeader**] tail: Double pointer to the tail PageHeader of a Bucket.

    [PageHeader*] new_page_header: Pointer to the PageHeader to append.

    [signed int*] current_page_count: Pointer to the current count of
    PageHeaders in the Bucket

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024

    Adapted to remove Node struct and use PageHeaders directly
    Aijun Hall, 7/17/2024
*/
void appendPageHeader(PageHeader** head, PageHeader** tail, PageHeader* new_page_header, signed int* current_page_count) {
    assert(new_page_header != NULL);
    assert(new_page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);

    if (*tail == NULL) {
        assert(*head == NULL);

        new_page_header->hash_prev_ptr = NULL;
        new_page_header->hash_next_ptr = NULL;

        *head = new_page_header;
        *tail = new_page_header;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        new_page_header->hash_prev_ptr = *tail;
        new_page_header->hash_next_ptr = NULL;

        assert((*tail)->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
        (*tail)->hash_next_ptr = new_page_header;
        *tail = new_page_header;

        assert(*tail != NULL);
        assert((*tail)->hash_next_ptr == NULL);
    }

    assert(*head != NULL);
    assert(*tail != NULL);
    (*current_page_count)++;
}

/*
DESCRIPTION
    Insert a PageHeader in a given Bucket via its head and tail pointer, relative to
    an insert target PageHeader. Note that insert will always be an appending action.
    Pointer operations to handle the insertion are handled within this function.

FUNCTION FIELDS
    [PageHeader**] head: Double pointer to the head PageHeader of a Bucket.

    [PageHeader**] tail: Double pointer to the tail PageHeader of a Bucket.

    [PageHeader*] insert_target_page_header: Pointer to the PageHeader to insert
    relative to.

    [PageHeader*] new_page_header: Pointer to the PageHeader to insert.

    [signed int*] current_page_count: Pointer to the current count of pages in the Bucket.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/5/2024

    Removed check for (head == NULL) case. insertNode() should not be used on
    empty bucket.
    Aijun Hall, 6/12/2024

    Adapted to remove Node struct and use PageHeaders directly
    Aijun Hall, 7/17/2024
*/
void insertPageHeader(PageHeader** head, PageHeader** tail, PageHeader* insert_target_page_header, PageHeader* new_page_header, signed int* current_page_count) {
    assert(new_page_header != NULL);
    assert(insert_target_page_header != NULL);
    assert(*head != NULL);
    assert(new_page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
    assert(insert_target_page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);

    if (insert_target_page_header == *tail) {
        new_page_header->hash_prev_ptr = *tail;
        new_page_header->hash_next_ptr = NULL;

        assert((*tail)->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
        (*tail)->hash_next_ptr = new_page_header;
        *tail = new_page_header;

        assert(*tail != NULL);
        assert((*tail)->hash_next_ptr == NULL);
    } else {
        new_page_header->hash_prev_ptr = insert_target_page_header;
        new_page_header->hash_next_ptr = insert_target_page_header->hash_next_ptr;

        if (insert_target_page_header->hash_next_ptr != NULL) {
            assert((insert_target_page_header->hash_next_ptr)->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
            insert_target_page_header->hash_next_ptr->hash_prev_ptr = new_page_header;
        }

        insert_target_page_header->hash_next_ptr = new_page_header;

        if (insert_target_page_header == *tail) {
            *tail = new_page_header;
        }

        assert(*tail != NULL);
        assert((*tail)->hash_next_ptr == NULL);
    }

    (*current_page_count)++;
}


/*
DESCRIPTION
    Delete a PageHeader from a Bucket via it's head and tail pointer.
    Pointer operations to handle changing head, tail, or inbetween pointers are
    handled within this function.

FUNCTION FIELDS
    [PageHeader**] head: Double pointer to the head PageHeader of a Bucket.

    [PageHeader**] tail: Double pointer to the tail PageHeader of a Bucket.

    [PageHeader*] page_header: Pointer to the PageHeader to delete.

    [int*] current_page_count: Pointer to the current count of pages in the Bucket

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024

    Refactored to make more simple. Now only check for 2 conditions:
    - if head == tail, then only 1 node in bucket.
    - else at least 2 nodes in bucket.
    Aijun Hall, 6/12/2024

    Adapted to remove Node struct and use PageHeaders directly.
    Renamed to "removePageHeader" from "deletePageHeader" since freeing the
    memory is no longer synonomous with Bucket removal.
    Aijun Hall, 7/17/2024
*/
void removePageHeader(PageHeader** head, PageHeader** tail, PageHeader* page_header, signed int* current_page_count) {
    assert(page_header != NULL);
    assert(*head != NULL && *tail != NULL);
    assert(page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);

    if (*head == *tail) {
        assert(page_header == *head);

        *head = NULL;
        *tail = NULL;
    } else {
        if (page_header == *head) {
            *head = page_header->hash_next_ptr;
            (*head)->hash_prev_ptr = NULL;

        } else if (page_header == *tail) {
            *tail = page_header->hash_prev_ptr;
            (*tail)->hash_next_ptr = NULL;

        } else {
            page_header->hash_prev_ptr->hash_next_ptr = page_header->hash_next_ptr;
            page_header->hash_next_ptr->hash_prev_ptr = page_header->hash_prev_ptr;

        }
    }

    (*current_page_count)--;
}

/*
DESCRIPTION
    Debugging function to print the current state of a bucket. Prints each
    PageHeader using PRINT_NODE macro defined at the top of this file, and then
    the bucket length.

FUNCTION FIELDS
    [PageBucket*] bucket: Pointer to the bucket to print.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024

    Adapted to remove Node struct and use PageHeaders directly
    Aijun Hall, 7/17/2024
*/
void printBucket(PageBucket* bucket) {
    PageHeader* current = bucket->head;

    while (current != NULL) {
        PRINT_PAGE_HEADER(current);
        current = current->hash_next_ptr;
    };

    printf("[LENGTH]\n%d\n", bucket->current_page_count);
}

/*
DESCRIPTION
    Helper Function for Testing. Walk through a Bucket with a given array of
    values, and assert that each node's data is correct according to the
    expected array values.

FUNCTION FIELDS
    [int[]] Array of integers that hold the expected values

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/18/2024

    Adapted to remove Node struct and use PageHeaders directly
    Aijun Hall, 7/17/2024
*/
void walkAndAssertBucket(PageHeader** head, PageHeader** tail, signed int* current_page_count, int *expected_values) {
    PageHeader* current = (*head);

    assert((*head)->hash_prev_ptr == NULL);

    for (int i=0;i<(*current_page_count);i++) {
        assert(current->data[0] == expected_values[i]);

        if (i == *(current_page_count)) {
            assert(current->hash_prev_ptr == (*tail));
        } else {
            current = current->hash_next_ptr;
        }

    }

    assert(current == NULL);
}