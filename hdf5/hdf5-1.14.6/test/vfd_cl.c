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
#define H5E_FRIEND
#define H5CL_FRIEND

#include "h5test.h"
#include "H5CLpkg.h"
#include "H5Epkg.h"

/* utility functions */
static bool cl_lexer_test_verify_token(H5CL_token_t * token_ptr, int token_num, int32_t expected_code, 
                                       const char * expected_str, int64_t expected_int_val, double expected_f_val, 
                                       uint8_t * expected_bb_ptr, size_t expected_bb_len, bool verbose);
static int cl_test_verify_nv_pair(H5CL_nv_pair_t * nv_pair_ptr, int nv_pair_num, const char * expected_name_ptr, 
                                  int expected_val_type, int64_t expected_int_val, double expected_f_val,
                                  const void * expected_vlen_val_ptr, size_t expected_len, bool verbose);
static int cl_test_verify_nv_pairs(H5CL_nv_pair_t * actual_nv_pairs, H5CL_nv_pair_t * expected_nv_pairs,
                                   int num_nv_pairs, bool verbose);
static int cl_test_verify_error_stack(hid_t maj_num, hid_t min_num, const char *desc, bool verbose);

/* test functions */
static herr_t cl_lexer_smoke_check(void);
static herr_t cl_lexer_detail_check(void);
static herr_t cl_lexer_error_check_1(void);
static herr_t cl_lexer_error_check_2(void);
static herr_t cl_lexer_error_check_3(void);
static herr_t cl_lexer_error_check_4(void);
static herr_t cl_parse_name_val_pair_smoke_check(void);
static herr_t cl_parse_nv_pair_error_check_1(void);
static herr_t cl_parse_nv_pair_error_check_2(void);
static herr_t cl_parse_nv_pair_error_check_3(void);
static herr_t cl_parse_nv_pair_error_check_4(void);
static herr_t cl_parse_nv_pair_error_check_5(void);
static herr_t cl_parse_nv_pair_error_check_6(void);
static herr_t cl_parse_nv_pair_error_check_7(void);
static herr_t cl_parse_name_val_pair_list_smoke_check(void);
static herr_t cl_parse_name_val_pair_list_err_check_1(void);
static herr_t cl_parse_name_val_pair_list_err_check_2(void);
static herr_t cl_parse_name_val_pair_list_err_check_3(void);
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

                fprintf(stdout, "0x%02x ", (unsigned)(token_ptr->bb_ptr[i]));
            }

            fprintf(stdout, "\nexpected bb = ");

            for ( i = 0; i < (int)expected_bb_len; i++ ) {

                fprintf(stdout, "0x%02x ", (unsigned)(expected_bb_ptr[i]));
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
 * cl_test_verify_error_stack()
 *
 * Verify that the bottom entry on the current error stack has major and 
 * minor error IDs and error message matching the supplied values.
 *
 *                                              JRM -- 1/10/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static int
cl_test_verify_error_stack(hid_t maj_num, hid_t min_num, const char *desc, bool verbose)
{
    int failures = 0;
    H5E_stack_t * estack_ptr;
    H5E_entry_t * entry_ptr;
    H5E_error2_t * err_ptr;

    if ( NULL == (estack_ptr = H5E__get_my_stack()) ) {

        failures++;

        if ( verbose ) {

            fprintf(stderr, "\ncl_test_verify_error_stack(): can't get error stack\n");
        }
    } else if ( estack_ptr->nused < 1 ) {

        failures++;

        if ( verbose ) {

            fprintf(stderr, "\ncl_test_verify_error_stack(): error stack is empty\n");
        }
    } else {

        entry_ptr = &(estack_ptr->entries[0]);
        err_ptr = &(entry_ptr->err);

        if ( ( maj_num != err_ptr->maj_num ) ||
             ( min_num != err_ptr->min_num ) ||
             ( 0 != strcmp(desc, err_ptr->desc) ) ) {

            failures++;

            if ( verbose ) {

                fprintf(stderr, "\n\nActual / Expected major error number = 0x%llx / 0x%llx.\n",
                        (long long)(err_ptr->maj_num), (long long)(maj_num));
                fprintf(stderr, "Actual / Expected minor error number = 0x%llx / 0x%llx.\n",
                        (long long)(err_ptr->min_num), (long long)(min_num));
                fprintf(stderr, "Actual error desc = \"%s\".\n", err_ptr->desc);
                fprintf(stderr, "Expected error desc = \"%s\".\n\n", desc);
            }
        }

        H5E__clear_stack(estack_ptr);
    }

    return(failures);

} /* cl_test_verify_error_stack() */


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
        /* err_ctx           = */ "",
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

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 0 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_L_PAREN_TOK, "(", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 1 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 2 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "symbol", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 3 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "1", 1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 4 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, "3.14159", 0, 3.14159, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 5 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_QSTRING_TOK, "Hello World", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 6 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--00010203", 0, 0.0, 
                                          bb_0, bb_0_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) < 0 ) /* 7 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_LIST_TOK, "( sec2 () )", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, true, &token_ptr, &lex_vars) < 0 ) /* 8 */
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
 * cl_lexer_detail_check()
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
cl_lexer_detail_check(void)
{
    int token_num = 0;
    const char * input_string = "(()())/* comment */)A1 1+1-1 2A2 1.1.1 +.2-.3\"i\"A/**/B\"\\\"\")"
                                "--0--123 --aAb --AaB --0ff)(/* commenta can appear in lists)"
                                "(ilegal characters, i.e.!@#$%^;:&*, can appear in lists)"
                                "( and ()(((arbitrary))nesting of((parens))))";
    uint8_t bb_0[] = { 0 };
    uint8_t bb_1[] = { 18, 48 };
    uint8_t bb_2[] = { 170, 176 };
    uint8_t bb_3[] = { 15, 240 };
    size_t bb_0_len = 1;
    size_t bb_1_len = 2;
    size_t bb_2_len = 2;
    size_t bb_3_len = 2;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language Lexer detail Check");

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

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 0 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_L_PAREN_TOK, "(", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 1 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_L_PAREN_TOK, "(", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 2 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 3 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_L_PAREN_TOK, "(", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 4 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 5 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 6 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 7 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "A1", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 8 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "1", 1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 9 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "+1", 1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 10 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "-1", -1, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 11 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_INT_TOK, "2", 2, 0.0, NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 12 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "A2", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 13 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, "1.1", 0, 1.1, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 14 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, ".1", 0, .1, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 15 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, "+.2", 0, .2, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 16 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_FLOAT_TOK, "-.3", 0, -.3, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 17 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_QSTRING_TOK, "i", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 18 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "A", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 19 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_SYMBOL_TOK, "B", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 20 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_QSTRING_TOK, "\\\"", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 21 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 22 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--0", 0, 0.0, 
                                          bb_0, bb_0_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 23 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--123", 0, 0.0, 
                                          bb_1, bb_1_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 24 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--aAb", 0, 0.0, 
                                          bb_2, bb_2_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 25 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--AaB", 0, 0.0, 
                                          bb_2, bb_2_len, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 26 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_BIN_BLOB_TOK, "--0ff", 0, 0.0, 
                                          bb_3, bb_3_len, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) < 0 ) /* 27 */
        TEST_ERROR;

    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_R_PAREN_TOK, ")", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) < 0 ) /* 28 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_LIST_TOK, 
                                         "(/* commenta can appear in lists)", 0, 0.0, NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) < 0 ) /* 29 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_LIST_TOK, 
                                         "(ilegal characters, i.e.!@#$%^;:&*, can appear in lists)", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;
    

    if ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) < 0 ) /* 30 */
        TEST_ERROR;
    
    if ( 0 != cl_lexer_test_verify_token(token_ptr, token_num++, H5CL_LIST_TOK, 
                                         "( and ()(((arbitrary))nesting of((parens))))", 0, 0.0, 
                                         NULL, 0, true) )
        TEST_ERROR;


    if ( H5CL__lex_read_token(false, true, &token_ptr, &lex_vars) < 0 ) /* 31 */
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

} /* cl_lexer_detail_check() */


/*******************************************************************************
 *
 * cl_lexer_error_check_1()
 *
 * Verify that the lexer detects and reports errors as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_lexer_error_check_1(void)
{
    const char * input_string = "* /* a comment */&/*another comment */    _=% {}[]\"unterminated string";
    bool verbose = true;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language Lexer error detection & reporting 1");

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

    /* should fail on illegal char '*' */
    if  ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Illagal char '*' in input string.  Context: * /* a comment */&/*another co...", 
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '&' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Illagal char '&' in input string.  Context: ...* a comment */&/*another comme...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '_' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char '_' in input string.  Context: ...comment */    _=% {}[]\"untermi...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '=' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char '=' in input string.  Context: ...omment */    _=% {}[]\"untermin...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '%' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Percent sign in input string.  Context: ...mment */    _=% {}[]\"untermina...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '{' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char '{' in input string.  Context: ...ent */    _=% {}[]\"unterminate...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '}' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char '}' in input string.  Context: ...nt */    _=% {}[]\"unterminated...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char '[' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char '[' in input string.  Context: ...t */    _=% {}[]\"unterminated ...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on illegal char ']' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Illagal char ']' in input string.  Context: ... */    _=% {}[]\"unterminated s...",
                                verbose) ) {

        TEST_ERROR;
    }

    /* should fail on an unterminated string' */
    if ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Un-terminate quote string in input string.  Context: ...rminated string",
                                verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_lexer_error_check_1() */


/*******************************************************************************
 *
 * cl_lexer_error_check_2()
 *
 * Verify that the lexer detects and reports errors as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_lexer_error_check_2(void)
{
    const char * input_string = "/* malformed numeric values */ + - . +. -. (an unterminated list";
    bool verbose = true;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language Lexer error detection & reporting 2");

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

    /* should fail on an ill formed numeric constantt */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Ill-formed numerical constant.  Context: ...eric values */ + - . +. -. (an...",
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on an ill formed numeric constantt */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Ill-formed numerical constant.  Context: ...ic values */ + - . +. -. (an u...",
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on an ill formed numeric constantt */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Ill-formed numerical constant.  Context: ... values */ + - . +. -. (an unt...",
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on an ill formed numeric constantt */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Ill-formed numerical constant.  Context: ...alues */ + - . +. -. (an unter...",
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on an ill formed numeric constantt */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Ill-formed numerical constant.  Context: ...es */ + - . +. -. (an untermin...",
                               verbose) ) {

        TEST_ERROR;
    }

    /* should fail on na unterminate list */
    if  ( H5CL__lex_read_token(true, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Un-terminated list in input string.  Context: ...terminated list",
                               verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_lexer_error_check_2() */


/*******************************************************************************
 *
 * cl_lexer_error_check_3()
 *
 * Verify that the lexer detects and reports errors as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_lexer_error_check_3(void)
{
    const char * input_string = " /* an empty input string to generate an unexpected EOI error */";
    bool verbose = true;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language Lexer error detection & reporting 3");

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

    /* should fail on an un enxpected end of input string error */
    if  ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Un-expected end of input string.  Context: ...ed EOI error */",
                               verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_lexer_error_check_3() */


/*******************************************************************************
 *
 * cl_lexer_error_check_4()
 *
 * Verify that the lexer detects and reports errors as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_lexer_error_check_4(void)
{
    const char * input_string = " /* end of input in a comment ";
    bool verbose = true;
    H5CL_token_t * token_ptr;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language Lexer error detection & reporting 4");

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

    /* should fail on an un enxpected end of input string error */
    if  ( H5CL__lex_read_token(false, false, &token_ptr, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                               "Un-expected end of input string.  Context: ...t in a comment ",
                               verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_lexer_error_check_4() */


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
        /* err_ctx           = */ "",
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

        if ( H5CL_init_nv_pair(&(nv_pairs[nv_pair_num])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(nv_pairs[nv_pair_num])) < 0 ) 
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
 * cl_parse_nv_pair_error_check_1()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_1(void)
{
    const char * input_string = "name 1 ) /* NV pair missing the opening paren */";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 1");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on a missing initial paren */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
            "Syntax error -- Initial '(' of name value pair expected.  Context: name 1 ) /* NV pair missing th...",
            verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_1() */

/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_2()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_2(void)
{
    const char * input_string = "( /* NV pair missing the name */ 1 --01020304 )";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 2");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on a missing name in the name value pair */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                    "Syntax error -- name of name value pair expected.  Context: ...g the name */ 1 --01020304 )",
                    verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_2() */


/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_3()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_3(void)
{
    const char * input_string = "( name /* NV pair missing the value */ )";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 3");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on a missing value in the name value pair */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                "Syntax error -- value of name value pair expected.  Context: ... the value */ )",
                                verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_3() */


/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_4()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_4(void)
{
    const char * input_string = "( name 1.1 /* NV pair with extra value */ --01020304 )";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 4");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on an extra value / missting closing paren in the name value pair */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                    "Syntax error -- Terminal ')' of name value pair expected.  Context: ...e */ --01020304 )",
                    verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_4() */


/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_5()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_5(void)
{
    const char * input_string = "( name \" unterminated quote string ";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 5");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on an unterminated quote string */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Un-terminate quote string in input string.  Context: ...d quote string ",
                                        verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_5() */


/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_6()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_6(void)
{
    const char * input_string = "( name ( unterminated list ";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 6");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on an unterminated list */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Un-terminated list in input string.  Context: ...erminated list ",
                                        verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_6() */


/*******************************************************************************
 *
 * cl_parse_nv_pair_error_check_7()
 *
 * Verify that the name value pair parser function detects and reports errors 
 * as expected.
 *
 *                                              JRM -- 1/8/26
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_nv_pair_error_check_7(void)
{
    const char * input_string = "( name 3.14159 /* unexpected EOI */ ";
    bool verbose = true;
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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
    H5CL_nv_pair_t nv_pair;

    TESTING("VFD Configuration Language NV pair err detection & reporting 7");

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

    nv_pair.struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

    if ( H5CL_init_nv_pair(&nv_pair) < 0 )
        TEST_ERROR;

    /* should fail on an unterminated list */
    if ( H5CL__parse_name_value_pair(&nv_pair, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Un-expected end of input string.  Context: ...xpected EOI */ ",
                                        verbose) ) {

        TEST_ERROR;
    }

    PASSED();

    return 0;

error:

    return -1;

} /* cl_parse_nv_pair_error_check_7() */


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
        /* err_ctx           = */ "",
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
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
 * cl_parse_name_val_pair_list_err_check_1()
 *
 * Name value pair errer detection and reporting test.
 *
 *                                              JRM -- 12/20/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_name_val_pair_list_err_check_1(void)
{
    bool verbose = true;
    int nv_pair_num = 0;
    const char * input_string = " ( name_0 1 ) ( name_1 3.14159 ) ( name_2 \"Hello World\" ) "
                                "( name_3 --10111213 ) ( name_4 ( sec2 () ) ) )";
    H5CL_nv_pair_t actual_nv_pairs[5];
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language NV Pair List err detect & report 1");

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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }

    /* missing initial left paren -- should fail with either left or right parent expected */
    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs, 5, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Syntax error -- Terminal \')\' of name value pair list or leading \'(\' "
                                        "of name value pair expected.  Context:  ( name_0 1 ) ( name_1 3.14159...",
                                        verbose) ) {

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

} /* cl_parse_name_val_pair_list_err_check_1() */


/*******************************************************************************
 *
 * cl_parse_name_val_pair_list_err_check_2()
 *
 * Name value pair errer detection and reporting test.
 *
 *                                              JRM -- 12/20/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_name_val_pair_list_err_check_2(void)
{
    bool verbose = true;
    int nv_pair_num = 0;
    const char * input_string = "  name_0 1 ) ( name_1 3.14159 ) ( name_2 \"Hello World\" ) "
                                "( name_3 --10111213 ) ( name_4 ( sec2 () ) ) )";
    H5CL_nv_pair_t actual_nv_pairs[5];
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language NV Pair List err detect & report 2");

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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }

    /* missing initial left paren -- should fail with either left or right parent expected */
    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs, 5, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Syntax error -- Initial \'(\' of name value pair list expected.  "
                                        "Context:   name_0 1 ) ( name_1 3.14159 ...",
                                        verbose) ) {

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

} /* cl_parse_name_val_pair_list_err_check_2() */


/*******************************************************************************
 *
 * cl_parse_name_val_pair_list_err_check_3()
 *
 * Name value pair errer detection and reporting test.
 *
 *                                              JRM -- 12/20/25
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

static herr_t
cl_parse_name_val_pair_list_err_check_3(void)
{
    bool verbose = true;
    int nv_pair_num = 0;
    const char * input_string = "( ( name_3 --10111213- ) ( name_4 ( sec2 () ) ) )";
    H5CL_nv_pair_t actual_nv_pairs[5];
    H5CL_lex_vars_t lex_vars = {
        /* struct_tag        = */ H5CL_LEX_VARS_STRUCT_TAG,
        /* input_str_ptr     = */ NULL,
        /* next_char_ptr     = */ NULL,
        /* end_of_input      = */ false, 
        /* err_ctx           = */ "",
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

    TESTING("VFD Configuration Language NV Pair List err detect & report 3");

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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs[nv_pair_num])) < 0 )
            TEST_ERROR;
    }

    /* missing initial left paren -- should fail with either left or right parent expected */
    if ( H5CL__parse_name_value_pair_list(actual_nv_pairs, 5, &lex_vars) >= 0 ) {

        TEST_ERROR;

    } else if ( 0 != cl_test_verify_error_stack(H5E_ARGS, H5E_BADVALUE, 
                                        "Ill-formed numerical constant.  "
                                        "Context: ...me_3 --10111213- ) ( name_4 ( ...",
                                        verbose) ) {

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

} /* cl_parse_name_val_pair_list_err_check_3() */


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
        /* err_ctx           = */ "",
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_0[i])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs_0[0])) < 0 )
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_1[i])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs_1[i])) < 0 )
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_2[i])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs_2[i])) < 0 )
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_3[i])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs_3[i])) < 0 )
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_4[i])) < 0 )
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

        if ( H5CL_take_down_nv_pair(&(actual_nv_pairs_4[i])) < 0 )
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

        if ( H5CL_init_nv_pair(&(actual_nv_pairs_5[i])) < 0 )
            TEST_ERROR;

        expected_nv_pairs_5[i].struct_tag = H5CL_NV_PAIR_STRUCT_TAG;

        if ( H5CL_init_nv_pair(&(expected_nv_pairs_5[i])) < 0 )
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
    nerrors += cl_lexer_detail_check() < 0 ? 1 : 0;
    nerrors += cl_lexer_error_check_1() < 0 ? 1 : 0;
    nerrors += cl_lexer_error_check_2() < 0 ? 1 : 0;
    nerrors += cl_lexer_error_check_3() < 0 ? 1 : 0;
    nerrors += cl_lexer_error_check_4() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_smoke_check() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_1() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_2() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_3() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_4() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_5() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_6() < 0 ? 1 : 0;
    nerrors += cl_parse_nv_pair_error_check_7() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_list_smoke_check() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_list_err_check_1() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_list_err_check_2() < 0 ? 1 : 0;
    nerrors += cl_parse_name_val_pair_list_err_check_3() < 0 ? 1 : 0;
    nerrors += cl_parser_smoke_check() < 0 ? 1 : 0;

    if (nerrors) {
        printf("***** %d Virtual File Driver Configuration Language TEST%s FAILED! *****\n", 
               nerrors, nerrors > 1 ? "S" : "");
        return EXIT_FAILURE;
    }

    printf("All Virtual File Driver Configuration Language tests passed.\n");

    return EXIT_SUCCESS;

} /* end main() */
