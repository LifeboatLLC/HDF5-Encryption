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

#ifndef H5CL_private_H
#define H5CL_private_H

/*
 * This file contains private header file information for the VFD
 * configuration language.
 */

/* Include package's public headers */
#include "H5CLpublic.h"
#include "H5CLdevelop.h"

/* Private headers needed by this file */


/**************************/
/* Library Private Macros */
/**************************/


/****************************/
/* Library Private Typedefs */
/****************************/


/*****************************/
/* Library Private Variables */
/*****************************/


/******************************/
/* Library Private Prototypes */
/******************************/

H5_DLL herr_t H5CL_parse_config(const char * input_str_ptr, char * expected_name_ptr, 
         H5CL_nv_pair_t nv_pairs[], int num_pairs);
H5_DLL herr_t H5CL_parse_config_group(const char * input_str_ptr, char * config_group_name_ptr,
         int num_configs, H5CL_config_spec configs[]);
H5_DLL herr_t H5CL_load_vfd_config_str_into_fapl(hid_t fapl_id, char * vfd_config_str_ptr);
H5_DLL herr_t H5CL_init_nv_pair(H5CL_nv_pair_t * nv_pair_ptr);
H5_DLL herr_t H5CL_take_down_nv_pair(H5CL_nv_pair_t * nv_pair_ptr);

#endif /* H5CL_private_H */
