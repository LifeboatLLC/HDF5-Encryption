/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * Purpose:	The public header file for the page buffer file driver (VFD)
 */

#ifndef H5FDpb_H
#define H5FDpb_H

/** Semi-unique constant used to help identify Page Buffer Config structure pointers */
#define H5FD_PB_CONFIG_MAGIC                0x504200

/** Semi-unique constant used to help identify Page Buffer structure pointers */
#define H5FD_PB_MAGIC                       0x504201

/** Initializer for the page buffer VFD */
#define H5FD_PB (H5FDperform_init(H5FD_pb_init))

/** Identifier for the page buffer VFD */
#define H5FD_PB_VALUE H5_VFD_PB

/** The version of the H5FD_pb_vfd_config_t structure used */
#define H5FD_CURR_PB_VFD_CONFIG_VERSION 	   1

/** The default page buffer page size in bytes */
#define H5FD_PB_DEFAULT_PAGE_SIZE		4096

/** The default maximum number of pages resident in the page buffer at any one time */
#define H5FD_PB_DEFAULT_MAX_NUM_PAGES	  	  64

/** The default default replacement policy to be used by the page buffer */
#define H5FD_PB_DEFAULT_REPLACEMENT_POLICY 	   0 /* dummy value -- replace this */


/******************************************************************************
 * Structure:   H5FD_pb_vfd_config_t
 *
 * Description:
 *
 * Configuration options for setting up the page buffer VFD.
 *
 * Fields:
 *
 * magic (int):
 *      Magic number to identify this struct. Must be H5FD_PB_CONFIG_MAGIC.
 *
 * version (unsigned int):
 *      Version number of this struct. Currently must be
 *      H5FD_CURR_PB_VFD_CONFIG_VERSION.
 *
 * page_size (size_t):
 *      Size of pages in the page buffer.
 *
 * max_num_pages (int):
 *      Maximum number of pages resident in the page buffer.
 *
 * rp (int):
 *      Integer code specifying the replacement policy to be used by the page
 *      buffer.
 *      Integer code: 0 = Least Recently Used (LRU)
 *                    1 = First In First Out (FIFO)
 *
 ******************************************************************************
 */
//! <!-- [H5FD_pb_vfd_config_t_snip] -->
/**
 * Configuration options for setting up the page buffer VFD
 */
typedef struct H5FD_pb_vfd_config_t {
    int32_t      magic;         /**< Magic number to identify this struct. Must be \p H5FD_PB_CONFIG_MAGIC. */
    unsigned int version;       /**< Version number of this struct. Currently must be \p
                                     H5FD_CURR_PB_VFD_CONFIG_VERSION. */
    size_t       page_size;     /**< Size of pages in the page buffer. */
    size_t       max_num_pages; /**< Maximum number of pages resident in the page buffer. */
    int32_t      rp;            /**< Integer code specifying the replacement policy to be used \p
                                     by the page buffer. */
    hid_t fapl_id;              /**< File-access property list for setting up the VFD stack below the page \p
                                     buffer VFD. Can be H5P_DEFAULT. */
} H5FD_pb_vfd_config_t;
//! <!-- [H5FD_pb_vfd_config_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/** @private
 *
 * \brief Private initializer for the page buffer VFD
 */
H5_DLL hid_t H5FD_pb_init(void);

/**
 * \ingroup FAPL
 *
 * \brief Sets the file access property list to use the page buffer driver
 *
 * \fapl_id
 * \param[in] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pset_fapl_pb() sets the file access property list identifier,
 *          \p fapl_id, to use the page buffer driver.
 *
 *          The page buffer VFD converts randon I/O requests to paged I/O
 *          requests as required, and then relays the paged I/O requests to 
 *	    the underlying VFD stack.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pset_fapl_pb(hid_t fapl_id, H5FD_pb_vfd_config_t *config_ptr);

/**
 * \ingroup FAPL
 *
 * \brief Gets bage buffer VFD configuration properties from the the file access property list
 *
 * \fapl_id
 * \param[out] config_ptr Configuration options for the VFD
 * \returns \herr_t
 *
 * \details H5Pget_fapl_pb() retrieves a copy of the H5FD_pb_vfd_config_t from the 
 *          indicated FAPL.
 *
 *          The page buffer VFD converts randon I/O requests to paged I/O
 *          requests as required, and then relays the paged I/O requests to 
 *	    the underlying VFD stack.
 *
 * \since 1.10.7, 1.12.1
 */
H5_DLL herr_t H5Pget_fapl_pb(hid_t fapl_id, H5FD_pb_vfd_config_t *config_ptr);

#ifdef __cplusplus
}
#endif

#endif
