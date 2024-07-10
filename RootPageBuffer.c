/*
RootPageBuffer.c
Root Structure of the Page Buffer
*/

#include "RootPageBuffer.h"
#include "PageBucket.h"
#include "PageHeader.h"

/*
DESCRIPTION
    Initialize fields of a freshly allocated RootPageBufferStatistics. Used as
    a helper function during RootPageBuffer initialization.

    TODO: Temporary skeletal mock just to hold PageBucket stats.

FUNCTION FIELDS
    [RootPageBufferStatistics*] stats: Pointer to RootPageBufferStatistics to
    initialize.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/2/2024
*/
void initializeRootPageBufferStatistics(RootPageBufferStatistics* stats) {
    assert(stats != NULL);

    stats->nodes_allocated = 0;
    stats->nodes_deleted = 0;
}

/*
DESCRIPTION
    Utility function to help setup the mock root page buffer for running the
    test suite.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [RootPageBuffer*] root_ptr: Pointer to the mock root page buffer.

CHANGELOG
    First created
    Aijun Hall, 6/5/2024

    Made root and stats static so that stats are passed easier to tests
    Aijun Hall, 6/6/2024

*/
void setupMockRootPageBuffer(RootPageBuffer* root, RootPageBufferStatistics* stats) {
    assert(root != NULL);
    assert(stats != NULL);

    initializeRootPageBufferStatistics(stats);

    root->sanity_check_tag = ROOT_PAGE_BUFFER_SANITY_CHECK_TAG;
    root->page_size = 4096; // #TODO HARDCODED PAGE SIZE
    root->stats = stats;
}

/*
DESCRIPTION
    Helper Function for Testing. Print Nodes Allocated Stat from the
    RootPageBufferStatistics.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/6/2024
*/
void printNodesAllocated(RootPageBufferStatistics* stats) {
    assert(stats != NULL);
    printf("Nodes Allocated: %d\n", stats->nodes_allocated);
}

/*
DESCRIPTION
    Unit Test for Initializing a Node. Ensure Node is properly allocated,
    and fields are initialized as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testMallocAndInitNode(RootPageBufferStatistics* stats) {
    Node* node = allocateNode(stats);
    assert(node != NULL);

    initializeNode(node, 10);
    assert(node->sanity_check_tag == NODE_SANITY_CHECK_TAG);
    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->data == 10);

    printf("Test 1 passed: Node created and initialized\n");

    node->sanity_check_tag = NODE_SANITY_CHECK_TAG_INVALID;
    free(node);

    return true;
}

/*
DESCRIPTION
    Unit Test for Appending Nodes into an empty bucket. Ensure Node is appended
    properly to an empty bucket and pointers are set as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/18/2024
*/
bool testAppendNodeToEmptyBucket(RootPageBufferStatistics* stats) {
    int expected_values[] = {10};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);

    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 2 passed: Node appended to empty list\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Appending Nodes. Ensure Node is appended onto tail of a
    bucket.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testAppendNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10};
    int expected_values[] = {10, 20};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    Node* node1 = allocateNode(stats);
    initializeNode(node1, 20);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    appendNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 3 passed: Node appended to the head\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Prepending Nodes. Ensure Nodes are prepended onto head as
    expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testPrependNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10};
    int expected_values[] = {20, 10};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    Node* node1 = allocateNode(stats);
    initializeNode(node1, 20);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    prependNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);

    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 4 passed: Node prepended to the head\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Inserting Nodes. Ensure Nodes are inserted into bucket as
    expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testInsertNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10};
    int expected_values[] = {10, 40, 20, 30};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    Node* node1 = allocateNode(stats);
    initializeNode(node1, 20);

    Node* node2 = allocateNode(stats);
    initializeNode(node2, 30);

    Node* node3 = allocateNode(stats);
    initializeNode(node3, 40);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    insertNode(&bucket.head, &bucket.tail, node0, node1, &bucket.current_page_count);
    insertNode(&bucket.head, &bucket.tail, node1, node2, &bucket.current_page_count);
    insertNode(&bucket.head, &bucket.tail, node0, node3, &bucket.current_page_count);

    assert(bucket.current_page_count == 4);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 5 passed: Nodes inserted into bucket\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node2, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node3, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a Head Node. Ensure that deleting the head node from
    a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testDeleteHeadNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10};

    Node* node = allocateNode(stats);
    initializeNode(node, 10);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    deleteNode(&bucket.head, &bucket.tail, node, &bucket.current_page_count);

    assert(bucket.current_page_count == 0);
    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);

    printf("Test 6 passed: Head node deleted\n");

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a Tail Node. Ensure that deleting the tail node from
    a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024
*/
bool testDeleteTailNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10, 20};
    int expected_values[] = {10};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    Node* node1 = allocateNode(stats);
    initializeNode(node1, 20);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    appendNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    deleteNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 7 passed: Tail node deleted\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a Node in the middle of a bucket. Ensure that
    deleting the middle node from a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/20/2024
*/
bool testDeleteMiddleNode(RootPageBufferStatistics* stats) {
    int expected_setup[] = {10, 20, 30};
    int expected_values[] = {10, 30};

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 10);

    Node* node1 = allocateNode(stats);
    initializeNode(node1, 20);

    Node* node2 = allocateNode(stats);
    initializeNode(node2, 30);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    appendNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);
    appendNode(&bucket.head, &bucket.tail, node2, &bucket.current_page_count);

    assert(bucket.current_page_count == 3);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    deleteNode(&bucket.head, &bucket.tail, node1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 8 passed: Middle node deleted\n");

    deleteNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);
    deleteNode(&bucket.head, &bucket.tail, node2, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Monte Carlo Unit Test for Ensuring Length of Bucket is properly tracked.
    First Generate a random number using preset testing seed.
    If random number is even, append node. If random number is odd, prepend node.
    Repeat 100 times and verify bucket length.

FUNCTION FIELDS
    [int] random_seed: Random seed integer used to make random number generator
    consistent and reproducable between tests.

    [RootPageBufferStatistics* stats] Pointer to Root Page Buffer stats

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/13/2024
*/
bool testRandomBucketLength(int random_seed, RootPageBufferStatistics* stats) {
    srand(random_seed);

    Node* node0 = allocateNode(stats);
    initializeNode(node0, 0);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    appendNode(&bucket.head, &bucket.tail, node0, &bucket.current_page_count);

    for (int i=1; i<100; i++) {
        int random = rand();

        Node* node = allocateNode(stats);
        initializeNode(node, i);

        if (random % 3 == 0) {
            insertNode(&bucket.head, &bucket.tail, node0, node, &bucket.current_page_count);

        }
        else if (random % 2 == 0) {
            appendNode(&bucket.head, &bucket.tail, node, &bucket.current_page_count);

        } else {
            prependNode(&bucket.head, &bucket.tail, node, &bucket.current_page_count);
        }
    }

    assert(bucket.current_page_count == 100);

    // Expected Values from set random seed; Won't be applicable for other seeds.
    int expected_values[] = {
        99, 98, 97, 90, 87, 86, 85, 84, 82, 80, 79, 75, 74,  70, 67, 66, 64, 62,
        57, 54, 45, 43, 42, 36, 33, 29, 26, 25, 24, 23, 20, 15, 14, 13, 11, 10,
        9, 8, 4, 2, 1, 0, 95, 83, 81, 78, 73, 71, 69, 65, 58, 56, 55, 53, 52,
        49, 48, 44, 41, 40, 39, 38, 35, 31, 28, 27, 22, 19, 18, 12, 7, 6, 3, 5,
        16, 17, 21, 30, 32, 34, 37, 46, 47, 50, 51, 59, 60, 61, 63, 68, 72, 76,
        77, 88, 89, 91, 92, 93, 94, 96
    };

    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 9 passed: Monte Carlo Bucket Length Test\n");

    // Clean up test by freeing memory for all nodes in bucket.

    return true;
}

/*
DESCRIPTION
    Test Suite for PageBucket.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/3/2024
*/
void runPageBucketTests() {
    RootPageBufferStatistics stats;
    RootPageBuffer root;
    setupMockRootPageBuffer(&root, &stats);

    printf("\nRunning Page Bucket Tests...\n");

    const int RANDOM_SEED = 12345678;
    printf("\nTesting With Random Seed: %d\n", RANDOM_SEED);

    // Test 1: Malloc and Init a fresh node
    assert(testMallocAndInitNode(&stats) == true);

    // Test 2: Append a node to an empty bucket.
    assert(testAppendNodeToEmptyBucket(&stats) == true);

    // Test 3: Append a node to a fresh bucket.
    assert(testAppendNode(&stats) == true);

    // Test 4: Prepend a node to a fresh bucket.
    assert(testPrependNode(&stats) == true);

    // Test 5: Generic Insert a node to a fresh bucket.
    assert(testInsertNode(&stats) == true);

    // Test 6: Delete head node from a fresh bucket of len 1.
    assert(testDeleteHeadNode(&stats) == true);

    // Test 7: Delete tail node from a fresh bucket of len 2.
    assert(testDeleteTailNode(&stats) == true);

    // Test 8: Delete middle node from a fresh bucket of len 3.
    assert(testDeleteMiddleNode(&stats) == true);

    // Test 9: Monte Carlo Testing for Bucket Length
    assert(testRandomBucketLength(RANDOM_SEED, &stats) == true);

    printf("Root Stats:\n");
    printNodesAllocated(&stats);
}

/*
DESCRIPTION
    Unit Test for Initializing a PageHeader. Ensure PageHeader is properly
    allocated, and fields are initialized as expected.

FUNCTION FIELDS
    [RootPageBufferStatistics* root] Pointer to Root, so that we can access
    the set page size.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/26/2024
*/
bool testMallocAndInitPageHeader(RootPageBuffer* root) {
    PageHeader* page_header = allocatePageHeader(root->page_size, root->stats);
    assert(page_header != NULL);

    initializePageHeader(page_header, 0x256, 10);

    assert(page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
    assert(page_header->page_offset_address == 0x256);
    // #TODO update from hash key function
    // assert(page_header->hash_key)
    assert(page_header->hash_next_ptr == NULL);
    assert(page_header->hash_prev_ptr == NULL);
    assert(page_header->rp_next_ptr == NULL);
    assert(page_header->rp_prev_ptr == NULL);

    assert(page_header->is_dirty == false);
    assert(page_header->is_busy == false);
    assert(page_header->is_read == false);
    assert(page_header->is_write == false);

    assert(page_header->data == 10);

    printf("Test 1 passed: PageHeader created and initialized\n");
    return true;
}

/*
DESCRIPTION
    Test Suite for PageHeader.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/24/2024
*/
void runPageHeaderTests() {
    RootPageBufferStatistics stats;
    RootPageBuffer root;
    setupMockRootPageBuffer(&root, &stats);

    printf("\nRunning Page Header Tests...\n");

    const int RANDOM_SEED = 12345678;
    printf("\nTesting With Random Seed: %d\n", RANDOM_SEED);

    // Test 1: Malloc and Init a fresh pageHeader
    assert(testMallocAndInitPageHeader(&root) == true);
}

int main() {
    runPageBucketTests();
    runPageHeaderTests();
    return 0;
}