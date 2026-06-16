/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by Lifeboat, LLC                                                *
 * All rights reserved.                                                      *
 *                                                                           *
 * The full copyright notice, including terms governing use, modification,   *
 * and redistribution, is contained in the COPYING file, which can be found  *
 * at the root of the source code distribution tree.                         *
 * If you do not have access to either file, you may request a copy from     *
 * help@lifeboat.llc                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/*-------------------------------------------------------------------------
 *
 * Created:             H5CLpublic.h
 *
 * Purpose:             Public declarations for the VFD configuration 
 *                      language.
 *
 *-------------------------------------------------------------------------
 */


#ifndef H5CL_public_H
#define H5CL_public_H

#include "H5public.h" /* Generic Functions */


/*****************/
/* Public Macros */
/*****************/


/*******************/
/* Public Typedefs */
/*******************/

/* Macros for various environment variables that HDF5 interprets */
/**
 * Used to specify the path to the file containing the configuration 
 * string for the necessary Virtual File Driver(s) (VFD).
 */
#define HDF5_VFD_CONFIG_PATH "HDF5_VFD_CONFIG_PATH"


/********************/
/* Public Variables */
/********************/


/*********************/
/* Public Prototypes */
/*********************/

/* Function prototypes */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5CL
 *
 * \brief Reads VFD configuration data from a config file and sets that config
 *        data into a fapl.
 *
 * \param[in] config_path path to the config file
 * \param[in] fapl_id index ID for the fapl to set the VFD configuration data
 * \return \herr_t
 *
 * \details H5CLset_config_from_file() opens the file from
 *          the config_path parameter and reads the configuration data in the
 *          file. Then stores that configuration data into the fapl from the
 *          provided fapl_id.
 *
 * \since 1.14.6
 */
H5_DLL herr_t H5CLset_config_from_file(hid_t fapl_id, const char *config_path);



#endif /* H5CL_public_H */
