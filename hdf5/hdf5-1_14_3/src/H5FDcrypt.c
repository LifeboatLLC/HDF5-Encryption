/* i * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
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
#include "H5FDcrypt.h"    /* Encryption file driver   */
#include "H5FLprivate.h"  /* Free Lists               */
#include "H5Iprivate.h"   /* IDs                      */
#include "H5MMprivate.h"  /* Memory management        */
#include "H5Pprivate.h"   /* Property lists           */
#include <gcrypt.h>



/* The driver identification number, initialized at runtime */
static hid_t H5FD_CRYPT_g = 0;

/******************************************************************************
 *
 * Structure:   H5FD_crypt_t
 *
 * Structure used to store all information required to manage the encryption
 * VFD.
 *
 * An instance of this structure is created when the file is "opened" and 
 * discarded when the file is closed.
 *
 * The fields of this structure are discussed individually below.
 *
 * pub:	An instance of H5FD_t which contains fields common to all VFDs.
 *      It must be the first item in this structure, since at higher levels,
 *      this structure will be treated as an instance of H5FD_t.
 *
 * fa:  An instance of H5FD_crypt_vfd_config_t containing all configuration 
 *      data needed to setup and run the encryption.  This data is contained in 
 *      an instance of H5FD_encryption_vfd_config_t for convenience in the get 
 *      and set FAPL calls.
 *
 * file: Pointer to the instance of H5FD_t used to manage the underlying 
 *	VFD.  Note that this VFD may or may not be terminal (i.e. perform 
 *	actual I/O on a file).
 *
 * ciphertext_buf:  
 *      Pointer to the dynamically allocated buffer used to storing encrypted 
 *      data either loaded from file and then decrypted, on a read, or 
 *      encrypted and then written to file on a write.
 *
 *      This buffer is allocated at file open time, and is of size 
 *      fa->encryption_buffer_size.  Note that this size must be some positive 
 *      multiple of fa->ciphertext_page_size.
 *      
 *      The field should be NULL if the buffer is not allocated.
 *
 * num_ct_buf_pages: 
 *      convenience field containing the size of the ciphertext_buf in 
 *      ciphertext pages.  This field should be zero if the ciphertext_buf is 
 *      undefined, and is computed at file open time.
 *
 * ciphertext_offset: 
 *      The encrypted file has two header pages, the first of which contains 
 *      configuration data. The second header page contains known encrypted 
 *      phrase and is used to verify that the supplied key is correct.
 *
 *      As a result, the encrypted HDF5 file proper starts two ciphertext
 *      pages after the beginning of the file.  Since the ciphertext_page_size
 *      is variable, the ciphertext_offset is set to 2 * 
 *      fa.ciphertext_page_size at file open time as a convenience in 
 *      computing the base address of I/O requests to the encrypted HDf5 file.
 *
 *      ciphertext_offset is computed at file open time.
 *
 *
 * EOA / EOF management:
 *
 * The encryption VFD introduces several problems with respect to EOA / EOF 
 * management.
 *
 * 1) The most obvious of these is the difference between plain text and 
 *    cipher text page size.  Since the VFD stack above the encrypting 
 *    VFD is un-aware of the fact that the HDF5 file is encrypted, it is 
 *    necessary to interpret between the two views of the EOA and EOF above
 *    and below the encrypting VFD..
 *
 * 2) At least at present, the first two ciphertext pages of the encrypted
 *    file are used to store configuration data on the encrypted file so 
 *    as to verify that this matches that passed in through the FAPL, and 
 *    to store a known phrase to verify that the provided key is correct.
 *    
 * 3) The encryption VFD accepts only paged I/O -- plain taxt pages above,
 *    and cipher text pages below.  From a plain text page perspective, 
 *    this should be handled at higher levels in the VFD stack -- if not,
 *    the encryption VFD should flag an error as appropriate.
 *
 * All these adjustments can be done on the fly, with no need for additional
 * fields in H5FD_crypt_t.  However, the following fields are added and 
 * maintained for debugging purposes.  We may choose to remove them in 
 * the future.
 *
 * eoa_up: The current EOA as seen by the VFD directly above the encryption 
 *      VFD in the VFD stack.  This value is set to zero at file open time,
 *      and retains that value until the first set eoa call.
 *
 * eoa_down: The current EOA as seen by the VFD directly below the encryption
 *      VFD in the VFD stack. This field is set to 2 * fa.ciphertext_page_size
 *      (which is also the value of the ciphertext_offset field) at file 
 *      open time to allow the encryption VFD to read the configuration pages.  
 *
 * eof_up: The current EOF as seen by the VFD directly above the encryption
 *      VFD in the VFD stack.  Note that this value is undefined until the 
 *      first get_eof call is received -- in this case the field is set to
 *      HADDR_UNDEF.
 *
 * eof_down: The current EOF as seen by the VFD directly below the encryption
 *      VFD in the VFD stack.  Note that this value is undefined until the
 *      first get_eof call is received -- in this case the field is set to
 *      HADDR_UNDEF.
 *
 ******************************************************************************/

typedef struct H5FD_crypt_t {
    H5FD_t                   pub;
    H5FD_crypt_vfd_config_t  fa;
    H5FD_t                 * file;

    /* encryption management fields */
    unsigned char          * ciphertext_buf;   
    uint64_t                 num_ct_buf_pages;
    haddr_t                  ciphertext_offset;

    haddr_t                  eoa_up;
    haddr_t                  eoa_down;
    haddr_t                  eof_up;
    haddr_t                  eof_down;

    /* encryption statistics fields */
} H5FD_crypt_t;


H5FD_crypt_vfd_config_t test_vfd_config = {
    /* magic                  = */ H5FD_CRYPT_MAGIC,
    /* version                = */ H5FD_CURR_CRYPT_VFD_CONFIG_VERSION,
    /* plaintext_page_size    = */ H5FD_CRYPT_DEFAULT_PLAINTEXT_PAGE_SIZE,
    /* ciphertext_page_size   = */ H5FD_CRYPT_DEFAULT_CIPHERTEXT_PAGE_SIZE,
    /* encryption_buffer_size = */ H5FD_CRYPT_DEFAULT_ENCRYPTION_BUFFER_SIZE,
    /* cipher                 = */ H5FD_CRYPT_DEFAULT_CIPHER,
    /* cipher_block_size      = */ H5FD_CRYPT_DEFAULT_CIPHER_BLOCK_SIZE,
    /* key_size               = */ H5FD_CRYPT_DEFAULT_KEY_SIZE,
    /* key                    = */ H5FD_CRYPT_TEST_KEY,
    /* iv_size                = */ H5FD_CRYPT_DEFAULT_IV_SIZE,
    /* mode                   = */ H5FD_CRYPT_DEFAULT_MODE,
    /* fapl_id                = */ H5P_DEFAULT,
};


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


#define H5FD_CRYPT_DEBUG_OP_CALLS 0 /* debugging print toggle; 0 disables */

#if H5FD_CRYPT_DEBUG_OP_CALLS
#define H5FD_CRYPT_LOG_CALL(name)                                                                         \
    do {                                                                                                     \
        printf("called %s()\n", (name));                                                                     \
        fflush(stdout);                                                                                      \
    } while (0)
#else
#define H5FD_CRYPT_LOG_CALL(name) /* no-op */       
#endif                               /* H5FD_CRYPT_DEBUG_OP_CALLS */
    

/* Private functions */

/* Prototypes */
static herr_t  H5FD__crypt_term(void);
static herr_t  H5FD__crypt_populate_config(H5FD_crypt_vfd_config_t *vfd_config,
                                           H5FD_crypt_vfd_config_t       *fapl_out);
static hsize_t H5FD__crypt_sb_size(H5FD_t *_file);
static herr_t  H5FD__crypt_sb_encode(H5FD_t *_file, char *name /*out*/, unsigned char *buf /*out*/);
static herr_t  H5FD__crypt_sb_decode(H5FD_t *_file, const char *name, const unsigned char *buf);
static void   *H5FD__crypt_fapl_get(H5FD_t *_file);
static void   *H5FD__crypt_fapl_copy(const void *_old_fa);
static herr_t  H5FD__crypt_fapl_free(void *_fapl);
static H5FD_t *H5FD__crypt_open(const char *name, unsigned flags, hid_t fapl_id, haddr_t maxaddr);
static herr_t  H5FD__crypt_close(H5FD_t *_file);
static int     H5FD__crypt_cmp(const H5FD_t *_f1, const H5FD_t *_f2);
static herr_t  H5FD__crypt_query(const H5FD_t *_file, unsigned long *flags /* out */);
static herr_t  H5FD__crypt_get_type_map(const H5FD_t *_file, H5FD_mem_t *type_map);
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
static haddr_t H5FD__crypt_alloc(H5FD_t *file, H5FD_mem_t type, hid_t dxpl_id, hsize_t size);
static herr_t  H5FD__crypt_free(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, hsize_t size);
#endif
static haddr_t H5FD__crypt_get_eoa(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type);
static herr_t  H5FD__crypt_set_eoa(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, haddr_t addr);
static haddr_t H5FD__crypt_get_eof(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type);
static herr_t  H5FD__crypt_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, void **file_handle);
static herr_t  H5FD__crypt_read(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
                                   void *buf);
static herr_t  H5FD__crypt_write(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
                                    const void *buf);
static herr_t  H5FD__crypt_flush(H5FD_t *_file, hid_t dxpl_id, bool closing);
static herr_t  H5FD__crypt_truncate(H5FD_t *_file, hid_t dxpl_id, bool closing);
static herr_t  H5FD__crypt_lock(H5FD_t *_file, bool rw);
static herr_t  H5FD__crypt_unlock(H5FD_t *_file);
static herr_t  H5FD__crypt_delete(const char *filename, hid_t fapl_id);
static herr_t  H5FD__crypt_ctl(H5FD_t *_file, uint64_t op_code, uint64_t flags, const void *input,
                                  void **output);
static herr_t  H5FD__crypt_read_first_page(H5FD_crypt_t *file_ptr);
static herr_t  H5FD__crypt_decrypt_test_phrase(H5FD_crypt_t *file_ptr);
static herr_t  H5FD__crypt_encrypt_page(H5FD_crypt_t *file_ptr, unsigned char *ciphertext, 
                                    const unsigned char *plaintext);
static herr_t  H5FD__crypt_decrypt_page(H5FD_crypt_t *file_ptr, unsigned char *ciphertext, 
                                    unsigned char *plaintext);
static herr_t  H5FD__crypt_write_first_page(H5FD_crypt_t *file_ptr);
static herr_t  H5FD__crypt_write_second_page(H5FD_crypt_t *file_ptr);
static void    H5FD__crypt_init_gcrypt_library(void);

static const H5FD_class_t H5FD_crypt_g = {
    H5FD_CLASS_VERSION,              /* struct version       */
    H5FD_CRYPT_VALUE,                /* value                */
    "encryption",                    /* name                 */
    MAXADDR,                         /* maxaddr              */
    H5F_CLOSE_WEAK,                  /* fc_degree            */
    H5FD__crypt_term,                /* terminate            */
    H5FD__crypt_sb_size,             /* sb_size              */
    H5FD__crypt_sb_encode,           /* sb_encode            */
    H5FD__crypt_sb_decode,           /* sb_decode            */
    sizeof(H5FD_crypt_vfd_config_t), /* fapl_size            */
    H5FD__crypt_fapl_get,            /* fapl_get             */
    H5FD__crypt_fapl_copy,           /* fapl_copy            */
    H5FD__crypt_fapl_free,           /* fapl_free            */
    0,                               /* dxpl_size            */
    NULL,                            /* dxpl_copy            */
    NULL,                            /* dxpl_free            */
    H5FD__crypt_open,                /* open                 */
    H5FD__crypt_close,               /* close                */
    H5FD__crypt_cmp,                 /* cmp                  */
    H5FD__crypt_query,               /* query                */
    H5FD__crypt_get_type_map,        /* get_type_map         */
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
    H5FD__crypt_alloc,               /* alloc                */
    H5FD__crypt_free,                /* free                 */
#else
    NULL,                            /* alloc                */
    NULL,                            /* free                 */
#endif
    H5FD__crypt_get_eoa,             /* get_eoa              */
    H5FD__crypt_set_eoa,             /* set_eoa              */
    H5FD__crypt_get_eof,             /* get_eof              */
    H5FD__crypt_get_handle,          /* get_handle           */
    H5FD__crypt_read,                /* read                 */
    H5FD__crypt_write,               /* write                */
    NULL,                            /* read_vector          */
    NULL,                            /* write_vector         */
    NULL,                            /* read_selection       */
    NULL,                            /* write_selection      */
    H5FD__crypt_flush,               /* flush                */
    H5FD__crypt_truncate,            /* truncate             */
    H5FD__crypt_lock,                /* lock                 */
    H5FD__crypt_unlock,              /* unlock               */
    H5FD__crypt_delete,              /* del                  */
    H5FD__crypt_ctl,                 /* ctl                  */
    H5FD_FLMAP_DICHOTOMY             /* fl_map               */
};

/* Declare a free list to manage the H5FD_crypt_t struct */
H5FL_DEFINE_STATIC(H5FD_crypt_t);

/* Declare a free list to manage the H5FD_crypt_vfd_config_t struct */
H5FL_DEFINE_STATIC(H5FD_crypt_vfd_config_t);



/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_init_gcrypt_library
 *
 * Purpose:     This function calls functions from the libgcrypt library to 
 *              allocate a pool of memory that is protected from swapping to 
 *              disk and can be wiped in a secure manner. Useful for storing 
 *              sensitive data like keys.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static void
H5FD__crypt_init_gcrypt_library(void)
{
    if (!gcry_check_version(GCRYPT_VERSION)) {
        perror("libgcrypt version mismatch\n");
        exit(2);
    }

    /**
     * Suspend warnings about secure memory not being initialized 
     * This is necessary because we haven't initialized secure memory yet
     */ 
    gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);

    /* Initialize secure memory */
    gcry_control(GCRYCTL_INIT_SECMEM, 4096, 0);

    /**
     * Resume warnings about secure memory not being initialized
     * So if the secure memory pool was not initialized successfully, we will
     * get a warning
     */ 
    gcry_control(GCRYCTL_RESUME_SECMEM_WARN);

    /* Let libgcrypt know that initialization is finished */
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
}


/*-----------------------------------------------------------------------------
 * Function:    H5FD_crypt_init
 *
 * Purpose:     Initialize the page buffer driver by registering it with the 
 *              library.
 *
 * Return:      Success:    The driver ID for the page buffer driver.
 *              Failure:    Negative
 *-----------------------------------------------------------------------------
 */
hid_t
H5FD_crypt_init(void)
{
    hid_t ret_value = H5I_INVALID_HID;

    FUNC_ENTER_NOAPI_NOERR

    H5FD_CRYPT_LOG_CALL(__func__);

    if (H5I_VFL != H5I_get_type(H5FD_CRYPT_g)) {
        H5FD_CRYPT_g = H5FDregister(&H5FD_crypt_g);
        H5FD__crypt_init_gcrypt_library();
    }

    //if (H5FD__crypt_init_gcrypt_library(void) < 0)
    //    HGOTO_ERROR(H5E_VFL, H5E_CANTINIT, FAIL, "unable to initialize libgcrypt");
    


    ret_value = H5FD_CRYPT_g;

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD_crypt_init() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_term
 *
 * Purpose:     Shut down the page buffer VFD.
 *
 * Returns:     SUCCEED (Can't fail)
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_term(void)
{
    FUNC_ENTER_PACKAGE_NOERR

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Reset VFL ID */
    H5FD_CRYPT_g = 0;

    FUNC_LEAVE_NOAPI(SUCCEED)

} /* end H5FD__crypt_term() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__copy_plist
 *
 * Purpose:     Sanity-wrapped H5P_copy_plist() for underlying VFD Utility 
 *              function for operation in multiple locations.
 *
 * Return:      0 on success, -1 on error.
 *-----------------------------------------------------------------------------
 */
static int
H5FD__copy_plist(hid_t fapl_id, hid_t *id_out_ptr)
{
    int             ret_value = 0;
    H5P_genplist_t *plist_ptr = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(id_out_ptr != NULL);

    if (false == H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, -1, 
                    "not a file access property list");

    plist_ptr = (H5P_genplist_t *)H5I_object(fapl_id);

    if (NULL == plist_ptr)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, -1, 
                    "unable to get property list");

    *id_out_ptr = H5P_copy_plist(plist_ptr, false);

    if (H5I_INVALID_HID == *id_out_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_BADTYPE, -1, 
                    "unable to copy file access property list");

done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__copy_plist() */

/*-----------------------------------------------------------------------------
 * Function:    H5Pset_fapl_crypt
 *
 * Purpose:     Sets the file access property list to use the encryption driver
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5Pset_fapl_crypt(hid_t fapl_id, H5FD_crypt_vfd_config_t *vfd_config)
{
    H5FD_crypt_vfd_config_t *info      = NULL;
    H5P_genplist_t       *plist_ptr = NULL;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i*!", fapl_id, vfd_config);

    H5FD_CRYPT_LOG_CALL(__func__);

    if (H5FD_CRYPT_MAGIC != vfd_config->magic)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "invalid configuration (magic number mismatch)");

    if (H5FD_CURR_CRYPT_VFD_CONFIG_VERSION != vfd_config->version)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "invalid config (version number mismatch)");

    if (NULL == (plist_ptr = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a valid property list");

    info = H5FL_CALLOC(H5FD_crypt_vfd_config_t);

    if (NULL == info)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                    "unable to allocate file access property list struct");

    if (H5FD__crypt_populate_config(vfd_config, info) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                    "can't setup driver configuration");

    ret_value = H5P_set_driver(plist_ptr, H5FD_CRYPT, info, NULL);

done:
    if (info)
        info = H5FL_FREE(H5FD_crypt_vfd_config_t, info);

    FUNC_LEAVE_API(ret_value)

} /* end H5Pset_fapl_crypt() */

/*-----------------------------------------------------------------------------
 * Function:    H5Pget_fapl_crypt
 *
 * Purpose:     Returns information about the encryption VFD file access 
 *              property list through the instance of H5FD_crypt_vfd_config_t
 *              pointed to by config.
 *
 *              Will fail if *config is received without pre-set valid magic 
 *              and version information.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
herr_t
H5Pget_fapl_crypt(hid_t fapl_id, H5FD_crypt_vfd_config_t *config /*out*/)
{
    const H5FD_crypt_vfd_config_t *fapl_ptr     = NULL;
    H5FD_crypt_vfd_config_t       *default_fapl = NULL;
    H5P_genplist_t                *plist_ptr    = NULL;
    herr_t                         ret_value    = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ix", fapl_id, config);

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    if (true != H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, 
                    "not a file access property list");

    if (config == NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "config pointer is null");

    if (H5FD_CRYPT_MAGIC != config->magic)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "info-out pointer invalid (magic number mismatch)");

    if (H5FD_CURR_CRYPT_VFD_CONFIG_VERSION != config->version)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "info-out pointer invalid (version unsafe)");

    /* Pre-set out FAPL ID with intent to replace these values */
    config->fapl_id = H5I_INVALID_HID;

    /* Check and get the encryption VFD fapl */
    if (NULL == (plist_ptr = H5P_object_verify(fapl_id, H5P_FILE_ACCESS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, 
                    "not a file access property list");

    if (H5FD_CRYPT != H5P_peek_driver(plist_ptr))
        HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "incorrect VFL driver");

    fapl_ptr = (const H5FD_crypt_vfd_config_t *)H5P_peek_driver_info(plist_ptr);

    if (NULL == fapl_ptr) {

        if (NULL == (default_fapl = H5FL_CALLOC(H5FD_crypt_vfd_config_t)))
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                        "unable to allocate file access property list struct");

        if (H5FD__crypt_populate_config(NULL, default_fapl) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                        "can't initialize driver configuration info");

        fapl_ptr = default_fapl;
    }

    /* Copy scalar data */
    config->plaintext_page_size      = fapl_ptr->plaintext_page_size;
    config->ciphertext_page_size     = fapl_ptr->ciphertext_page_size;
    config->encryption_buffer_size   = fapl_ptr->encryption_buffer_size;
    config->cipher                   = fapl_ptr->cipher;
    config->cipher_block_size        = fapl_ptr->cipher_block_size;
    config->key_size                 = fapl_ptr->key_size;
    config->iv_size                  = fapl_ptr->iv_size;
    config->mode                     = fapl_ptr->mode;
#if 0 /* delete this -- see comment in typedef for H5FD_crypt_vfd_config_t */
    config->min_ciphertext_page_size = fapl_ptr->min_ciphertext_page_size;

    config->eof                      = fapl_ptr->eof;
    config->eoa                      = fapl_ptr->eoa;
    config->checksum                 = fapl_ptr->checksum;
#endif

    /* copy Key */
    if ( fapl_ptr->key_size > 0 ) {

        memcpy((void *)config->key, (const void *)fapl_ptr->key, 
                (size_t)fapl_ptr->key_size);
    }

    /* Copy FAPL */
    if (H5FD__copy_plist(fapl_ptr->fapl_id, &(config->fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, FAIL, "can't copy underlying FAPL");

done:
    if (default_fapl)
        H5FL_FREE(H5FD_crypt_vfd_config_t, default_fapl);

    FUNC_LEAVE_API(ret_value)

} /* end H5Pget_fapl_crypt() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_populate_config
 *
 * Purpose:    Populates a H5FD_crypt_vfd_config_t structure with the provided
 *             values, supplying defaults where values are not provided.
 *
 * Return:    Non-negative on success/Negative on failure
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_populate_config(H5FD_crypt_vfd_config_t *vfd_config, 
                            H5FD_crypt_vfd_config_t *fapl_out)
{
    H5P_genplist_t *def_plist;
    H5P_genplist_t *plist;
    bool            free_config = false;
    herr_t          ret_value   = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(fapl_out);

    /* Checks magic number */
    if ( ( vfd_config ) && ( H5FD_CRYPT_MAGIC != vfd_config->magic ) )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                            "Incorrect H5FD_crypt_vfd_config_t magic field");

    /* Checks version*/
    if ( ( vfd_config ) && 
                ( H5FD_CURR_CRYPT_VFD_CONFIG_VERSION != vfd_config->version ) )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                            "Unknown H5FD_crypt_vfd_config_t version");

    /* Checks key is an appropriate size */
    if ( ( vfd_config ) && ( H5FD_CRYPT_MAX_KEY_SIZE < vfd_config->key_size ) )
        HGOTO_ERROR(H5E_ARGS, H5E_BADRANGE, FAIL, "key_size too big");

    /** 
     * Checks the ciphertext page size is at least the size of plaintext page
     * size + IV size to store both the ciphertext and the IV.
     */
    if ( ( vfd_config ) && ( vfd_config->iv_size > 0 ) )
        if ( ( vfd_config->ciphertext_page_size < 
                    vfd_config->plaintext_page_size + vfd_config->iv_size))
            HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                            "ciphertext_page_size too small");

    /* Checks encryption buffer size is a multiple of ciphertext page size */
    if ( ( vfd_config ) && ( vfd_config->encryption_buffer_size % 
                                vfd_config->ciphertext_page_size != 0 ) )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
            "encryption_buffer_size not a multiple of ciphertext_page_size");
    
    /* add more checks as needed */

    memset(fapl_out, 0, sizeof(H5FD_crypt_vfd_config_t));

    if ( NULL == vfd_config ) {

        vfd_config = H5MM_calloc(sizeof(H5FD_crypt_vfd_config_t));

        if (NULL == vfd_config)
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                        "unable to allocate file access property list struct");

        vfd_config->magic                  = H5FD_CRYPT_MAGIC;
        vfd_config->version                = H5FD_CURR_CRYPT_VFD_CONFIG_VERSION;
        vfd_config->plaintext_page_size    = H5FD_CRYPT_DEFAULT_PLAINTEXT_PAGE_SIZE;
        vfd_config->ciphertext_page_size   = H5FD_CRYPT_DEFAULT_CIPHERTEXT_PAGE_SIZE;
        vfd_config->encryption_buffer_size = H5FD_CRYPT_DEFAULT_ENCRYPTION_BUFFER_SIZE;
        vfd_config->cipher                 = H5FD_CRYPT_DEFAULT_CIPHER;
        vfd_config->cipher_block_size      = H5FD_CRYPT_DEFAULT_CIPHER_BLOCK_SIZE;
#if 0
        vfd_config->key_size               = H5FD_CRYPT_DEFAULT_KEY_SIZE;
        vfd_config->key                    = H5FD_CRYPT_DEFAULT_KEY;
#else
        /* Should we provide a default key? -- I would think not, hence the 
         * following mod.  Note that I have made the key field into an array of
         * uint8_t -- thus the key is set to all zeros since the instance of 
         * H5FD_crypt_vfd_config_t was calloc-ed.
         */
        vfd_config->key_size               = 0;
#endif
        vfd_config->iv_size                = H5FD_CRYPT_DEFAULT_IV_SIZE;
        vfd_config->mode                   = H5FD_CRYPT_DEFAULT_MODE;

        vfd_config->fapl_id                = H5P_DEFAULT;

        free_config = true;
    }

    fapl_out->magic                  = vfd_config->magic;
    fapl_out->version                = vfd_config->version;
    fapl_out->plaintext_page_size    = vfd_config->plaintext_page_size;
    fapl_out->ciphertext_page_size   = vfd_config->ciphertext_page_size;
    fapl_out->encryption_buffer_size = vfd_config->encryption_buffer_size;
    fapl_out->cipher                 = vfd_config->cipher;
    fapl_out->cipher_block_size      = vfd_config->cipher_block_size;
    fapl_out->key_size               = vfd_config->key_size;
    fapl_out->iv_size                = vfd_config->iv_size;
    fapl_out->mode                   = vfd_config->mode;

    /* copy Key */
    if ( vfd_config->key_size > 0 ) {

        memcpy((void *)fapl_out->key, (const void *)vfd_config->key, 
                                            (size_t)vfd_config->key_size);
    }

    /* pre-set value */
    fapl_out->fapl_id               = H5P_FILE_ACCESS_DEFAULT; 

    if (NULL == (def_plist = 
                    (H5P_genplist_t *)H5I_object(H5P_FILE_ACCESS_DEFAULT)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, 
                                "not a file access property list");

    /* Set non-default underlying FAPL ID in page buffer configuration info */
    if (H5P_DEFAULT != vfd_config->fapl_id) {

        if (false == H5P_isa_class(vfd_config->fapl_id, H5P_FILE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file access list");

        fapl_out->fapl_id = vfd_config->fapl_id;

    } else {

        /* Use copy of default file access property list for underlying FAPL ID
         * The Sec2 driver is explicitly set on the FAPL ID, as the default
         * driver might have been replaced with the page buffer VFD, which
         * would cause recursion.
         */
        if ((fapl_out->fapl_id = H5P_copy_plist(def_plist, false)) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTCOPY, FAIL, 
                                            "can't copy property list");

        if (NULL == (plist = (H5P_genplist_t *)H5I_object(fapl_out->fapl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, 
                                            "not a file access property list");

        if (H5P_set_driver_by_value(plist, H5_VFD_SEC2, NULL, true) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                                "can't set default driver on underlying FAPL");
    }

done:
    if ( free_config && vfd_config ) {

        H5MM_free(vfd_config);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_populate_config() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_flush
 *
 * Purpose:     Flush underlying VFD.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_flush(H5FD_t *_file, hid_t H5_ATTR_UNUSED dxpl_id, bool closing)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Public API for dxpl "context" */
    if (H5FDflush(file->file, dxpl_id, closing) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTFLUSH, FAIL, 
                                        "unable to flush underlying file");

done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_flush() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_read
 *
 * Purpose:     Reads the specified pages from the underlying file, decrypt 
 *              them, and returns the associated plaintext.
 *
 *              Note that size must be a multiple of the plaintext page size, 
 *              and that addr must lie on a plaintext page boundary. Due to the
 *              first two pages being used to store the encryption 
 *              configuration data and the test encryption phrase, the offset 
 *              is padded to take the address and add 2 * ciphertext page size 
 *              to skip the first two pages.
 *
 * Return:      Success:    SUCCEED
 *                          The read result is written into the BUF buffer
 *                          which should be allocated by the caller.
 *
 *              Failure:    FAIL
 *                          The contents of BUF are undefined.
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_read(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, 
                    hid_t H5_ATTR_UNUSED dxpl_id, haddr_t addr, 
                    size_t size, void *buf)
{
    H5FD_crypt_t *   file_ptr      = (H5FD_crypt_t *)_file;
    H5P_genplist_t * plist_ptr = NULL;
    haddr_t          ciphertext_addr;  /* addr converted for ciphertext size */
    haddr_t          ct_addr;          /* current addr of the read */
    size_t           ciphertext_size;  /* size converted for ciphertext size */
    size_t           ct_size;          /* size currently in ciphertext buf */
    uint64_t         pages_remaining;  /* number of pages remaining */
    /* number of pages remaining in ciphertext buf */
    uint64_t         ct_buf_pages_remaining; 
    unsigned char *  pt_ptr = NULL;    /* ptr to plaintext buffer */
    unsigned char *  ct_ptr = NULL;    /* ptr to ciphertext buffer */
    herr_t           ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(file_ptr && file_ptr->pub.cls);
    assert(buf);

    /* verify the DXPL -- we don't use the dxpl_id, so do we need this? */
    if (NULL == (plist_ptr = (H5P_genplist_t *)H5I_object(dxpl_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");

    /* Check for overflow conditions -- do we need these tests? */
    if (!H5_addr_defined(addr))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "addr undefined, addr = %llu", (unsigned long long)addr);

    if (REGION_OVERFLOW(addr, size))
        HGOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, FAIL, "addr overflow, addr = %llu", 
                        (unsigned long long)addr);

    /* Checks that size is a multiple of the plaintext page size */
    if ( (size % file_ptr->fa.plaintext_page_size) != 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "size must be a multiple of the plaintext page size");

    /* Checks that addr is on a plaintext page boudnary */
    if ( (addr % file_ptr->fa.plaintext_page_size) != 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                        "addr must lie on a plaintext page boundary");

    pages_remaining = size / file_ptr->fa.plaintext_page_size;
    assert( 0 < pages_remaining );

    /* Compute the cipher text addr from the plain text addr.  Note that
     * we must add two cipher text pages to account for the two header pages.
     */
    ciphertext_addr = (addr / (haddr_t)(file_ptr->fa.plaintext_page_size)) *
                      ((haddr_t)(file_ptr->fa.ciphertext_page_size)) +
                      file_ptr->ciphertext_offset;

    /* compute the cipher text size from the plain text sizc. */
    ciphertext_size = (size / file_ptr->fa.plaintext_page_size) * 
                                        file_ptr->fa.ciphertext_page_size;

    /* Read the cipher text, decrypt it, and copy the plaintext into the 
     * provided buffer. Since the ciphertext may be larger than the ciphertext 
     * buffer, must allow for multiple reads if the ciphertext buffer must be 
     * loaded repeatedly
     */

    ct_buf_pages_remaining = 0;
    pt_ptr = (unsigned char *)buf;
    ct_ptr = NULL;
    ct_addr = ciphertext_addr;
    ct_size = 0;

    /* While loop for multiple iterations if pages are > encryption buf size */
    while ( pages_remaining > 0 ) {

         
        /* If the ciphertext pages remaining is 0 loads ciphertext pages into 
         * the buffer, either all of the pages needed or the maximum number the 
         * buffer can hold if there are too many.
         */
        if ( 0 == ct_buf_pages_remaining ) {

            /* If the number of pages remaining is larger than what the 
             * ciphertext buffer can hold
             */
            if ( pages_remaining >= file_ptr->num_ct_buf_pages ) {

                /* Load the ciphertext buf with it's maximum page size */
                ct_size = file_ptr->fa.encryption_buffer_size;
                ct_buf_pages_remaining = file_ptr->num_ct_buf_pages;

            /* If the pages remaining < ciphertext buffer size*/
            } else {

                /* Load all remaining pages into the ciphertext buffer */
                ct_size = pages_remaining * file_ptr->fa.ciphertext_page_size;
                ct_buf_pages_remaining = pages_remaining;
            }

            assert( ct_size <= file_ptr->fa.encryption_buffer_size );
            assert( ct_buf_pages_remaining <= pages_remaining );

            ct_ptr = file_ptr->ciphertext_buf;

            if ( H5FDread(file_ptr->file, type, dxpl_id, ct_addr, ct_size, 
                            (void *)(ct_ptr)) < 0 )
                HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                                        "Read of encryption buffer failed");

            /* update ct_addr for the next read */
            ct_addr += (haddr_t)ct_size;
        }

        /* Decrypt and write the ciphertext into plaintext buffer per page */
        if ( H5FD__crypt_decrypt_page(file_ptr, ct_ptr, pt_ptr) != SUCCEED )
            HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, "Can't decrypt page.");

        /* decrements the plaintext pages remaining and adjusts pointers */
        pages_remaining--;
        pt_ptr += file_ptr->fa.plaintext_page_size;

        /* decrements the ciphertext pages remaining and adjusts pointers */
        ct_buf_pages_remaining--;
        ct_ptr += file_ptr->fa.ciphertext_page_size;

    } /* while */

    /* verify that we read the correct amount of ciphertext */
    assert( (size_t)(ct_addr - ciphertext_addr) == ciphertext_size );

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_read() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_write
 *
 * Purpose:     Encrypt the supplied plaintext pages and write the 
 *              corresponding ciphertext pages to the equivalent location in 
 *              the encrypted file.
 *
 *              Note that size must be a multiple of the plaintext page size, 
 *              and that addr must lie on a plaintext page boundary.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_write(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, 
                    size_t size, const void *buf)
{
    H5FD_crypt_t *   file_ptr      = (H5FD_crypt_t *)_file;
    H5P_genplist_t * plist_ptr = NULL;
    haddr_t          ciphertext_addr;  /* addr converted for ciphertext size */
    haddr_t          ct_addr;          /* current addr of the read */
    size_t           ciphertext_size;  /* size converted for ciphertext size */
    size_t           ct_size;          /* size currently in ciphertext buf */
    uint64_t         pages_remaining;  /* number of pages remaining */
    /* number of pages remaining in ciphertext buf */
    uint64_t         ct_buf_pages_remaining; 
    const unsigned char *  pt_ptr = NULL;    /* ptr to plaintext buffer */
    unsigned char *  ct_ptr = NULL;    /* ptr to ciphertext buffer */
    herr_t           ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(file_ptr && file_ptr->pub.cls);
    assert(buf);

    /* verify the DXPL -- we don't use the dxpl_id, do we really need this? */
    if (NULL == (plist_ptr = (H5P_genplist_t *)H5I_object(dxpl_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");

    /* Checks that size is a multiple of the plaintext page size */
    if ( (size % file_ptr->fa.plaintext_page_size) != 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "size must be a multiple of the plaintext page size");

    /* Checks that addr is on a plaintext page boudnary */
    if ( (addr % file_ptr->fa.plaintext_page_size) != 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, 
                    "addr must lie on a plaintext page boundary");

    pages_remaining = size / file_ptr->fa.plaintext_page_size;
    assert( 0 < pages_remaining );

    /* Compute the cipher text addr from the plain text addr.  Note that
     * we must add two cipher text pages to account for the two header pages.
     */
    ciphertext_addr = (addr / (haddr_t)(file_ptr->fa.plaintext_page_size)) * 
                      ((haddr_t)(file_ptr->fa.ciphertext_page_size)) + 
                      file_ptr->ciphertext_offset;

    /* Compute the cipher text size from the plain text size. */
    ciphertext_size = (size / file_ptr->fa.plaintext_page_size) * 
                        file_ptr->fa.ciphertext_page_size;

    /* encrypt the plaintext and write it to the underlying file.  Since the 
     * plaintext may be larger than the ciphertext buffer, must allow for 
     * multiple writes as the ciphertext buffer fills and must be flushed.
     */

    ct_buf_pages_remaining = file_ptr->num_ct_buf_pages;
    pt_ptr = (const unsigned char *)buf;
    ct_ptr = file_ptr->ciphertext_buf;
    ct_addr = ciphertext_addr;
    ct_size = 0;

    /* While loop for multiple iterations if pages are > encryption buf size */
    while ( pages_remaining > 0 ) {

        assert(ct_buf_pages_remaining > 0);

        /* Encrypt and write the plaintext into ciphertext buffer per page */
        if ( H5FD__crypt_encrypt_page(file_ptr, ct_ptr, pt_ptr) != SUCCEED )
            HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, "Can't encrypt page.");

        /* increment ct_size and ct_ptr by the ciphertext page size */
        ct_size += file_ptr->fa.ciphertext_page_size;
        ct_ptr += file_ptr->fa.ciphertext_page_size;

        /* decrements the plaintext pages remaining and adjusts pointers */
        pages_remaining--;
        pt_ptr += file_ptr->fa.plaintext_page_size;

        /* decrements the ciphertext pages remaining */
        ct_buf_pages_remaining--;

        if ( ( 0 == pages_remaining ) || ( 0 == ct_buf_pages_remaining ) ) {

            /* Either we have filled the ciphertext buffer, or we have run out 
             * of plaintext. In either case, flush the ciphertext buffer, and 
             * update the ct variables for another pass through the ciphertext 
             * buffer.
             */
           if ( H5FDwrite(file_ptr->file, type, dxpl_id, ct_addr, ct_size, 
                          (void *)(file_ptr->ciphertext_buf)) < 0 ) 
               HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, 
                                "Write of encryption buffer failed.");

           /* adjust ct_addr for the next encryption buffer write. Similarly, 
            * update ct_ptr, ct_size, and ct_buf_pages_remaining for the next 
            * pass through the the encryption buffer.
            */

           ct_addr += (haddr_t)ct_size;
           ct_ptr = file_ptr->ciphertext_buf;
           ct_size = 0;
           ct_buf_pages_remaining = file_ptr->num_ct_buf_pages;
        } 
    } /* while */

    assert( (size_t)(ct_addr - ciphertext_addr) == ciphertext_size );

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_write() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_fapl_get
 *
 * Purpose:     Returns a file access property list which indicates how the
 *              specified file is being accessed. The return list could be used
 *              to access another file the same way.
 *
 * Return:      Success:    Ptr to new file access property list with all
 *                          members copied from the file struct.
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
static void *
H5FD__crypt_fapl_get(H5FD_t *_file)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    void      *ret_value = NULL;

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_CRYPT_LOG_CALL(__func__);

    ret_value = H5FD__crypt_fapl_copy(&(file->fa));

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_fapl_get() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_fapl_copy
 *
 * Purpose:     Copies the file access properties.
 *
 * Return:      Success:    Pointer to a new property list info structure.
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
static void *
H5FD__crypt_fapl_copy(const void *_old_fa)
{
    const H5FD_crypt_vfd_config_t *old_fa_ptr = 
                                (const H5FD_crypt_vfd_config_t *)_old_fa;
    H5FD_crypt_vfd_config_t       *new_fa_ptr = NULL;
    void                          *ret_value  = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(old_fa_ptr);

    new_fa_ptr = H5FL_CALLOC(H5FD_crypt_vfd_config_t);

    if (NULL == new_fa_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, 
                        "unable to allocate encryption VFD FAPL");

    H5MM_memcpy(new_fa_ptr, old_fa_ptr, sizeof(H5FD_crypt_vfd_config_t));

    /* Copy the FAPL */
    if (H5FD__copy_plist(old_fa_ptr->fapl_id, &(new_fa_ptr->fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, NULL, "can't copy underlying FAPL");

    ret_value = (void *)new_fa_ptr;

done:

    if (NULL == ret_value) {

        if (new_fa_ptr) {

            new_fa_ptr = H5FL_FREE(H5FD_crypt_vfd_config_t, new_fa_ptr);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_fapl_copy() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_fapl_free
 *
 * Purpose:     Releases the file access lists
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_fapl_free(void *_fapl)
{
    H5FD_crypt_vfd_config_t *fapl      = (H5FD_crypt_vfd_config_t *)_fapl;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(fapl);

    if (H5I_dec_ref(fapl->fapl_id) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDEC, FAIL, 
                            "can't close underlying FAPL ID");

    /* Free the property list */
    fapl = H5FL_FREE(H5FD_crypt_vfd_config_t, fapl);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_fapl_free() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_open
 *
 * Purpose:     Create and/or opens a file as an HDF5 file.
 *
 * Return:      Success:    A pointer to a new file data structure. The public 
 *                          fields will be initialized by the caller, which is 
 *                          always H5FD_open().
 *              Failure:    NULL
 *-----------------------------------------------------------------------------
 */
static H5FD_t *
H5FD__crypt_open(const char *name, unsigned flags, hid_t crypt_fapl_id, 
                        haddr_t maxaddr)
{
    /* page buffer VFD info */
    H5FD_crypt_t                  *file_ptr     = NULL; 
    /* Driver-specific property list */
    const H5FD_crypt_vfd_config_t *fapl_ptr     = NULL; 
    H5P_genplist_t                *plist_ptr    = NULL;
    H5FD_t                        *ret_value    = NULL;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    if (!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid file name");

    if (0 == maxaddr || HADDR_UNDEF == maxaddr)
        HGOTO_ERROR(H5E_ARGS, H5E_BADRANGE, NULL, "bogus maxaddr");

    if (ADDR_OVERFLOW(maxaddr))
        HGOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, NULL, "bogus maxaddr");

    /* H5FD_CRYPT initializes the encryption VFD if it is not already 
     * initialized.  
     */
    if (H5FD_CRYPT != H5Pget_driver(crypt_fapl_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, 
                        "driver is not encryption VFD");

    file_ptr = (H5FD_crypt_t *)H5FL_CALLOC(H5FD_crypt_t);

    if (NULL == file_ptr)
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, NULL, 
                        "unable to allocate file struct");

    /* Get the driver-specific file access properties */
    plist_ptr = (H5P_genplist_t *)H5I_object(crypt_fapl_id);

    if (NULL == plist_ptr)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, 
                        "not a file access property list");

    fapl_ptr = 
            (const H5FD_crypt_vfd_config_t *)H5P_peek_driver_info(plist_ptr);

    /* Since the encryption VFD requires a valid cipher and key to function, 
     * I'm not sure it makes sense to set this up in a default.  Thus, for now
     * at least, just fail if the fapl_ptr is NULL.
     *
     * Note: we will probably set up text string initialization for the 
     * encryption VFD -- but probably not in the prototype.
     */
    if ( ! fapl_ptr ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, 
                    "no encryption VFD configuration on FAPL");

    /* sanity check *fapl_ptr.  In principle, we shouldn't have to do much 
     * here, as sanity checking should be handled by H5Pset_fapl_crypt() and 
     * H5FD__crypt_populate_config(). That said, do at least minimal sanity 
     * checking, or set up a validate function and call it here and in 
     * H5FD__crypt_populate_config().
     */
    if (H5FD_CRYPT_MAGIC != fapl_ptr->magic)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, 
                        "invalid configuration (magic number mismatch)");

    /* Copy *fapl_ptr into file_ptr->fa.  Do this with a memcpy(), followed by 
     * touch up where required.
     */
    H5MM_memcpy(&(file_ptr->fa), fapl_ptr, sizeof(H5FD_crypt_vfd_config_t));

    /* the key is currently an array in H5FD_crypt_vfd_config_t, so it was 
     * copied by the memcopy.  If this changes, add appropriate code here.
     */

    /* copy the FAPL for the underlying VFD stack. */
    if (H5FD__copy_plist(fapl_ptr->fapl_id, &(file_ptr->fa.fapl_id)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, NULL, "can't copy underlying FAPL");

    /* Allocate encryption buffer */
    file_ptr->ciphertext_buf = 
                (unsigned char *)malloc(file_ptr->fa.encryption_buffer_size);

    if ( NULL == file_ptr->ciphertext_buf )
        HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, 
                        "unable to allocate encryption buffer");

    /* for convenience, size of encryption buf in number of ciphertext pages */
    file_ptr->num_ct_buf_pages = file_ptr->fa.encryption_buffer_size / 
                                            file_ptr->fa.ciphertext_page_size;

    /* compute file_ptr->ciphertext_offset */
    file_ptr->ciphertext_offset = (haddr_t)(2 * 
                                        file_ptr->fa.ciphertext_page_size);

    /* open the underlying VFD / file */
    if ( NULL == (file_ptr->file = H5FD_open(name, flags, fapl_ptr->fapl_id, 
                                                HADDR_UNDEF)) )
        HGOTO_ERROR(H5E_VFL, H5E_CANTOPENFILE, NULL, 
                        "unable to open underlying file");

    /* Initialize the eoa/eof up/down fields */
    file_ptr->eoa_up = 0;
    file_ptr->eoa_down = (haddr_t)file_ptr->ciphertext_offset;
    file_ptr->eof_up = HADDR_UNDEF;
    file_ptr->eof_down = HADDR_UNDEF;

    /* Set the eoa before trying to write to a file to avoid addr overflow*/
    if (H5FD__crypt_set_eoa((H5FD_t *)file_ptr, H5FD_MEM_DRAW, file_ptr->eoa_up) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, NULL, 
                                    "H5FDset_eoa failed for underlying file");

    /* If we are either truncating or creating the underlying file, we must 
     * setup the header ciphertext pages.
     */
    if ( ( H5F_ACC_TRUNC & flags ) || ( H5F_ACC_CREAT & flags ) ) {

        /* Write cipher information to the first page */
        if ( H5FD__crypt_write_first_page(file_ptr) < 0 )
            HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, NULL, 
                    "cannot write first header page to the underlying file");

        /* Write IV and test phrase to second page */
        if ( H5FD__crypt_write_second_page(file_ptr) < 0 )
            HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, NULL, 
                    "cannot write second header page to the underlying file");
    }

    /* Read the first page of the file and verify that the configuration 
     * information in the first page matches that provided in the FAPL.
     */
    if ( SUCCEED != H5FD__crypt_read_first_page(file_ptr) ) 
        HGOTO_ERROR(H5E_VFL, H5E_CANTOPENFILE, NULL, 
                    "first header page validation failed.");

    /* Decrypt second page test phrase and compare it with expected value */
    if ( SUCCEED != H5FD__crypt_decrypt_test_phrase(file_ptr) )
        HGOTO_ERROR(H5E_VFL, H5E_CANTOPENFILE, NULL, 
                                "second header page validation failed.");

    ret_value = (H5FD_t *)file_ptr;

done:

    if (NULL == ret_value) {  /* do error cleanup */

        if (file_ptr) {

            if (H5I_INVALID_HID != file_ptr->fa.fapl_id) {

                H5I_dec_ref(file_ptr->fa.fapl_id);
            }

            if (file_ptr->file) {

                H5FD_close(file_ptr->file);
            }

            H5FL_FREE(H5FD_crypt_t, file_ptr);
        }
    } /* end if error */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_open() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_close
 *
 * Purpose:     Closes the underlying file and take down the page buffer.
 *
 * Return:      Success:    SUCCEED
 *              Failure:    FAIL, file not closed.
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_close(H5FD_t *_file)
{
    H5FD_crypt_t * file_ptr      = (H5FD_crypt_t *)_file;
    herr_t         ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file_ptr);

    if (H5I_dec_ref(file_ptr->fa.fapl_id) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_ARGS, FAIL, 
                                    "can't close underlying VFD FAPL");

    if (file_ptr->file) {

        if (H5FD_close(file_ptr->file) == FAIL)
            HGOTO_ERROR(H5E_VFL, H5E_CANTCLOSEFILE, FAIL, 
                                    "unable to close underlying file");
    }

    /* discard encryption related data structures. */
    if (file_ptr->ciphertext_buf)
    {
        free(file_ptr->ciphertext_buf);
        file_ptr->ciphertext_buf = NULL;
    }

    /* Release the file info */
    file_ptr = H5FL_FREE(H5FD_crypt_t, file_ptr);
    file_ptr = NULL;

    /* Freeing the secure memory */
    gcry_control(GCRYCTL_TERM_SECMEM);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_close() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_get_eoa
 *
 * Purpose:     Returns the end-of-address marker for the file. The EOA marker 
 *              is the first address past the last byte allocated in the format
 *              address space.
 *
 *              Due to the fact that ciphertext pages are typically larger 
 *              plain text pages, and the current use of the first two cipher
 *              text pages to store configuration and test data, the EOA 
 *              above the encryption VFD is different from that below.
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
 *              first set_eoa call since the eoa is set as part of the 
 *              H5FD__crypt_open() call.
 *
 * Return:      Success:    The end-of-address-marker
 *
 *              Failure:    HADDR_UNDEF
 *-----------------------------------------------------------------------------
 */
static haddr_t
H5FD__crypt_get_eoa(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type)
{
    haddr_t             eoa_down;
    const H5FD_crypt_t *file_ptr  = (const H5FD_crypt_t *)_file;
    haddr_t             ret_value = HADDR_UNDEF;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file_ptr);
    assert(file_ptr->file);

    if ((eoa_down = H5FD_get_eoa(file_ptr->file, type)) == HADDR_UNDEF)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, HADDR_UNDEF, "unable to get eoa");

    if ( H5_addr_ne(eoa_down, file_ptr->eoa_down) ) 
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, HADDR_UNDEF, "eoa_down mismatch");

    ret_value = file_ptr->eoa_up;

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__crypt_get_eoa(): eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)(file_ptr->eoa_up), 
           (unsigned long long)(file_ptr->eoa_down));
#endif /* JRM */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_get_eoa */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_set_eoa
 *
 * Purpose:     Set the end-of-address marker for the file. This function is
 *              called shortly after an existing HDF5 file is opened in order 
 *              to tell the driver where the end of the HDF5 data is located.
 *
 *              In the encryption VFD case, we must extend the supplied EOA
 *              to the next clear text boundary, then divide by the clear text
 *              page size, add 2, multiply by the ciphertext page size, and 
 *              pass the result to the underlying VFD.  If the call is 
 *              successful, set eoa_up and eoa_down to the supplied and 
 *              computed values respectively, and then return.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_set_eoa(H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type, 
                        haddr_t addr)
{
    haddr_t eoa_up;
    haddr_t eoa_down;
    int64_t page_num;
    H5FD_crypt_t *file_ptr  = (H5FD_crypt_t *)_file;
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__)

    /* Sanity check */
    assert(file_ptr);
    assert(file_ptr->file);

    eoa_up = addr;

    page_num = (int64_t)(addr / (haddr_t)((file_ptr->fa.plaintext_page_size)));

    assert(page_num >= 0);

    if ( 0 < (addr % (haddr_t)(file_ptr->fa.plaintext_page_size)) ) {

        page_num++;
    }

    page_num += 2; /* to adjust for header pages */

    eoa_down = ((haddr_t)(page_num)) * ((haddr_t)(file_ptr->fa.ciphertext_page_size));

    if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                        "H5FDset_eoa failed for underlying file");

    file_ptr->eoa_up = eoa_up;
    file_ptr->eoa_down = eoa_down;

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__crypt_set_eoa(): addr = 0x%llx, eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)addr, (unsigned long long)(file_ptr->eoa_up), 
           (unsigned long long)(file_ptr->eoa_down));
#endif /* JRM */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_set_eoa() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_get_eof
 *
 * Purpose:     Returns the end-of-address marker for the file. The EOA marker 
 *              is the first address past the last byte allocated in the format
 *              address space.
 *
 *              As with the EOA, the encryption VFD must translate the EOF 
 *              from below the VFD to that which is expected above the 
 *              VFD.  However, there is a major problem that doesn't appear
 *              in the EOA case.
 *
 *              Specifically, the reported EOF need not be accurate -- indeed, 
 *              the underlying VFD may report HADDR_UNDEF or the maximum file 
 *              size.
 *
 *              The major uses of the get_eof call are:
 *
 *              1) Determine whether the file is empty.  This is used to 
 *                 trigger creation of a new superblock.
 *
 *              2) Deterine whether the file has been truncated, and thus
 *                 can't be opened.  This is done by comparing the reported
 *                 EOF with that stored in the superblock, and refusing to 
 *                 open if the reported EOF is smaller.
 *
 *              3) When extending the EOA, the library gets the reported 
 *                 EOA and EOF, and adds the desired increment to the 
 *                 maximum of the EOA and EOF to obtain the new EOA.
 *                 
 *              If the EOF is reported correctly, the above issues are 
 *              moot.  The EOF will be a multiple of the ciphertext 
 *              page size, which is easily converted to the same multiple
 *              of cleartext pages less two, and reported.
 *
 *              The more interesting question is how to handle errors in 
 *              the EOF.  The obvious solution is to flag an error on obvious
 *              errors (i.e. EOF less than 2 * ciphertext page size or not 
 *              a multiple of ciphertext page size) and apply the usual
 *              conversion to any EOF that passes these simple checks.
 *              The only exception to this is the error value HADDR_UNDEF, 
 *              which is simply relayed up the VFD stack if it is received 
 *              from below.
 *
 *              This is the approach I am using for now.  It may cause
 *              problems with some underlying VFD stacks, but it should 
 *              be fine with sec2.
 *                                                 JRM -- 08/20/24
 *
 * Return:      Success:    The end-of-address-marker
 *
 *              Failure:    HADDR_UNDEF
 *
 *-----------------------------------------------------------------------------
 */
static haddr_t
H5FD__crypt_get_eof(const H5FD_t *_file, H5FD_mem_t H5_ATTR_UNUSED type)
{
    haddr_t             eof_up;
    haddr_t             eof_down;
    uint64_t            num_pages;
    H5_GCC_CLANG_DIAG_OFF("cast-qual")
    H5FD_crypt_t        *file_ptr  = (H5FD_crypt_t *)_file;
    H5_GCC_CLANG_DIAG_ON("cast-qual")
    haddr_t             ret_value = HADDR_UNDEF; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file_ptr);
    assert(file_ptr->file);

    if (HADDR_UNDEF == (eof_down = H5FD_get_eof(file_ptr->file, type)))
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, HADDR_UNDEF, "unable to get eof");

    num_pages = (uint64_t)(eof_down / file_ptr->fa.ciphertext_page_size);

    if ( num_pages < 2 ) 
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, HADDR_UNDEF, "underlying EOF incompatible with an encrypted file");

    num_pages -= 2;

    if ( 0 != (eof_down % file_ptr->fa.ciphertext_page_size) ) {

        /* An encrypted file must have length some multiple of the ciphertext 
         * page size. Flag an error for now, but note that we will probably 
         * have to make some adjustments to allow for user blocks.
         */
        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, HADDR_UNDEF, "underlying EOF not a multiple of ciphertext page size");
    }

    eof_up = ((haddr_t)(num_pages)) * ((haddr_t)(file_ptr->fa.plaintext_page_size));

    file_ptr->eof_up = eof_up;
    file_ptr->eof_down = eof_down;

    ret_value = eof_up;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_get_eof */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_truncate
 *
 * Purpose:     Notify driver to truncate the file back to the allocated size.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_truncate(H5FD_t *_file, hid_t dxpl_id, bool closing)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(file);
    assert(file->file);

    if (H5FDtruncate(file->file, dxpl_id, closing) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTUPDATE, FAIL, "unable to truncate file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_truncate */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_sb_size
 *
 * Purpose:     Obtains the number of bytes required to store the driver file
 *              access data in the HDF5 superblock.
 *
 * Return:      Success:    Number of bytes required.
 *
 *              Failure:    0 if an error occurs or if the driver has no data 
 *                          to store in the superblock.
 *
 * NOTE: no public API for H5FD_sb_size, it needs to be added
 *-----------------------------------------------------------------------------
 */
static hsize_t
H5FD__crypt_sb_size(H5FD_t *_file)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    hsize_t    ret_value = 0;

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (file->file) {

        ret_value = H5FD_sb_size(file->file);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_sb_size */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_sb_encode
 *
 * Purpose:     Encode driver-specific data into the output arguments.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_sb_encode(H5FD_t *_file, char *name /*out*/, 
                        unsigned char *buf /*out*/)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (file->file && H5FD_sb_encode(file->file, name, buf) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTENCODE, FAIL, 
                            "unable to encode the superblock in file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_sb_encode */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_sb_decode
 *
 * Purpose:     Decodes the driver information block.
 *
 * Return:      SUCCEED/FAIL
 *
 * NOTE: no public API for H5FD_sb_size, need to add
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_sb_decode(H5FD_t *_file, const char *name, 
                        const unsigned char *buf)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Sanity check */
    assert(file);
    assert(file->file);

    if (H5FD_sb_load(file->file, name, buf) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDECODE, FAIL, 
                        "unable to decode the superblock in file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_sb_decode */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_cmp
 *
 * Purpose:     Compare the keys of two files.
 *
 * Return:      Success:    A value like strcmp()
 *              Failure:    Must never fail
 *-----------------------------------------------------------------------------
 */
static int
H5FD__crypt_cmp(const H5FD_t *_f1, const H5FD_t *_f2)
{
    const H5FD_crypt_t *f1 = (const H5FD_crypt_t *)_f1;
    const H5FD_crypt_t *f2 = (const H5FD_crypt_t *)_f2;
    herr_t ret_value    = 0; /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(f1);
    assert(f2);

    ret_value = H5FD_cmp(f1->file, f2->file);

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_cmp */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_get_handle
 *
 * Purpose:     Returns a pointer to the file handle of low-level virtual
 *              file driver.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, 
                        void **file_handle)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
    herr_t     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);
    assert(file_handle);

    if (H5FD_get_vfd_handle(file->file, file->fa.fapl_id, file_handle) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, FAIL, 
                                        "unable to get handle of file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_get_handle */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_lock
 *
 * Purpose:     Sets a file lock.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_lock(H5FD_t *_file, bool rw)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file; /* VFD file struct */
    herr_t        ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    assert(file);
    assert(file->file);

    /* Place the lock on each file */
    if (H5FD_lock(file->file, rw) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTLOCKFILE, FAIL, "unable to lock file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_lock */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_unlock
 *
 * Purpose:     Removes a file lock.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_unlock(H5FD_t *_file)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file; /* VFD file struct */
    herr_t        ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);

    if (H5FD_unlock(file->file) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTUNLOCKFILE, FAIL, 
                                            "unable to unlock R/W file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_unlock */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_ctl
 *
 * Purpose:     Encryption VFD version of the ctl callback.
 *
 *              The desired operation is specified by the op_code parameter.
 *
 *              The flags parameter controls management of op_codes that are 
 *              unknown to the callback
 *
 *              The input and output parameters allow op_code specific input 
 *              and output
 *
 *              At present, this VFD supports no op codes of its own and 
 *              simply passes ctl calls on to the underlying VFD
 *
 * Return:      Non-negative on success/Negative on failure
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_ctl(H5FD_t *_file, uint64_t op_code, uint64_t flags, 
                        const void *input, void **output)
{
    H5FD_crypt_t *file      = (H5FD_crypt_t *)_file;
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
                    HGOTO_ERROR(H5E_VFL, H5E_FCNTL, FAIL, 
                                                "VFD ctl request failed");
            }
            else {
                /* If no valid VFD routing flag is specified, fail for unknown 
                 * op code if H5FD_CTL_FAIL_IF_UNKNOWN_FLAG flag is set.
                 */
                if (flags & H5FD_CTL_FAIL_IF_UNKNOWN_FLAG)
                    HGOTO_ERROR(H5E_VFL, H5E_FCNTL, FAIL,
                        "VFD ctl request failed (unknown op code and fail if unknown flag is set)");
            }

            break;
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_ctl() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_query
 *
 * Purpose:     Set the flags that this VFL driver is capable of supporting.
 *              (listed in H5FDpublic.h)
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_query(const H5FD_t *_file, unsigned long *flags /* out */)
{
    const H5FD_crypt_t *file      = (const H5FD_crypt_t *)_file;
    herr_t           ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    if (file) {
        assert(file);
        assert(file->file);

        if (H5FDquery(file->file, flags) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTLOCK, FAIL, 
                                    "unable to query R/W file");

#if 0 /* zero out the data sieve flag */
        *flags = *flags & (unsigned long)(~H5FD_FEAT_DATA_SIEVE);
        *flags = *flags & (unsigned long)(~H5FD_FEAT_AGGREGATE_SMALLDATA);
#endif
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

} /* end H5FD__crypt_query() */

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
/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_alloc
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
 *              
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

 *
 * Return:      Address of allocated space (HADDR_UNDEF if error).
 *-----------------------------------------------------------------------------
 */
static haddr_t
H5FD__crypt_alloc(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, hsize_t size)
{
    haddr_t       eoa_up;
    haddr_t       eoa_down;
    haddr_t       size_down;
    int64_t       num_pages;
    H5FD_crypt_t *file_ptr  = (H5FD_crypt_t *)_file; /* VFD file struct */
    haddr_t       ret_value = HADDR_UNDEF;        /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(file_ptr);
    assert(file_ptr->file);

    /* convert the supplied size to the equivalent value in ciphertext pages,
     * rounding up to the next cipher text page boundary.
     */
    num_pages = (int64_t)(size / (haddr_t)((file_ptr->fa.plaintext_page_size)));
    
    assert(num_pages >= 0);
    
    if ( 0 < (size % (haddr_t)(file_ptr->fa.plaintext_page_size)) ) {
        
        num_pages++;
    }

    size_down = (hsize_t)(((haddr_t)num_pages) * (haddr_t)((file_ptr->fa.ciphertext_page_size)));
    

    if ( H5FDalloc(file_ptr->file, type, dxpl_id, size_down) == HADDR_UNDEF)
        HGOTO_ERROR(H5E_VFL, H5E_CANTINIT, HADDR_UNDEF, 
                                "unable to allocate for underlying file");

    /* Depending on the underlying VFD, the H5FDalloc() call may simply set 
     * the underlying EOA to size_down, or may add size_down to the current 
     * eoa.  
     * 
     * In the former case, we must extend the underlying eoa by two times
     * the ciphertext page size, set the eoa below to this value, and set 
     * eoa_up accordingly.
     *
     * In the latter case, we must extend the eoa up by num_pages clear text
     * pages, and set eoa up according.
     *
     * In either case, we must verify that eoa_up and eoa_down have the 
     * the correct relationship to each other, and return eoa_up.
     *
     * In all other case, set the lower EOA equal to file_ptr->eao_down, 
     * flag an error and return HADDR_UNDEF.
     */

    if ((eoa_down = H5FD_get_eoa(file_ptr->file, type)) == HADDR_UNDEF)
        HGOTO_ERROR(H5E_VFL, H5E_BADVALUE, HADDR_UNDEF, "unable to get eoa");

    if ( eoa_down == size_down ) { /* alloc call is equivalent to a set eoa call */

        eoa_down += 2 * file_ptr->fa.ciphertext_page_size;

        if ( eoa_down != ((haddr_t)(num_pages + 2) * (haddr_t)(file_ptr->fa.ciphertext_page_size)) ) {

            fprintf(stderr, "eoa_down     = 0x%llx\n", (unsigned long long)eoa_down);
            fprintf(stderr, "num_pages    = %lld\n", (unsigned long long)num_pages);
            fprintf(stderr, "ct_page_size = 0x%lld\n", 
                    (unsigned long long)(file_ptr->fa.ciphertext_page_size));
        }

        assert( eoa_down == ((haddr_t)(num_pages + 2) * (haddr_t)(file_ptr->fa.ciphertext_page_size)) );

        if (H5FD_set_eoa(file_ptr->file, type, eoa_down) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, HADDR_UNDEF, "H5FDset_eoa failed for underlying file");

        eoa_up = (haddr_t)num_pages * (haddr_t)(file_ptr->fa.plaintext_page_size);

        file_ptr->eoa_down = eoa_down;
#if 0
        file_ptr->eoa_up   = eoa_up;

        ret_value = eoa_up;
#else 
        file_ptr->eoa_up   = size;

        ret_value = size;
#endif

    } else if ( eoa_down == (file_ptr->eoa_down + size_down) ) { /* alloc adds addr to current eoa */

        if ( 0 != (eoa_down % file_ptr->fa.ciphertext_page_size) ) {

            fprintf(stderr, "\neoa_down                    = 0x%llx\n", (unsigned long long)eoa_down);
            fprintf(stderr, "ct_page_size                = 0x%llx\n", 
                    (unsigned long long)(file_ptr->fa.ciphertext_page_size));
            fprintf(stderr, "eoa_down mod ct_page_size   = 0x%llx\n", 
                    (unsigned long long)(eoa_down % file_ptr->fa.ciphertext_page_size));
        }

        assert( 0 == (eoa_down % file_ptr->fa.ciphertext_page_size) );

        num_pages = (int64_t)(eoa_down / file_ptr->fa.ciphertext_page_size);

        assert( 2 <= num_pages );

        eoa_up = ((haddr_t)(num_pages - 2)) * (haddr_t)(file_ptr->fa.plaintext_page_size);

        file_ptr->eoa_down = eoa_down;
        file_ptr->eoa_up   = eoa_up;

        ret_value = eoa_up;

    } else { /* un-expected eoa_down -- fail */

        HGOTO_ERROR(H5E_VFL, H5E_CANTSET, HADDR_UNDEF, "un-expected eoa for underlying file");
    }

done:

#if 0 /* JRM */
    fprintf(stderr, "\nH5FD__crypt_alloc(): size = 0x%llx, eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)size, (unsigned long long)(file_ptr->eoa_up), 
           (unsigned long long)(file_ptr->eoa_down));
#endif /* JRM */

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_alloc() */
#endif

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_get_type_map
 *
 * Purpose:     Retrieve the memory type mapping for this file
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_get_type_map(const H5FD_t *_file, H5FD_mem_t *type_map)
{
    const H5FD_crypt_t *file      = (const H5FD_crypt_t *)_file;
    herr_t           ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(file);
    assert(file->file);

    /* Retrieve memory type mapping for the underlying file */
    if (H5FD_get_fs_type_map(file->file, type_map) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTGET, FAIL, 
                            "unable to get type map for the underlying file");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_get_type_map() */

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
/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_free
 *
 * Purpose:     Pass the free call down to the underlying VFD.
 *
 * Return:      SUCCEED/FAIL
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_free(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, 
                    hsize_t size)
{
    H5FD_crypt_t *file_ptr = (H5FD_crypt_t *)_file; /* VFD file struct */
    herr_t     ret_value   = SUCCEED;            /* Return value */

    FUNC_ENTER_PACKAGE

    H5FD_CRYPT_LOG_CALL(__func__);

    /* Check arguments */
    assert(file_ptr);
    assert(file_ptr->file);

    if (H5FDfree(file_ptr->file, type, dxpl_id, addr, size) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTFREE, FAIL, 
                            "unable to free space in the underlying file");

done:

#if 0 /* JRM */
    fprintf(stderr, 
            "\nH5FD__crypt_free(): addr / size = 0x%llx /0x%llx,  eoa_up = 0x%llx, eoa_down = 0x%llx\n",
           (unsigned long long)addr,
           (unsigned long long)size,
           (unsigned long long)(file_ptr->eoa_up),
           (unsigned long long)(file_ptr->eoa_down));
#endif

    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5FD__crypt_free() */
#endif

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_delete
 *
 * Purpose:     Delete a file
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_delete(const char *filename, hid_t fapl_id)
{
    const H5FD_crypt_vfd_config_t *fapl_ptr     = NULL;
    H5FD_crypt_vfd_config_t       *default_fapl = NULL;
    H5P_genplist_t             *plist;
    herr_t                      ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    assert(filename);

    /* Get the driver info */
    if (H5P_FILE_ACCESS_DEFAULT == fapl_id) {

        if (NULL == (default_fapl = H5FL_CALLOC(H5FD_crypt_vfd_config_t)))
            HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                        "unable to allocate file access property list struct");

        if (H5FD__crypt_populate_config(NULL, default_fapl) < 0)
            HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                        "can't initialize driver configuration info");

        fapl_ptr = default_fapl;
    }
    else {

        if (NULL == (plist = (H5P_genplist_t *)H5I_object(fapl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, 
                        "not a file access property list");

        if (NULL == (fapl_ptr = 
                (const H5FD_crypt_vfd_config_t *)H5P_peek_driver_info(plist))){

            if (NULL == (default_fapl = H5FL_CALLOC(H5FD_crypt_vfd_config_t)))
                HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL,
                        "unable to allocate file access property list struct");

            if (H5FD__crypt_populate_config(NULL, default_fapl) < 0)
                HGOTO_ERROR(H5E_VFL, H5E_CANTSET, FAIL, 
                        "can't initialize driver configuration info");

            fapl_ptr = default_fapl;
        }
    }

    if (H5FDdelete(filename, fapl_ptr->fapl_id) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTDELETEFILE, FAIL, 
                            "unable to delete file");

done:

    if (default_fapl)
        H5FL_FREE(H5FD_crypt_vfd_config_t, default_fapl);

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_delete() */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_read_first_page
 *
 * Purpose:     Reads the first page of the file to get the cipher 
 *              configuration details (plaintext size, ciphertext size, 
 *              encryption buffer size, cipher, cipher block size, key size, 
 *              iv size, mode of operation, minimum ciphertext page size). Then 
 *              it validates the configuration details by comparing them with 
 *              the fapl information. 
 * 
 *              If the validation fails, return FAIL. Otherwise return SUCCEED.
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_read_first_page(H5FD_crypt_t *file_ptr)
{
    /* temp location for parsed data */
    H5FD_crypt_vfd_config_t parsed_config;       
    herr_t                  ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(file_ptr->fa.ciphertext_page_size <= 
                            file_ptr->fa.encryption_buffer_size);
    assert(file_ptr->ciphertext_buf); /* verify the buffer was allocated */

    /* Read the first page of the file */
    if (H5FD_read(file_ptr->file, H5FD_MEM_DRAW, (haddr_t)0, 
                    file_ptr->fa.ciphertext_page_size, 
                    (void *)(file_ptr->ciphertext_buf)) < 0 )
        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                            "can't read first header page");

    /* Parse to get the encryption algorithm and mode */
    if (sscanf((char *)file_ptr->ciphertext_buf,
            "plaintext_page_size: %zu\n"
            "ciphertext_page_size: %zu\n"
            "encryption_buffer_size: %zu\n"
            "cipher: %d\n"
            "cipher_block_size: %zu\n"
            "key_size: %zu\n"
            "iv_size: %zu\n"
            "mode: %d\n",
            &parsed_config.plaintext_page_size,
            &parsed_config.ciphertext_page_size,
            &parsed_config.encryption_buffer_size,
            &parsed_config.cipher,
            &parsed_config.cipher_block_size,
            &parsed_config.key_size,
            &parsed_config.iv_size,
            &parsed_config.mode) != 8) {

        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                                "can't parse first header page");
    }

    /* Validate the parsed details */
    if (parsed_config.plaintext_page_size       != file_ptr->fa.plaintext_page_size ||
        parsed_config.ciphertext_page_size      != file_ptr->fa.ciphertext_page_size ||
        parsed_config.encryption_buffer_size    != file_ptr->fa.encryption_buffer_size ||
        parsed_config.cipher                    != file_ptr->fa.cipher ||
        parsed_config.cipher_block_size         != file_ptr->fa.cipher_block_size ||
        parsed_config.key_size                  != file_ptr->fa.key_size ||
        parsed_config.iv_size                   != file_ptr->fa.iv_size ||
        parsed_config.mode                      != file_ptr->fa.mode) {

        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                            "First header page / FAPL config mismatch");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_read_first_page() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_decrypt_test_phrase
 *
 * Purpose:     Reads the second page of the file and decrypts it to compare 
 *              the expected test phrase with the decrypted phrase to validate 
 *              the decryption process.
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_decrypt_test_phrase(H5FD_crypt_t *file_ptr)
{
    unsigned char * decrypted_page = NULL; /* buffer for decrypted page */
    const char *    expected_phrase = "Decryption works";  
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    if ( NULL == (decrypted_page = 
        (unsigned char *)malloc((size_t)(file_ptr->fa.plaintext_page_size))) ) 

        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                            "can't allocate buffer for plaintext page");


    /* Read the encrypted second page of the file into the ciphertext buffer */
    if (H5FD_read(file_ptr->file, H5FD_MEM_DRAW, 
                    (haddr_t)(file_ptr->fa.ciphertext_page_size),
                    file_ptr->fa.ciphertext_page_size, 
                    (void *)(file_ptr->ciphertext_buf)) < 0 )

        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                                "can't read second header page");


    /* Decrypt and write the second page into decrypted_page buffer */
    if ( H5FD__crypt_decrypt_page(file_ptr, file_ptr->ciphertext_buf, 
                                    decrypted_page) != SUCCEED ) 

        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                                "can't decrypt second header page");

    /* Compare the decrypted phrase with the expected phrase */
    if ( memcmp(decrypted_page, expected_phrase, strlen(expected_phrase)) != 0)

        HGOTO_ERROR(H5E_VFL, H5E_READERROR, FAIL, 
                        "Unexpected test phrase in second header page.");

done:

    if ( decrypted_page ) {

        free(decrypted_page);
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_decrypt_test_phrase() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_decrypt_page
 *
 * Purpose:     Decrypts a page of data using the cipher configuration details 
 *              read from the first page of the file. The function initializes 
 *              a handle variable of type gcry_cipher_hd_t which is used to 
 *              store the context for the cryptographic operations.
 *
 *              The key is loaded from the fapl (test_vfd_config struct) and 
 *              is set to the handle for encryption.
 *
 *              An Initialization Vector (IV) is generated and is stored in the
 *              first block of the ciphertext buffer, and then also set to the 
 *              handle for encryption.
 *
 *              The input data (the ciphertext page data starting after the IV 
 *              block in the ciphertext_buf) is decrypted and stored in the 
 *              output buffer (plaintext_buf).
 *
 *              NOTE: each cipher function is followed by a call to 
 *              handle_gcrypt_error(err) which checks for errors and converts 
 *              them to a string message.
 *
 * Cipher Integer List:
 *              Cipher = 0 means AES256
 *              Cipher = 1 means TWOFISH 
 * 
 * Mode Integer List:
 *              Mode = 0 means CBC
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_decrypt_page(H5FD_crypt_t *file_ptr, unsigned char *ciphertext_buf,
                            unsigned char *plaintext_buf)
{   
    char error_string[256];
    gcry_cipher_hd_t handle; 
    gcry_error_t err;
    herr_t   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)
    
    /***********************************/
    /********** AES256 CIPHER **********/
    /***********************************/
    if ( file_ptr->fa.cipher == 0 ) { 

        /* Initializes the handle with the cipher and mode (AES256, CBC) */
        if ( 0 != (err = gcry_cipher_open(&handle, GCRY_CIPHER_AES256, 
                                            GCRY_CIPHER_MODE_CBC, 0)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Sets the key to the handle for the decryption */
        if ( 0 != (err = gcry_cipher_setkey(handle, test_vfd_config.key, 32)) )
        {
            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                                gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /*  Sets the IV to the handle for decryption using the IV read above */
        if ( 0 != (err = gcry_cipher_setiv(handle, ciphertext_buf, 16)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Decrypts the ciphertext page using the handle */
        if ( 0 != (err = gcry_cipher_decrypt(
                handle,                                /* cipher config */
                plaintext_buf,                         /* plaintext */
                file_ptr->fa.plaintext_page_size,      /* plaintext size */
                ciphertext_buf + file_ptr->fa.iv_size, /* ciphertext */
                file_ptr->fa.plaintext_page_size       /* ciphertext size */
                )) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        gcry_cipher_close(handle);

    } 

    /************************************/
    /********** TWOFISH CIPHER **********/
    /************************************/
    else if ( file_ptr->fa.cipher == 1 ) { 

        /* Initializes the handle with the cipher and mode (TWOFISH, CBC)*/
        if ( 0 != (err = gcry_cipher_open(&handle, GCRY_CIPHER_TWOFISH, 
                                            GCRY_CIPHER_MODE_CBC, 0)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Sets the key to the handle for the decryption */
        if ( 0 != (err = gcry_cipher_setkey(handle, test_vfd_config.key, 32)) )
        {
            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Sets the IV to the handle for decryption using the IV read above */
        if ( 0 != (err = gcry_cipher_setiv(handle, ciphertext_buf, 16)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Decrypts the ciphertext page using the handle */
        if ( 0 != (err = gcry_cipher_decrypt(
                handle,                                /* cipher config */
                plaintext_buf,                         /* plaintext */
                file_ptr->fa.plaintext_page_size,      /* plaintext size */
                ciphertext_buf + file_ptr->fa.iv_size, /* ciphertext */
                file_ptr->fa.plaintext_page_size       /* ciphertext size */
                )) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

    }
    else {

        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, "Unknown cipher");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_decrypt_page() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_encrypt_page
 *
 * Purpose:     Encrypts a page of data using the cipher configurationdetails 
 *              read from the first page of the file. The function initializes 
 *              a handle variable of type gcry_cipher_hd_t which is used to 
 *              store the context for the cryptographic operations.
 *
 *              The key is loaded from the fapl (test_vfd_config struct) and is
 *              set to the handle for encryption.
 *
 *              An Initialization Vector (IV) is generated and is stored in the 
 *              first block of the ciphertext buffer, and then also set to the 
 *              handle for encryption.
 *
 *              The input data (plaintext page in the plaintext_buf) is 
 *              encrypted and stored in the output buffer (ciphertext_buf) 
 *              after the IV block.
 *
 *              NOTE: each cipher function is followed by a call to 
 *              handle_gcrypt_error(err) which checks for errors and converts 
 *              them to a string message.
 *
 * Cipher Integer List:
 *              Cipher = 0 means AES256
 *              Cipher = 1 means TWOFISH 
 * 
 * Mode Integer List:
 *              Mode = 0 means CBC
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_encrypt_page(H5FD_crypt_t *file_ptr, unsigned char *ciphertext_buf, 
                         const unsigned char *plaintext_buf)
{
    char error_string[256];
    gcry_cipher_hd_t handle;
    gcry_error_t     err;
    herr_t   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /***********************************/
    /********** AES256 CIPHER **********/
    /***********************************/
    if (file_ptr->fa.cipher == 0) {

        /* Initializes the handle with the cipher and mode (AES256, CBC) */
        if ( 0 != (err = gcry_cipher_open(&handle, GCRY_CIPHER_AES256, 
                                            GCRY_CIPHER_MODE_CBC, 0)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                            gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Sets the key to the handle for the encryption */
        if ( 0 != (err = gcry_cipher_setkey(handle, test_vfd_config.key, 32)) ) 
        {
            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        } 

        /* Generates a random IV (nonce = number used once) for each page */
        gcry_create_nonce(ciphertext_buf, 16);

        /* Set the IV to the handle */
        if ( 0 != (err = gcry_cipher_setiv(handle, ciphertext_buf, 16)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Encrypts the plaintext page using the handle */
        if ( 0 != (err = gcry_cipher_encrypt(
                handle,                                /* cipher config */
                ciphertext_buf + file_ptr->fa.iv_size, /* ciphertext */ 
                file_ptr->fa.plaintext_page_size,      /* ciphertext size */
                plaintext_buf,                         /* plaintext */
                file_ptr->fa.plaintext_page_size       /* plaintext size */
                )) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        gcry_cipher_close(handle);

    } 

    /************************************/
    /********** TWOFISH CIPHER **********/
    /************************************/
    else if (file_ptr->fa.cipher == 1) { 

        /* Initializes the handle with the cipher and mode (TWOFISH, CBC) */
        if ( 0 != (err = gcry_cipher_open(&handle, GCRY_CIPHER_TWOFISH, 
                                            GCRY_CIPHER_MODE_CBC, 0)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Sets the key to the handle for the encryption */
        if ( 0 != (err = gcry_cipher_setkey(handle, test_vfd_config.key, 32)) ) 
        {
            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Generates a random IV (nonce = number used once) for each page */
        gcry_create_nonce(ciphertext_buf, 16);

        // Set the IV to the handle
        if ( 0 != (err = gcry_cipher_setiv(handle, ciphertext_buf, 16)) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        /* Encrypts the plaintext page using the handle */
        if ( 0 != (err = gcry_cipher_encrypt(
                handle,                                /* cipher config */
                ciphertext_buf + file_ptr->fa.iv_size, /* ciphertext */ 
                file_ptr->fa.plaintext_page_size,      /* ciphertext size */
                plaintext_buf,                         /* plaintext */
                file_ptr->fa.plaintext_page_size       /* plaintext size */
                )) ) {

            snprintf(error_string, (size_t)256, "gcrypt error: %s", 
                        gcry_strerror(err));
            HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, error_string);
        }

        gcry_cipher_close(handle);
    }

    else {

        HGOTO_ERROR(H5E_VFL, H5E_SYSTEM, FAIL, "Unknown cipher");
    }

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_encrypt_page() */


/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_write_first_page
 *
 * Purpose:     Writes the cipher configuration details from the fapl to the 
 *              first page of the file
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_write_first_page(H5FD_crypt_t *file_ptr)
{
    herr_t   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(file_ptr->fa.ciphertext_page_size <= 
                file_ptr->fa.encryption_buffer_size);
    assert(file_ptr->ciphertext_buf); /* verify the buffer was allocated */

    /* initialize the first page of the cipher text buffer with NULLs */
    memset((void *)(file_ptr->ciphertext_buf), '\0', 
                    file_ptr->fa.ciphertext_page_size);


    /* 512 bytes is larger than the data produced to write to the page 
     * this double checks the ciphertext page is large enough 
     */
    assert(file_ptr->fa.ciphertext_page_size > 512);

    /* Write configuration data to the ciphertext buffer.  We are using the 
     * ciphertext buffer to assemble plaintext configuration data for write to 
     * the first page of the file, which is an odd use of it.  However, it 
     * exists and we know it is big enough, so why not. 
     */
    snprintf((char *)file_ptr->ciphertext_buf, 
                    test_vfd_config.ciphertext_page_size,
             "plaintext_page_size: %zu\n"
             "ciphertext_page_size: %zu\n"
             "encryption_buffer_size: %zu\n"
             "cipher: %d\n"
             "cipher_block_size: %zu\n"
             "key_size: %zu\n"
             "iv_size: %zu\n"
             "mode: %d\n",
             file_ptr->fa.plaintext_page_size,
             file_ptr->fa.ciphertext_page_size,
             file_ptr->fa.encryption_buffer_size,
             file_ptr->fa.cipher,
             file_ptr->fa.cipher_block_size,
             file_ptr->fa.key_size,
             file_ptr->fa.iv_size,
             file_ptr->fa.mode);

    /* Right now we are putting the header pages at offset 0.  Need to think 
     * on how this will interact with user blocks.  No need to solve this now, 
     * but must address it in phase 2, if we get it.
     */
    if (H5FD_write(file_ptr->file, H5FD_MEM_DRAW, (haddr_t)0, 
                    file_ptr->fa.ciphertext_page_size, 
                    (void *)(file_ptr->ciphertext_buf)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, 
                        "Write of first header page failed.");

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_write_first_page */

/*-----------------------------------------------------------------------------
 * Function:    H5FD__crypt_write_second_page
 *
 * Purpose:     Encrypts the test phrase and writes it (and the IV in the first
 *              block on the page) to the second page of the file.
 *
 * Return:      SUCCEED/FAIL
 *
 *-----------------------------------------------------------------------------
 */
static herr_t
H5FD__crypt_write_second_page(H5FD_crypt_t *file_ptr)
{   
    const char *test_phrase = "Decryption works";
    unsigned char * test_phrase_buf = NULL;
    herr_t   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(file_ptr->fa.ciphertext_page_size <= 
                file_ptr->fa.encryption_buffer_size);
    assert(file_ptr->ciphertext_buf); /* verify the buffer was allocated */
    assert(file_ptr->fa.plaintext_page_size > strlen(test_phrase));

    /* initialize the first page of the cipher text buffer with NULLs */
    memset((void *)(file_ptr->ciphertext_buf), '\0', 
                file_ptr->fa.ciphertext_page_size);

    /* Allocate the test phrase buffer.  Don't do it on the stack since the 
     * plaintext page size is unbounded, and may blow out the stack.
     */
    test_phrase_buf = (unsigned char *)calloc(file_ptr->fa.ciphertext_page_size, 
                        sizeof(unsigned char));
  
    if ( NULL == test_phrase_buf )
        HGOTO_ERROR(H5E_VFL, H5E_CANTALLOC, FAIL, 
                        "unable to allocate the test phrase buffer");
    
    /* Copy the test phrase to the buffer after the IV */
    memcpy(test_phrase_buf, test_phrase, strlen(test_phrase) + 1);
    
    /* Encrypt the buffer */
    if (H5FD__crypt_encrypt_page(file_ptr, file_ptr->ciphertext_buf, 
                                    test_phrase_buf) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, 
                        "Can't encrypt the second header page.");

    if (H5FDwrite(file_ptr->file, H5FD_MEM_DRAW, H5P_DEFAULT, 
                    (haddr_t)(file_ptr->fa.ciphertext_page_size), 
                    file_ptr->fa.ciphertext_page_size, 
                    (void *)(file_ptr->ciphertext_buf)) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_WRITEERROR, FAIL, 
                            "Write of second header page failed.");

done:

    if ( test_phrase_buf ) {

        free(test_phrase_buf);
        test_phrase_buf = NULL;
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5FD__crypt_write_second_page() */


