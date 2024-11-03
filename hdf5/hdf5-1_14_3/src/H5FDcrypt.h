/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by Lifeboat, LLC                                                *
 * All rights reserved.                                                      *
 *                                                                           * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The public header file for the encryption file driver (VFD)
 */

#ifndef H5FDcrypt_H
#define H5FDcrypt_H

/** Initializer for the page buffer VFD */
#define H5FD_CRYPT          (H5FDperform_init(H5FD_crypt_init))

/** Identifier for the page buffer VFD */
#define H5FD_CRYPT_VALUE H5_VFD_CRYPT

/** Semi-unique constant used to help identify Encryption Config structure pointers */
#define H5FD_CRYPT_CONFIG_MAGIC                         0x504200

/** Semi-unique constant used to help identify Encryption structure pointers */
#define H5FD_CRYPT_MAGIC                                0x504201 

/** The version of the H5FD_crypt_vfd_config_t structure used */
#define H5FD_CURR_CRYPT_VFD_CONFIG_VERSION 	            1

/** The default clear text page size in bytes */
#define H5FD_CRYPT_DEFAULT_PLAINTEXT_PAGE_SIZE		    4096

/** The default cypher text page size in bytes */
#define H5FD_CRYPT_DEFAULT_CIPHERTEXT_PAGE_SIZE	        4112

/** The default offset for the ciphertext */
#define H5FD_CRYPT_DEFAULT_CIPHERTEXT_OFFSET            8224

/** The default encryption buffer size in bytes */
#define H5FD_CRYPT_DEFAULT_ENCRYPTION_BUFFER_SIZE       65792

/** The default encryption cipher */    /* 0 is code for GCRY_CIPHER_AES256 */
#define H5FD_CRYPT_DEFAULT_CIPHER                     	0 

/** The default block size used by the default encryption cipher */
#define H5FD_CRYPT_DEFAULT_CIPHER_BLOCK_SIZE            16

/** The default key size in bytes used by the default encryption cipher */
#define H5FD_CRYPT_DEFAULT_KEY_SIZE                     32

/** The maximum key size in bytes.  This value is used to set the size of the 
    key buffer in H5FD_crypt_vfd_config_t. */
#define H5FD_CRYPT_MAX_KEY_SIZE                         1024  

#define H5FD_CRYPT_TEST_KEY             "^sÿâ,ªT]õaiÎ_}Õ¬#¾Ló;h#ÀýÁ!S²"

/** The default initialization vector (IV) size in bytes */
#define H5FD_CRYPT_DEFAULT_IV_SIZE                      16

/** The default mode of operation */    /* 0 = GCRY_CIPHER_MODE_CBC */
#define H5FD_CRYPT_DEFAULT_MODE                         0

/** The default minimum ciphertext page size in bytes */
#define H5FD_CRYPT_DEFAULT_MINIMUM_CIPHERTEXT_PAGE_SIZE 4096


/******************************************************************************
 * 
 * Structure: H5FD_crypt_vfd_config_t
 * 
 * Description: 
 * 
 * Structure to hold configuration options for the page buffer VFD.
 * 
 * Fields: 
 * 
 * magic (int32_t):
 *      Magic number to identify this struct. Must be H5FD_CRYPT_MAGIC.
 * 
 * version (unsigned int):
 *      Version number of this struct. Currently must be 
 *      H5FD_CURR_CRYPT_VFD_CONFIG_VERSION.
 * 
 * plaintext_page_size (size_t):
 *      Size of the plaintext page size in bytes.
 * 
 * ciphertext_page_size (size_t):
 *      Size of the ciphertext page size in bytes. Should be the size of 
 *      the plaintext page size plus the size of the encryption overhead 
 *      (this iteration the only thing adding to the size is the IV size).
 * 
 * encryption_buffer_size (size_t):
 *      Size of the encryption buffer in bytes. Must be a multiple of the
 *      ciphertext page size.
 * 
 * cipher (int):
 *      Integer code specifying the desired cipher. 
 *      int code: 0 = GCRY_CIPHER_AES256
 *                1 = GCRY_CIPHER_TWOFISH
 * 
 * cipher_block_size (size_t):
 *      Size of the cipher block in bytes.
 * 
 * key_size (size_t):
 *      Size of the key in bytes.
 * 
 * unit8_t key[H5FD_CRYPT_MAX_KEY_SIZE]:
 *      Buffer to hold the key. Next iteration the key will be in a secure
 *      memory pool made by the libgcrypt library.
 * 
 * iv_size (size_t):
 *      Size of the initialization vector in bytes. Normally same size as
 *      block size.
 * 
 * mode (int):
 *      Mode of operation for the encryption, which controls how multiple
 *      cipher blocks are handled. This iteration the only mode of operation
 *      is Cipher Block Chaining (CBC).
 * 
 * fapl_id (hid_t):
 *      File-access property list for setting up the VFD stack      
 * 
 ******************************************************************************
 */
typedef struct H5FD_crypt_vfd_config_t {
    int32_t      magic;            
    unsigned int version;          
    size_t plaintext_page_size;     
    size_t ciphertext_page_size;    
    size_t encryption_buffer_size;       
    int cipher;                     

    /* add fields for key, etc */
    size_t cipher_block_size;        
    size_t key_size;                

    uint8_t key[H5FD_CRYPT_MAX_KEY_SIZE];

    size_t iv_size;                 
    int mode;          

    hid_t fapl_id;                 
} H5FD_crypt_vfd_config_t;
//! <!-- [H5FD_crypt_vfd_config_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the page buffer VFD
 */
H5_DLL hid_t H5FD_crypt_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Sets the file access property list to use the page buffer driver
 *
 * \fapl_id
 * \param[in] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pset_fapl_crypt() sets the file access property list identifier,
 *          \p fapl_id, to use the page buffer driver.
 *
 *          The page buffer VFD converts randon I/O requests to paged I/O
 *          requests as required, and then relays the paged I/O requests to 
 *	    the underlying VFD stack.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pset_fapl_crypt(hid_t fapl_id, 
                                    H5FD_crypt_vfd_config_t *config_ptr);

/**
 * \ingroup FAPL
 *
 * \brief Gets bage buffer VFD configuration properties from the the file access property list
 *
 * \fapl_id
 * \param[out] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pget_fapl_crypt() retrieves a copy of the H5FD_crypt_vfd_config_t from the 
 *          indicated FAPL.
 *
 *          The page buffer VFD converts randon I/O requests to paged I/O
 *          requests as required, and then relays the paged I/O requests to 
 *	    the underlying VFD stack.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pget_fapl_crypt(hid_t fapl_id, H5FD_crypt_vfd_config_t *config_ptr);

#ifdef __cplusplus
}
#endif

#endif
