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

/*
 * This file contains public declarations for the H5FD configuration language
 *      developer support routines.
 */

#ifndef H5CL_develop_H
#define H5CL_develop_H

/* Include package's public header */
#include "H5CLpublic.h"

/*****************/
/* Public Macros */
/*****************/

/*******************************************************************************/
/* The H5CL_VAL #defines are used to indicate the type of a the value          */
/* stored in an instance of H5CL_nv_pair_t.                                    */
/*******************************************************************************/

#define H5CL_VAL_NONE                             0
#define H5CL_VAL_INT                              1
#define H5CL_VAL_FLOAT                            2
#define H5CL_VAL_QSTR                             3
#define H5CL_VAL_BB                               4
#define H5CL_VAL_LIST                             5

#define H5CL_MAX_VAL_CODE                         5


/*******************/
/* Public Typedefs */
/*******************/

/*******************************************************************************
 *
 * struct H5CL_nv_pair_t
 *
 * The structure used to store the name and value from a successfully parsed
 * name value pair.
 *
 * The fields in the structure are discussed individually below.
 *
 * struct_tag:  unsigned integer which must always contain the the value
 *              H5CL_NV_PAIR_STRUCT_TAG.  The struct_tag field allows us to 
 *              verify that a pointer to struct H5FD_cl_nv_pair_t does in fact 
 *              point to an instance of same.
 *
 * name_ptr:    Pointer to a dynamically allocated string containing the
 *              name in the name value pair, or NULL if the name is undefined.
 *
 * val_type:    Integer code indicating the type of the value stored in
 *              this instance of H5FD_cl_nv_pair_t.  Possible values are:
 *
 *              H5CL_VAL_NONE: The name value pair is undefined.
 *
 *              H5CL_VAL_INT: The value associated with the name value
 *                      pair is an integer stored in int_val field.
 *
 *              H5CL_VAL_FLOAT: The value associated with the name value
 *                      pair is an integer stored in f_val field.
 *
 *              H5CL_VAL_QSTR: The value associated with the name value
 *                      pair is a quote string stored without its leading
 *                      and trailing double quotes and with a terminating
 *                      null char (\0) in a dynamically allocated vector
 *                      of char pointed to by the vlen_val field.  The
 *                      length of this string is stored in the len field.
 *                      Note that this len does not include the terminating
 *                      null char.
 *
 *              H5CL_VAL_BB: The value associated with the name value
 *                      pair is a binary blob stored in a dynamically allocated
 *                      vector of uint8_t pointed to by the vlen_val field.
 *                      The length of this vector is stored in the len field.
 *
 *              H5CL_VAL_LIST: The value associated with the name value
 *                      pair is a configuration language sub expression
 *                      most likely containing configuration data for
 *                      underlying VFD(s).  It is stored as a string in
 *                      a dynamically allocated vector of char.  The
 *                      length of the sub expression (less the terminating
 *                      null char) is stored in the len field.
 *
 * int_val:     int64_t containing the integer value associated with the
 *              name value pair if val_type == H5CL_VAL_INT.  In all other
 *              cases, int_val should be set to 0.
 *
 * f_val:       double containing the floating point value associated with
 *              the name value pair if val_type == H5CL_VAL_FLOAT.  In all
 *              other cases, f_val should be set to 0.0.
 *
 * vlen_val_ptr: void pointer whose value depends on the value of val_type.
 *
 *              If val_type == H5CL_VAL_QSTR, vlen_val_ptr points to a 
 *              dynamically allocated  vector of char containing a text string 
 *              of length len.  In this case, the string contains a quote string 
 *              less its leading and trailing double quotes.  Note that any 
 *              escape sequences in the string have not been resolved by the 
 *              lexer.
 *
 *              If val_type == H5CL_VAL_BB, vlen_val_ptr points to a 
 *              dynamically allocated vector of uint8_t that contains a 
 *              binary blob of length len.
 *
 *              If val_type == H5CL_VAL_LIST, vlen_val_ptr points to a 
 *              dynamically allocated vector of char containing a sub-expression 
 *              in the configuration language expressed as a null terminated C 
 *              string.  This sub-expression will typically contain configuration
 *              data for underlying VFD(s).  The length of this string
 *              (less the terminating null char) is stored in len..
 *
 *              In all other case, vlen_val_ptr should be set to NULL.
 *
 * len          size_t field containing the length of the string or
 *              binary blob pointed to by vlen_val, or 0 if vlen_val is NULL.
 *
 *******************************************************************************/

#define H5CL_NV_PAIR_STRUCT_TAG           0x007A
#define H5CL_INVALID_NV_PAIR_STRUCT_TAG   0x07A0


typedef struct H5CL_nv_pair_t
{
        uint32_t struct_tag;

        char *name_ptr;

        int val_type;

        int64_t int_val;

        double f_val;

        void *vlen_val_ptr;

        size_t len;

} H5CL_nv_pair_t;
             

/*******************************************************************************
 *
 * struct H5CL_config_spec
 *
 * Arrays of instances of H5CL_config_spec are used to pass arrays of instances
 * of H5CL_nv_pair_t and the assoicated configuration names and max number of 
 * parameters into H5CL_parse_config_group(). 
 *
 * The fields in the structure are discussed individually below. 
 *
 * struct_tag:  unsigned integer which must always contain the the value
 *      H5CL_CONFIG_SPEC_STRUCT_TAG.  The struct_tag field allows us to
 *      verify that a pointer to struct H5CL_config_spec does in fact
 *      point to an instance of same.
 *
 * config_name: Pointer to a string containing the name of the target 
 *      configuration.
 *
 * max_num_params: Integer field containing the length of the array of 
 *      instances of H5CL_nv_pair_t pointed to by nv_pairs (below).  Note
 *      that this value must be greater than or equal to the maximum 
 *      number of parameters that may appear in the targer configuration.
 * 
 * nv_pairs: Base address of the array of H5CL_nv_pair_t prepared to receive
 *      the name value pairs in the configuration as they are parsed.  Note
 *      that the tag field of each entry in this array must be set to 
 *      H5CL_NV_PAIR_STRUCT_TAG on entry.
 *
 * parsed: Boolean flag used to track whether the indication configuration 
 *      has been parsed and its name value pairs have been loaded into 
 *      the array of H5CL_nv_pair_t pointed to by the nv_pairs field above.
 *
 *******************************************************************************/

#define H5CL_CONFIG_SPEC_STRUCT_TAG  0x008A

typedef struct H5CL_config_spec
{
    unsigned         struct_tag;

    char *           config_name;

    int              max_num_params;

    H5CL_nv_pair_t * nv_pairs;

    bool             parsed;

} H5CL_config_spec;


/********************/
/* Public Variables */
/********************/


/*********************/
/* Public Prototypes */
/*********************/


#endif /* H5CL_develop_H */

