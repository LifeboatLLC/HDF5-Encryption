/*
RootPageBuffer.c
Root Structure of the Page Buffer
*/

#include "RootPageBuffer.h"

/*
DESCRIPTION
    Allocate memory for PageHeader

FUNCTION FIELDS
    [page_size]: Fixed page memory size
    [RootPageBuffer*] root: Pointer to the RootPageBuffer for stats

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/25/2024
*/
PageHeader* allocatePageHeader(RootPageBuffer* root) {
    PageHeader* new_page_header = (PageHeader*)malloc(root->PAGE_SIZE * sizeof(PageHeader));
    assert(new_page_header != NULL);

    new_page_header->data = (uint8_t*)malloc(root->PAGE_SIZE);
    assert(new_page_header->data != NULL);

    // ROOT STATISTICS
    (root->page_headers_allocated)++;

    return new_page_header;
}

/*
DESCRIPTION
    Hashkey function. Change a PageHeader address to a hashkey to be used in
    the PageHashTable.

    Operation Sequence:
    1. Clip off number of bits equal to the power of 2 in page size
    2. Right Shift by that number of bits
    3. Bit AND operation with size of hashtable - 1

FUNCTION FIELDS
    [int] page_offset_address: Page offset address to change into hashkey
    [int] page_size: Fixed page memory size

RETURN TYPE
    [int] hashkey: Calculated and converted hashkey

CHANGELOG
    First created
    Aijun Hall, 7/12/2024
*/
int calculatePageHeaderHashKey(int page_offset_address, int page_size) {
    // Calculate the number of bits to shift based on the page size
    // Assume page size is a power of 2)
    assert(page_size % 2 == 0);

    int bits_to_shift = 0;
    int temp_page_size = page_size;

    while (temp_page_size > 1) {
        temp_page_size >>= 1;
        bits_to_shift++;
    }

    // Clip off the bits, right shift, and perform the bit AND operation
    int shifted_address = page_offset_address >> bits_to_shift;
    int hashkey = shifted_address & (page_size - 1);

    return hashkey;
}

/*
DESCRIPTION
    Initialize Allocated PageHeader. Used for newly allocated PageHeaders, and
    recycled ones.

FUNCTION FIELDS
    [PageHeader*] target_page_header: PageHeader to be initialized

    [int] page_offset_address: Offset of the page address in the Buffer

    [int] page_size: Fixed memory size of page.

    [uint8_t*]: Pointer to the raw data of the page.

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/26/2024
*/
void initializePageHeader(PageHeader* target_page_header, int page_offset_address, int page_size, uint8_t* data) {
    assert(target_page_header != NULL);
    assert(data >= 0);

    target_page_header->sanity_check_tag = PAGE_HEADER_SANITY_CHECK_TAG;

    target_page_header->page_offset_address = page_offset_address;
    target_page_header->hash_key = calculatePageHeaderHashKey(page_offset_address, page_size);

    target_page_header->hash_next_ptr = NULL;
    target_page_header->hash_prev_ptr = NULL;
    target_page_header->rp_next_ptr = NULL;
    target_page_header->rp_prev_ptr = NULL;

    target_page_header->is_dirty = false;
    target_page_header->is_busy = false;
    target_page_header->is_read = false;
    target_page_header->is_write = false;

    target_page_header->data = data;
}

/*
DESCRIPTION
    Delete a PageHeader and free it to the heap.

FUNCTION FIELDS
    [PageHeader*] page_header: Pointer to the PageHeader to delete

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/26/2024
*/
void deletePageHeader(PageHeader* page_header, RootPageBuffer* root) {
    assert(page_header != NULL);

    page_header->sanity_check_tag = PAGE_HEADER_SANITY_CHECK_TAG_INVALID;

    // Free the allocated page data
    page_header->data = NULL;

    // ROOT STATISTICS
    (root->page_headers_deleted)++;

    free(page_header->data);
    free(page_header);
}

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
        assert(*current_page_count == 0);

        new_page_header->hash_prev_ptr = NULL;
        new_page_header->hash_next_ptr = NULL;

        *head = new_page_header;
        *tail = new_page_header;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        assert(*current_page_count > 0);

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
        assert(*current_page_count == 0);

        new_page_header->hash_prev_ptr = NULL;
        new_page_header->hash_next_ptr = NULL;

        *head = new_page_header;
        *tail = new_page_header;

        assert(*head != NULL);
        assert(*tail != NULL);

    } else {
        assert(*current_page_count > 0);

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
    assert(*current_page_count > 0);

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
    assert(*current_page_count > 0);

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
PageHashTableEntry** initializePageHashTable(int PAGE_HASH_TABLE_SIZE) {
    PageHashTableEntry** page_hash_table = (PageHashTableEntry**)malloc(PAGE_HASH_TABLE_SIZE * sizeof(PageHashTableEntry*));

    for (int i = 0; i < PAGE_HASH_TABLE_SIZE; i++) {
        page_hash_table[i] = (PageHashTableEntry*)malloc(sizeof(PageHashTableEntry));

        page_hash_table[i]->hash_key = 0;
        page_hash_table[i]->bucket = NULL;
    }

    return page_hash_table;
}

/*
DESCRIPTION
    Utility function to help setup the mock root page buffer for running the
    test suite.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

CHANGELOG
    First created
    Aijun Hall, 6/5/2024

    Made root and stats static so that stats are passed easier to tests
    Aijun Hall, 6/6/2024

*/
void setupMockRootPageBuffer(RootPageBuffer* root) {
    assert(root != NULL);

    root->sanity_check_tag = ROOT_PAGE_BUFFER_SANITY_CHECK_TAG;
    root->PAGE_SIZE = 4096;
    root->PAGE_HASH_TABLE_SIZE = 16;

    // STATS
    root->page_headers_allocated = 0;
    root->page_headers_deleted = 0;
}

/*
DESCRIPTION
    Utility function to reset RootPageBuffer stats. Resets all stats at once.

FUNCTION FIELDS
    N/A

RETURN TYPE
    [RootPageBuffer*] root: Pointer to the mock root page buffer.

CHANGELOG
    First created
    Aijun Hall, 7/29/2024
*/
void resetMockRootPageBufferStatistics(RootPageBuffer* root) {
    assert(root != NULL);

    root->page_headers_allocated = 0;
    root->page_headers_deleted = 0;
}

/*
DESCRIPTION
    Helper Function for Testing. Print PageHeaders Allocated Stat from the
    RootPageBuffer.

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
void printPageHeadersAllocated(RootPageBuffer* root) {
    assert(root != NULL);
    printf("PageHeaders Allocated: %d\n", root->page_headers_allocated);
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
    PageHeader* page_header = allocatePageHeader(root);

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
    PageHeader* page_header0 = allocatePageHeader(root);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root);
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
    PageHeader* page_header0 = allocatePageHeader(root);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root);
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
    PageHeader* page_header0 = allocatePageHeader(root);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    uint8_t data2 = 30;
    PageHeader* page_header2 = allocatePageHeader(root);
    initializePageHeader(page_header2, 0x4080, root->PAGE_SIZE, &data2);

    uint8_t data3 = 40;
    PageHeader* page_header3 = allocatePageHeader(root);
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
    PageHeader* page_header = allocatePageHeader(root);
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
    PageHeader* page_header0 = allocatePageHeader(root);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root);
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
    PageHeader* page_header0 = allocatePageHeader(root);
    initializePageHeader(page_header0, 0x4080, root->PAGE_SIZE, &data0);

    uint8_t data1 = 20;
    PageHeader* page_header1 = allocatePageHeader(root);
    initializePageHeader(page_header1, 0x4080, root->PAGE_SIZE, &data1);

    uint8_t data2 = 30;
    PageHeader* page_header2 = allocatePageHeader(root);

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
    PageHeader* page_header0 = allocatePageHeader(root);

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
        PageHeader* page_header = allocatePageHeader(root);

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
    RootPageBuffer root;

    // #TODO this should initialize the root hash table as well- so bucket
    // DDL functions can point directly to the root and index the hash entry
    // needed instead of using the bucket head and tail pointer like right now.
    setupMockRootPageBuffer(&root);

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
    printPageHeadersAllocated(&root);
}

/*
DESCRIPTION
    Unit Test for Initializing a New PageHeader. Ensure PageHeader is properly
    allocated, and fields are initialized as expected. Separate Test for
    freshly initializing a recycled PageHeader below.

FUNCTION FIELDS
    [RootPageBuffer* root] Pointer to Root, so that we can access
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
    PageHeader* page_header = allocatePageHeader(root);
    assert(page_header != NULL);

    int expected_hash_key = 5;
    int expected_page_offset_address = root->PAGE_SIZE * 5;

    initializePageHeader(page_header, expected_page_offset_address, root->PAGE_SIZE, "10");

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

    assert(page_header->data != NULL);

    printf("Test 1 passed: PageHeader created and initialized\n");
    return true;
}

/*
DESCRIPTION
    Unit Test for verifying PageHeader data field. Ensure data is written
    and read correctly using file operations

FUNCTION FIELDS
    N/A

RETURN TYPE
    [void]

CHANGELOG
    First created
    Aijun Hall, 7/28/2024
*/
bool testPageHeaderData (RootPageBuffer* root) {
    PageHeader* page_header = allocatePageHeader(root);
    assert(page_header != NULL);

    int expected_hash_key = 5;
    int expected_page_offset_address = root->PAGE_SIZE * 5;

    uint8_t data0;
    FILE *file1 = fopen("OnePageTestData.txt", "r");
    assert (file1 != NULL);
    fread(&data0, sizeof(uint8_t), 1, file1);
    fclose(file1);

    initializePageHeader(page_header, expected_page_offset_address, root->PAGE_SIZE, &data0);

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

    uint8_t data1;
    FILE *file2 = fopen("OnePageTestData.txt", "r");
    assert(file2 != NULL);
    fread(&data1, sizeof(uint8_t), 1, file2);
    fclose(file2);

    printf("\n%u\n", &data0);
    printf("\n%u\n", &data1);
    fflush(stdout);

    assert(page_header->data == &data1);

    printf("Test 2 passed: PageHeader data successfully created and read\n");
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
    RootPageBuffer root;
    setupMockRootPageBuffer(&root);

    printf("\nRunning Page Header Tests...\n");

    const int RANDOM_SEED = 12345678;
    printf("\nTesting With Random Seed: %d\n", RANDOM_SEED);

    // Test 1: Malloc and Init a fresh pageHeader
    assert(testMallocAndInitNewPageHeader(&root) == true);

    // Test 2: Malloc and Init a fresh pageHeader. Load and assert data from
    // ./OnePageTestData.txt
    assert(testPageHeaderData(&root) == true);
}

int main() {
    runPageBucketTests();
    runPageHeaderTests();
    return 0;
}