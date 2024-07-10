/*
PageBucket.c
Linked List Utility Functions for Bucket and LRU
*/
#include "PageBucket.h"

/*
DESCRIPTION
    Allocate memory for a node in the Bucket.

FUNCTION FIELDS
    [RootPageBufferStatistics*] stats: Pointer to the RootPageBuffer statistics

RETURN TYPE
    [Node*] new_node: Pointer to newly created Node.

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
Node* allocateNode(RootPageBufferStatistics* stats) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    assert(new_node != NULL);

    // ROOT STATISTICS
    (stats->nodes_allocated)++;

    return new_node;
}

/*
DESCRIPTION
    Initialize fields of a freshly allocated Node with given data.

FUNCTION FIELDS
    [Node*] target_node: Pointer to the node to initialize.

    [uint8_t] data: #TODO placeholder value for PageHeaders

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
void initializeNode(Node* target_node, uint8_t data) {
    assert(target_node != NULL);

    target_node->sanity_check_tag = NODE_SANITY_CHECK_TAG;
    target_node->data = data;
    target_node->next = NULL;
    target_node->prev = NULL;
}

/*
DESCRIPTION
    Prepend a Node to a given Bucket via it's head and tail pointer.
    Pointer operations to handle new head are handled within this function.

FUNCTION FIELDS
    [Node**] head: Double pointer to the head of a Bucket.

    [Node**] tail: Double pointer to the tail of a Bucket.

    [Node*] new_node: Pointer to the Node to prepend.

    [signed int*] current_page_count: Pointer to the current count of pages in the Bucket

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024
*/
void prependNode(Node** head, Node** tail, Node* new_node, signed int* current_page_count) {
    assert(new_node != NULL);
    assert(new_node->sanity_check_tag == NODE_SANITY_CHECK_TAG);

    if (*head == NULL) {
        assert(*tail == NULL);

        new_node->prev = NULL;
        new_node->next = NULL;

        *head = new_node;
        *tail = new_node;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        new_node->next = *head;
        new_node->prev = NULL;

        assert((*head)->sanity_check_tag == NODE_SANITY_CHECK_TAG);
        (*head)->prev = new_node;
        *head = new_node;

        assert(*head != NULL);
        assert((*head)->prev == NULL);
    }

    assert(*head != NULL);
    assert(*tail != NULL);
    (*current_page_count)++;
}

/*
DESCRIPTION
    Append a Node to a given Bucket via it's head and tail pointer.
    Pointer operations are handled within this function.

FUNCTION FIELDS
    [Node**] head: Double pointer to the head of a Bucket.

    [Node**] tail: Double pointer to the tail of a Bucket.

    [Node*] new_node: Pointer to the Node to append.

    [signed int*] current_page_count: Pointer to the current count of pages in the Bucket

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024
*/
void appendNode(Node** head, Node** tail, Node* new_node, signed int* current_page_count) {
    assert(new_node != NULL);
    assert(new_node->sanity_check_tag == NODE_SANITY_CHECK_TAG);

    if (*tail == NULL) {
        assert(*head == NULL);

        new_node->prev = NULL;
        new_node->next = NULL;

        *head = new_node;
        *tail = new_node;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        new_node->prev = *tail;
        new_node->next = NULL;

        assert((*tail)->sanity_check_tag == NODE_SANITY_CHECK_TAG);
        (*tail)->next = new_node;
        *tail = new_node;

        assert(*tail != NULL);
        assert((*tail)->next == NULL);
    }

    assert(*head != NULL);
    assert(*tail != NULL);
    (*current_page_count)++;
}

/*
DESCRIPTION
    Insert a Node in a given Bucket via its head and tail pointer, relative to
    an insert target node. Note that insert will always be an appending action.
    Pointer operations to handle the insertion are handled within this function.

FUNCTION FIELDS
    [Node**] head: Double pointer to the head of a Bucket.

    [Node**] tail: Double pointer to the tail of a Bucket.

    [Node*] insert_target_node: Pointer to the Node to insert relative to.

    [Node*] new_node: Pointer to the Node to insert.

    [signed int*] current_page_count: Pointer to the current count of pages in the Bucket.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/5/2024

    Removed check for (head == NULL) case. insertNode() should not be used on
    empty bucket.
    Aijun Hall, 6/12/2024
*/
void insertNode(Node** head, Node** tail, Node* insert_target_node, Node* new_node, signed int* current_page_count) {
    assert(new_node != NULL);
    assert(insert_target_node != NULL);
    assert(*head != NULL);
    assert(new_node->sanity_check_tag == NODE_SANITY_CHECK_TAG);
    assert(insert_target_node->sanity_check_tag == NODE_SANITY_CHECK_TAG);

    if (insert_target_node == *tail) {
        new_node->prev = *tail;
        new_node->next = NULL;

        assert((*tail)->sanity_check_tag == NODE_SANITY_CHECK_TAG);
        (*tail)->next = new_node;
        *tail = new_node;

        assert(*tail != NULL);
        assert((*tail)->next == NULL);
    } else {
        new_node->prev = insert_target_node;
        new_node->next = insert_target_node->next;

        if (insert_target_node->next != NULL) {
            assert((insert_target_node->next)->sanity_check_tag == NODE_SANITY_CHECK_TAG);
            insert_target_node->next->prev = new_node;
        }

        insert_target_node->next = new_node;

        if (insert_target_node == *tail) {
            *tail = new_node;
        }

        assert(*tail != NULL);
        assert((*tail)->next == NULL);
    }

    (*current_page_count)++;
}


/*
DESCRIPTION
    Delete a Node from a Bucket via it's head and tail pointer.
    Pointer operations to handle changing head, tail, or inbetween pointers are
    handled within this function.

FUNCTION FIELDS
    [Node**] head: Double pointer to the head of a Bucket.

    [Node**] tail: Double pointer to the tail of a Bucket.

    [Node*] node: Pointer to the Node to delete.

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
*/
void deleteNode(Node** head, Node** tail, Node* node, signed int* current_page_count) {
    assert(node != NULL);
    assert(*head != NULL && *tail != NULL);
    assert(node->sanity_check_tag == NODE_SANITY_CHECK_TAG);

    if (*head == *tail) {
        assert(node == *head);

        *head = NULL;
        *tail = NULL;
    } else {
        if (node == *head) {
            *head = node->next;
            (*head)->prev = NULL;

        } else if (node == *tail) {
            *tail = node->prev;
            (*tail)->next = NULL;

        } else {
            node->prev->next = node->next;
            node->next->prev = node->prev;

        }
    }

    (*current_page_count)--;
    node->sanity_check_tag = NODE_SANITY_CHECK_TAG_INVALID;
    free(node);
}

/*
DESCRIPTION
    Debugging function to print the current state of a bucket. Prints each node
    using PRINT_NODE macro defined at the top of this file, and then the bucket
    length.

FUNCTION FIELDS
    [PageBucket*] bucket: Pointer to the bucket to print.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024
*/
void printBucket(PageBucket* bucket) {
    Node* current = bucket->head;

    while (current != NULL) {
        PRINT_NODE(current);
        current = current->next;
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
*/
void walkAndAssertBucket(Node** head, Node** tail, signed int* current_page_count, int *expected_values) {
    Node* current = (*head);

    assert((*head)->prev == NULL);

    for (int i=0;i<(*current_page_count);i++) {
        assert(current->data == expected_values[i]);

        if (i == *(current_page_count)) {
            assert(current->prev == (*tail));
        } else {
            current = current->next;
        }

    }

    assert(current == NULL);
}