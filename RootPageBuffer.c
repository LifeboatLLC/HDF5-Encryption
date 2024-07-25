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

    stats->page_headers_allocated = 0;
    stats->page_headers_deleted = 0;
}

/*
DESCRIPTION
    Initialize fields of a PageHashTable for use in the RootPageBuffer structure.

FUNCTION FIELDS
    [int] PAGE_HASH_TABLE_SIZE: Size of hash table, how many entries it can
    store.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 7/23/2024
*/
void initializePageHashTable(PageHashTableEntry* page_hash_table, int PAGE_HASH_TABLE_SIZE) {
    assert(page_hash_table != NULL);

    page_hash_table = (PageHashTableEntry**)malloc(PAGE_HASH_TABLE_SIZE * sizeof(PAGE_HASH_TABLE_SIZE*));

    for (int i = 0; i < PAGE_HASH_TABLE_SIZE; i++) {
        page_hash_table[i] = (PageHashTableEntry*)malloc(sizeof(PageHashTableEntry));

        page_hash_table[i]->hash_key = 0;
        page_hash_table[i]->bucket = NULL;
    }
}

/*
DESCRIPTION
    Utility function to help setup the mock root page buffer for running the
    test suite.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

    [RootPageBufferStatistics*] stats: Pointer to RootPageBufferStatistics to
    initialize.

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
    root->PAGE_SIZE = 4096; // #TODO HARDCODED PAGE SIZE
    root->PAGE_HASH_TABLE_SIZE = 16;
    // root->page_hash_table = #TODO initialize hash table
    root->stats = stats;
}

/*
DESCRIPTION
    Helper Function for Testing. Print PageHeaders Allocated Stat from the
    RootPageBufferStatistics.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 6/6/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
void printPageHeadersAllocated(RootPageBufferStatistics* stats) {
    assert(stats != NULL);
    printf("PageHeaders Allocated: %d\n", stats->page_headers_allocated);
}

/*
DESCRIPTION
    Unit Test for Appending PageHeader to an empty bucket. Ensure PageHeader
    is appended properly to an empty bucket and pointers are set as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/18/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testAppendPageHeaderToEmptyBucket(RootPageBuffer* root) {
    int expected_values[] = {10};

    uint8_t data = 10;
    PageHeader* page_header = allocatePageHeader(root->PAGE_SIZE, root->stats);

    initializePageHeader(page_header, 0x4080, root->PAGE_SIZE, &data);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);

    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 1 passed: Node appended to empty list\n");

    // #TODO properly free data

    return true;
}

/*
DESCRIPTION
    Unit Test for Appending PageHeaders. Ensure PageHeader is appended onto tail
    of a bucket.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testAppendPageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10};
    int expected_values[] = {10, 20};

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    appendPageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 2 passed: Node appended to the head\n");

    // #TODO properly free data

    return true;
}

/*
DESCRIPTION
    Unit Test for Prepending PageHeaders. Ensure PageHeaders are prepended onto
    head as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testPrependPageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10};
    int expected_values[] = {20, 10};

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    prependPageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 3 passed: Node prepended to the head\n");

    // #TODO properly free data

    return true;
}

/*
DESCRIPTION
    Unit Test for Inserting PageHeaders. Ensure PageHeaders are inserted into
    bucket as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testInsertPageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10};
    int expected_values[] = {10, 40, 20, 30};

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    uint8_t data2 = 30;
    PageHeader* page_header2 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header2, 0x4080, root->PAGE_SIZE, &data2);

    uint8_t data3 = 40;
    PageHeader* page_header3 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header3, 0x4080, root->PAGE_SIZE, &data3);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    insertPageHeader(&bucket.head, &bucket.tail, page_header0, page_header1, &bucket.current_page_count);
    insertPageHeader(&bucket.head, &bucket.tail, page_header1, page_header2, &bucket.current_page_count);
    insertPageHeader(&bucket.head, &bucket.tail, page_header0, page_header3, &bucket.current_page_count);

    assert(bucket.current_page_count == 4);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 4 passed: Nodes inserted into bucket\n");

    // #TODO properly free data

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a Head PageHeader. Ensure that deleting the head node
    from a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testDeleteHeadPageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10};

    uint8_t data = 10;
    PageHeader* page_header = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header, 0x4080, root->PAGE_SIZE, &data);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    removePageHeader(&bucket.head, &bucket.tail, page_header, &bucket.current_page_count);

    assert(bucket.current_page_count == 0);
    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);

    printf("Test 5 passed: Head node deleted\n");

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a Tail PageHeader. Ensure that deleting the tail
    PageHeader from a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/10/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testDeleteTailPageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10, 20};
    int expected_values[] = {10};

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);
    appendPageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    removePageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);

    assert(bucket.current_page_count == 1);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 6 passed: Tail node deleted\n");

    removePageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

    return true;
}

/*
DESCRIPTION
    Unit Test for Deleting a PageHeader in the middle of a bucket. Ensure that
    deleting the middle PageHeader from a bucket behaves as expected.

FUNCTION FIELDS
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/20/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testDeleteMiddlePageHeader(RootPageBuffer* root) {
    int expected_setup[] = {10, 20, 30};
    int expected_values[] = {10, 30};

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root->PAGE_SIZE, root->stats);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    uint8_t data2 = 30;
    PageHeader* page_header2 = allocatePageHeader(root->PAGE_SIZE, root->stats);

    initializePageHeader(page_header2, 0x4080, root->PAGE_SIZE, &data2);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    assert(bucket.head == NULL);
    assert(bucket.tail == NULL);
    assert(bucket.current_page_count == 0);

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);
    appendPageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);
    appendPageHeader(&bucket.head, &bucket.tail, page_header2, &bucket.current_page_count);

    assert(bucket.current_page_count == 3);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_setup);

    removePageHeader(&bucket.head, &bucket.tail, page_header1, &bucket.current_page_count);

    assert(bucket.current_page_count == 2);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 7 passed: Middle node deleted\n");

    // #TODO properly free data

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

    [RootPageBuffer*] root: Pointer to the mock root page buffer.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/13/2024

    Adapted to remove Node and directly use PageHeaders
    Aijun Hall, 7/18/2024
*/
bool testRandomBucketLength(int random_seed, RootPageBuffer* root) {
    // Expected Values from set random seed; Won't be applicable for other seeds.
    int expected_values[] = {
        99, 98, 97, 90, 87, 86, 85, 84, 82, 80, 79, 75, 74,  70, 67, 66, 64, 62,
        57, 54, 45, 43, 42, 36, 33, 29, 26, 25, 24, 23, 20, 15, 14, 13, 11, 10,
        9, 8, 4, 2, 1, 0, 95, 83, 81, 78, 73, 71, 69, 65, 58, 56, 55, 53, 52,
        49, 48, 44, 41, 40, 39, 38, 35, 31, 28, 27, 22, 19, 18, 12, 7, 6, 3, 5,
        16, 17, 21, 30, 32, 34, 37, 46, 47, 50, 51, 59, 60, 61, 63, 68, 72, 76,
        77, 88, 89, 91, 92, 93, 94, 96
    };

    srand(random_seed);

    uint8_t data0 = 10;
    PageHeader* page_header0 = allocatePageHeader(root->PAGE_SIZE, root->stats);

    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    PageBucket bucket;
    bucket.head = NULL;
    bucket.tail = NULL;
    bucket.current_page_count = 0;

    appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

    for (int i=1; i<100; i++) {
        printBucket(&bucket);
        int random = rand();

        uint8_t data = i;
        PageHeader* page_header = allocatePageHeader(root->PAGE_SIZE, root->stats);

        initializePageHeader(page_header, 0x4080, root->PAGE_SIZE, &data);

        if (random % 3 == 0) {
            insertPageHeader(&bucket.head, &bucket.tail, page_header0, page_header, &bucket.current_page_count);

        }
        else if (random % 2 == 0) {
            appendPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);

        } else {
            prependPageHeader(&bucket.head, &bucket.tail, page_header0, &bucket.current_page_count);
        }
    }

    assert(bucket.current_page_count == 100);

    printBucket(&bucket);
    walkAndAssertBucket(&bucket.head, &bucket.tail, &bucket.current_page_count, expected_values);

    printf("Test 8 passed: Monte Carlo Bucket Length Test\n");

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

    // #TODO this should initialize the root hash table as well- so bucket
    // DDL functions can point directly to the root and index the hash entry
    // needed instead of using the bucket head and tail pointer like right now.
    setupMockRootPageBuffer(&root, &stats);

    printf("\nRunning Page Bucket Tests...\n");

    const int RANDOM_SEED = 12345678;
    printf("\nTesting With Random Seed: %d\n", RANDOM_SEED);

    // Test 1: Append a node to an empty bucket.
    assert(testAppendPageHeaderToEmptyBucket(&root) == true);

    // Test 2: Append a node to a fresh bucket.
    assert(testAppendPageHeader(&root) == true);

    // Test 3: Prepend a node to a fresh bucket.
    assert(testPrependPageHeader(&root) == true);

    // Test 4: Generic Insert a node to a fresh bucket.
    assert(testInsertPageHeader(&root) == true);

    // Test 5: Delete head node from a fresh bucket of len 1.
    assert(testDeleteHeadPageHeader(&root) == true);

    // Test 6: Delete tail node from a fresh bucket of len 2.
    assert(testDeleteTailPageHeader(&root) == true);

    // Test 7: Delete middle node from a fresh bucket of len 3.
    assert(testDeleteMiddlePageHeader(&root) == true);

    // Test 8: Monte Carlo Testing for Bucket Length
    // assert(testRandomBucketLength(RANDOM_SEED, &root) == true);

    printf("Root Stats:\n");
    printPageHeadersAllocated(&stats);
}

/*
DESCRIPTION
    Unit Test for Initializing a New PageHeader. Ensure PageHeader is properly
    allocated, and fields are initialized as expected. Separate Test for
    freshly initializing a recycled PageHeader below.

FUNCTION FIELDS
    [RootPageBufferStatistics* root] Pointer to Root, so that we can access
    the set page size.

RETURN TYPE
    [bool] Return True if test passes

CHANGELOG
    First created
    Aijun Hall, 6/26/2024

    Renamed to testMallocAndInitNewPageHeader from testMallocAndInitPageHeader
    to better reflect this test's scope.
    Aijun Hall, 7/25/2024
*/
bool testMallocAndInitNewPageHeader(RootPageBuffer* root) {
    PageHeader* page_header = allocatePageHeader(root->PAGE_SIZE, root->stats);
    assert(page_header != NULL);

    // Allocate an array of len 10, with integers 1,2,3...10
    // Memcpy this into page data and verify
    int *expected_data_array = (int*)malloc(10*sizeof(int));

    for (int i=0;i<10;i++) {
        expected_data_array[i] = i + 1;
    }

    uint8_t *expected_page_header_data = (uint8_t*)malloc(10 * sizeof(uint8_t));

    memcpy(expected_page_header_data, expected_data_array, 10 * sizeof(uint8_t));

    int expected_hash_key = 4;
    int expected_page_offset_address = 0x4080;

    initializePageHeader(page_header, expected_page_offset_address, root->PAGE_SIZE, expected_page_header_data);

    assert(page_header->sanity_check_tag == PAGE_HEADER_SANITY_CHECK_TAG);
    assert(page_header->page_offset_address == expected_page_offset_address);
    assert(page_header->hash_key == expected_hash_key);

    assert(page_header->hash_next_ptr == NULL);
    assert(page_header->hash_prev_ptr == NULL);
    assert(page_header->rp_next_ptr == NULL);
    assert(page_header->rp_prev_ptr == NULL);

    assert(page_header->is_dirty == false);
    assert(page_header->is_busy == false);
    assert(page_header->is_read == false);
    assert(page_header->is_write == false);

    assert(page_header->data == expected_page_header_data);

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
    assert(testMallocAndInitNewPageHeader(&root) == true);
}

int main() {
    runPageBucketTests();
    runPageHeaderTests();
    return 0;
}