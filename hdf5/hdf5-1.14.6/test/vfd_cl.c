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
 * Purpose:     Tests the basic features of Virtual File Drivers
 */

#define H5CL_FRIEND

#include "h5test.h"
#include "H5CLpkg.h"

/* utility functions */
static bool cl_lexer_test_verify_token(H5CL_token_t * token_ptr, int token_num, int32_t expected_code, 
                                       const char * expected_str, int64_t expected_int_val, double expected_f_val, 
                                       uint8_t * expected_bb_ptr, size_t expected_bb_len, bool verbose);
static int cl_test_verify_nv_pair(H5CL_nv_pair_t * nv_pair_ptr, int nv_pair_num, const char * expected_name_ptr, 
                                  int expected_val_type, int64_t expected_int_val, double expected_f_val,
                                  const void * expected_vlen_val_ptr, size_t expected_len, bool verbose);
static int cl_test_verify_nv_pairs(H5CL_nv_pair_t * actual_nv_pairs, H5CL_nv_pair_t * expected_nv_pairs,
                                   int num_nv_pairs, bool verbose);

/* test functions */
static herr_t cl_lexer_smoke_check(void);
static herr_t cl_parse_name_val_pair_smoke_check(void);
static herr_t cl_parse_name_val_pair_list_smoke_check(void);
herr_t cl_parser_smoke_check(void);


/*******************************************************************************
 *
 * cl_lexer_test_verify_token()
 *
 * Verify that the supplied instance of cl_token_t contains the expected data.
 *
 *                                              JRM -- 12/16/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static bool
cl_lexer_test_verify_token(H5CL_token_t * token_ptr, int token_num, 
                           int32_t expected_code, const char * expected_str, 
                           int64_t expected_int_val, double expected_f_val, 
                           uint8_t * expected_bb_ptr, size_t expected_bb_len, 
                           bool verbose)
{
    int failures = 0;
    int i;

    assert(token_ptr);
    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( ( token_ptr->code != expected_code ) ||
         ( 0 != strcmp(token_ptr->str_ptr, expected_str) ) ||
         ( token_ptr->str_len != strlen(expected_str) ) ||
         ( token_ptr->int_val != expected_int_val ) || 
         ( token_ptr->f_val < expected_f_val ) ||      /* circumlocution to keep */
         ( token_ptr->f_val > expected_f_val ) ||      /* the compiler happy     */
         ( token_ptr->bb_len != expected_bb_len ) ) {

        failures++;

    } else {

        if ( H5CL_BIN_BLOB_TOK == expected_code ) {

            for ( i = 0; i < (int)expected_bb_len; i++ ) {

                if ( expected_bb_ptr[i] != token_ptr->bb_ptr[i] ) {

                    failures++;
                }
            }
        }
    }

    if ( ( failures > 0 ) && ( verbose ) ) {

        fprintf(stdout, "\n\nToken %d verify failed:\n", token_num);
        fprintf(stdout, "token actual / expected code    = %d / %d\n", token_ptr->code, expected_code);
        fprintf(stdout, "token actual / expected str_ptr = \"%s\" / \"%s\"\n", 
                token_ptr->str_ptr, expected_str);
        fprintf(stdout, "token actual / expected str_len = %ld / %ld\n", 
                token_ptr->str_len, strlen(expected_str));
        fprintf(stdout, "token actual / expected int_val = %lld / %lld\n", 
                (long long int)(token_ptr->int_val), (long long int)(expected_int_val));
        fprintf(stdout, "token actual / expected f_val   = %lf / %lf\n", 
                token_ptr->f_val, expected_f_val);
        fprintf(stdout, "bb_len actual / expected        = %ld / %ld\n", token_ptr->bb_len, expected_bb_len);

        if ( expected_bb_len > 0 ) {

            fprintf(stdout, "actual bb   = ");

            for ( i = 0; i < (int)expected_bb_len; i++ ) {

                fprintf(stdout, "%2x ", (unsigned)(token_ptr->bb_ptr[i]));
            }

            fprintf(stdout, "\nexpected bb = ");

            for ( i = 0; i < (int)expected_bb_len; i++ ) {

                fprintf(stdout, "%2x ", (unsigned)(expected_bb_ptr[i]));
            }

            fprintf(stdout, "\n");
        }
    }

    return(failures);

} /* cl_lexer_test_verify_token() */


/*******************************************************************************
 *
 * cl_test_verify_nv_pair()
 *
 * Verify that the supplied instance of cl_nv_pair_t contains the expected 
 * data.
 *
 *                                              JRM -- 12/19/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static int
cl_test_verify_nv_pair(H5CL_nv_pair_t * nv_pair_ptr, int nv_pair_num, 
                       const char * expected_name_ptr, int expected_val_type,
                       int64_t expected_int_val, double expected_f_val,
                       const void * expected_vlen_val_ptr, size_t expected_len,
                       bool verbose)
{
    int failures = 0;
    int i;

    assert(nv_pair_ptr);
    assert(H5CL_NV_PAIR_STRUCT_TAG == nv_pair_ptr->struct_tag);

    if ( ( 0 != strcmp(nv_pair_ptr->name_ptr, expected_name_ptr) ) ||
         ( expected_val_type != nv_pair_ptr->val_type ) ||
         ( expected_int_val != nv_pair_ptr->int_val ) ||
         ( expected_f_val < nv_pair_ptr->f_val ) ||     /* circumlocution to keep the */
         ( expected_f_val > nv_pair_ptr->f_val ) ||     /* the compiler happy         */
         ( expected_len != nv_pair_ptr->len ) ) {

        failures++;

    } else {

        switch ( nv_pair_ptr->val_type ) {

            case H5CL_VAL_QSTR:
            case H5CL_VAL_LIST:
                if ( 0 != strcmp((char *)(nv_pair_ptr->vlen_val_ptr), (const char *)(expected_vlen_val_ptr)) ) {

                    failures++;

                } else if ( strlen((char *)(nv_pair_ptr->vlen_val_ptr)) != nv_pair_ptr->len ) {

                    failures++;
                }
                break;

            case H5CL_VAL_BB:

                for ( i = 0; i < (int)expected_len; i++ ) {

                    if ( ((const uint8_t *)(expected_vlen_val_ptr))[i] != 
                         ((uint8_t *)(nv_pair_ptr->vlen_val_ptr))[i] ) {

                        failures++;
                    }
                }
                break;

            default:
                if ( ( NULL != nv_pair_ptr->vlen_val_ptr ) ||
                     ( NULL != expected_vlen_val_ptr ) ) {

                    failures++;
                }
                break;
        }
    }

    if ( ( failures > 0 ) && ( verbose ) ) {

        fprintf(stdout, "\n\nName / Value Pair %d verify failed:\n", nv_pair_num);
        fprintf(stdout, "nv pair actual / expected name     = \"%s\" / \"%s\" \n", 
                nv_pair_ptr->name_ptr, expected_name_ptr);
        fprintf(stdout, "nv pair actual / expected val_type = %d / %d\n", 
                nv_pair_ptr->val_type, expected_val_type);
        fprintf(stdout, "nv pair actual / expected int_val  = %lld / %lld\n", 
                (long long int)(nv_pair_ptr->int_val), (long long int)(expected_int_val));
        fprintf(stdout, "nv pair actual / expected f_val    = %lf / %lf\n", 
                nv_pair_ptr->f_val, expected_f_val);

        switch ( expected_val_type ) {

            case H5CL_VAL_QSTR:
            case H5CL_VAL_LIST:
                fprintf(stdout, "nv pair actual vlen val   = \"%s\"\n", (char *)(nv_pair_ptr->vlen_val_ptr));
                fprintf(stdout, "nv pair expected vlen val = \"%s\"\n", (const char *)(expected_vlen_val_ptr));
                break;

            case H5CL_VAL_BB:
                if ( expected_len > 0 ) {

                    fprintf(stdout, "nv pair actual vlen val   = ");

                    for ( i = 0; i < (int)expected_len; i++ ) {

                        fprintf(stdout, "%2x ", (unsigned)(((uint8_t*)(nv_pair_ptr->vlen_val_ptr))[i]));
                    }

                    fprintf(stdout, "\nnv pair expected vlen val = ");

                    for ( i = 0; i < (int)expected_len; i++ ) {

                        fprintf(stdout, "%2x ", (unsigned)(((const uint8_t*)(expected_vlen_val_ptr))[i]));
                    }

                    fprintf(stdout, "\n");
                }
                break;

            default:
                fprintf(stdout, "nv pair actual / expected vlen_val_ptr = 0x%llx / 0x%llx\n", 
                        (unsigned long long)(nv_pair_ptr->vlen_val_ptr), 
                        (unsigned long long)(expected_vlen_val_ptr));
        }

        fprintf(stdout, "nv pair len / expected len         = %ld / %ld\n", 
                nv_pair_ptr->len, expected_len);
    }

    return(failures);

} /* cl_test_verify_nv_pair() */


/*******************************************************************************
 *
 * cl_test_verify_nv_pair_vector()
 *
 * Verify that the supplied vectors of cl_nv_pair_t are identical.
 * 
 *
 *                                              JRM -- 12/19/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static int
cl_test_verify_nv_pairs(H5CL_nv_pair_t * actual_nv_pairs, 
                        H5CL_nv_pair_t * expected_nv_pairs,
                        int num_nv_pairs, bool verbose)
{
    int failures = 0;
    int i;

    for ( i = 0; i < num_nv_pairs; i++ ) {

        assert(H5CL_NV_PAIR_STRUCT_TAG == expected_nv_pairs[i].struct_tag);

        failures += cl_test_verify_nv_pair(&(actual_nv_pairs[i]), i, 
                                           expected_nv_pairs[i].name_ptr,
                                           expected_nv_pairs[i].val_type,
                                           expected_nv_pairs[i].int_val,
                                           expected_nv_pairs[i].f_val,
                                           expected_nv_pairs[i].vlen_val_ptr,
                                           expected_nv_pairs[i].len,
                                           verbose);
    }

    return(failures);

} /* cl_test_verify_nv_pairs() */


/*******************************************************************************
 *
 * cl_lexer_smoke_check()
 *
 * Initial set of lexer tests designed to verify basic functionality.  Note that
 * these tests do not trigger any error conditinos in the lexer.
 *
 *                                              JRM -- 12/16/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_lexer_smoke_check(void)
{
    int token_num = 0;
    const char * input_string = "( ) /* comment */ symbol 1 3.14159 \"Hello World\" --00010203 ( sec2 () )";
    uint8_t bb_0[] = { 0, 1, 2, 3};
    size_t bb_0_len = 4;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* line_num          = */ 0,
        /* char_num          = */ 0, 
        /* token             = */ {
        /* token.struct_tag  = */    H5CL_TOKEN_STRUCT_TAG,
        /* token.code        = */    H5CL_ERROR_TOK,
        /* token.str_ptr     = */    NULL,
        /* token.str_len     = */    0,
        /* token.max_str_len = */    0,
        /* token.int_val     = */    1,    /* should be overwritten on init */
        /* token.f_val       = */    1.0,  /* should be overwritten on init */
        /* token.bb_ptr      = */    NULL,
        /* token.bb_len      = */    0
        /* end of token        */ }
    };

    TESTING("VFD Configuration Language Lexer Smoke Check");

    if ( H5CL__init_lex_vars(input_string, &lex_vars) < 0 ) {

        TEST_ERROR;
    }

    if ( ( H5CL_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL == lex_vars.input_str_ptr ) ||
         ( input_string == lex_vars.input_str_ptr ) ||
         ( 0 != strcmp(input_string, lex_vars.input_str_ptr) ) ||
         ( lex_vars.input_str_ptr != lex_vars.next_char_ptr ) || 
         ( H5CL_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( H5CL_ERROR_TOK != lex_vars.token.code ) ||
         ( NULL == lex_vars.token.str_ptr ) ||
         ( 0 != lex_vars.token.str_len ) ||
         ( strlen(input_string) != lex_vars.token.max_str_len ) ||
         ( 0 != lex_vars.token.int_val ) ||
         ( 0.0 < lex_vars.token.f_val ) ||    /* circumlocution to keep */
         ( 0.0 > lex_vars.token.f_val ) ||    /* the compier happy      */
         ( NULL == lex_vars.token.bb_ptr ) ||
         ( 0 != lex_vars.token.bb_len ) ) {

        TEST_ERROR;
    }

    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 0 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_L_PAREN_TOK, "(", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 1 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 2 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "symbol", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 3 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "1", 1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 4 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, "3.14159", 0, 3.14159, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 5 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_QSTRING_TOK, "Hello World", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 6 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--00010203", 0, 0.0, 
                                          bb_0, bb_0_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(true, &token_ptr, &lex_vars) < 0 ) /* 7 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_LIST_TOK, "( sec2 () )", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, &token_ptr, &lex_vars) < 0 ) /* 8 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_EOS_TOK, "", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;
    

    if ( ( H5CL_INVALID_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL != lex_vars.input_str_ptr ) ||
         ( H5CL_INVALID_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( NULL != lex_vars.token.str_ptr ) ||
         ( NULL != lex_vars.token.bb_ptr ) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_lexer_smoke_check() */


/*******************************************************************************
 *
 * cl_parse_name_val_pair_smoke_check()
 *
 * Initial set of parse tests designed to verify basic functionality of the 
 * function that parses name value pairs.  Note that theses tests do not 
 * trigger any error conditinos in the parser.
 *
 *                                              JRM -- 12/17/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_name_val_pair_smoke_check(void)
{
    int nv_pair_num = 0;
    const char * input_string = "( name_0 1 ) ( name_1 3.14159 ) ( name_2 \"Hello World\" ) "
                          "( name_3 --10111213 ) ( name_4 ( sec2 () ) )";
    uint8_t bb_0[] = { 0x10, 0x11, 0x12, 0x13 };
    size_t bb_0_len = 4;
    H5CL_nv_pair_t nv_pairs[5];
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* line_num          = */ 0,
        /* char_num          = */ 0, 
        /* token             = */ {
        /* token.struct_tag  = */    H5CL_TOKEN_STRUCT_TAG,
        /* token.code        = */    H5CL_ERROR_TOK,
        /* token.str_ptr     = */    NULL,
        /* token.str_len     = */    0,
        /* token.max_str_len = */    0,
        /* token.int_val     = */    1,    /* should be overwritten on init */
        /* token.f_val       = */    1.0,  /* should be overwritten on init */
        /* token.bb_ptr      = */    NULL,
        /* token.bb_len      = */    0
        /* end of token        */ }
    };

    TESTING("VFD Configuration Language Parse Name Value Pair Smoke Check");

    if ( H5CL__init_lex_vars(input_string, &lex_vars) < 0 ) 
        TEST_ERROR;


    if ( ( H5CL_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL == lex_vars.input_str_ptr ) ||
         ( input_string == lex_vars.input_str_ptr ) ||
         ( 0 != strcmp(input_string, lex_vars.input_str_ptr) ) ||
         ( lex_vars.input_str_ptr != lex_vars.next_char_ptr ) || 
         ( H5CL_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( H5CL_ERROR_TOK != lex_vars.token.code ) ||
         ( NULL == lex_vars.token.str_ptr ) ||
         ( 0 != lex_vars.token.str_len ) ||
         ( strlen(input_string) != lex_vars.token.max_str_len ) ||
         ( 0 != lex_vars.token.int_val ) ||
         ( 0.0 < lex_vars.token.f_val ) ||    /* circumlocution to keep */
         ( 0.0 > lex_vars.token.f_val ) ||    /* the compier happy      */
         ( NULL == lex_vars.token.bb_ptr ) ||
         ( 0 != lex_vars.token.bb_len ) ) {

        TEST_ERROR;
    }

    /* initialize the array of instance of cl_nv_pair_t */
    for ( nv_pair_num = 0; nv_pair_num < 5; nv_pair_num++ ) {

        nv_pairs[nv_pair_num].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }


    if ( H5CL__parse_name_value_pair(&(nv_pairs[0]), &lex_vars) < 0 )
        TEST_ERROR;
  
    if ( 0 != cl_test_verify_nv_pair(&(nv_pairs[0]), 0, "name_0", H5CL_VAL_INT, 1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__parse_name_value_pair(&(nv_pairs[1]), &lex_vars) < 0 )
        TEST_ERROR;
  
    if ( 0 != cl_test_verify_nv_pair(&(nv_pairs[1]), 1, "name_1", H5CL_VAL_FLOAT, 0, 3.14159, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__parse_name_value_pair(&(nv_pairs[2]), &lex_vars) < 0 )
        TEST_ERROR;
  
    if ( 0 != cl_test_verify_nv_pair(&(nv_pairs[2]), 2, "name_2", H5CL_VAL_QSTR, 0, 0.0, "Hello World", 11, true) )
        TEST_ERROR;


    if ( H5CL__parse_name_value_pair(&(nv_pairs[3]), &lex_vars) < 0 )
        TEST_ERROR;
  
    if ( 0 != cl_test_verify_nv_pair(&(nv_pairs[3]), 3, "name_3", H5CL_VAL_BB, 0, 0.0, bb_0, bb_0_len, true) )
        TEST_ERROR;


    if ( H5CL__parse_name_value_pair(&(nv_pairs[4]), &lex_vars) < 0 )
        TEST_ERROR;
  
    if ( 0 != cl_test_verify_nv_pair(&(nv_pairs[4]), 4, "name_4", H5CL_VAL_LIST, 0, 0.0, "( sec2 () )", 11, true) )
        TEST_ERROR;


    /* take down the array of instance of cl_nv_pair_t */
    for ( nv_pair_num = 0; nv_pair_num < 5; nv_pair_num++ ) {

        if ( H5CL__take_down_nv_pair(&(nv_pairs[nv_pair_num])) < 0 ) 
            TEST_ERROR;
    }


    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    if ( ( H5CL_INVALID_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL != lex_vars.input_str_ptr ) ||
         ( H5CL_INVALID_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( NULL != lex_vars.token.str_ptr ) ||
         ( NULL != lex_vars.token.bb_ptr ) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_name_val_pair_smoke_check() */


/*******************************************************************************
 *
 * cl_parse_name_val_pair_list_smoke_check()
 *
 * Initial set of parse tests designed to verify basic functionality of the 
 * function that parses name value pair lists.  Note that theses tests do not 
 * trigger any error conditinos in the parser.
 *
 *                                              JRM -- 12/20/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_name_val_pair_list_smoke_check(void)
{
    int nv_pair_num = 0;
    const char * input_string = "( ( name_0 1 ) ( name_1 3.14159 ) ( name_2 \"Hello World\" ) "
                                "( name_3 --10111213 ) ( name_4 ( sec2 () ) ) )";
    uint8_t bb_0[] = { 0x10, 0x11, 0x12, 0x13 };
    size_t bb_0_len = 4;
    H5CL_nv_pair_t actual_nv_pairs[5];
    char name_0[7] = "name_0";
    char name_1[7] = "name_1";
    char name_2[7] = "name_2";
    char name_3[7] = "name_3";
    char name_4[7] = "name_4";
    char hello_world[12] = "Hello World";
    char test_list[12] = "( sec2 () )";
    H5CL_nv_pair_t expected_nv_pairs[5] = 
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ name_0,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 1,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ name_1,
            /* val_type     = */ H5CL_VAL_FLOAT,
            /* int_val      = */ 0,
            /* f_val        = */ 3.14159,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ name_2,
            /* val_type     = */ H5CL_VAL_QSTR,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ hello_world,
            /* len          = */ 11
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ name_3,
            /* val_type     = */ H5CL_VAL_BB,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ (void *)bb_0,
            /* len          = */ bb_0_len
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ name_4,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ test_list,
            /* len          = */ 11 
        }
    };
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* line_num          = */ 0,
        /* char_num          = */ 0, 
        /* token             = */ {
        /* token.struct_tag  = */    H5CL_TOKEN_STRUCT_TAG,
        /* token.code        = */    H5CL_ERROR_TOK,
        /* token.str_ptr     = */    NULL,
        /* token.str_len     = */    0,
        /* token.max_str_len = */    0,
        /* token.int_val     = */    1,    /* should be overwritten on init */
        /* token.f_val       = */    1.0,  /* should be overwritten on init */
        /* token.bb_ptr      = */    NULL,
        /* token.bb_len      = */    0
        /* end of token        */ }
    };

    TESTING("VFD Configuration Language Parse NV Pair List Smoke Check");

    if ( H5CL__init_lex_vars(input_string, &lex_vars) < 0 ) 
        TEST_ERROR;


    if ( ( H5CL_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL == lex_vars.input_str_ptr ) ||
         ( input_string == lex_vars.input_str_ptr ) ||
         ( 0 != strcmp(input_string, lex_vars.input_str_ptr) ) ||
         ( lex_vars.input_str_ptr != lex_vars.next_char_ptr ) || 
         ( H5CL_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( H5CL_ERROR_TOK != lex_vars.token.code ) ||
         ( NULL == lex_vars.token.str_ptr ) ||
         ( 0 != lex_vars.token.str_len ) ||
         ( strlen(input_string) != lex_vars.token.max_str_len ) ||
         ( 0 != lex_vars.token.int_val ) ||
         ( 0.0 < lex_vars.token.f_val ) ||    /* circumlocution to keep */
         ( 0.0 > lex_vars.token.f_val ) ||    /* the compier happy      */
         ( NULL == lex_vars.token.bb_ptr ) ||
         ( 0 != lex_vars.token.bb_len ) ) {

        TEST_ERROR;
    }

    /* initialize the array of instance of cl_nv_pair_t */
    for ( nv_pair_num = 0; nv_pair_num < 5; nv_pair_num++ ) {

        actual_nv_pairs[nv_pair_num].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }


    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs, 5, &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs, expected_nv_pairs, 5, true) )
        TEST_ERROR;


    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( nv_pair_num = 0; nv_pair_num < 5; nv_pair_num++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;

    if ( ( H5CL_INVALID_LEX_VARS_STRUCT_TAG != lex_vars.struct_tag ) ||
         ( NULL != lex_vars.input_str_ptr ) ||
         ( H5CL_INVALID_TOKEN_STRUCT_TAG != lex_vars.token.struct_tag ) ||
         ( NULL != lex_vars.token.str_ptr ) ||
         ( NULL != lex_vars.token.bb_ptr ) ) {

        TEST_ERROR;
    }


    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_name_val_pair_list_smoke_check() */


/*******************************************************************************
 *
 * cl_parser_smoke_check()
 *
 * Initial full configuraion language parser smoke checks. Note that theses 
 * tests do not trigger any error conditinos in the parser.
 *
 *                                              JRM -- 12/20/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

herr_t
cl_parser_smoke_check(void)
{
    int i;
    int num_nv_pairs_0 = 1;
    int num_nv_pairs_1 = 4;
    int num_nv_pairs_2 = 1;
    int num_nv_pairs_3 = 10;
    int num_nv_pairs_4 = 1;
    int num_nv_pairs_5 = 1;
    const char * input_string_0 = 
        "( page_buffer "
        "  ( ( page_size 4096 )"
        "    ( max_num_pages 16 )"
        "    ( replacement_policy 0 )"
        "    ( underlying_VFD "
        "      ( encryption_VFD "
        "        ( ( plaintext_page_size  4096 )"
        "          ( ciphertext_page_size 4112 )"
        "          ( encryption_buffer_size 65792 )"
        "          ( cipher  0 )"
        "          ( cipher_block_size 16 )"
        "          ( key_size  32 )"
        "          ( key --0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF )"
        "          ( iv_size 16 )"
        "          ( mode 0 )"
        "          ( underlying_VFD ( sec2 () ) )"
        "        )"
        "      )"
        "    )"
        "  )"
        ")";
    char input_string_1[521] = 
          "( ( page_size 4096 )"
        "    ( max_num_pages 16 )"
        "    ( replacement_policy 0 )"
        "    ( underlying_VFD "
        "      ( encryption_VFD "
        "        ( ( plaintext_page_size  4096 )"
        "          ( ciphertext_page_size 4112 )"
        "          ( encryption_buffer_size 65792 )"
        "          ( cipher  0 )"
        "          ( cipher_block_size 16 )"
        "          ( key_size  32 )"
        "          ( key --0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF )"
        "          ( iv_size 16 )"
        "          ( mode 0 )"
        "          ( underlying_VFD ( sec2 () ) )"
        "        )"
        "      )"
        "    )"
        "  )";
    char input_string_2[405] = 
             "( encryption_VFD "
        "        ( ( plaintext_page_size  4096 )"
        "          ( ciphertext_page_size 4112 )"
        "          ( encryption_buffer_size 65792 )"
        "          ( cipher  0 )"
        "          ( cipher_block_size 16 )"
        "          ( key_size  32 )"
        "          ( key --0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF )"
        "          ( iv_size 16 )"
        "          ( mode 0 )"
        "          ( underlying_VFD ( sec2 () ) )"
        "        )"
        "      )";
    char input_string_3[373] = 
                "( ( plaintext_page_size  4096 )"
        "          ( ciphertext_page_size 4112 )"
        "          ( encryption_buffer_size 65792 )"
        "          ( cipher  0 )"
        "          ( cipher_block_size 16 )"
        "          ( key_size  32 )"
        "          ( key --0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF )"
        "          ( iv_size 16 )"
        "          ( mode 0 )"
        "          ( underlying_VFD ( sec2 () ) )"
        "        )";
    char input_string_4[12] = "( sec2 () )";
    char  input_string_5[3] = "()";
    uint8_t key[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                      0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                      0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                      0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    size_t key_len = 32;
    H5CL_nv_pair_t actual_nv_pairs_0[1];
    H5CL_nv_pair_t actual_nv_pairs_1[4];
    H5CL_nv_pair_t actual_nv_pairs_2[1];
    H5CL_nv_pair_t actual_nv_pairs_3[11];
    H5CL_nv_pair_t actual_nv_pairs_4[1];
    H5CL_nv_pair_t actual_nv_pairs_5[1];
    char l0_page_buffer[12] = "page_buffer";
    H5CL_nv_pair_t expected_nv_pairs_0[1] =
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l0_page_buffer,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ input_string_1,
            /* len          = */ 511 
        }
    };
    char l1_page_size[]          = "page_size";
    char l1_max_num_pages[]      = "max_num_pages";
    char l1_replacement_policy[] = "replacement_policy";
    char l1_underlying_vfd[]     = "underlying_VFD";
    H5CL_nv_pair_t expected_nv_pairs_1[4] = 
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l1_page_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 4096,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l1_max_num_pages,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 16,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l1_replacement_policy,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l1_underlying_vfd,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ input_string_2,
            /* len          = */ 404
        }
    };
    char l2_encryption_VFD[] = "encryption_VFD";
    H5CL_nv_pair_t expected_nv_pairs_2[1] = 
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l2_encryption_VFD,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ input_string_3,
            /* len          = */ 372
        }
    };
    char l3_plaintext_page_size[]    = "plaintext_page_size";
    char l3_ciphertext_page_size[]   = "ciphertext_page_size";
    char l3_encryption_buffer_size[] = "encryption_buffer_size";
    char l3_cipher[]                 = "cipher";
    char l3_cipher_block_size[]      = "cipher_block_size";
    char l3_key_size[]               = "key_size";
    char l3_key[]                    = "key";
    char l3_iv_size[]                = "iv_size";
    char l3_mode[]                   = "mode";
    char l3_underlying_VFD[]         = "underlying_VFD";
    H5CL_nv_pair_t expected_nv_pairs_3[11] = 
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_plaintext_page_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 4096,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_ciphertext_page_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 4112,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_encryption_buffer_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 65792,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_cipher,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_cipher_block_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 16,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_key_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 32,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_key,
            /* val_type     = */ H5CL_VAL_BB,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ key,
            /* len          = */ key_len
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_iv_size,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 16,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_mode,
            /* val_type     = */ H5CL_VAL_INT,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ NULL,
            /* len          = */ 0
        },
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l3_underlying_VFD,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ input_string_4,
            /* len          = */ 11
        }
    };
    char l4_sec2[] = "sec2";
    H5CL_nv_pair_t expected_nv_pairs_4[1] = 
    { 
        {
            /* struct_tag   = */ H5CL_NV_PAIR_STRUCT_TAG,
            /* name_ptr     = */ l4_sec2,
            /* val_type     = */ H5CL_VAL_LIST,
            /* int_val      = */ 0,
            /* f_val        = */ 0.0,
            /* vlen_val_ptr = */ input_string_5,
            /* len          = */ 2
        }
    };
    H5CL_nv_pair_t expected_nv_pairs_5[1];

    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* line_num          = */ 0,
        /* char_num          = */ 0, 
        /* token             = */ {
        /* token.struct_tag  = */    H5CL_TOKEN_STRUCT_TAG,
        /* token.code        = */    H5CL_ERROR_TOK,
        /* token.str_ptr     = */    NULL,
        /* token.str_len     = */    0,
        /* token.max_str_len = */    0,
        /* token.int_val     = */    1,    /* should be overwritten on init */
        /* token.f_val       = */    1.0,  /* should be overwritten on init */
        /* token.bb_ptr      = */    NULL,
        /* token.bb_len      = */    0
        /* end of token        */ }
    };

    TESTING("VFD Configuration Language Parser Smoke Check");

    /* Level 0 */

    if ( H5CL__init_lex_vars(input_string_0, &lex_vars) < 0 )
        TEST_ERROR;


    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_0; i++ ) {

        actual_nv_pairs_0[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_0[i])) < 0 )
            TEST_ERROR;

    }

    if ( H5CL__parse_name_value_pair(&(actual_nv_pairs_0[0]), &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs_0, expected_nv_pairs_0, 1, true) )
        TEST_ERROR;


    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( i = 0; i < num_nv_pairs_0; i++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs_0[0])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 ) 
        TEST_ERROR;


    /* level 1 */

    lex_vars.struct_tag = H5CL_LEX_VARS_STRUCT_TAG;
    if ( H5CL__init_lex_vars(input_string_1, &lex_vars) < 0 )
        TEST_ERROR;

    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_1; i++ ) {

        actual_nv_pairs_1[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_1[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs_1, num_nv_pairs_1, &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs_1, expected_nv_pairs_1, num_nv_pairs_1, true) )
        TEST_ERROR;


    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( i = 0; i < num_nv_pairs_1; i++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs_1[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    /* level 2 */

    lex_vars.struct_tag = H5CL_LEX_VARS_STRUCT_TAG;
    if ( H5CL__init_lex_vars(input_string_2, &lex_vars) < 0 )
        TEST_ERROR;

    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_2; i++ ) {

        actual_nv_pairs_2[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_2[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__parse_name_value_pair(&(actual_nv_pairs_2[0]), &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs_2, expected_nv_pairs_2, num_nv_pairs_2, true) )
        TEST_ERROR;

    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( i = 0; i < num_nv_pairs_2; i++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs_2[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    /* level 3 */

    lex_vars.struct_tag = H5CL_LEX_VARS_STRUCT_TAG;
    if ( H5CL__init_lex_vars(input_string_3, &lex_vars) < 0 )
        TEST_ERROR;

    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_3; i++ ) {

        actual_nv_pairs_3[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_3[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs_3, num_nv_pairs_3, &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs_3, expected_nv_pairs_3, num_nv_pairs_3, true) )
        TEST_ERROR;

    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( i = 0; i < num_nv_pairs_3; i++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs_3[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    /* level 4 */

    lex_vars.struct_tag = H5CL_LEX_VARS_STRUCT_TAG;
    if ( H5CL__init_lex_vars(input_string_4, &lex_vars) < 0 )
        TEST_ERROR;

    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_4; i++ ) {

        actual_nv_pairs_4[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_4[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__parse_name_value_pair(&(actual_nv_pairs_4[0]), &lex_vars) < 0 )
        TEST_ERROR;

    if ( 0 != cl_test_verify_nv_pairs(actual_nv_pairs_4, expected_nv_pairs_4, num_nv_pairs_4, true) )
        TEST_ERROR;

    /* Don't take down expected name value pairs since all strings are either constant
     * or allocated on the stack.
     */

    for ( i = 0; i < num_nv_pairs_4; i++ ) {

        if ( H5CL__take_down_nv_pair(&(actual_nv_pairs_4[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    /* level 5 */

    lex_vars.struct_tag = H5CL_LEX_VARS_STRUCT_TAG;
    if ( H5CL__init_lex_vars(input_string_5, &lex_vars) < 0 )
        TEST_ERROR;

    /* initialize the array of instance of cl_nv_pair_t */
    for ( i = 0; i < num_nv_pairs_5; i++ ) {

        actual_nv_pairs_5[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(actual_nv_pairs_5[i])) < 0 )
            TEST_ERROR;

        expected_nv_pairs_5[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL__init_nv_pair(&(expected_nv_pairs_5[i])) < 0 )
            TEST_ERROR;
    }

    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs_5, num_nv_pairs_5, &lex_vars) < 0 ) 
        TEST_ERROR;

    if ( actual_nv_pairs_5[0].val_type != H5CL_VAL_NONE )
        TEST_ERROR;

    /* Don't take down the actual and expected name value pairs since they contain no strings */

    if ( H5CL__take_down_lex_vars(&lex_vars) < 0 )
        TEST_ERROR;


    PASSED();

    return 0;

error:

    return -1;

} /* cl_parser_smoke_check() */


/*-------------------------------------------------------------------------
 * Function:    main
 *
 * Purpose:     Tests VFD configuration language functionality
 *
 * Return:      EXIT_SUCCESS/EXIT_FAILURE
 *
 *-------------------------------------------------------------------------
 */
int
main(void)
{
    int         nerrors = 0;

    h5_test_init();

    printf("Testing Virtual File Driver Configuration Language functionality.\n");

    nerrors += cl_lexer_smoke_check() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_smoke_check() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_list_smoke_check() < 0 ? 1 : 0;
    nerrors += cl_parser_smoke_check() < 0 ? 1 : 0;

    if (nerrors) {
        printf("***** %d Virtual File Driver Configuration Language TEST%s FAILED! *****\n", 
               nerrors, nerrors > 1 ? "S" : "");
        return EXIT_FAILURE;
    }

    printf("All Virtual File Driver Configuration Language tests passed.\n");

    return EXIT_SUCCESS;

} /* end main() */
