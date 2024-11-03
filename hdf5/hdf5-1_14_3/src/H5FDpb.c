/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by Lifeboat, LLC                                                *
 * All rights reserved.                                                      *
 *                                                                           * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:     The page buffer VFD converts random I/O requests to page 
 *		aligned I/O requests, which it then passes down to the 
 *		underlying VFD.
 */

/* This source code file is part of the H5FD driver module */
#include "H5FDdrvr_module.h"

#include "H5private.h"    /* Generic Functions        */
#include "H5Eprivate.h"   /* Error handling           */
#include "H5Fprivate.h"   /* File access              */
#include "H5FDprivate.h"  /* File drivers             */
#include "H5FDpb.h"       /* Page Buffer file driver  */
#include "H5FLprivate.h"  /* Free Lists               */
#include "H5Iprivate.h"   /* IDs                      */
#include "H5MMprivate.h"  /* Memory management        */
#include "H5Pprivate.h"   /* Property lists           */

/** Semi-unique constant used to help identify Page Header structure pointers */
#define H5FD_PB_PAGEHEADER_MAGIC                0x504202

/** Semi-unique constant used to help identify Hash Table structure pointers */
#define H5FD_PB_HASH_TABLE_MAGIC                0x504203

/** Semi-unique constant used to help identify Replacement Policy structure pointers */
#define H5FD_PB_RP_MAGIC                        0x504204

/** The version of the H5FD_pb_vfd_config_t structure used */
#define H5FD_CURR_PB_VFD_CONFIG_VERSION         1

/** The default page buffer page size in bytes */
#define H5FD_PB_DEFAULT_PAGE_SIZE               4096

/** The default maximum number of pages resident in the page buffer at any one time */
#define H5FD_PB_DEFAULT_MAX_NUM_PAGES           64

/** The default replacement policy to be used by the page buffer */
#define H5FD_PB_DEFAULT_REPLACEMENT_POLICY      0 /* 0 = Least Recently Used */

/** The default number of buckets in the hash table */
#define H5FD_PB_DEFAULT_NUM_HASH_BUCKETS        16

/******************************************************************************
 *Bitfield for the PageHeader flags
 *
 * A bitfield used to indicate the status of the H5FD_pb_pageheader_t structure
 * (defined in the next structure) and the page it contains. 
 *
 * DIRTY_FLAG
 *      Indicates the page inside the H5FD_pb_pageheader_t structure has been
 *      modified and is more current than the version of the page in the file.
 *
 * BUSY_FLAG
 *      Indicates the H5FD_pb_pageheader_t structure is currently being used, 
 *      either being read from or written to, or is about to be read from or 
 *      written to.
 *
 * READ_FLAG
 *      Indicates the H5FD_pb_pageheader_t structure is currently being read 
 *      from.
 *
 * WRITE_FLAG 
 *      Indicates the H5FD_pb_pageheader_t structure is currently being written 
 *      to.
 *
 * INVALID_FLAG
 *      Indicates the page inside the H5FD_pb_pageheader_t structure has been 
 *      flagged as invalid. An invalid page means that a middle write has been
 *      done on that page directly to the file making the version in the page
 *      buffer out of date. 
 *      If flagged as invalid the H5FD_pb_pageheader_t structure is removed 
 *      from the hash table to not show up in searches and is adjusted in the
 *      replacement policy list to be the next H5FD_pb_pageheader_t to be 
 *      evicted.
 ******************************************************************************
 */
#define H5FD_PB_DIRTY_FLAG      0X0001    /* 0b00000001 */
#define H5FD_PB_BUSY_FLAG       0x0002    /* 0b00000010 */
#define H5FD_PB_READ_FLAG       0x0004    /* 0b00000100 */
#define H5FD_PB_WRITE_FLAG      0x0008    /* 0b00001000 */
#define H5FD_PB_INVALID_FLAG    0x0010    /* 0b00010000 */


/* The driver identification number, initialized at runtime */
static hid_t H5FD_PB_g = 0;


/******************************************************************************
 *
 * Structure:   H5FD_pb_pageheader_t
 *
 * Description:
 *
 * The H5FD_pb_pageheader_t structure is used to store a page from the file in
 * the page buffer, and details about the page.
 * 
 * The hash table and replacement policy (these two structures are in the 
 * H5FD_pb_t structure) use this structure as nodes to track the pages in the 
 * page buffer and the order of pages to be evicted.
 *
 * Fields:
 *
 * magic (int32_t):
 *      Magic number to identify this struct. Must be H5FD_PB_PAGE_HEADER_MAGIC
 *
 * hash_code (uint32_t):
 *      Key used to determine which bucket in the hash table this instance of
 *      H5FD_pb_pageheader_t is stored in. The method of calculating the hash
 *      code is described in the H5FD__pb_calc_hash_code() function's header
 *      comment.
 *
 * ht_next_ptr (H5FD_pb_pageheader_t *):
 *      Pointer to the next H5FD_pb_pageheader_t structure in the hash table 
 *      bucket, or NULL if this instance of the strucure is the tail.
 *
 * ht_prev_ptr (H5FD_pb_pageheader_t *):
 *      Pointer to the previous H5FD_pb_pageheader_t structure in the hash 
 *      table bucket, or NULL if this instance of the strucutre is the head.
 *
 * rp_next_ptr (H5FD_pb_pageheader_t *):
 *      Pointer to the next H5FD_pb_pageheader_t structure in the replacement 
 *      policy list, or NULL if this instance of the structure is the tail 
 *      (i.e. this instance is the next one to be evicted).
 *
 * rp_prev_ptr (H5FD_pb_pageheader_t *):
 *      Pointer to the previous H5FD_pb_pageheader_t structure in the 
 *      replacement policy list, or NULL if this instance is the head 
 *      (i.e. this instance is the most recently added to the list).
 *
 * flags (int32_t):
 *      Integer field used to store various flags that indicate the state of
 *      the page header. The flags are stored as bits in the integer field.
 *      The flags are as follows:
 *          - 0b00000001: dirty     (modified since last write)
 *          - 0b00000010: busy      (currently being read/written)
 *          - 0b00000100: read      (queued up to be read)
 *          - 0b00001000: write     (queued up to be written)
 *          - 0b00010000: invalid   (contains old data, page must be discarded)
 *
 * page_addr (haddr_t):
 *      Integer value indicating the addr of the page (also can be thought of 
 *      as the offset from the beginning of the file in bytes). This is the 
 *      location of the page in the file, and is used to identify the page and
 *      calculate the hash key.
 *
 * type (H5FD_mem_t):
 *      Type of memory in the page.  This is the type associated with the 
 *      I/O request that occasioned the load of the page into the page 
 *      buffer.
 *
 * page (unsigned char):
 *      buffer containing the actual data of the page. This is the data that
 *      is read from the file and stored in the page buffer.
 *
 ******************************************************************************
 */
typedef struct H5FD_pb_pageheader_t {
    int32_t             magic;
    uint32_t            hash_code;
    struct H5FD_pb_pageheader_t * ht_next_ptr;
    struct H5FD_pb_pageheader_t * ht_prev_ptr;
    struct H5FD_pb_pageheader_t * rp_next_ptr;
    struct H5FD_pb_pageheader_t * rp_prev_ptr;
    int32_t             flags;
    haddr_t             page_addr;
    H5FD_mem_t          type;
    unsigned char       page[];
} H5FD_pb_pageheader_t;


/******************************************************************************
 *
 * Structure:   H5FD_pb_t
 *
 * Description:
 *
 * Root structure used to store all information required to manage the page
 * buffer. An instance of this strucutre is created when the file is "opened"
 * and is discarded when the file is "closed".
 *
 * Fields:
 *
 * pub:	An instance of H5FD_t which contains fields common to all VFDs.
 *      It must be the first item in this structure, since at higher levels,
 *      this structure will be treated as an instance of H5FD_t.
 *
 * magic (int32_t):
 *      Magic number to identify this struct. Must be H5FD_PB_MAGIC.
 *
 * fa:  An instance of H5FD_pb_vfd_config_t containing all configuration data 
 *      needed to setup and run the page buffer.  This data is contained in 
 *      an instance of H5FD_pb_vfd_config_t for convenience in the get and 
 *      set FAPL calls.
 *
 * file: Pointer to the instance of H5FD_t used to manage the underlying 
 *	VFD.  Note that this VFD may or may not be terminal (i.e. perform 
 *	actual I/O on a file).
 *
 * Hash Table Description:
 *      The hash table indexes the valid pages that currently reside in the 
 *      page buffer for quick retrieval. The hash table uses a simple hash 
 *      function (described in the H5FD__pb_calc_hash_code() function's header
 *      comment) to determine which bucket in the hash table a 
 *      H5FD_pb_pageheader_t instance should be stored in.
 * 
 *      The number of buckets must be a power of 2.
 *
 *      NOTE: The number of buckets in the hash table is currently fixed, but
 *      will be made configurable in future versions.
 *
 * Hast Table Fields:
 *      index:
 *          An integer value to signify which bucket in the hash table we are
 *          currently looking at.
 * 
 *      num_pages_in_bucket:
 *          An integer value to track of the number of pages that are stored in
 *          the bucket. This is a statistic used for debugging andperformance 
 *          analysis.
 *
 *      ht_head_ptr:
 *          Pointer to the head of the doubly linked list of page headers in
 *          the bucket. 
 * 
 *      NOTE: the tail pointers for the hash table buckets are not needed, so 
 *            they are not included in the structure.
 * 
 *
 * Replacement Policy Description:
 *      A doubly linked list data structure used to track all 
 *      H5FD_pb_pageheader_t instances and determine eviction order based on
 *      the replacement policy used.
 * 
 *      NOTE: Currenlty LRU (least recently used) and FIFO (first-in first-out) 
 *            are the replacement policies implemented.
 *
 * Replacement Policy Fields:
 *      rp_policy:
 *          An integer value used to determine which replacement policy will be
 *          used. 
 *          0 = LRU 
 *          1 = FIFO 
 *
 *      rp_head_ptr:
 *          Pointer to the replacement policy list's head of 
 *          H5FD_pb_pageheader_t structures. The head is the location in the 
 *          list where the structures are added to.
 *
 *      rp_tail_ptr:
 *          Pointer to the replacement policy list's tail of 
 *          H5FD_pb_pageheader_t structures. The tail is the location in the 
 *          lsit where the structures are selected to be evicted.
 * 
 * 
 * EOA management:
 *
 * The page buffer VFD introduces an issue with respect to EOA management.
 *
 * Specifically, the page buffer converts random I/O to paged I/O.  As 
 * a result, when it receives a set EOA directive, it must extend the 
 * supplied EOA to the next page boundary lest the write of data in the
 * final page in the file fail.
 *
 * Similarly, when the current EOA is requested, the page buffer must 
 * return the most recent EOA set from above, not the EOA returned by 
 * the underlying VFD.
 *
 * Righly or wrongly, we make no attempt to adjust the reported EOF.
 * This may result in waste space in files.  If this becomes excessive, 
 * we will have to re-visit this issue.
 *
 * eoa_up: The current EOA as seen by the VFD directly above the encryption
 *      VFD in the VFD stack.  This value is set to zero at file open time,
 *      and retains that value until the first set eoa call.
 *
 * eoa_down: The current EOA as seen by the VFD directly below the encryption
 *      VFD in the VFD stack. This field is set to eoa_up extended to the 
 *      next page boundary.  As with eoa_up, this alue is set to zero at 
 *      file open time, and retains that value until the first set eoa call.
 *
 * 
 * Page Buffer Statistics Fields:
 *
 * num_pages (size_t):
 *      The total number of pages that were stored in the page buffer over the
 *      session. This is a statistic used for debugging and performance analysis.
 *
 * largest_num_in_bucket (size_t):
 *      The largest number of pages that were stored in a single bucket in the
 *      hash table over the session. This is a statistic used for debugging
 *      and performance analysis.
 *
 * num_hits (size_t):
 *      The number of times a page was found in the hash table and did not
 *      require a read from the file. This is a statistic used for debugging
 *      and performance analysis.
 *
 * num_misses (size_t):
 *      The number of times a page was not found in the hash table and required
 *      a read from the file. This is a statistic used for debugging and
 *      performance analysis.
 *
 * max_search_depth (size_t):
 *      The maximum depth that was searched in the hash table and return a hit.
 *      This is a statistic used for debugging and performance analysis.
 *
 * total_success_depth (size_t):
 *      A commulative sum of the depths that were searched in the hash table
 *      and returned a hit. This is a statistic used for debugging and
 *      performance analysis.
 *
 * total_fail_depth (size_t):
 *      A commulative sum of the depths that were searched in the hash table
 *      and returned a miss. This is a statistic used for debugging and
 *      performance analysis.
 *
 * total_evictions (size_t):
 *      The total number of times a page was evicted from the page buffer. 
 *      This is a statistic used for debugging and performance analysis.
 *
 * total_dirty (size_t):
 *      The total number of pages that were marked as dirty. This is a
 *      statistic used for debugging and performance analysis.
 *
 * total_invalidated (size_t):
 *      The total number of pages that were marked as invalid. This is a
 *      statistic used for debugging and performance analysis.
 *
 * total_flushed (size_t):
 *      The total number of pages that were flushed to the file. This is a
 *      statistic used for debugging and performance analysis.
 ******************************************************************************
 */

typedef struct H5FD_pb_t {

    H5FD_t               pub;

    int32_t              magic;
    H5FD_pb_vfd_config_t fa;
    H5FD_t              *file;

    /* hash table fields */
    struct {
        int32_t               index;
        int32_t               num_pages_in_bucket;
        H5FD_pb_pageheader_t *ht_head_ptr;
    } ht_bucket[H5FD_PB_DEFAULT_NUM_HASH_BUCKETS];

    /* replacement policy fields */
    int32_t                   rp_policy;
    H5FD_pb_pageheader_t     *rp_head_ptr;
    H5FD_pb_pageheader_t     *rp_tail_ptr;
    int64_t                   rp_pageheader_count;
    int64_t                   rp_dirty_count;


    /* eoa management fields */
    haddr_t                  eoa_up;
    haddr_t                  eoa_down;

    /* page buffer statistics fields */
    /* Side comments are where these get updated */
    size_t                    num_pages;            /* get_pageheader */
    size_t                    num_heads;            /* ht_insert_pageheader */
    size_t                    num_tails;            /* ht_insert_pageheader */
    size_t                    largest_num_in_bucket; /* ht_insert_pageheader */
    size_t                    num_hits;             /* ht_search_pageheader */
    size_t                    num_misses;           /* ht_search_pageheader */
    size_t                    total_middle_reads;   /* pb_read_pageheader */
    size_t                    total_middle_writes;  /* pb_write_pageheader */
    size_t                    max_search_depth;     /* ht_search_pageheader */
    size_t                    total_success_depth;  /* ht_search_pageheader */
    size_t                    total_fail_depth;     /* ht_search_pageheader */
    size_t                    total_evictions;      /* rp_evict_pageheader */
    size_t                    total_dirty;          /* pb_write */
    size_t                    total_invalidated;    /* pb_invalidate_pageheader */
    size_t                    total_flushed;        /* rp_evict_pageheader */

} H5FD_pb_t;

/*
 * These macros check for overflow of various quantities.  These macros
 * assume that HDoff_t is signed and haddr_t and size_t are unsigned.
 *
 * ADDR_OVERFLOW:   Checks whether a file address of type `haddr_t'
 *                  is too large to be represented by the second argument
 *                  of the file seek function.
 *
 * SIZE_OVERFLOW:   Checks whether a buffer size of type `hsize_t' is too
 *                  large to be represented by the `size_t' type.
 *
 * REGION_OVERFLOW: Checks whether an address and size pair describe data
 *                  which can be addressed entirely by the second
 *                  argument of the file seek function.
 */
#define MAXADDR          (((haddr_t)1 << (8 * sizeof(HDoff_t) - 1)) - 1)
#define ADDR_OVERFLOW(A) (HADDR_UNDEF == (A) || ((A) & ~(haddr_t)MAXADDR))
#define SIZE_OVERFLOW(Z) ((Z) & ~(hsize_t)MAXADDR)
#define REGION_OVERFLOW(A, Z)                                                                                \
    (ADDR_OVERFLOW(A) || SIZE_OVERFLOW(Z) || HADDR_UNDEF == (A) + (Z) || (HDoff_t)((A) + (Z)) < (HDoff_t)(A))


#define H5FD_PB_DEBUG_OP_CALLS 0 /* debugging print toggle; 0 disables */

#if H5FD_PB_DEBUG_OP_CALLS
#define H5FD_PB_LOG_CALL(name)                                                                         \
    do {                                                                                                     \
        printf("called %s()\n", (name));                                                                     \
        fflush(stdout);                                                                                      \
    } while (0)
#else
#define H5FD_PB_LOG_CALL(name) /* no-op */       
#endif                               /* H5FD_PB_DEBUG_OP_CALLS */
    

/* Private functions */

/* Prototypes */
static herr_t  H5FD__pb_term(void);
static herr_t  H5FD__pb_populate_config(H5FD_pb_vfd_config_t *vfd_config,
                                              H5FD_pb_vfd_config_t       *fapl_out);
static hsize_t H5FD__pb_sb_size(H5FD_t *_file);
static herr_t  H5FD__pb_sb_encode(H5FD_t *_file, char *name /*out*/, unsigned char *buf /*out*/);
static herr_t  H5FD__pb_sb_decode(H5FD_t *_file, const char *name, const unsigned char *buf);
static void   *H5FD__pb_fapl_get(H5FD_t *_file);
static void   *H5FD__pb_fapl_copy(const void *_old_fa);
static herr_t  H5FD__pb_fapl_free(void *_fapl);
static H5FD_t *H5FD__pb_open(const char *name, unsigned flags, hid_t fapl_id, haddr_t maxaddr);
static herr_t  H5FD__pb_close(H5FD_t *_file);
static int     H5FD__pb_cmp(const H5FD_t *_f1, const H5FD_t *_f2);
static herr_t  H5FD__pb_query(const H5FD_t *_file, unsigned long *flags /* out */);
static herr_t  H5FD__pb_get_type_map(const H5FD_t *_file, H5FD_mem_t *type_map);
#if 0
/* The current implementations of the alloc and free callbacks
 * cause space allocation failures in the upper library.  This 
 * has to be dealt with if we want the page buffer and encryption
 * to run with VFDs that require it (split, multi).
 * However, since targeting just the sec2 VFD is sufficient 
 * for the Phase I prototype, we will bypass the issue
 * for now.
 *                                JRM -- 9/6/24
 */
static haddr_t H5FD__pb_alloc(H5FD_t *file, H5FD_mem_t type, hid_t H5_ATTR_UNUSED dxpl_id, hsize_t size);
static herr_t  H5FD__pb_free(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, hsize_t size);
#endif
static haddr_t H5FD__pb_get_eoa(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type);
static herr_t  H5FD__pb_set_eoa(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, haddr_t addr);
static haddr_t H5FD__pb_get_eof(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type);
static herr_t  H5FD__pb_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, void **file_handle);
static herr_t  H5FD__pb_read(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
                                   void *buf);
static herr_t  H5FD__pb_write(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
                                    const void *buf);
static herr_t  H5FD__pb_flush(H5FD_t *_file, hid_t dxpl_id, bool closing);
static herr_t  H5FD__pb_truncate(H5FD_t *_file, hid_t dxpl_id, bool closing);
static herr_t  H5FD__pb_lock(H5FD_t *_file, bool rw);
static herr_t  H5FD__pb_unlock(H5FD_t *_file);
static herr_t  H5FD__pb_delete(const char *filename, hid_t fapl_id);
static herr_t  H5FD__pb_ctl(H5FD_t *_file, uint64_t op_code, uint64_t flags, const void *input,
                                  void **output);

static const H5FD_class_t H5FD_pb_g = {
    H5FD_CLASS_VERSION,           /* struct version       */
    H5FD_PB_VALUE,                /* value                */
    "page buffer",                /* name                 */
    MAXADDR,                      /* maxaddr              */
    H5F_CLOSE_WEAK,               /* fc_degree            */
    H5FD__pb_term,                /* terminate            */
    H5FD__pb_sb_size,             /* sb_size              */
    H5FD__pb_sb_encode,           /* sb_encode            */
    H5FD__pb_sb_decode,           /* sb_decode            */
    sizeof(H5FD_pb_vfd_config_t), /* fapl_size            */
    H5FD__pb_fapl_get,            /* fapl_get             */
    H5FD__pb_fapl_copy,           /* fapl_copy            */
    H5FD__pb_fapl_free,           /* fapl_free            */
    0,                            /* dxpl_size            */
    NULL,                         /* dxpl_copy            */
    NULL,                         /* dxpl_free            */
    H5FD__pb_open,                /* open                 */
    H5FD__pb_close,               /* close                */
    H5FD__pb_cmp,                 /* cmp                  */
    H5FD__pb_query,               /* query                */
    H5FD__pb_get_type_map,        /* get_type_map         */
#if 0
    /* The current implementations of the alloc and free callbacks
     * cause space allocation failures in the upper library.  This 
     * has to be dealt with if we want the page buffer and encryption
     * to run with VFDs that require it (split, multi).
     * However, since targeting just the sec2 VFD is sufficient 
     * for the Phase I prototype, we will bypass the issue
     * for now.
     *                                JRM -- 9/6/24
     */
    H5FD__pb_alloc,               /* alloc                */
    H5FD__pb_free,                /* free                 */
#else
    NULL,                         /* alloc                */
    NULL,                         /* free                 */
#endif
    H5FD__pb_get_eoa,             /* get_eoa              */
    H5FD__pb_set_eoa,             /* set_eoa              */
    H5FD__pb_get_eof,             /* get_eof              */
    H5FD__pb_get_handle,          /* get_handle           */
    H5FD__pb_read,                /* read                 */
    H5FD__pb_write,               /* write                */
    NULL,                         /* read_vector          */
    NULL,                         /* write_vector         */
    NULL,                         /* read_selection       */
    NULL,                         /* write_selection      */
    H5FD__pb_flush,               /* flush                */
    H5FD__pb_truncate,            /* truncate             */
    H5FD__pb_lock,                /* lock                 */
    H5FD__pb_unlock,              /* unlock               */
    H5FD__pb_delete,              /* del                  */
    H5FD__pb_ctl,                 /* ctl                  */
    H5FD_FLMAP_DICHOTOMY          /* fl_map               */
};


herr_t   H5FD__pb_flush_page(H5FD_pb_t * file_ptr, hid_t dxpl_id, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_invalidate_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
H5FD_pb_pageheader_t * H5FD__pb_alloc_and_init_pageheader(H5FD_pb_t * file_ptr, haddr_t addr, 
                                                          uint32_t hash_code);
H5FD_pb_pageheader_t* H5FD__pb_get_pageheader(H5FD_pb_t *file_ptr, H5FD_mem_t type, hid_t dxpl_id, 
                                              haddr_t addr, uint32_t hash_code);
uint32_t H5FD__pb_calc_hash_code(H5FD_pb_t *file_ptr, haddr_t addr);

/* Hash Table Functions */
herr_t   H5FD__pb_ht_insert_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_ht_remove_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
H5FD_pb_pageheader_t *H5FD__pb_ht_search_pageheader(H5FD_pb_t *file_ptr, haddr_t addr, uint32_t hash_code);

/* Replacement Policy Functions */
herr_t   H5FD__pb_rp_insert_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_rp_prepend_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_rp_append_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_rp_remove_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
herr_t   H5FD__pb_rp_touch_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader);
H5FD_pb_pageheader_t *H5FD__pb_rp_evict_pageheader(H5FD_pb_t *file_ptr, haddr_t addr, uint32_t hash_code);

/* Testing Functions */
haddr_t  *H5FD__pb_rp_eviction_check(H5FD_t *_file, haddr_t *current_rp_addrs);


/* Declare a free list to manage the H5FD_pb_t struct */
H5FL_DEFINE_STATIC(H5FD_pb_t);

/* Declare a free list to manage the H5FD_pb_vfd_config_t struct */
H5FL_DEFINE_STATIC(H5FD_pb_vfd_config_t);

/*-------------------------------------------------------------------------
 * Function:    H5FD_pb_init
 *
 * Purpose:     Initialize the page buffer driver by registering it with 
 *		        the library.
 *
 * Return:      Success:    The driver ID for the page buffer driver.
 *              Failure:    Negative
 *-------------------------------------------------------------------------
 */
hid_t
H5FD_pb_init(void)
{
    hid_t ret_value = H5I_INVALID_HID;

    FUNC_ENTER_NOAPI_NOERR

    H5FD_PB_LOG_CALL(__func__);

    if (H5I_VFL != H5I_get_type(H5FD_PB_g))
        H5FD_PB_g = H5FDregister(&H5FD_pb_g);

    ret_value = H5FD_PB_g;

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD_pb_init() */

/*---------------------------------------------------------------------------
 * Function:    H5FD__pb_term
 *
 * Purpose:     Shut down the page buffer VFD.
 *
 * Returns:     SUCCEED (Can't fail)
 *---------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_term(void)
{
    FUNC_ENTER_PACKAGE_NOERR

    H5FD_PB_LOG_CALL(__func__);

    /* Reset VFL ID */
    H5FD_PB_g = 0;

    FUNC_LEAVE_NOAPI(SUCCEED)

} /* end H5FD__pb_term() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__copy_plist
 *
 * Purpose:     Sanity-wrapped H5P_copy_plist() for each channel.
 *              Utility function for operation in multiple locations.
 *
 * Return:      0 on success, -1 on error.
 *-------------------------------------------------------------------------
 */
static int
H5FD__copy_plist(hid_t fapl_id, hid_t *id_out_ptr)
{
    int             ret_value = 0;
    H5P_genplist_t *plist_ptr = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert(id_out_ptr != NULL);

    if (false == H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, -1, "not a file access property list");

    plist_ptr = (H5P_genplist_t *)H5I_object(fapl_id);

    if (NULL == plist_ptr)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, -1, "unable to get property list");

    *id_out_ptr = H5P_copy_plist(plist_ptr, false);

    if (H5I_INVALID_HID == *id_out_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_BADTYPE, -1, "unable to copy file access property list");

done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__copy_plist() */

/*-------------------------------------------------------------------------
 * Function:    H5Pset_fapl_pb
 *
 * Purpose:     Sets the file access property list to use the
 *              page buffer driver.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
herr_t
H5Pset_fapl_pb(hid_t fapl_id, H5FD_pb_vfd_config_t *vfd_config)
{
    H5FD_pb_vfd_config_t *info      = NULL;
    H5P_genplist_t       *plist_ptr = NULL;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i*!", fapl_id, vfd_config);

    H5FD_PB_LOG_CALL(__func__);

    if (H5FD_PB_CONFIG_MAGIC != vfd_config->magic)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid configuration (magic number mismatch)");

    if (H5FD_CURR_PB_VFD_CONFIG_VERSION != vfd_config->version)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid config (version number mismatch)");

    if (NULL == (plist_ptr = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a valid property list");

    info = H5FL_CALLOC(H5FD_pb_vfd_config_t);

    if (NULL == info)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, "unable to allocate file access property list struct");

    if (H5FD__pb_populate_config(vfd_config, info) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "can't setup driver configuration");

    ret_value = H5P_set_driver(plist_ptr, H5FD_PB, info, NULL);

done:
    if (info)
        info = H5FL_FREE(H5FD_pb_vfd_config_t, info);

    FUNC_LEAVE_API(ret_value)

} /* end H5Pset_fapl_pb() */

/*-------------------------------------------------------------------------
 * Function:    H5Pget_fapl_pb
 *
 * Purpose:     Returns information about the page buffer file access 
 *              property list through the instance of H5FD_pb_vfd_config_t
 *              pointed to by config.
 *
 *              Will fail if *config is received without pre-set valid
 *              magic and version information.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
herr_t
H5Pget_fapl_pb(hid_t fapl_id, H5FD_pb_vfd_config_t *config /*out*/)
{
    const H5FD_pb_vfd_config_t *fapl_ptr     = NULL;
    H5FD_pb_vfd_config_t       *default_fapl = NULL;
    H5P_genplist_t             *plist_ptr    = NULL;
    herr_t                      ret_value    = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ix", fapl_id, config);

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    if (true != H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access property list");

    if (config == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "config pointer is null");

    if (H5FD_PB_CONFIG_MAGIC != config->magic)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "info-out pointer invalid (magic number mismatch)");

    if (H5FD_CURR_PB_VFD_CONFIG_VERSION != config->version)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "info-out pointer invalid (version unsafe)");

    /* Pre-set out FAPL ID with intent to replace this value */
    config->fapl_id = H5I_INVALID_HID;

    /* Check and get the page buffer fapl */
    if (NULL == (plist_ptr = H5P_object_verify(fapl_id, H5P_FILE_ACCESS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access property list");

    if (H5FD_PB != H5P_peek_driver(plist_ptr))
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "incorrect VFL driver");

    fapl_ptr = (const H5FD_pb_vfd_config_t *)H5P_peek_driver_info(plist_ptr);

    if (NULL == fapl_ptr) {

        if (NULL == (default_fapl = H5FL_CALLOC(H5FD_pb_vfd_config_t)))
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, "unable to allocate file access property list struct");

        if (H5FD__pb_populate_config(NULL, default_fapl) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "can't initialize driver configuration info");

        fapl_ptr = default_fapl;
    }

    /* Copy scalar data */
    config->page_size     = fapl_ptr->page_size;
    config->max_num_pages = fapl_ptr->max_num_pages;
    config->rp            = fapl_ptr->rp;

    /* Copy FAPL */
    if (H5FD__copy_plist(fapl_ptr->fapl_id, &(config->fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, FAIL, "can't copy underlying FAPL");

done:
    if (default_fapl)
        H5FL_FREE(H5FD_pb_vfd_config_t, default_fapl);

    FUNC_LEAVE_API(ret_value)

} /* end H5Pget_fapl_pb() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_populate_config
 *
 * Purpose:    Populates a H5FD_pb_vfd_config_t structure with the provided
 *             values, supplying defaults where values are not provided.
 *
 * Return:     Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_populate_config(H5FD_pb_vfd_config_t *vfd_config, H5FD_pb_vfd_config_t *fapl_out)
{
    H5P_genplist_t *def_plist;
    H5P_genplist_t *plist;
    bool            free_config = false;
    herr_t          ret_value   = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(fapl_out);

    assert( ( NULL == vfd_config ) || ( H5FD_PB_CONFIG_MAGIC == vfd_config->magic ) );
    assert( ( NULL == vfd_config ) || ( H5FD_CURR_PB_VFD_CONFIG_VERSION == vfd_config->version ) );

    memset(fapl_out, 0, sizeof(H5FD_pb_vfd_config_t));

    if ( NULL == vfd_config ) {

        vfd_config = H5MM_calloc(sizeof(H5FD_pb_vfd_config_t));

        if (NULL == vfd_config)
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, "unable to allocate file access property list struct");

        vfd_config->magic         = H5FD_PB_CONFIG_MAGIC;
        vfd_config->version       = H5FD_CURR_PB_VFD_CONFIG_VERSION;
        vfd_config->page_size     = H5FD_PB_DEFAULT_PAGE_SIZE;
        vfd_config->max_num_pages = H5FD_PB_DEFAULT_MAX_NUM_PAGES;
        vfd_config->rp            = H5FD_PB_DEFAULT_REPLACEMENT_POLICY;
        vfd_config->fapl_id       = H5P_DEFAULT;

        free_config = true;
    }

    fapl_out->magic         = vfd_config->magic;
    fapl_out->version       = vfd_config->version;
    fapl_out->page_size     = vfd_config->page_size;
    fapl_out->max_num_pages = vfd_config->max_num_pages;
    fapl_out->rp            = vfd_config->rp;
    fapl_out->fapl_id       = H5P_FILE_ACCESS_DEFAULT; /* pre-set value */

    if (NULL == (def_plist = (H5P_genplist_t *)H5I_object(H5P_FILE_ACCESS_DEFAULT)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access property list");

    /* Set non-default underlying FAPL ID in page buffer configuration info */
    if (H5P_DEFAULT != vfd_config->fapl_id) {

        if (false == H5P_isa_class(vfd_config->fapl_id, H5P_FILE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access list");

        fapl_out->fapl_id = vfd_config->fapl_id;

    } else {

        /* Use copy of default file access property list for underlying FAPL ID.
         * The Sec2 driver is explicitly set on the FAPL ID, as the default
         * driver might have been replaced with the page buffer VFD, which
         * would cause recursion.
         */
        if ((fapl_out->fapl_id = H5P_copy_plist(def_plist, false)) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTCOPY, FAIL, "can't copy property list");

        if (NULL == (plist = (H5P_genplist_t *)H5I_object(fapl_out->fapl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access property list");

        if (H5P_set_driver_by_value(plist, H5_VFD_SEC2, NULL, true) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "can't set default driver on underlying FAPL");
    }

done:
    if ( free_config && vfd_config ) {

        H5MM_free(vfd_config);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_populate_config() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_flush
 *
 * Purpose:     Flushes all data from the page buffer, and then flush
 *		        the underlying VFD.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_flush(H5FD_t *_file, hid_t H5_ATTR_UNUSED dxpl_id, bool closing)
{
    int64_t                pages_visited = 0;
    H5FD_pb_pageheader_t * pageheader = NULL;
    H5FD_pb_t            * file_ptr = (H5FD_pb_t *)_file;
    herr_t                 ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* flush the page buffer */

    pageheader = file_ptr->rp_tail_ptr;

    while ( pageheader ) {

        /* if the page is valid and dirty, flush it */
        if ( ( 0 == (pageheader->flags & H5FD_PB_INVALID_FLAG) ) &&
             ( pageheader->flags & H5FD_PB_DIRTY_FLAG ) ) {

            if ( H5FD__pb_flush_page(file_ptr, dxpl_id, pageheader) )
                HGOTO_ERROR(H5E_VFL, H5E_CANTFLUSH, FAIL, "unable to flush page");
        } 

        pages_visited++;

        pageheader = pageheader->rp_prev_ptr;

    } /* end while */

    /* Verifies that all pages in the page buffer have been searched */
    assert(pages_visited == file_ptr->rp_pageheader_count);

    /* Verifies that all dirty pages have been flushed */
    assert(0 == file_ptr->rp_dirty_count);

    /* Public API for dxpl "context" */
    if (H5FDflush(file_ptr->file, dxpl_id, closing) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTFLUSH, FAIL, "unable to flush underlying file");


done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_flush() */



/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_read
 *
 * Purpose:     Reads SIZE bytes of data from the page buffer and/or the 
 *		        underlying VFD beginning at address ADDR, into buffer 
 *              BUF according to data transfer properties in DXPL_ID.
 * 
 *              Turns random I/O read requests into paged I/O read requests.
 *
 * Return:      Success:    SUCCEED
 *                          The read result is written into the BUF buffer
 *                          which should be allocated by the caller.
 *
 *              Failure:    FAIL
 *                          The contents of BUF are undefined.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_read(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, hid_t H5_ATTR_UNUSED dxpl_id, haddr_t addr,
                    size_t size, void *buf)
{
    bool       head_exists        = FALSE;         
    bool       middle_exists      = FALSE;
    bool       tail_exists        = FALSE;
    uint64_t   num_pages          = 0;           /* number of pages in the read request */
    uint64_t   page_num;                         /* page number of the head page */
    haddr_t    head_page_addr     = HADDR_UNDEF; /* addr for beginning of the head page */
    haddr_t    head_start_addr    = HADDR_UNDEF; /* addr for where the write starts in the head page */
    size_t     head_size          = 0;
    haddr_t    middle_start_addr  = HADDR_UNDEF;    
    size_t     middle_size        = 0;
    uint64_t   middle_page_count  = 0;
    uint64_t   middle_read_count = 0;            /* for calculating addrs in multi-paged middles */
    uint64_t   middle_check_count = 0;           /* counts and checks if pages exist in the ht */
    haddr_t    tail_start_addr    = HADDR_UNDEF;
    size_t     tail_size          = 0;
    haddr_t    addr_of_remainder;                /* Used to calculate if head, middle, and tails exist */
    size_t     size_of_remainder;                /* Used to calculate if head, middle, and tails exist */
    haddr_t    expected_addr;
    uint32_t   hash_code;
    size_t     read_count         = 0;           /* total count of all writes */
    haddr_t    current_addr;                     /* tracks addr for multi-paged middles */
    size_t     accumulated_size;                 /* size of straight through read middles */
    haddr_t    accumulated_addr;                 /* addr for straight through read middles */

    H5FD_pb_pageheader_t *head = NULL;
    H5FD_pb_pageheader_t *tail = NULL;
    H5FD_pb_pageheader_t *middle = NULL;
    H5FD_pb_t *           file_ptr  = (H5FD_pb_t *)_file;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert( file_ptr );
    assert( file_ptr->pub.cls );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( buf );

    /* Check for overflow conditions */
    if (!H5_addr_defined(addr))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "addr undefined, addr = %llu", (unsigned long long)addr);

    if (REGION_OVERFLOW(addr, size))
        HGOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, FAIL, "addr overflow, addr = %llu", (unsigned long long)addr);


    addr_of_remainder = addr;
    size_of_remainder = size;



    /***** Checks if a head exists *****/

    if ( addr_of_remainder % file_ptr->fa.page_size != 0 ) {

        head_exists = TRUE;
        page_num = addr / file_ptr->fa.page_size;
        head_page_addr = page_num * file_ptr->fa.page_size;
        head_start_addr = head_page_addr + ( addr % file_ptr->fa.page_size );
        head_size = file_ptr->fa.page_size - ( addr % file_ptr->fa.page_size );

        /* If head is larger then remainder then entire read is in the head */
        if ( head_size >= size_of_remainder ) {

            head_size = size_of_remainder;
            size_of_remainder = 0;
        }
        else {

            size_of_remainder -= head_size;
            addr_of_remainder += head_size;

            assert( size_of_remainder > 0 );
            assert( 0 == ( addr_of_remainder % file_ptr->fa.page_size ) );
        }

        num_pages++; 
    }
    
    assert( ( size_of_remainder == 0 ) || ( 0 == ( addr_of_remainder % file_ptr->fa.page_size )) );



    /***** Checks if a middle exits *****/

    if ( ( size_of_remainder > 0 ) && ( ( size_of_remainder / file_ptr->fa.page_size ) > 0 )) {

        middle_exists = TRUE;
        middle_start_addr = addr_of_remainder;
        middle_page_count = size_of_remainder / file_ptr->fa.page_size;
        middle_size = middle_page_count * file_ptr->fa.page_size;

        assert( middle_size <= size_of_remainder );

        size_of_remainder -= middle_size;
        addr_of_remainder += middle_size;

        assert( size_of_remainder < file_ptr->fa.page_size );
        assert( 0 == ( addr_of_remainder % file_ptr->fa.page_size ) );

        num_pages += middle_page_count;
    }



    /***** Checks if a tail exists *****/

    if ( size_of_remainder > 0 ) {

        assert( 0 == addr_of_remainder % file_ptr->fa.page_size );

        tail_exists = TRUE;
        tail_start_addr = addr_of_remainder;
        tail_size = size_of_remainder;

        num_pages++; 
    }



    /***** Check the head, middle, and tail sizes and starting addrs *****/

    assert( head_size + middle_size + tail_size == size );

    expected_addr = addr;

    if ( head_exists ) {

        assert( addr == head_start_addr );

        if ( middle_exists || tail_exists ) {
            
            assert ( 0 == (( head_start_addr + head_size ) % file_ptr->fa.page_size ) );
        }
        else {

            assert( (( head_start_addr + head_size ) - addr ) <= file_ptr->fa.page_size );
        }

        expected_addr += head_size;        
    } 

    if ( middle_exists ) {

        assert( middle_start_addr == expected_addr ); 

        expected_addr += middle_size;
    }
    
    if ( tail_exists ) {

        assert( tail_start_addr == expected_addr );
    } 



    /***** Performs read operation *****/


    /***** Head operations *****/

    if ( head_exists ) {

        hash_code = H5FD__pb_calc_hash_code( file_ptr, head_page_addr );

        head = H5FD__pb_ht_search_pageheader( file_ptr, head_page_addr, hash_code );

        if ( head == NULL ) {

            if ( NULL == ( head = H5FD__pb_get_pageheader( file_ptr, type, dxpl_id, head_page_addr, hash_code )) )
                HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, "Head page could not be loaded");

            /* Make sure the H5FD_pb_pageheader_t structure is not busy or invalid */
            assert( 0 == ( head->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and read flag immediately */
            head->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );
            file_ptr->num_heads++;
        }
        else {

            /* Make sure the H5FD_pb_pageheader_t structure is not busy or invalid */
            assert( head );
            assert( head->magic == H5FD_PB_PAGEHEADER_MAGIC );
            assert( 0 == ( head->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and read flag immediately */
            head->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

            if ( file_ptr->fa.rp == 0 ) {
                H5FD__pb_rp_touch_pageheader( file_ptr, head );
            }
        }

        assert( head );
        assert( head->magic == H5FD_PB_PAGEHEADER_MAGIC );

        assert( head->flags & H5FD_PB_BUSY_FLAG );
        assert( head->flags & H5FD_PB_READ_FLAG );
        assert( 0 == ( head->flags & H5FD_PB_WRITE_FLAG ) );
        assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

        H5MM_memcpy( buf, head->page + ( head_start_addr - head_page_addr ), head_size );

        /* Resets flags now that the pageheader is done being read */
        head->flags &= ~( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

        read_count++;
    }


    /***** Middle operations *****/

    if ( middle_exists ) {

        current_addr = middle_start_addr;
        accumulated_size = 0;
        accumulated_addr = HADDR_UNDEF;

        /* Iterates through all pages in the middle */
        for ( middle_check_count = 0; middle_check_count < middle_page_count; middle_check_count++ ) {

            hash_code = H5FD__pb_calc_hash_code( file_ptr, current_addr );

            middle = H5FD__pb_ht_search_pageheader( file_ptr, current_addr, hash_code );

            /* If a middle page doesn't exist in ht then it gets added to accumulator*/
            if ( middle == NULL ) {

                if ( accumulated_size == 0 )
                    accumulated_addr = current_addr;

                accumulated_size += file_ptr->fa.page_size;

            }

            /**
             * If a middle page does exist in the ht:
             * 
             * 1. Read any accumulated pages from the file or lower VFD to the buffer.
             * 2. Touch the pageheader to update the replacement policy.
             * 3. Read (copy) the page from the H5FD_pb_pageheader_t into the buffer.
             */
            else {

                if ( accumulated_size > 0 ) {

                    if ( H5FDread( file_ptr->file, type, dxpl_id, accumulated_addr, accumulated_size, 
                                   (void *)(((char *)buf ) + head_size + 
                                   ( middle_read_count * file_ptr->fa.page_size ))) < 0 ) {
                        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, \
                                    "Middle page could not be read from file or lower VFD");
                    }

                    middle_read_count += accumulated_size / file_ptr->fa.page_size;
                    accumulated_size = 0;
                }

                assert( middle );
                assert( middle->magic == H5FD_PB_PAGEHEADER_MAGIC );

                /* Make sure H5FD_pb_pageheader_t is not busy or invalid */
                assert( 0 == ( middle->flags & H5FD_PB_BUSY_FLAG ) );
                assert( 0 == ( middle->flags & H5FD_PB_INVALID_FLAG ) );

                /* Sets the busy and read flags */
                middle->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

                assert( middle->flags & H5FD_PB_BUSY_FLAG );
                assert( middle->flags & H5FD_PB_READ_FLAG );
                assert( 0 == ( middle->flags & H5FD_PB_INVALID_FLAG ) );

                if ( file_ptr->fa.rp == 0 ) {
                   H5FD__pb_rp_touch_pageheader( file_ptr, middle );
                }

                H5MM_memcpy( (void *)(((char *)buf ) + head_size + (middle_check_count * file_ptr->fa.page_size)),
                             middle->page, file_ptr->fa.page_size );

                middle_read_count++;

                middle->flags &= ~( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

            }            

            current_addr += file_ptr->fa.page_size;
        }

        /* If any accumulated pages haven't been read when the loop ends read them now. */
        if ( accumulated_size > 0) {
                
                if ( H5FDread( file_ptr->file, type, dxpl_id, accumulated_addr, accumulated_size, 
                            (void *)(((char *)buf) + head_size + ( middle_read_count * file_ptr->fa.page_size ))) < 0 )
                    HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, \
                                "Middle page could not be read from file or lower VFD");
        }

        file_ptr->total_middle_reads += middle_page_count;
        read_count += middle_page_count;
    }


    /***** Tail operations *****/

    if ( tail_exists ) {

        hash_code = H5FD__pb_calc_hash_code( file_ptr, tail_start_addr );

        tail = H5FD__pb_ht_search_pageheader( file_ptr, tail_start_addr, hash_code );

        if ( tail == NULL ) {

            if ( NULL == ( tail = H5FD__pb_get_pageheader( file_ptr, type, dxpl_id, tail_start_addr, hash_code )) )
                HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, "Tail page could not be loaded");

            /* Make sure H5FD_pb_pageheader_t is not busy or invalid */
            assert( 0 == ( tail->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and read flags */
            tail->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );
            file_ptr->num_tails++;
        }
        else {
            /* Make sure H5FD_pb_pageheader_t is not busy or invalid */
            assert( tail );
            assert( tail->magic == H5FD_PB_PAGEHEADER_MAGIC );
            assert( 0 == ( tail->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the pagehader as busy and read */
            tail->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

            if ( file_ptr->fa.rp == 0 ) {
                H5FD__pb_rp_touch_pageheader( file_ptr, tail );
            }
        }

        assert( tail );
        assert( tail->magic == H5FD_PB_PAGEHEADER_MAGIC );

        assert( tail->flags & H5FD_PB_BUSY_FLAG );
        assert( tail->flags & H5FD_PB_READ_FLAG );
        assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

        H5MM_memcpy( (void *)(((char *)buf ) + head_size + ( read_count * file_ptr->fa.page_size )), 
                     tail->page, tail_size );

        tail->flags &= ~( H5FD_PB_BUSY_FLAG | H5FD_PB_READ_FLAG );

        read_count++;
    }

    assert( read_count == num_pages );

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_read() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_write
 *
 * Purpose:     Writes SIZE bytes of data to the page buffer and / or the 
 *              underlying VFD beginning at address ADDR, from buffer BUF 
 *		        according to data transfer properties in DXPL_ID.
 *
 *              Turns random I/O write requests into paged I/O write 
 *              requests.
 * 
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_write(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
                     const void *buf)
{
    bool       head_exists        = FALSE;         
    bool       middle_exists      = FALSE;
    bool       tail_exists        = FALSE;
    uint64_t   num_pages          = 0;           /* number of pages in the request */
    uint64_t   page_num;                         /* page number of the head page */
    haddr_t    head_page_addr     = HADDR_UNDEF; /* addr for the beginning of the head page */
    haddr_t    head_start_addr    = HADDR_UNDEF; /* addr for where the write starts in the head page */
    size_t     head_size          = 0;
    haddr_t    middle_start_addr  = HADDR_UNDEF;    
    size_t     middle_size        = 0;
    uint64_t   middle_page_count  = 0;           
    uint64_t   middle_check_count = 0;           /* counts and checks if pages exist in the ht */
    haddr_t    tail_start_addr    = HADDR_UNDEF;
    size_t     tail_size          = 0;
    haddr_t    addr_of_remainder;                /* Used to calculate if head, middle, and tails exist */
    size_t     size_of_remainder;                /* Used to calculate if head, middle, and tails exist */
    haddr_t    expected_addr;
    uint32_t   hash_code;
    size_t     write_count        = 0;           /* total count of all writes */
    haddr_t    current_addr;                     /* used to track addr for multi-paged middles */

    H5FD_pb_pageheader_t *head = NULL;
    H5FD_pb_pageheader_t *tail = NULL;
    H5FD_pb_pageheader_t *middle = NULL;
    H5FD_pb_t *           file_ptr  = (H5FD_pb_t *)_file;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert( file_ptr );
    assert( file_ptr->pub.cls );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( buf );

    /* Check for overflow conditions */
    if (!H5_addr_defined(addr))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "addr undefined, addr = %llu", (unsigned long long)addr);

    if (REGION_OVERFLOW(addr, size))
        HGOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, FAIL, "addr overflow, addr = %llu", (unsigned long long)addr);


    addr_of_remainder = addr;
    size_of_remainder = size;



    /***** Checks if a head exists *****/

    if ( addr_of_remainder % file_ptr->fa.page_size != 0 ) {

        head_exists = TRUE;
        page_num = addr / file_ptr->fa.page_size;
        head_page_addr = page_num * file_ptr->fa.page_size;
        head_start_addr = head_page_addr + ( addr % file_ptr->fa.page_size );
        head_size = file_ptr->fa.page_size - ( addr % file_ptr->fa.page_size );

        /* If head is smaller then remainder then entire write is in the head */
        if ( head_size >= size_of_remainder ) {

            head_size = size_of_remainder;
            size_of_remainder = 0;

        }
        else {

            size_of_remainder -= head_size;
            addr_of_remainder += head_size;

            assert( size_of_remainder > 0 );
            assert( 0 == ( addr_of_remainder % file_ptr->fa.page_size ) );
        }

        num_pages++; 
    }

    assert( ( size_of_remainder == 0 ) || ( 0 == ( addr_of_remainder % file_ptr->fa.page_size )) );



    /***** Checks if a middle exits *****/

    if ( ( size_of_remainder > 0 ) && ( ( size_of_remainder / file_ptr->fa.page_size ) > 0 )) {

        middle_exists = TRUE;
        middle_start_addr = addr_of_remainder;
        middle_page_count = size_of_remainder / file_ptr->fa.page_size;
        middle_size = middle_page_count * file_ptr->fa.page_size;

        assert( middle_size <= size_of_remainder );

        size_of_remainder -= middle_size;
        addr_of_remainder += middle_size;

        assert( size_of_remainder < file_ptr->fa.page_size );
        assert( 0 == ( addr_of_remainder % file_ptr->fa.page_size ) );

        num_pages += middle_page_count;
    }



    /***** Checks if a tail exists *****/

    if ( size_of_remainder > 0 ) {

        assert( 0 == addr_of_remainder % file_ptr->fa.page_size );

        tail_exists = TRUE;
        tail_start_addr = addr_of_remainder;
        tail_size = size_of_remainder;

        num_pages++; 
    }



    /***** Check the head, middle, and tail sizes and starting addrs *****/

    assert( head_size + middle_size + tail_size == size );

    expected_addr = addr;

    if ( head_exists ) {

        assert( addr == head_start_addr );

        if ( middle_exists || tail_exists ) {
            
            assert ( 0 == (( head_start_addr + head_size ) % file_ptr->fa.page_size) );
        }
        else {

            assert( (( head_start_addr + head_size) - addr ) <= file_ptr->fa.page_size );
        }

        expected_addr += head_size;        
    } 

    if ( middle_exists ) {

        assert( middle_start_addr == expected_addr ); 

        expected_addr += middle_size;
    }
    
    if ( tail_exists ) {

        assert( tail_start_addr == expected_addr );
    } 



    /***** Performs write operation *****/

    /***** Head operations *****/

    if ( head_exists ) {

        hash_code = H5FD__pb_calc_hash_code( file_ptr, head_page_addr );

        head = H5FD__pb_ht_search_pageheader( file_ptr, head_page_addr, hash_code );

        if ( head == NULL ) {

            if ( NULL == ( head = H5FD__pb_get_pageheader( file_ptr, type, dxpl_id, head_page_addr, hash_code )) )
                HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, "Head page could not be loaded");

            /* Make sure the H5FD_pb_pageheader_t structure is not busy or invalid */
            assert( 0 == ( head->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and write flag immediately */
            head->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

            file_ptr->num_heads++;
        }
        else {

            /* Make sure the H5FD_pb_pageheader_t structure is not busy or invalid */
            assert( head );
            assert( head->magic == H5FD_PB_PAGEHEADER_MAGIC );
            assert( 0 == ( head->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and write flag immediately */
            head->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

            if ( file_ptr->fa.rp == 0 ) {
                H5FD__pb_rp_touch_pageheader( file_ptr, head );
            }
        }

        assert( head );
        assert( H5FD_PB_PAGEHEADER_MAGIC == head->magic );

        head->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

        assert( head->flags & H5FD_PB_BUSY_FLAG );
        assert( head->flags & H5FD_PB_WRITE_FLAG );
        assert( 0 == ( head->flags & H5FD_PB_INVALID_FLAG ) );

        H5MM_memcpy( head->page + ( head_start_addr - head_page_addr ), buf, head_size );

        if ( 0 == (head->flags & H5FD_PB_DIRTY_FLAG )) {

            head->flags |= H5FD_PB_DIRTY_FLAG;
            file_ptr->rp_dirty_count++;
        }

        file_ptr->total_dirty++;

        head->flags &= ~( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

        write_count++;
    }


    /***** Middle operations *****/

    if ( middle_exists ) {

        current_addr = middle_start_addr;

        /* Iterates through all pages in the middle */
        for ( middle_check_count = 0; middle_check_count < middle_page_count; middle_check_count++ ) {

            hash_code = H5FD__pb_calc_hash_code( file_ptr, current_addr );

            middle = H5FD__pb_ht_search_pageheader( file_ptr, current_addr, hash_code );

            /* If a middle page exists in the ht invalidate it */
            if ( middle != NULL ) {

                assert( middle );
                assert( H5FD_PB_PAGEHEADER_MAGIC == middle->magic );

                H5FD__pb_invalidate_pageheader( file_ptr, middle );

                assert( middle->flags & H5FD_PB_INVALID_FLAG );
            }

            current_addr += file_ptr->fa.page_size;

        }

        /* Writes all middle pages */
        if ( H5FDwrite( file_ptr->file, type, dxpl_id, middle_start_addr, middle_size,
                        (const void *)(((const char *)buf ) + head_size )) < 0 ) {
            HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, \
                        "Middle page could not be written from file or lower VFD");
        }

        file_ptr->total_middle_writes += middle_page_count;

        write_count += middle_page_count;

    }


    /***** Tail operations *****/

    if ( tail_exists ) {

        hash_code = H5FD__pb_calc_hash_code( file_ptr, tail_start_addr );

        tail = H5FD__pb_ht_search_pageheader( file_ptr, tail_start_addr, hash_code );

        if ( tail == NULL ) {
            
            if ( NULL == ( tail = H5FD__pb_get_pageheader( file_ptr, type, dxpl_id, tail_start_addr, hash_code )) )
                HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, "Tail page could not be loaded");

            /* Make sure H5FD_pb_pageheader_t is not busy or invalid */
            assert( 0 == ( tail->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and write flags */
            tail->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

            file_ptr->num_tails++;
        }
        else {

            /* Make sure H5FD_pb_pageheader_t is not busy or invalid */
            assert( tail );
            assert( tail->magic == H5FD_PB_PAGEHEADER_MAGIC );
            assert( 0 == ( tail->flags & H5FD_PB_BUSY_FLAG ) );
            assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

            /* Sets the busy and write flags */
            tail->flags |= ( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

            if ( file_ptr->fa.rp == 0 ) {
                H5FD__pb_rp_touch_pageheader( file_ptr, tail );
            }
        }

        assert( tail );
        assert( tail->magic == H5FD_PB_PAGEHEADER_MAGIC );

        assert( tail->flags & H5FD_PB_BUSY_FLAG );
        assert( tail->flags & H5FD_PB_WRITE_FLAG );
        assert( 0 == ( tail->flags & H5FD_PB_INVALID_FLAG ) );

        H5MM_memcpy( tail->page, (const void *)(((const char *)buf ) + ( write_count * file_ptr->fa.page_size )), tail_size );

        if ( 0 == (tail->flags & H5FD_PB_DIRTY_FLAG )) {

            tail->flags |= H5FD_PB_DIRTY_FLAG;
            file_ptr->rp_dirty_count++;
        }

        file_ptr->total_dirty++;

        tail->flags &= ~( H5FD_PB_BUSY_FLAG | H5FD_PB_WRITE_FLAG );

        write_count++;
    }

    assert( write_count == num_pages );

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_write() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_fapl_get
 *
 * Purpose:     Returns a file access property list which indicates how the
 *              specified file is being accessed. The return list could be
 *              used to access another file the same way.
 *
 * Return:      Success:    Ptr to new file access property list with all
 *                          members copied from the file struct.
 *              Failure:    NULL
 *-------------------------------------------------------------------------
 */
static void *
H5FD__pb_fapl_get(H5FD_t *_file)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    void      *ret_value = NULL;

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_PB_LOG_CALL(__func__);

    ret_value = H5FD__pb_fapl_copy(&(file->fa));

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_fapl_get() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_fapl_copy
 *
 * Purpose:     Copies the file access properties.
 *
 * Return:      Success:    Pointer to a new property list info structure.
 *              Failure:    NULL
 *-------------------------------------------------------------------------
 */
static void *
H5FD__pb_fapl_copy(const void *_old_fa)
{
    const H5FD_pb_vfd_config_t *old_fa_ptr = (const H5FD_pb_vfd_config_t *)_old_fa;
    H5FD_pb_vfd_config_t       *new_fa_ptr = NULL;
    void                       *ret_value  = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert(old_fa_ptr);

    new_fa_ptr = H5FL_CALLOC(H5FD_pb_vfd_config_t);
    if (NULL == new_fa_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate log file FAPL");

    H5MM_memcpy(new_fa_ptr, old_fa_ptr, sizeof(H5FD_pb_vfd_config_t));

    /* Copy the FAPL */
    if (H5FD__copy_plist(old_fa_ptr->fapl_id, &(new_fa_ptr->fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, NULL, "can't copy underlying FAPL");

    ret_value = (void *)new_fa_ptr;

done:

    if (NULL == ret_value) {

        if (new_fa_ptr) {

            new_fa_ptr = H5FL_FREE(H5FD_pb_vfd_config_t, new_fa_ptr);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_fapl_copy() */

/*--------------------------------------------------------------------------
 * Function:    H5FD__pb_fapl_free
 *
 * Purpose:     Releases the file access lists
 *
 * Return:      SUCCEED/FAIL
 *--------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_fapl_free(void *_fapl)
{
    H5FD_pb_vfd_config_t *fapl      = (H5FD_pb_vfd_config_t *)_fapl;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(fapl);

    if (H5I_dec_ref(fapl->fapl_id) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDEC, FAIL, "can't close underlying FAPL ID");

    /* Free the property list */
    fapl = H5FL_FREE(H5FD_pb_vfd_config_t, fapl);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_fapl_free() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_open
 *
 * Purpose:     Create and/or opens a file as an HDF5 file, and initializes
 *              the data structures for the Page Buffer VFD.
 *
 * Return:      Success:    A pointer to a new file data structure. The
 *                          public fields will be initialized by the
 *                          caller, which is always H5FD_open().
 *              Failure:    NULL
 *-------------------------------------------------------------------------
 */
static H5FD_t *
H5FD__pb_open(const char *name, unsigned flags, hid_t pb_fapl_id, haddr_t maxaddr)
{
    H5FD_pb_t                  *file_ptr     = NULL; /* page buffer VFD info */
    const H5FD_pb_vfd_config_t *fapl_ptr     = NULL; /* Driver-specific property list */
    H5FD_pb_vfd_config_t       *default_fapl = NULL;
    H5P_genplist_t             *plist_ptr    = NULL;
    H5FD_t                     *ret_value    = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid file name");

    if (0 == maxaddr || HADDR_UNDEF == maxaddr)
        HGOTO_ERROR(H5E_ARGS, H5E_BADRANGE, NULL, "bogus maxaddr");

    if (ADDR_OVERFLOW(maxaddr))
        HGOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, NULL, "bogus maxaddr");

    if (H5FD_PB != H5Pget_driver(pb_fapl_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "driver is not page buffer");

    file_ptr = (H5FD_pb_t *)H5FL_CALLOC(H5FD_pb_t);

    if (NULL == file_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate file struct");

    file_ptr->magic      = H5FD_PB_MAGIC;

    file_ptr->fa.magic   = H5FD_PB_CONFIG_MAGIC;
    file_ptr->fa.version = H5FD_CURR_PB_VFD_CONFIG_VERSION;
    file_ptr->fa.fapl_id = H5I_INVALID_HID;

    /* Get the driver-specific file access properties */
    plist_ptr = (H5P_genplist_t *)H5I_object(pb_fapl_id);

    if ( NULL == plist_ptr )
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file access property list");

    fapl_ptr = (const H5FD_pb_vfd_config_t *)H5P_peek_driver_info(plist_ptr);

    if ( NULL == fapl_ptr ) {

        if ( NULL == ( default_fapl = H5FL_CALLOC(H5FD_pb_vfd_config_t) ))
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate file access property list struct");

        if ( H5FD__pb_populate_config( NULL, default_fapl ) < 0 )
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, NULL, "can't initialize driver configuration info");

        fapl_ptr = default_fapl;
    }

    /* Copy data from *fapl_ptr to file_ptr->fa */
    file_ptr->fa.page_size     = fapl_ptr->page_size;
    file_ptr->fa.max_num_pages = fapl_ptr->max_num_pages;
    file_ptr->fa.rp            = fapl_ptr->rp;

    /* copy the FAPL for the underlying VFD */
    if ( H5FD__copy_plist( fapl_ptr->fapl_id, &(file_ptr->fa.fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, NULL, "can't copy underlying FAPL");

    /* open the underlying VFD / file */
    file_ptr->file = H5FD_open( name, flags, fapl_ptr->fapl_id, HADDR_UNDEF );

    if ( NULL == file_ptr->file )
        HGOTO_ERROR(H5E_VFL, H5E_CANTOPENFILE, NULL, "unable to open underlying file");

    /* initialize the hash table */
    for ( int32_t i = 0; i < H5FD_PB_DEFAULT_NUM_HASH_BUCKETS; i++ ) {
        file_ptr->ht_bucket[i].index               = i;
        file_ptr->ht_bucket[i].num_pages_in_bucket = 0;
        file_ptr->ht_bucket[i].ht_head_ptr         = NULL;
    }
    
    /* initialize the replacement policy */
    file_ptr->rp_policy             = file_ptr->fa.rp;
    file_ptr->rp_head_ptr           = NULL;
    file_ptr->rp_tail_ptr           = NULL;
    file_ptr->rp_pageheader_count   = 0;
    file_ptr->rp_dirty_count        = 0;

    /* initialize EOA management fields */
    file_ptr->eoa_up                = 0;
    file_ptr->eoa_down              = 0;

#if 0 /* JRM */
    /* eventually we will want a stats reset function -- don't do this now 
     * unless you have time.
     */
#endif /* JRM */
    
    /* initialize statistics */
    file_ptr->num_pages             = 0;
    file_ptr->num_heads             = 0;
    file_ptr->num_tails             = 0;
    file_ptr->largest_num_in_bucket = 0;
    file_ptr->num_hits              = 0;
    file_ptr->num_misses            = 0;
    file_ptr->total_middle_reads    = 0;
    file_ptr->total_middle_writes   = 0;
    file_ptr->max_search_depth      = 0;
    file_ptr->total_success_depth   = 0;
    file_ptr->total_fail_depth      = 0;
    file_ptr->total_evictions       = 0;
    file_ptr->total_dirty           = 0;
    file_ptr->total_invalidated     = 0;
    file_ptr->total_flushed         = 0;


    ret_value = (H5FD_t *)file_ptr;

done:

    if ( default_fapl )
        H5FL_FREE(H5FD_pb_vfd_config_t, default_fapl);

    if ( NULL == ret_value ) {  /* do error cleanup */

        if ( file_ptr ) {

            if ( H5I_INVALID_HID != file_ptr->fa.fapl_id ) {

                H5I_dec_ref( file_ptr->fa.fapl_id );
            }

            if ( file_ptr->file ) {

                H5FD_close( file_ptr->file );
            }

            H5FL_FREE(H5FD_pb_t, file_ptr);
        }
    } /* end if error */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_open() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_close
 *
 * Purpose:     Closes the underlying file and take down the page buffer.
 *
 *              To do this, we must:
 *
 *              1) flush the page buffer.
 *
 *              2) close the underlying VFD
 *
 *              3) Discard all pages in the page buffer,
 *
 *              4) Discard the instance of H5FD_pb_t.
 *
 * Return:      Success:    SUCCEED
 *              Failure:    FAIL, file not closed.
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_close(H5FD_t *_file)
{
    int                    i;
    H5FD_pb_t            * file_ptr  = (H5FD_pb_t *)_file;
    H5FD_pb_pageheader_t * pageheader;
    H5FD_pb_pageheader_t * discard_page_ptr;
    herr_t     ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );

    /* must flush the page buffer as writes can occur after the flush on file close */
    if ( H5FD__pb_flush( _file, H5P_DEFAULT, TRUE) < 0 ) 
        HGOTO_ERROR(H5E_VFL, H5E_CANTFLUSH, FAIL, "unable to flush pagebuffer on close");


    /* Verify there are no dirty pages in the page buffer */
    assert( 0 == file_ptr->rp_dirty_count );


    /* close the underlying VFD */
    if ( H5I_dec_ref( file_ptr->fa.fapl_id ) < 0 )
        HGOTO_ERROR(H5E_VFL, H5E_ARGS, FAIL, "can't close underlying VFD FAPL");

    if ( file_ptr->file ) {

        if ( H5FD_close( file_ptr->file ) == FAIL )
            HGOTO_ERROR(H5E_VFL, H5E_CANTCLOSEFILE, FAIL, "unable to close underlying file");
    }

    /* discard page buffer data structures */

    pageheader = file_ptr->rp_tail_ptr;

    while ( pageheader ) {

        discard_page_ptr = pageheader;

        pageheader = pageheader->rp_prev_ptr;

        /* remove the page from the replacement policy DLL */
        if ( H5FD__pb_rp_remove_pageheader( file_ptr, discard_page_ptr ) != 0 )
            HGOTO_ERROR(H5E_VFL, H5E_CANTCLOSEFILE, FAIL, "Can't remove page from RP list");

        if ( 0 == ( discard_page_ptr->flags & H5FD_PB_INVALID_FLAG ) ) {

            /* Invalid flag is not set, so page must be in the hash table -- remove it */
            if ( H5FD__pb_ht_remove_pageheader( file_ptr, discard_page_ptr ) != 0 )
                HGOTO_ERROR(H5E_VFL, H5E_CANTCLOSEFILE, FAIL, "Can't remove page from hash table");
        }

        discard_page_ptr->magic = 0;
        free( discard_page_ptr );
    }
    assert( 0 == file_ptr->rp_pageheader_count );

    /* verify that the hash table is empty */
    for ( i = 0; i < H5FD_PB_DEFAULT_NUM_HASH_BUCKETS; i++ ) {

        assert( i == file_ptr->ht_bucket[i].index );
        assert( 0 == file_ptr->ht_bucket[i].num_pages_in_bucket );
        assert( NULL == file_ptr->ht_bucket[i].ht_head_ptr );
    }


    /* Release the file info */
    file_ptr = H5FL_FREE(H5FD_pb_t, file_ptr);
    file_ptr = NULL;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_close() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_get_eoa
 *
 * Purpose:     Returns the end-of-address marker for the file. The EOA
 *              marker is the first address past the last byte allocated in
 *              the format address space.
 *
 *              Due to the fact that the page buffer converts random I/O
 *              to paged I/O, the EOA above the page buffer VFD is usually
 *              different from that below.  Specfically, the lower EOA must
 *              always be on a page boundary, and be greater than or ewual 
 *              to the upper EOA.
 *
 *              Since we currently maintain the file_ptr->eoa_up and
 *              file_ptr->eoa_down fields, in principle, it is sufficient to
 *              simply return eoa_up.
 *
 *              However, as a sanity check, we request the eoa from the
 *              underlying VFD, and fail if it doesn't match file_ptr->eoa_down.
 *
 *              If this test passes, we return file_ptr->eoa_up.
 *
 *              Note that we don't have to concern outselves with the
 *              case in which the first get_eoa call arrives before the
 *              first set_eoa call since since both eoa_up and eoa_down
 *              are initialzed to zero on file open
 *
 * Return:      Success:    The end-of-address-marker
 *
 *              Failure:    HADDR_UNDEF
 *-------------------------------------------------------------------------
 */
static haddr_t
H5FD__pb_get_eoa(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type)
{
    haddr_t          eoa_down;
    const H5FD_pb_t *file_ptr  = (const H5FD_pb_t *)_file;
    haddr_t          ret_value = HADDR_UNDEF;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert(file_ptr);
    assert(file_ptr->file);
    assert(H5FD_PB_MAGIC == file_ptr->magic);

    if ((eoa_down = H5FD_get_eoa(file_ptr->file, type)) == HADDR_UNDEF) 
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, HADDR_UNDEF, "unable to get eoa");

    if ( H5_addr_ne(eoa_down, file_ptr->eoa_down) )
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, HADDR_UNDEF, "eoa_down mismatch");

    ret_value = file_ptr->eoa_up;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_get_eoa */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_set_eoa
 *
 * Purpose:     Set the end-of-address marker for the file. This function is
 *              called shortly after an existing HDF5 file is opened in order
 *              to tell the driver where the end of the HDF5 data is located.
 *
 *              In the page buffer VFD case, we must extend the supplied EOA
 *              to the next page boundary.  and pass the result to the 
 *              underlying VFD.  If the call is successful, set eoa_up and 
 *              eoa_down to the supplied and computed values respectively, 
 *              and then return.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_set_eoa(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, haddr_t addr)
{
    haddr_t    eoa_up;
    haddr_t    eoa_down;
    int64_t    page_num;
    H5FD_pb_t *file_ptr = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__)

    /* Sanity check */
    assert(file_ptr);
    assert(H5FD_PB_MAGIC == file_ptr->magic);
    assert(file_ptr->file);

    eoa_up = addr;

    page_num = (int64_t)(addr / (haddr_t)((file_ptr->fa.page_size)));

    assert(page_num >= 0);

    if ( 0 < (addr % (haddr_t)(file_ptr->fa.page_size)) ) {

        page_num++;
    }

    eoa_down = ((haddr_t)(page_num)) * ((haddr_t)(file_ptr->fa.page_size));

    if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "H5FD_set_eoa failed for underlying file");

    file_ptr->eoa_up = eoa_up;
    file_ptr->eoa_down = eoa_down;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_set_eoa() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_get_eof
 *
 * Purpose:     Returns the end-of-address marker for the file. The EOA
 *              marker is the first address past the last byte allocated in
 *              the format address space.
 *
 * Return:      Success:    The end-of-address-marker
 *
 *              Failure:    HADDR_UNDEF
 *-------------------------------------------------------------------------
 */
static haddr_t
H5FD__pb_get_eof(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type)
{
    const H5FD_pb_t *file      = (const H5FD_pb_t *)_file;
    haddr_t          ret_value = HADDR_UNDEF; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (HADDR_UNDEF == (ret_value = H5FD_get_eof(file->file, type)))
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, HADDR_UNDEF, "unable to get eof");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_get_eof */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_truncate
 *
 * Purpose:     Notify driver to truncate the file back to the allocated size.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_truncate(H5FD_t *_file, hid_t dxpl_id, bool closing)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert(file);
    assert(file->file);

    /* need to to invalidate any pages truncated out of existance. 
     * Need to think on how to handle any partially truncaed pages
     */
    if (H5FDtruncate(file->file, dxpl_id, closing) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTUPDATE, FAIL, "unable to truncate file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_truncate */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_sb_size
 *
 * Purpose:     Obtains the number of bytes required to store the driver file
 *              access data in the HDF5 superblock.
 *
 * Return:      Success:    Number of bytes required.
 *
 *              Failure:    0 if an error occurs or if the driver has no
 *                          data to store in the superblock.
 *
 * NOTE: no public API for H5FD_sb_size, it needs to be added
 *-------------------------------------------------------------------------
 */
static hsize_t
H5FD__pb_sb_size(H5FD_t *_file)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    hsize_t    ret_value = 0;

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (file->file) {

        ret_value = H5FD_sb_size(file->file);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_sb_size */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_sb_encode
 *
 * Purpose:     Encode driver-specific data into the output arguments.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_sb_encode(H5FD_t *_file, char *name /*out*/, unsigned char *buf /*out*/)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (file->file && H5FD_sb_encode(file->file, name, buf) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTENCODE, FAIL, "unable to encode the superblock in file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_sb_encode */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_sb_decode
 *
 * Purpose:     Decodes the driver information block.
 *
 * Return:      SUCCEED/FAIL
 *
 * NOTE: no public API for H5FD_sb_size, need to add
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_sb_decode(H5FD_t *_file, const char *name, const unsigned char *buf)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (H5FD_sb_load(file->file, name, buf) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDECODE, FAIL, "unable to decode the superblock in file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_sb_decode */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_cmp
 *
 * Purpose:     Compare the keys of two files.
 *
 * Return:      Success:    A value like strcmp()
 *              Failure:    Must never fail
 *-------------------------------------------------------------------------
 */
static int
H5FD__pb_cmp(const H5FD_t *_f1, const H5FD_t *_f2)
{
    const H5FD_pb_t *f1 = (const H5FD_pb_t *)_f1;
    const H5FD_pb_t *f2 = (const H5FD_pb_t *)_f2;
    herr_t ret_value    = 0; /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_PB_LOG_CALL(__func__);

    assert(f1);
    assert(f2);

    ret_value = H5FD_cmp(f1->file, f2->file);

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_cmp */

/*--------------------------------------------------------------------------
 * Function:    H5FD__pb_get_handle
 *
 * Purpose:     Returns a pointer to the file handle of low-level virtual
 *              file driver.
 *
 * Return:      SUCCEED/FAIL
 *--------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, void **file_handle)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);
    assert(file_handle);

    if (H5FD_get_vfd_handle(file->file, file->fa.fapl_id, file_handle) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, FAIL, "unable to get handle of file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_get_handle */

/*--------------------------------------------------------------------------
 * Function:    H5FD__pb_lock
 *
 * Purpose:     Sets a file lock.
 *
 * Return:      SUCCEED/FAIL
 *--------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_lock(H5FD_t *_file, bool rw)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file; /* VFD file struct */
    herr_t     ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    assert(file);
    assert(file->file);

    /* Place the lock on underlying file */
    if (H5FD_lock(file->file, rw) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTLOCKFILE, FAIL, "unable to lock file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_lock */

/*--------------------------------------------------------------------------
 * Function:    H5FD__pb_unlock
 *
 * Purpose:     Removes a file lock.
 *
 * Return:      SUCCEED/FAIL
 *--------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_unlock(H5FD_t *_file)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file; /* VFD file struct */
    herr_t     ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);

    if (H5FD_unlock(file->file) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTUNLOCKFILE, FAIL, "unable to unlock file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_unlock */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_ctl
 *
 * Purpose:     Page buffer VFD version of the ctl callback.
 *
 *              The desired operation is specified by the op_code
 *              parameter.
 *
 *              The flags parameter controls management of op_codes that
 *              are unknown to the callback
 *
 *              The input and output parameters allow op_code specific
 *              input and output
 *
 *              At present, this VFD supports no op codes of its own and
 *              simply passes ctl calls on to the underlying VFD
 *
 * Return:      Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_ctl(H5FD_t *_file, uint64_t op_code, uint64_t flags, const void *input, void **output)
{
    H5FD_pb_t *file      = (H5FD_pb_t *)_file;
    herr_t     ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Sanity checks */
    assert(file);

    switch (op_code) {

        /* probably want to add options for displaying and reseting stats */

        /* Unknown op code */
        default:
            if (flags & H5FD_CTL_ROUTE_TO_TERMINAL_VFD_FLAG) {

                /* Pass ctl call down to underlying VFD */

                if (H5FDctl(file->file, op_code, flags, input, output) < 0)
                    HGOTO_ERROR(H5E_VFL, H5E_FCNTL, FAIL, "VFD ctl request failed");
            }
            else {
                /* If no valid VFD routing flag is specified, fail for unknown op code
                 * if H5FD_CTL_FAIL_IF_UNKNOWN_FLAG flag is set.
                 */
                if (flags & H5FD_CTL_FAIL_IF_UNKNOWN_FLAG)
                    HGOTO_ERROR(H5E_VFL, H5E_FCNTL, FAIL,
                                "VFD ctl request failed (unknown op code and fail if unknown flag is set)");
            }

            break;
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_ctl() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_query
 *
 * Purpose:     Set the flags that this VFL driver is capable of supporting.
 *              (listed in H5FDpublic.h)
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_query(const H5FD_t *_file, unsigned long *flags /* out */)
{
    const H5FD_pb_t *file      = (const H5FD_pb_t *)_file;
    herr_t           ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    if (file) {
        assert(file);
        assert(file->file);

        if (H5FDquery(file->file, flags) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTLOCK, FAIL, "unable to query R/W file");
    }
    else {
        /* There is no file. Because this is a pure passthrough VFD,
         * it has no features of its own.
         */
        if (flags)
            *flags = 0;
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_query() */

#if 0
/* The current implementations of the alloc and free callbacks
 * cause space allocation failures in the upper library.  This 
 * has to be dealt with if we want the page buffer and encryption
 * to run with VFDs that require it (split, multi).
 * However, since targeting just the sec2 VFD is sufficient 
 * for the Phase I prototype, we will bypass the issue
 * for now.
 *                                JRM -- 9/6/24
 */
#if 0
/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_alloc
 *
 * Purpose:     Allocate file memory.
 *
 *              Handling this call as a pass through while maintaining the 
 *              eoa_up and eoa_down is made more complicated by the fact 
 *              that some VFDs treat this call as a set eoa call, and 
 *              others (multi / split file driver only?) implement it as 
 *              an addition to the current EOA (for the specified type).
 *
 *              This is a bit awkward for the page buffer.  For now, 
 *              solve this by:
 *
 *              1) pass through the alloc call.
 *
 *              2) make a get eoa call on the underlying VFD to obtain
 *                 the current eoa from below.  Call this value eoa_up
 *
 *              3) if eoa_up is not on a page boundary, increment it to
 *                 the next page boundary, set the eoa of the underlying
 *                 VFD to the is value, and save it in eoa_down.
 * 
 *              4) store the new values of eoa_up and eoa_down in *file_ptr.
 * 
 * Return:      Address of allocated space (HADDR_UNDEF if error).
 *-------------------------------------------------------------------------
 */

static haddr_t
H5FD__pb_alloc(H5FD_t *_file, H5FD_mem_t type, hid_t  dxpl_id, hsize_t size)
{
    haddr_t    eoa_up;
    haddr_t    eoa_down;
    int64_t    page_num;
    H5FD_pb_t *file_ptr  = (H5FD_pb_t *)_file; /* VFD file struct */
    haddr_t    ret_value = HADDR_UNDEF;        /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file_ptr);
    assert(H5FD_PB_MAGIC == file_ptr->magic);
    assert(file_ptr->file);

    /* 1) pass through the alloc call. */
    if ((ret_value = H5FDalloc(file_ptr->file, type, dxpl_id, size)) == HADDR_UNDEF)
        HGOTO_ERROR(H5E_VFL, H5E_CANTINIT, HADDR_UNDEF, "unable to allocate for underlying file");
    
    /* 2) make a get eoa call on the underlying VFD to obtain
     *    the current eoa from below.  Call this value eoa_up
     */
    if ((eoa_up = H5FD_get_eoa(file_ptr->file, type)) == HADDR_UNDEF)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, HADDR_UNDEF, "unable to get eoa");

    /* 3) if eoa_up is not on a page boundary, increment it to
     *    the next page boundary, set the eoa of the underlying
     *    VFD to the is value, and save it in eoa_down.
     */
    page_num = (int64_t)(eoa_up / (haddr_t)((file_ptr->fa.page_size)));

    assert(page_num >= 0);

    if ( 0 < (eoa_up % (haddr_t)(file_ptr->fa.page_size)) ) {

        page_num++;
    }

    eoa_down = ((haddr_t)(page_num)) * ((haddr_t)(file_ptr->fa.page_size));

    if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, HADDR_UNDEF, "H5FD_set_eoa failed for underlying file");

    /* 4) store the new values of eoa_up and eoa_down in *file_ptr. */
    file_ptr->eoa_up = eoa_up;
    file_ptr->eoa_down = eoa_down;

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__pb_alloc(): size = 0x%llx,  eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)size,
           (unsigned long long)(file_ptr->eoa_up),
           (unsigned long long)(file_ptr->eoa_down));
#endif

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_alloc() */

#else

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_alloc
 *
 * Purpose:     Allocate file memory.
 *
 *              Handling this call as a pass through while maintaining the 
 *              eoa_up and eoa_down is made more complicated by the fact 
 *              that there seems to be some variation in how this call 
 *              is implemented by the various VFDs.  Since the immediate
 *              target is sec2, and sec2 (and any VFD that doesn't have an
 *              alloc call) simply adds size bytes to the current EOA,
 *              the page buffer alloc call will do the same with 
 *              adjustments to the lower eoa to force it to fall on 
 *              a page boundary.
 *
 *              This will have to be re-visited if we target VFDs that
 *              don't handle their alloc calls this way.
 *
 *              Proceed as follows:
 *
 *              1) Add the size to the current eoa_up.
 *
 *              2) Set eoa_down equal to eoa_up.  If eoa_down, doesn't 
 *                 lie on a page boundary, extend it to the next page
 *                 boundary.
 *
 *              3) Call the set eoa function on the underlying VFD to 
 *                 set its EOA to eoa_down.
 *
 *              4) Store the new values of eoa_up and eoa_down in
 *                 the fields of that name in *file_ptr.
 *
 *              5) Return the new value of eoa_down.
 *
 *              Note that the calloc call of the underlying VFD is never
 *              used -- the set eoa call is used instead.
 * 
 * Return:      Address of allocated space (HADDR_UNDEF if error).
 *-------------------------------------------------------------------------
 */

static haddr_t
H5FD__pb_alloc(H5FD_t *_file, H5FD_mem_t type, hid_t H5_ATTR_UNUSED dxpl_id, hsize_t size)
{
    haddr_t    eoa_up;
    haddr_t    eoa_down;
    int64_t    page_num;
    H5FD_pb_t *file_ptr  = (H5FD_pb_t *)_file; /* VFD file struct */
    haddr_t    ret_value = HADDR_UNDEF;        /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file_ptr);
    assert(H5FD_PB_MAGIC == file_ptr->magic);
    assert(file_ptr->file);

    /* 1) Add the size to the current eoa_up. */

    eoa_up = file_ptr->eoa_up + (haddr_t)size;
 

    /* 2) Set eoa_down equal to eoa_up.  If eoa_down, doesn't 
     *    lie on a page boundary, extend it to the next page
     *    boundary.
     */
    page_num = (int64_t)(eoa_up / (haddr_t)((file_ptr->fa.page_size)));

    assert(page_num >= 0);

    if ( 0 < (eoa_up % (haddr_t)(file_ptr->fa.page_size)) ) {

        page_num++;
    }

    eoa_down = ((haddr_t)(page_num)) * ((haddr_t)(file_ptr->fa.page_size));


    /* 3) Call the set eoa function on the underlying VFD to 
     *    set its EOA to eoa_down.
     */
    if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, HADDR_UNDEF, "H5FD_set_eoa failed for underlying file");

 
    /* 4) Store the new values of eoa_up and eoa_down in
     *    the fields of that name in *file_ptr.
     */
    file_ptr->eoa_up = eoa_up;
    file_ptr->eoa_down = eoa_down;


    /* 5) Return the new value of eoa_down. */
    ret_value = eoa_up;

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__pb_alloc(): size = 0x%llx,  eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)size,
           (unsigned long long)(file_ptr->eoa_up),
           (unsigned long long)(file_ptr->eoa_down));
#endif

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_alloc() */
#endif
#endif

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_get_type_map
 *
 * Purpose:     Retrieve the memory type mapping for this file
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_get_type_map(const H5FD_t *_file, H5FD_mem_t *type_map)
{
    const H5FD_pb_t *file      = (const H5FD_pb_t *)_file;
    herr_t           ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);

    /* Retrieve memory type mapping for the underlying file */
    if (H5FD_get_fs_type_map(file->file, type_map) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, FAIL, "unable to get type map for the underlying file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_get_type_map() */

#if 0
/* The current implementations of the alloc and free callbacks
 * cause space allocation failures in the upper library.  This 
 * has to be dealt with if we want the page buffer and encryption
 * to run with VFDs that require it (split, multi).
 * However, since targeting just the sec2 VFD is sufficient 
 * for the Phase I prototype, we will bypass the issue
 * for now.
 *                                JRM -- 9/6/24
 */
/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_free
 *
 * Purpose:     Pass the free call down to the underlying VFD.
 *
 * Return:      SUCCEED/FAIL
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_free(H5FD_t *_file, H5FD_mem_t type, hid_t H5_ATTR_UNUSED dxpl_id, haddr_t addr, hsize_t size)
{
    int64_t    page_num;
    haddr_t    eoa_up;
    haddr_t    eoa_down;
    H5FD_pb_t *file_ptr  = (H5FD_pb_t *)_file; /* VFD file struct */
    herr_t     ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_PB_LOG_CALL(__func__);

    /* Check arguments */
    assert(file_ptr);
    assert(file_ptr->file);

    if ( file_ptr->eoa_up == (addr + (haddr_t)size) ) {

        eoa_up = addr;

        page_num = (int64_t)(eoa_up / (haddr_t)((file_ptr->fa.page_size)));

        assert(page_num >= 0);

        if ( 0 < (eoa_up % (haddr_t)(file_ptr->fa.page_size)) ) {

            page_num++;
        }

        eoa_down = ((haddr_t)(page_num)) * ((haddr_t)(file_ptr->fa.page_size));

        if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "H5FD_set_eoa failed for underlying file");
 
        file_ptr->eoa_up = eoa_up;
        file_ptr->eoa_down = eoa_down;
    }

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__pb_free(): addr / size = 0x%llx /0x%llx,  eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)addr,
           (unsigned long long)size,
           (unsigned long long)(file_ptr->eoa_up),
           (unsigned long long)(file_ptr->eoa_down));
#endif

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_free() */
#endif

/*-------------------------------------------------------------------------
 * Function:    H5FD__pb_delete
 *
 * Purpose:     Delete a file
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__pb_delete(const char *filename, hid_t fapl_id)
{
    const H5FD_pb_vfd_config_t *fapl_ptr     = NULL;
    H5FD_pb_vfd_config_t       *default_fapl = NULL;
    H5P_genplist_t             *plist;
    herr_t                      ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(filename);

    /* Get the driver info */
    if (H5P_FILE_ACCESS_DEFAULT == fapl_id) {

        if (NULL == (default_fapl = H5FL_CALLOC(H5FD_pb_vfd_config_t)))
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, "unable to allocate file access property list struct");

        if (H5FD__pb_populate_config(NULL, default_fapl) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "can't initialize driver configuration info");

        fapl_ptr = default_fapl;
    }
    else {

        if (NULL == (plist = (H5P_genplist_t *)H5I_object(fapl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access property list");

        if (NULL == (fapl_ptr = (const H5FD_pb_vfd_config_t *)H5P_peek_driver_info(plist))) {

            if (NULL == (default_fapl = H5FL_CALLOC(H5FD_pb_vfd_config_t)))
                HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL,
                            "unable to allocate file access property list struct");

            if (H5FD__pb_populate_config(NULL, default_fapl) < 0)
                HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, "can't initialize driver configuration info");

            fapl_ptr = default_fapl;
        }
    }

    if (H5FDdelete(filename, fapl_ptr->fapl_id) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDELETEFILE, FAIL, "unable to delete file");

done:

    if (default_fapl)
        H5FL_FREE(H5FD_pb_vfd_config_t, default_fapl);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__pb_delete() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_alloc_and_init_pageheader
 *
 * Purpose:     Allocates and initializes a H5FD_pb_pageheader_t structure.
 *
 * Return:      Success:    A pointer to the H5FD_pb_pageheader_t structure.
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
H5FD_pb_pageheader_t *
H5FD__pb_alloc_and_init_pageheader(H5FD_pb_t * file_ptr, haddr_t addr, uint32_t hash_code)
{
    H5FD_pb_pageheader_t *pageheader = NULL;
    H5FD_pb_pageheader_t *ret_value = NULL;

    FUNC_ENTER_NOAPI(FAIL)

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );

    /* Allocates space for the pageheader */
    if ( NULL == (pageheader = (H5FD_pb_pageheader_t *)malloc(sizeof(H5FD_pb_pageheader_t) +
                                                file_ptr->fa.page_size )))
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate page header");


    pageheader->magic           = H5FD_PB_PAGEHEADER_MAGIC;
    pageheader->hash_code       = hash_code;
    pageheader->ht_next_ptr     = NULL;
    pageheader->ht_prev_ptr     = NULL;
    pageheader->rp_next_ptr     = NULL;
    pageheader->rp_prev_ptr     = NULL;
    pageheader->flags           = 0;
    pageheader->page_addr       = addr;
    pageheader->type            = H5FD_MEM_DEFAULT;

    ret_value = pageheader;


done:

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_alloc_and_init_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_invalidate_pageheader
 *
 * Purpose:     When a H5FD_pb_pageheader_t structure needs to be marked 
 *              invalid. The invalid flag is set to signify that this page is 
 *              not a valid page, and it is removed from the hash table and 
 *              replacement policy (rp) list and then appended to the tail of 
 *              the replacement policy list to ensure invalid pages are the 
 *              next to be evicted.
 *
 * Return:      SUCCESS/FAIL
 *
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_invalidate_pageheader(H5FD_pb_t *file_ptr, H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    pageheader->flags |= H5FD_PB_INVALID_FLAG;
    
    if ( pageheader->flags & H5FD_PB_DIRTY_FLAG ) {

        pageheader->flags &= ~H5FD_PB_DIRTY_FLAG;
        file_ptr->rp_dirty_count--;
    }
    

    assert( 0 == ( pageheader->flags & H5FD_PB_BUSY_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_DIRTY_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_READ_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_WRITE_FLAG ));
    assert( pageheader->flags & H5FD_PB_INVALID_FLAG );

    H5FD__pb_ht_remove_pageheader( file_ptr, pageheader );
    H5FD__pb_rp_remove_pageheader( file_ptr, pageheader );
    H5FD__pb_rp_append_pageheader( file_ptr, pageheader );

    file_ptr->total_invalidated++;

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_invalidate_pageheader() */



/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_flush_page
 *
 * Purpose:     When a H5FD_pb_pageheader_t with a page flagged as dirty is 
 *              selected to be evicted from the page buffer by the replacement
 *              policy, upon the closing of the file, or any other reason a 
 *              flush needs to occur, this function is called to write the
 *              the dirty page to the file.
 *
 * Return:      void
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_flush_page(H5FD_pb_t * file_ptr, hid_t dxpl_id, H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    if ( H5FDwrite( file_ptr->file, pageheader->type, dxpl_id, pageheader->page_addr, 
                    file_ptr->fa.page_size, pageheader->page ) < 0 )
        HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, "Page could not be flushed to underlying VFD.");

    pageheader->flags &= ~H5FD_PB_DIRTY_FLAG;

    file_ptr->rp_dirty_count--;
    file_ptr->total_flushed++;

    assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_flush_page() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_get_pageheader
 *
 * Purpose:     Selects a H5FD_pb_pageheader_t structure, either by allocating 
 *              a new one if not at the maximum number of pages, or if the 
 *              maximum number of pages has been reached, by evicting a page 
 *              from a H5FD_pb_pageheader_t structure from the replacement
 *              policy list based on the selected replacement policy.
 *
 * Return:      Success:    A pointer to the H5FD_pb_pageheader_t structure.
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
H5FD_pb_pageheader_t*
H5FD__pb_get_pageheader(H5FD_pb_t *file_ptr, H5FD_mem_t type, hid_t dxpl_id, 
                        haddr_t addr, uint32_t hash_code)
{
    H5FD_pb_pageheader_t *pageheader = NULL;
    H5FD_pb_pageheader_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );


    if ( file_ptr->rp_pageheader_count < H5FD_PB_DEFAULT_MAX_NUM_PAGES ) {

        if ( NULL == (pageheader = H5FD__pb_alloc_and_init_pageheader( file_ptr, addr, hash_code )) )
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate page");

    }

    else {

        pageheader = H5FD__pb_rp_evict_pageheader( file_ptr, addr, hash_code );

        if ( NULL == pageheader ) {
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, "unable to allocate page");
        }
    }

    assert( pageheader );
    assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );
    assert( pageheader->hash_code == hash_code );
    assert( pageheader->page_addr == addr );

    assert( 0 == ( pageheader->flags & H5FD_PB_BUSY_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_INVALID_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_READ_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_WRITE_FLAG ));
    assert( 0 == ( pageheader->flags & H5FD_PB_DIRTY_FLAG ));

    pageheader->type = type;

    if (H5FDread( file_ptr->file, type, dxpl_id, addr, file_ptr->fa.page_size, pageheader->page ) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_READERROR, NULL, "Reading from underlying VFD failed");

    if (H5FD__pb_rp_insert_pageheader( file_ptr, pageheader ) != 0) {
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, NULL, "Pageheader could not be inserted into rp");
    }

    if (H5FD__pb_ht_insert_pageheader( file_ptr, pageheader ) != 0) {
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, NULL, "Pageheader could not be inserted into ht");
    }

    assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );

    file_ptr->num_pages++;

    ret_value = pageheader;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_get_pageheader() */




/*******************************/
/**** Hash Table Functions *****/
/*******************************/


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_calc_hash_code
 *
 * Purpose:     Generates a hash code for a H5FD_pb_pageheader_t structure 
 *              based on the address (addr) of the page contained within that
 *              structure, to determine which bucket to store the pageheader.
 *
 *              The hash code is calculated by discarding the lower order bits
 *              of the addr (which is based on the page size) and right
 *              shifting the remaining bits. This gives us the page number
 *              (this method is more efficient that division). The page number
 *              is then taken modulo by the number of buckets in the hash table
 *              to get the hash code for that page.
 *
 * Return:      uint32_t hash_code
 *
 *-----------------------------------------------------------------------------
 */
uint32_t
H5FD__pb_calc_hash_code(H5FD_pb_t *file_ptr, haddr_t addr)
{
    uint32_t        hash_code;
    uint64_t        lower_order_bits;
    uint64_t        page_number;
    uint32_t        ret_value;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );

    /* Gets the number of lower order bits based on the page size */
    lower_order_bits = (uint64_t)llrint(ceil(log2((double)(file_ptr->fa.page_size))));

    /* Discards the lower order bits and right shits the remaing bits */
    page_number = addr >> lower_order_bits;

    /* Mod the page number by the number of buckets in the hash table */
    hash_code = page_number % H5FD_PB_DEFAULT_NUM_HASH_BUCKETS;

    ret_value = hash_code;

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_calc_hash_code() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_ht_insert_pageheader
 *
 * Purpose:     Inserts a H5FD_pb_pageheader_t structure into the hash table 
 *              (ht) at the bucket index that matches the hash code. Currently 
 *              this insert function works by prepending the 
 *              H5FD_pb_pageheader_t structure.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_ht_insert_pageheader(H5FD_pb_t *file_ptr, 
                              H5FD_pb_pageheader_t *pageheader)
{
    uint32_t                hash_code = pageheader->hash_code;
    H5FD_pb_pageheader_t  **bucket_head_ptr = &file_ptr->ht_bucket[hash_code].ht_head_ptr;
    int32_t               *num_pages_in_bucket = &file_ptr->ht_bucket[hash_code].num_pages_in_bucket;

    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert(file_ptr);
    assert(H5FD_PB_MAGIC == file_ptr->magic);
    assert(pageheader);
    assert(H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic);

    /* If the bucket is empty, the pageheader is inserted as the head */
    if ( *bucket_head_ptr == NULL ) {

        *bucket_head_ptr = pageheader;
    }

    else {

        (*bucket_head_ptr)->ht_prev_ptr = pageheader;

        pageheader->ht_next_ptr = *bucket_head_ptr;
        
        *bucket_head_ptr = pageheader;
    }

    /* stats update*/
    assert(0 <= (*num_pages_in_bucket));

    (*num_pages_in_bucket)++;

    if ( (*num_pages_in_bucket) > (int32_t)(file_ptr->largest_num_in_bucket)) {

        file_ptr->largest_num_in_bucket = (size_t)(*num_pages_in_bucket);

    }
    /* end stats update */

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_ht_insert_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_ht_remove_pageheader
 *
 * Purpose:     Removes a H5FD_pb_pageheader_t structure from the hash table
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_ht_remove_pageheader(H5FD_pb_t *file_ptr, 
                              H5FD_pb_pageheader_t *pageheader)
{
    uint32_t               hash_code = pageheader->hash_code;
    H5FD_pb_pageheader_t **bucket_head_ptr = &file_ptr->ht_bucket[hash_code].ht_head_ptr;
    int32_t               *num_pages_in_bucket = &file_ptr->ht_bucket[hash_code].num_pages_in_bucket;


    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert(file_ptr);
    assert(H5FD_PB_MAGIC == file_ptr->magic);
    assert(pageheader);
    assert(H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic);

    assert(*bucket_head_ptr);
    assert(0 < *num_pages_in_bucket);

    if ( pageheader->ht_next_ptr ) {

        pageheader->ht_next_ptr->ht_prev_ptr = pageheader->ht_prev_ptr;
    }

    if ( pageheader->ht_prev_ptr ) {

        pageheader->ht_prev_ptr->ht_next_ptr = pageheader->ht_next_ptr;
    }

    if ( pageheader == *bucket_head_ptr ) {

        *bucket_head_ptr = pageheader->ht_next_ptr;
    }

    (*num_pages_in_bucket)--;


    pageheader->ht_prev_ptr = NULL;
    pageheader->ht_next_ptr = NULL;

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_ht_remove_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_ht_search_pageheader
 *
 * Purpose:     Searches the hash table (ht) for a H5FD_pb_pageheader_t 
 *              structure based on its address (addr) and hash_code. The 
 *              hash_code determines which bucket (the index of the hash table)
 *              to search, and the addr is unique to that page specifying
 *              exactly which page to search for. If the H5FD_pb_pageheader_t 
 *              with that hash_code and addr is found, it is returned, 
 *              otherwise NULL is returned.
 *
 * Return:      Success:    A pointer to the H5FD_pb_pageheader_t structure.
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
H5FD_pb_pageheader_t*
H5FD__pb_ht_search_pageheader(H5FD_pb_t *file_ptr, haddr_t addr, 
                              uint32_t hash_code)
{
    int32_t               search_depth; /* Stats varaibles */
    H5FD_pb_pageheader_t *pageheader = NULL;
    H5FD_pb_pageheader_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );

    pageheader = file_ptr->ht_bucket[hash_code].ht_head_ptr;
    search_depth = 0;

    while ( pageheader != NULL ) {

        assert( pageheader );
        assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );

        search_depth++;

        if ( pageheader->page_addr == addr ) {

            ret_value = pageheader;

            /* stats update */
            file_ptr->num_hits++;
            file_ptr->total_success_depth += (size_t)search_depth;
            /* end stats update*/
            break;
        }

        pageheader = pageheader->ht_next_ptr;

    } /* end while */

    if ( pageheader ) {

        assert( file_ptr->ht_bucket[hash_code].num_pages_in_bucket > 0 );
        assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );
    }


    /* stats update */
    if ( (size_t)search_depth > file_ptr->max_search_depth ) {

        file_ptr->max_search_depth = (size_t)search_depth;
    }

    if ( ret_value == NULL ) {

        file_ptr->num_misses++;
        file_ptr->total_fail_depth += (size_t)search_depth;
    }
    /* End stats update*/

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_ht_search_pageheader() */



/*******************************************/
/**** Replacement Policy List Functions ****/
/*******************************************/

/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_rp_insert_pageheader
 *
 * Purpose:     Inserts a H5FD_pb_pageheader_t strucutre into the replacement 
 *              policy (rp) list according to the selected rp or other factors 
 *              (i.e. invalid flag will cause the H5FD_pb_pageheader_t to be 
 *              inserted to the tail, making it the next evicted).
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_rp_insert_pageheader(H5FD_pb_t *file_ptr, 
                              H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    /* If the page is invalid, it's appended to be evicted next */
    if ( pageheader->flags & H5FD_PB_INVALID_FLAG )
    {
        H5FD__pb_rp_append_pageheader( file_ptr, pageheader );

        assert( pageheader->rp_next_ptr == NULL );
        assert( file_ptr->rp_tail_ptr = pageheader );
    }

    else if ( file_ptr->fa.rp == 0 || file_ptr->fa.rp == 1 ) {

        H5FD__pb_rp_prepend_pageheader( file_ptr, pageheader );

        assert( pageheader->rp_prev_ptr == NULL );
        assert( file_ptr->rp_head_ptr = pageheader );
    }

    else {
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, "unsupported replacement policy");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_rp_insert_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_rp_prepend_pageheader
 *
 * Purpose:     Prepends a H5FD_pb_pageheader_t structure to the replacement 
 *              policy (inserts it at the head of the list to be evicted last).
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_rp_prepend_pageheader(H5FD_pb_t *file_ptr, 
                                H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );


    if ( file_ptr->rp_head_ptr == NULL ) {

        file_ptr->rp_head_ptr = pageheader;

        file_ptr->rp_tail_ptr = pageheader;
    }


    else {

        file_ptr->rp_head_ptr->rp_prev_ptr = pageheader;

        pageheader->rp_next_ptr = file_ptr->rp_head_ptr;

        file_ptr->rp_head_ptr = pageheader;
    }

    file_ptr->rp_pageheader_count++; 

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_rp_prepend_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_rp_append_pageheader
 *
 * Purpose:     Appends a H5FD_pb_pageheader_t structure to the replacement 
 *              policy (inserts it at the tail of the list to be evicted next).
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_rp_append_pageheader(H5FD_pb_t *file_ptr, 
                              H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    if (file_ptr->rp_tail_ptr == NULL) {
        
        file_ptr->rp_head_ptr = pageheader;
        file_ptr->rp_tail_ptr = pageheader;
    }

    else {

        file_ptr->rp_tail_ptr->rp_next_ptr = pageheader;

        pageheader->rp_prev_ptr = file_ptr->rp_tail_ptr;
        
        file_ptr->rp_tail_ptr = pageheader;
    }

    file_ptr->rp_pageheader_count++;

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_rp_append_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_rp_remove_pageheader
 *
 * Purpose:     Removes a H5FD_pb_pageheader_t strucutre from the replacement 
 *              policy list.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_rp_remove_pageheader(H5FD_pb_t * file_ptr, 
                                H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    if ( pageheader->rp_next_ptr ) {

        pageheader->rp_next_ptr->rp_prev_ptr = pageheader->rp_prev_ptr;
    }

    if ( pageheader->rp_prev_ptr ) {

        pageheader->rp_prev_ptr->rp_next_ptr = pageheader->rp_next_ptr;
    }

    if ( file_ptr->rp_head_ptr == pageheader ) {

        file_ptr->rp_head_ptr = pageheader->rp_next_ptr;
    }

    if ( file_ptr->rp_tail_ptr == pageheader ) {

        file_ptr->rp_tail_ptr = pageheader->rp_prev_ptr;
    }

    pageheader->rp_prev_ptr = NULL;
    pageheader->rp_next_ptr = NULL;

    file_ptr->rp_pageheader_count--;

    assert( file_ptr->rp_pageheader_count >= 0 );

    FUNC_LEAVE_NOAPI(ret_value);

} /* end H5FD__pb_rp_remove_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_touch_pageheader
 *
 * Purpose:     Updates a H5FD_pb_pageheader_t structure's position in the 
 *              replacement policy (rp), depending on the selected rp.
 * 
 *              Current supported replacement policies:
 *                RP 0 == LRU (least recently used)
 *                RP 1 == FIFO (first in first out)  
 *                
 *              NOTE: FIFO does not call this function because touch doesn't
 *              affect FIFO order
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5FD__pb_rp_touch_pageheader(H5FD_pb_t *file_ptr, 
                             H5FD_pb_pageheader_t *pageheader)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );
    assert( pageheader );
    assert( H5FD_PB_PAGEHEADER_MAGIC == pageheader->magic );

    /* Replacement Policy 0 == LRU (least recently used) */
    if ( 0 == file_ptr->fa.rp ) {

        H5FD__pb_rp_remove_pageheader( file_ptr, pageheader );
        H5FD__pb_rp_prepend_pageheader( file_ptr, pageheader );
    }

    else {
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, "unsupported replacement policy");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_touch_pageheader() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__pb_rp_evict_pageheader
 *
 * Purpose:     When the maximum number of pages (and therefore 
 *              H5FD_pb_pageheader_t structures) has been reached, and a new
 *              page must be added to the page buffer, the replacement policy
 *              selects an eviction candidate, if dirty flushes the associated 
 *              page evicts it, and re-uses the H5FD_pb_pageheader_t 
 *              structure to store the new page in the page buffer.
 *              
 *              Replacement Policy 0 == LRU (least recently used)
 *              Replacement Policy 1 == FIFO (first in first out)
 *
 * Return:      void
 *-----------------------------------------------------------------------------
 */
H5FD_pb_pageheader_t*
H5FD__pb_rp_evict_pageheader(H5FD_pb_t *file_ptr, haddr_t addr, uint32_t hash_code)
{
    H5FD_pb_pageheader_t *pageheader = NULL;
    H5FD_pb_pageheader_t *ret_value = NULL;

    FUNC_ENTER_PACKAGE

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );


    /** 
     * rp_policy:
     * 0 == LRU 
     * 1 == FIFO
     */   
    /* If the policy is LRU or FIFO the eviction is done the same way */
    if ( file_ptr->rp_policy == 0 || file_ptr->rp_policy == 1 ) {

        pageheader = file_ptr->rp_tail_ptr;

        assert( pageheader);
        assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );

        /* If busy check the next H5FD_pb_pageheader_t structure in the list */
        while ( pageheader->flags == H5FD_PB_BUSY_FLAG ) {

            pageheader = pageheader->rp_prev_ptr;
        }

        /* If dirty flush to file/lower VFD before evicting */
        if ( pageheader->flags == H5FD_PB_DIRTY_FLAG ) {

            H5FD__pb_flush_page(file_ptr, H5P_DEFAULT, pageheader);

            assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );
        } 

        if ( H5FD__pb_rp_remove_pageheader( file_ptr, pageheader ) != 0 ) 
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, NULL, "Pageheader could not be removed from rp");

        /** 
         * If the H5FD_pb_pageheader_t structure is valid it must be removed from the hash table.
         * Otherwise it was removed when it was invalidated.
         */
        if ( 0 == ( pageheader->flags & H5FD_PB_INVALID_FLAG )) { 

            if ( H5FD__pb_ht_remove_pageheader( file_ptr, pageheader ) != 0 )
                HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, NULL, "Pageheader could not be removed from ht");
        }
    }

    else {

        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, NULL, "Replacement policy not supported");
    }

    pageheader->flags       = 0;
    pageheader->hash_code   = hash_code;
    pageheader->page_addr = addr;

    file_ptr->total_evictions++;

    ret_value = pageheader;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__pb_rp_evict_pageheader() */




/****************************/
/**** Testing Functions *****/
/****************************/


/*-----------------------------------------------------------------------------
 * Function:    H5FDpb_rp_eviction_check
 *
 * Purpose:     Testing function to return the H5FD_pb_pageheader_t structure 
 *              being evicted to compared it to the H5FD_pb_pageheader_t
 *              expected to be evicted, ensuring the correct instance of 
 *              H5FD_pb_pageheader_t is the one being evicted.
 *
 * Return:      Success:    A pointer to the H5FD_pb_pageheader_t structure.
 * 
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
haddr_t*
H5FD__pb_rp_eviction_check(H5FD_t *_file, haddr_t *current_rp_addrs)
{
    H5FD_pb_t            *file_ptr = (H5FD_pb_t *)_file;
    H5FD_pb_pageheader_t *pageheader = NULL;
    int32_t               i;
    haddr_t              *ret_value = NULL;

    FUNC_ENTER_PACKAGE_NOERR

    assert( file_ptr );
    assert( H5FD_PB_MAGIC == file_ptr->magic );

    pageheader = file_ptr->rp_tail_ptr;

    i = 0;
    while ( pageheader ) {

        assert( pageheader) ;
        assert( pageheader->magic == H5FD_PB_PAGEHEADER_MAGIC );

        current_rp_addrs[i] = pageheader->page_addr;
        i++;
        pageheader = pageheader->rp_prev_ptr;

    }

    ret_value = current_rp_addrs;

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FDpb_rp_eviction_check() */



