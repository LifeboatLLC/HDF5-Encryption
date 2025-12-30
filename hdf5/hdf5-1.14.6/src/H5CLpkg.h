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

/*******************************************************************************/
/*                                                                             */
/*                     VFD Configuration Language                              */
/*                                                                             */
/* Header file for code implementing the VFD configuration language.           */
/*                                                                             */
/* The grammar of the configuration language is as follows:                    */
/*                                                                             */
/*      <name_value_pair> ::= ‘(‘ <identifier> <value> ’)’                     */
/*                                                                             */
/*      <value> ::= <integer> | <float> | <quote_string> | <binary_blob> |     */
/*                  <name_value_pair_list>                                     */
/*                                                                             */
/*      <name_value_pair_list> ::= ‘(‘ (<name_value_pair>)* ‘)’                */
/*                                                                             */
/* where the non-terminals not defined above are loosely defined below:        */
/*                                                                             */
/* <identifier>         a valid C identifier.                                  */
/*                                                                             */
/* <integer>            a C integer constant                                   */
/*                      (only decimal at present)                              */
/*                                                                             */
/* <float>              a C floating point constant                            */
/*                      (only decimal, no exponents at present                 */
/*                                                                             */
/* <quote_string>       a C quote string                                       */
/*                      (embedded double quotes must be escaped with a back    */
/*                      slash (i.e. "\""). Escape sequences are not decoded.)  */
/*                                                                             */
/* <binary_blob>        a hex representation of an arbitrary sequence of       */
/*                      binary bytes with a ‘--’ prefix to distinguish it from */
/*                      identifiers and integer constants.                     */
/*                                                                             */
/* Since the purpose of the configuration language is to support construction  */
/* of VFD stacks, the recursive descent parser used to parse strings in the    */
/* in the configuration language is designed for a bredth first instead of a   */
/* depth first parse.  Specifically, it must parse all configuration data for  */
/* the current VFD before parsing that for the next VFD down in the stack.     */
/*                                                                             */
/* The major design change required to support this is in                      */
/* the lexer, which must recognize <name_value_pair_lists> when required.      */
/*                                                                             */
/* Syntactically, such instances of <name_value_pair_lists> contain            */
/* configuration data for underlying VFDs.  The associated string is parsed    */
/* later to identify and configure the underlying VFDs.                        */
/*                                                                             */
/* Note that the lexer is directed to lex a <name_value_pair_lists> as a       */
/* single token whenever a value in a name value pair is expected.             */
/*                                                                             */
/*                                                  JRM - 12/02/25             */
/*                                                                             */
/*******************************************************************************/

/*
 * Purpose: This file contains declarations which are visible only within
 *          the H5FD package and the configuration language code in 
 *          particular.  Source files outside the H5FD package should
 *          include H5CLprivate.h instead.
 */

#if !(defined H5CL_FRIEND || defined H5CL_MODULE)
#error "Do not include this file outside the H5CL package!"
#endif

#ifndef H5CL_pkg_H
#define H5CL_pkg_H

/* Get package's private header */
#include "H5CLprivate.h" /* VFD Configuration Language */

/* Other private headers needed by this file */

/**************************/
/* Package Private Macros */
/**************************/

/*******************************************************************************/
/* The H5FDL_TOKEN #defines are used to indicate the type of a token           */
/* recognized by the lexical analyzer.                                         */
/*******************************************************************************/

#define H5CL_ERROR_TOK			     0
#define H5CL_L_PAREN_TOK		     1
#define H5CL_R_PAREN_TOK		     2
#define H5CL_SYMBOL_TOK			     3
#define H5CL_INT_TOK		             4
#define H5CL_FLOAT_TOK		             5
#define H5CL_QSTRING_TOK                     6
#define H5CL_BIN_BLOB_TOK                    7
#define H5CL_LIST_TOK                        8
#define H5CL_EOS_TOK                         9

#define H5CL_MAX_TOKEN_CODE                  9

/****************************/
/* Package Private Typedefs */
/****************************/

/*******************************************************************************
 *                                                                            
 * struct H5CL_token_t                                                          
 *                                                                            
 * The token structure is used to store tokens as they are read from the      
 * input string by the lexical analyzer.  The fields in the structure are     
 * discussed individually below.                                              
 *                                                                            
 * struct_tag:  unsigned integer which must always contain the the value               
 *		H5CL_TOKEN_STRUCT_TAG.  The struct_tag field allows us to 
 *              verify that a pointer to struct H5FD_cl_token_t does in fact 
 *              point to an instance of same.                                           
 *                                                                         
 * code:        Integer code indicating the type of the token.  The set of 
 *              alowable values for this field is equal to the set of 
 *              H5FD_CL_TOKEN #defines earlier in this file.                                
 *                                                                          
 * str_ptr:     Pointer to char.  str_ptr points to a dynamically allocated
 *		string which contains a text representation of the token. 
 *              Note that in the case of numerical values, *str_ptr need not 
 *              agree with val, as *str_ptr may contain a text representation 
 *              of an out of range value. 
 *                                                                         
 * str_len:     size_t containing the length of the null terminated string 
 *		pointed to by str_ptr.                                     
 *                                                                           
 * max_str_len: size_t containing the number of bytes in the buffer pointed
 *		to by str_ptr.  Note that should always be larger than     
 *              str_len.  Since we recycle *str_ptr, it is possible for   
 *              max_str_len to be much larger than str_len.              
 *                                                                      
 * int_val:     int64_t containing any integer value associated with the 
 *              instance of H5FD_cl_token_t.                                 
 *                                                                     
 * f_val:       Double containg any floating point value associated with the 
 *              instance of H5FD_cl_token_t.                                    
 *
 * bb_ptr:      Pointer to a dynamically vector of uint8_t of length equal 
 *              to max_str_len.  This vector is used to store the value of 
 *              a binary blob. Note that the size of the buffer pointed to 
 *              bb_ptr is always max_str_len.
 *
 * bb_len       size_t field containing the number of bytes in the binary 
 *              blob. As per str_len, this value must always be lets than
 *              max_str_len.
 *                                                                          
 *******************************************************************************/

#define H5CL_TOKEN_STRUCT_TAG 		0x005A
#define H5CL_INVALID_TOKEN_STRUCT_TAG 	0x05A0

typedef struct H5CL_token_t
{
	uint32_t struct_tag;
	
	int32_t code;
	
	char *str_ptr;
	
	size_t str_len;
	
	size_t max_str_len;

        int64_t int_val;
	
	double f_val;

        uint8_t * bb_ptr;

        size_t bb_len;

} H5CL_token_t;


/******************************************************************************* 
 *                                                                       
 * struct H5CL_lex_vars_t                                                   
 *                                                                         
 * A single instance of H5CL_lex_vars_t is used to consolidate all the       
 * variables employed by the configuration language lexer code. The fields of
 * this structure are discussed individually below.
 *                               
 * struct_tag:  Unsigend integer which must always contain the the value
 *		H5CL_LEX_VARS_SRUCT_TAG.  The struct_tag field allows us to 
 *              easily verify that a pointer to struct H5CL_lex_vars_t does in 
 *              fact point to an instance of same.
 *
 * input_str_ptr: Pointer to a dynamically allocated string that has been loaded
 *              with the configuration language string to be lexed.
 *
 * next_char_ptr: Pointer to the next character to be lexed.  This pointer is 
 *              initialized to input_str_ptr, and then incremented as tokens
 *              are recognized.
 *
 * end_of_input: Boolean that is initialized to false, and set to true when 
 *              the end of the input string is reached.
 *
 * line_num:    Integer value initialized to zero, and incremented each time 
 *              next_char_ptr is incremented past a new line character.
 *
 * char_num:    Integer value initialized to zero, and incremented each time
 *              next_char_ptr is incremented.  It is reset to zero each time
 *              line_num is incremented.
 *
 * token:       Instance of struct H5CL_token_t.  A pointer to this instance 
 *              is returned by the lexer, and reused for each new token 
 *              recognized.
 * 
 *******************************************************************************/

#define H5CL_LEX_VARS_STRUCT_TAG 		0x006A
#define H5CL_INVALID_LEX_VARS_STRUCT_TAG 	0x06A0

typedef struct H5CL_lex_vars_t
{
	uint32_t struct_tag;

        char * input_str_ptr;

        char * next_char_ptr;

        bool end_of_input;

        int32_t line_num;

        int32_t char_num;

        struct H5CL_token_t token;
	
} H5CL_lex_vars_t;


/*****************************/
/* Package Private Variables */
/*****************************/

/******************************/
/* Package Private Prototypes */
/******************************/

H5_DLL herr_t H5CL__init_lex_vars(const char * input_str_ptr, H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__take_down_lex_vars(H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__lex_get_non_blank(H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__lex_peek_next_char(char * next_char_ptr, H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__lex_read_token(bool value_expected, H5CL_token_t **token_ptr_ptr,
                                     H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__init_nv_pair(H5CL_nv_pair_t * nv_pair_ptr);
H5_DLL herr_t H5CL__take_down_nv_pair(H5CL_nv_pair_t * nv_pair_ptr);
H5_DLL herr_t H5CL__parse_name_value_pair(H5CL_nv_pair_t *nv_pair_ptr, H5CL_lex_vars_t * lex_vars_ptr);
H5_DLL herr_t H5CL__parse_name_value_pair_list(H5CL_nv_pair_t * nv_pairs, int max_nv_pairs,
                                                H5CL_lex_vars_t * lex_vars_ptr);

#endif /* H5CL_pkg_H */
