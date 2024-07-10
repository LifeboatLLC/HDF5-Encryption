/*
PageHeader.c
Root Structure of the Page Buffer
*/

#include "PageHeader.h"

/*
DESCRIPTION
    Allocate memory for PageHeader

FUNCTION FIELDS
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

    // ROOT STATISTICS
    (stats->page_headers_allocated)++;

    return new_page_header;
}

/*
DESCRIPTION
    Initialize Newly Allocated PageHeader

FUNCTION FIELDS
    [RootPageBufferStatistics*] stats: Pointer to the RootPageBuffer statistics

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/26/2024
*/
void initializePageHeader(PageHeader* target_page_header, int page_offset_address, uint8_t data) {
    assert(target_page_header != NULL);

    target_page_header->sanity_check_tag = PAGE_HEADER_SANITY_CHECK_TAG;

    target_page_header->page_offset_address = page_offset_address;
    target_page_header->hash_key = 0; // #TODO write out hashkey function

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
    Delete a PageHeader and free it from the heap.

FUNCTION FIELDS
    [PageHeader*] page_header: Pointer to the PageHeader to delete

RETURN TYPE
    [PageHeader*] new_page_header: Pointer to newly created Page Header

CHANGELOG
    First created
    Aijun Hall, 6/26/2024
*/
void deletePageHeader(PageHeader* page_header) {
    assert(page_header != NULL);

    page_header->sanity_check_tag = PAGE_HEADER_SANITY_CHECK_TAG_INVALID;
    free(page_header);
}