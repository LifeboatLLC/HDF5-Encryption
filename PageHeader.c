/*
PageHeader.c
Root Structure of the Page Buffer
*/

#include "PageHeader.h"

/*
DESCRIPTION
    Allocate memory for PageHeader

FUNCTION FIELDS
    [page_size]: Fixed page memory size
    [RootPageBufferStatistics*] stats: Pointer to the RootPageBuffer statistics

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/25/2024
*/
PageHeader* allocatePageHeader(int page_size, RootPageBufferStatistics* stats) {
    PageHeader* new_page_header = (PageHeader*)malloc(page_size * sizeof(PageHeader));
    assert(new_page_header != NULL);

    new_page_header->data = (uint8_t*)malloc(page_size);
    assert(new_page_header->data != NULL);

    // ROOT STATISTICS
    (stats->page_headers_allocated)++;

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
    [RootPageBufferStatistics*] stats: Pointer to the RootPageBuffer statistics

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
void deletePageHeader(PageHeader* page_header, RootPageBufferStatistics* stats) {
    assert(page_header != NULL);

    page_header->sanity_check_tag = PAGE_HEADER_SANITY_CHECK_TAG_INVALID;

    // Free the allocated page data
    page_header->data = NULL;

    // ROOT STATISTICS
    (stats->page_headers_deleted)++;

    free(page_header->data);
    free(page_header);
}