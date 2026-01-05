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
 * Purpose: The VFD configuration language is intended to enable setup 
 *          of VFD stacks without the current system of creating stacks 
 *          of FAPLs.  See H5FDcl_pkg.h for a quick overview of the 
 *          language.
 */

/****************/
/* Module Setup */
/****************/

#include "H5CLmodule.h" /* This source code file is part of the H5FD module */


/***********/
/* Headers */
/***********/
#include "H5private.h"   /* Generic Functions                        */
#include "H5CLpkg.h"   /* VFD Configuration language               */
#include "H5Eprivate.h"  /* Error handling                           */


/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/


/*******************************************************************************
 *
 * Function:    H5CL__init_lex_vars()
 *
 * Purpose:     Initialize the supplied instance of struct_H5CL_lex_vars_t 
 *              to lex the supplied input string.  The struct_tag field is 
 *              presumed to be set, but the instance of H5CL_lex_vars_t 
 *              (and its instance of H5CL_token_t) are assumed to be 
 *              otherwise un-initialized.
 *
 *              Note that this function allocates several strings that must 
 *              be freed by a matching call to H5CL__take_down_lex_vars() 
 *              at the end of the parse.
 * 
 *                                              JRM -- 12/5/25
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

herr_t
H5CL__init_lex_vars(const char * input_str_ptr, H5CL_lex_vars_t * lex_vars_ptr)
{
    size_t input_str_len;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(input_str_ptr);
    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);

    input_str_len = strlen(input_str_ptr);

    if ( input_str_len <= 0 ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "empty input string");

    /* copy the input string into *lex_vars_ptr */
    if ( NULL == (lex_vars_ptr->input_str_ptr = (char *)H5MM_malloc(input_str_len + 1) ) )
        HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for input string");

    strncpy(lex_vars_ptr->input_str_ptr, input_str_ptr, input_str_len);
    lex_vars_ptr->input_str_ptr[input_str_len] = '\0';
    
    assert(strlen(input_str_ptr) == strlen(lex_vars_ptr->input_str_ptr));

    /* next_char_ptr to the first character in the input string */
    lex_vars_ptr->next_char_ptr = lex_vars_ptr->input_str_ptr;

    /* Set line_num and char_num to zero.  Note that line and char
     * numbers are relative to the supplied input string, which 
     * may be a subset of the externally supplied configuration 
     * string.
     */
    lex_vars_ptr->line_num = 0;
    lex_vars_ptr->char_num = 9;

    /* now set up the token. */

    lex_vars_ptr->token.struct_tag = H5CL_TOKEN_STRUCT_TAG;

    lex_vars_ptr->token.code = H5CL_ERROR_TOK;

    if ( NULL == (lex_vars_ptr->token.str_ptr = (char *)H5MM_malloc(input_str_len + 1) ) )
        HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for token string");

    lex_vars_ptr->token.str_len = 0;

    lex_vars_ptr->token.max_str_len = input_str_len;

    lex_vars_ptr->end_of_input = false;

    lex_vars_ptr->token.int_val = 0;

    lex_vars_ptr->token.f_val = 0.0;

    if ( NULL == (lex_vars_ptr->token.bb_ptr = (uint8_t *)H5MM_malloc(input_str_len + 1) ) )
        HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for token bb buffer");

    lex_vars_ptr->token.bb_len = 0;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__init_lex_vars() */


/*******************************************************************************
 *
 * Function:    H5CL__take_down_lex_vars()
 *
 * Purpose:     Discard all dynamically allocated memory associates with the 
 *              supplied instance of struct_H5CL_lex_vars_t and set its 
 *              struct tag to an invalid value.
 * 
 *                                              JRM -- 12/5/25
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

herr_t
H5CL__take_down_lex_vars(H5CL_lex_vars_t * lex_vars_ptr)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);
    assert(H5CL_TOKEN_STRUCT_TAG == lex_vars_ptr->token.struct_tag);
   
    if ( ( NULL == lex_vars_ptr->input_str_ptr ) ||
         ( NULL == lex_vars_ptr->token.str_ptr ) ||
         ( NULL == lex_vars_ptr->token.bb_ptr ) ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "*lex_vars_ptr never set up?");
        
    /* invalidate the struct tags */
    lex_vars_ptr->struct_tag       = H5CL_INVALID_LEX_VARS_STRUCT_TAG;
    lex_vars_ptr->token.struct_tag = H5CL_INVALID_TOKEN_STRUCT_TAG;

    /* free the dynamically allocated strings */
    H5MM_xfree(lex_vars_ptr->input_str_ptr);
    lex_vars_ptr->input_str_ptr = NULL;
    H5MM_xfree(lex_vars_ptr->token.str_ptr);
    lex_vars_ptr->token.str_ptr = NULL;
    H5MM_xfree(lex_vars_ptr->token.bb_ptr);
    lex_vars_ptr->token.bb_ptr = NULL;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__init_lex_vars() */


/*******************************************************************************
 *
 * Function:    H5CL__lex_get_non_blank()
 *
 * Purpose:     Advance lex_vars_ptr->next_char until it points to either 
 *              a non white space character or a null character ('\0).  If 
 *              lex_vars_ptr->next_char already points to a non-blank 
 *              character, the function does nothing.
 *    
 *              Note that this routine recognizes C style comments, and 
 *              treats them as white space.  Recall that the beginning of 
 *              a comment is indicated by a slash star combination, 
 *              and is terminated by a star slash combination. 
 *
 *              The function returns when it finds the first non-blank 
 *              character that is not part of a comment. 
 *
 *                                                    JRM - 12/8/25/
 *
 * Parameters:
 *
 *    lex_vars_ptr: Pointer to the instance of H5CL_lex_vars_t containing 
 *              the input string.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 *
 *  Changes:
 *
 *        - None.
 *
 *******************************************************************************/

herr_t
H5CL__lex_get_non_blank(H5CL_lex_vars_t * lex_vars_ptr)
{
    char next_char;
    bool in_comment;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);
    assert(H5CL_TOKEN_STRUCT_TAG == lex_vars_ptr->token.struct_tag);

    next_char = '\0';
    in_comment = false;

    if ( lex_vars_ptr->end_of_input )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "*lex_vars_ptr->end_of_input is set");

    if ( ! lex_vars_ptr->end_of_input ) {

        next_char = *lex_vars_ptr->next_char_ptr;

        if ( ( '/' == next_char ) && ( '*' == *(lex_vars_ptr->next_char_ptr + 1) ) )
        {
            in_comment = true;

            lex_vars_ptr->next_char_ptr++;
            lex_vars_ptr->next_char_ptr++;
            next_char = *lex_vars_ptr->next_char_ptr;
        }

        while ( ! lex_vars_ptr->end_of_input ) {

            if ( '\0' == next_char ) {

                lex_vars_ptr->end_of_input = true;

            } else if ( isspace(next_char) ) {

                /* next_char is either space, tab, new line(\n), carrage return (\r), 
                 * vertical tab (\v), or form feed (\f) -- just increment 
                 * lex_vars_ptr->next_char_ptr.
                 */
                lex_vars_ptr->next_char_ptr++;
                next_char = *lex_vars_ptr->next_char_ptr;

            } else if ( ( '/' == next_char ) && ( '*' == *(lex_vars_ptr->next_char_ptr + 1) ) ) {

                /* the beginning of a comment is indicated by a '/' followed by a '*'.  Note that
                 * it doesn't matter if in_comment is already true.
                 *
                 * Set in_comment to true and increment lex_vars_ptr->next_char_ptr past the 
                 * slash star.
                 */
                in_comment = true;

                lex_vars_ptr->next_char_ptr++;
                lex_vars_ptr->next_char_ptr++;
                next_char = *lex_vars_ptr->next_char_ptr;
                
            } else if ( in_comment ) {

                /* test for end of comment */
                if ( ( '*' == next_char ) && ( '/' == *(lex_vars_ptr->next_char_ptr + 1) ) ) {

                    /* end the comment and increment lext_vars->next_char_ptr accordingly */
                    in_comment = false;

                    lex_vars_ptr->next_char_ptr++;
                    lex_vars_ptr->next_char_ptr++;
                    next_char = *lex_vars_ptr->next_char_ptr;

                } else {

                    /* the comment continues -- just increment lex_vars_ptr->next_char_ptr. */
                    lex_vars_ptr->next_char_ptr++;
                    next_char = *lex_vars_ptr->next_char_ptr;
                }
            } else if ( ( isalnum(next_char) ) || ( '(' == next_char ) || ( ')' == next_char ) ||
                        ( '"' == next_char ) || ( '+' == next_char ) || ( '-' == next_char ) ||
                        ( '.' == next_char ) ) {

                /* next_char is a graphical char that can appear as the first character 
                 * of a token in a valid configuration languate string.  Break and return 
                 * next_char.
                 */
                break;

            } else {

                /* We have encountered an illegal character.  Throw an error. */

                /* replace with error call */
                assert(false); 
            }

        } /* while */

    } /* if */

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__lex_get_non_blank() */


/*******************************************************************************
 *
 * Function:    H5CL__lex_peek_next_char
 *
 * Purpose:     Return the next non-blank character in the input string in 
 *              *next_char_ptr.  Note that this character is not consumed, 
 *              and will be the first character in the next token recognized 
 *              by the lexer.
 *
 *                                                      JRM - 12/20/25
 *
 * Parameters:
 *
 *    next_char_ptr: Pointer to character.  If successful, *next_char_ptr
 *              is set equal to the next non-blank character in the input 
 *              string.
 *
 *    lex_vars_ptr: Pointer to the instance of H5CL_lex_vars_t that contains 
 *              the input string, lexer vars, and the instance of cl_tok_t to 
 *              be loaded with the next token and returned to the caller.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 *  Changes:
 *
 *        - None.
 *
 *******************************************************************************/

herr_t
H5CL__lex_peek_next_char(char * next_char_ptr, H5CL_lex_vars_t * lex_vars_ptr)
{
    char next_char;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(next_char_ptr);
    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);
    assert(H5CL_TOKEN_STRUCT_TAG == lex_vars_ptr->token.struct_tag);
    
    if ( lex_vars_ptr->end_of_input ) {

        next_char = '\0';

    } else if ( H5CL__lex_get_non_blank(lex_vars_ptr) < 0 ) {

        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_get_non_blank() failed.");

    } else {

        next_char = *(lex_vars_ptr->next_char_ptr);
    }

    *next_char_ptr = next_char;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__lex_peek_next_char() */


/*******************************************************************************
 *
 * Function:    H5CL__lex_read_token
 *
 * Purpose:     Read the next token from the input string in lex_vars, load it 
 *              into instace of H5CL_token_t incorporated into *lex_vars_ptr, 
 *              and return a pointer to same in *token_ptr_ptr..
 *
 *                                                      JRM - 12/7/25
 *
 * Parameters:
 *
 *    value_expected: Boolean flag that is set to true when the value 
 *              in a name value pair is expected.  When set, this flag
 *              causes the lexer to treat any string starting with a 
 *              '(' up to the matching ')' as a single token.  This is 
 *              necessary to support the bredth first parsing needed 
 *              to configure an arbitrary stack of VFD.  The token 
 *              so recognized in passed into an open call, which 
 *              parses it sufficiently to obtain the name of the 
 *              underlying VFD and its configuration string, and 
 *              then calls the open routine for the target VFD with 
 *              the supplied configuration string.
 *
 *    toke_ptr_ptr: Pointer to pointer to token.  On successful return,
 *              *token_ptr_ptr is set to point to the newly recognized 
 *              token -- always &(lex_vars_ptr->token) at present.
 *
 *    lex_vars_ptr: Pointer to the instance of H5CL_lex_vars_t that 
 *              contains the input string, lexer variables, and the 
 *              instance of H5CL_token_t to be loaded with the next 
 *              token and returned to the caller.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 * Changes:
 *
 *        - None.
 *
 *******************************************************************************/

herr_t
H5CL__lex_read_token(bool value_expected, H5CL_token_t **token_ptr_ptr, 
                       H5CL_lex_vars_t * lex_vars_ptr)
{
    char next_char;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(token_ptr_ptr);
    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);
    assert(H5CL_TOKEN_STRUCT_TAG == lex_vars_ptr->token.struct_tag);
    
    if ( lex_vars_ptr->end_of_input )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Attempt to read past end of input string.");

    /* reset the token.  Will update as required. */
    lex_vars_ptr->token.str_ptr[0] = '\0';
    lex_vars_ptr->token.str_len = 0;
    lex_vars_ptr->token.int_val = 0;
    lex_vars_ptr->token.f_val = 0.0;
    lex_vars_ptr->token.bb_len = 0;

    if ( H5CL__lex_get_non_blank(lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_get_non_blank() failed.");

    next_char = *(lex_vars_ptr->next_char_ptr);

    if ( '(' == next_char ) { /* left parent or list */

        if ( ! value_expected ) { /* left paren */

            lex_vars_ptr->token.code = H5CL_L_PAREN_TOK;
            lex_vars_ptr->token.str_ptr[0] = '(';
            lex_vars_ptr->token.str_ptr[1] = '\0';
            lex_vars_ptr->token.str_len = 1;

            lex_vars_ptr->next_char_ptr++;

        } else {  /* list */

            int paren_depth = 0;

            lex_vars_ptr->token.code = H5CL_LIST_TOK;

            do {

                if ( '(' == next_char ) {

                    paren_depth++;

                } else if ( ')' == next_char ) {

                    paren_depth--;
                }

                (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;
                lex_vars_ptr->next_char_ptr++;
                next_char = *(lex_vars_ptr->next_char_ptr);

                assert(lex_vars_ptr->token.str_len < lex_vars_ptr->token.max_str_len);

            } while ( ( '\0' != next_char ) && ( paren_depth > 0 ) );

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len] = '\0';
        }

    } else if ( ')' == next_char ) { /* left paren */

        lex_vars_ptr->token.code = H5CL_R_PAREN_TOK;
        lex_vars_ptr->token.str_ptr[0] = ')';
        lex_vars_ptr->token.str_ptr[1] = '\0';
        lex_vars_ptr->token.str_len = 1;

        lex_vars_ptr->next_char_ptr++;

    } else if ( '"' == next_char ) { /* quote string */

        /* Load the quote string token into lex_vars_ptr->token verbatum but 
         * without the leading and trailing double quotes.  Note that 
         * embedded double quotes are allowed, but they must be escaped
         * with a leading backslash -- i.e. \".
         *
         * Note that no embedded escape sequences are resolved including 
         * escaped double quotes.  While I doubt that this is a significant 
         * issue for the target application, it is a departure from common
         * practice.  It is made in defference to THGs decision to avoid 
         * choosing a standard VFD configuration language.  By not cooking 
         * the contents of the quote string, we should make it easier to 
         * embed a string in an arbitrary configuration language in a 
         * string in this configuratino language.  Recall however, that 
         * any embedded double quotes must be escaped, which will cause
         * issues if a string in this language is embedded in the 
         * alternate language.
         */
        bool escape = false;

        lex_vars_ptr->token.code = H5CL_QSTRING_TOK;
        lex_vars_ptr->token.str_len = 0;

        lex_vars_ptr->next_char_ptr++;
        next_char = *(lex_vars_ptr->next_char_ptr);

        while ( ( '"' != next_char ) && ( ! escape ) ) {

            if ( '\\' == next_char ) {

                escape = true;

            } else {

                escape = false;
            }

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;

            lex_vars_ptr->next_char_ptr++;
            next_char = *(lex_vars_ptr->next_char_ptr);
        }

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len] = '\0';

        assert( '"' == next_char );

        lex_vars_ptr->next_char_ptr++;

    } else if ( ( '-' == next_char ) && ( '-' == *(lex_vars_ptr->next_char_ptr + 1) ) ) { /* binary blop */

        char byte_str[3] = "00";
        uint8_t next_byte = 0;
        bool have_high_nibble = false;
        bool have_low_nibble = false;

        lex_vars_ptr->token.code = H5CL_BIN_BLOB_TOK;

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;

        lex_vars_ptr->next_char_ptr++;
        next_char = *(lex_vars_ptr->next_char_ptr);

        assert('-' == next_char);

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;

        lex_vars_ptr->next_char_ptr++;
        next_char = *(lex_vars_ptr->next_char_ptr);

        while ( isxdigit(next_char) ) {

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;

            if ( ! have_high_nibble ) {

                have_high_nibble = true;
                assert( ! have_low_nibble );

                byte_str[0] = next_char;

            } else {

                assert( ! have_low_nibble );

                have_low_nibble = true;

                byte_str[1] = next_char;
            }

            if ( ( have_high_nibble ) && ( have_low_nibble ) ) {

                have_high_nibble = false;
                have_low_nibble = false;

                next_byte = (uint8_t)strtoll(byte_str, NULL, 16);

                byte_str[0] = '0';
                byte_str[1] = '0';

                (lex_vars_ptr->token.bb_ptr)[lex_vars_ptr->token.bb_len++] = next_byte;
            }

            lex_vars_ptr->next_char_ptr++;
            next_char = *(lex_vars_ptr->next_char_ptr);
        }

        if ( ( have_high_nibble ) && ( ! have_low_nibble ) ) {

            /* binary blob contains a odd number of hex characters -- finsih up */

            next_byte = (uint8_t)strtoll(byte_str, NULL, 16);

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = (char)next_byte;
        }

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len] = '\0';

    } else if ( isalpha(next_char) ) {  /* name */

        lex_vars_ptr->token.code = H5CL_SYMBOL_TOK;
        lex_vars_ptr->token.str_len = 0;

        do {

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;
            lex_vars_ptr->next_char_ptr++;
            next_char = *(lex_vars_ptr->next_char_ptr);

            assert(lex_vars_ptr->token.str_len < lex_vars_ptr->token.max_str_len);

        } while ( ( isalnum(next_char) ) || ( '_' == next_char ) );

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len] = '\0';

    } else if ( ( '+' == next_char ) || ( '-' == next_char ) || 
                ( '.' == next_char ) || ( isdigit(next_char) ) ) {  /* integer or float */

        /* read integer or float token */

        bool is_float = false;

        lex_vars_ptr->token.str_len = 0;

        if ( '.' == next_char ) {

             is_float = true;
        }

        do {

            if ( '.' == next_char ) {

                 is_float = true;
            }

            (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len++] = next_char;
            lex_vars_ptr->next_char_ptr++;
            next_char = *(lex_vars_ptr->next_char_ptr);

            assert(lex_vars_ptr->token.str_len < lex_vars_ptr->token.max_str_len);

        } while ( ( isdigit(next_char) ) || ( ( '.' == next_char ) && ( ! is_float ) ) );

        (lex_vars_ptr->token.str_ptr)[lex_vars_ptr->token.str_len] = '\0';

        if ( is_float ) {

            lex_vars_ptr->token.code = H5CL_FLOAT_TOK;
            lex_vars_ptr->token.f_val = strtod(lex_vars_ptr->token.str_ptr, NULL);

        } else {

            lex_vars_ptr->token.code = H5CL_INT_TOK;
            lex_vars_ptr->token.int_val = strtoll(lex_vars_ptr->token.str_ptr, NULL, 10);
        }

        assert(0 == errno);

    } else if ( '\0' == next_char ) { /* end of input */

        /* end of input string */

        lex_vars_ptr->token.code = H5CL_EOS_TOK;
        lex_vars_ptr->token.str_ptr[0] = '\0';
        lex_vars_ptr->token.str_len = 0;

    } else {

        /* should be un-reachable */
        assert(false);

    }

    *token_ptr_ptr = &(lex_vars_ptr->token);

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__lex_read_token() */


/*******************************************************************************
 *
 * Function:    H5CL__init_nv_pair()
 *
 * Purpose:     Initialize the supplied instance of struct H5CL_nv_pair_t. 
 *              The struct_tag is presumed to be set, but all other fields 
 *              are set to the expected initial state.
 * 
 *                                              JRM -- 12/18/25
 *
 * Parameters:
 *
 *    nv_pair_ptr: Pointer to the instance of H5CL_lex_vars_t to be 
 *              initialized.  Note that the struct_tag is persumed to 
 *              be set to H5CL_NV_PAIR_STRUCT_TAG.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

herr_t
H5CL__init_nv_pair(H5CL_nv_pair_t * nv_pair_ptr)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(nv_pair_ptr);

    if ( H5CL_NV_PAIR_STRUCT_TAG != nv_pair_ptr->struct_tag ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid nv_pair_ptr->struct_tag.");

    nv_pair_ptr->name_ptr     = NULL;
    nv_pair_ptr->val_type     = H5CL_VAL_NONE;
    nv_pair_ptr->int_val      = 0;
    nv_pair_ptr->f_val        = 0.0;
    nv_pair_ptr->vlen_val_ptr = NULL;
    nv_pair_ptr->len          = 0;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__init_nv_pair() */


/*******************************************************************************
 *
 * Function:    H5CL__take_down_nv_pair()
 *
 * Purpose:     Take down the supplied instance of struct H5CL_nv_pair_t. 
 *
 *              In particular, set the struct tag to an invalid value, and 
 *              discard any dynamically allocated memory.
 * 
 *                                              JRM -- 12/18/25
 *
 * Parameters:
 *
 *    nv_pair_ptr: Pointer to the instance of H5CL_lex_vars_t to be 
 *              taken down.  
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 * Changes:
 *
 *    None.
 *
 *******************************************************************************/

herr_t
H5CL__take_down_nv_pair(H5CL_nv_pair_t * nv_pair_ptr)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert(nv_pair_ptr);

    if ( H5CL_NV_PAIR_STRUCT_TAG != nv_pair_ptr->struct_tag ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid nv_pair_ptr->struct_tag.");

    nv_pair_ptr->struct_tag = H5CL_INVALID_NV_PAIR_STRUCT_TAG;

    if ( nv_pair_ptr->name_ptr ) {

        H5MM_xfree(nv_pair_ptr->name_ptr);
    }

    nv_pair_ptr->name_ptr     = NULL;
    nv_pair_ptr->val_type     = H5CL_VAL_NONE;
    nv_pair_ptr->int_val      = 0;
    nv_pair_ptr->f_val        = 0.0;

    if ( nv_pair_ptr->vlen_val_ptr ) {

        H5MM_xfree(nv_pair_ptr->vlen_val_ptr);
    }

    nv_pair_ptr->vlen_val_ptr = NULL;
    nv_pair_ptr->len          = 0;

done:

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__take_down_nv_pair() */


/*******************************************************************************
 *
 * Function:    H5CL__parse_name_value_pair() 
 *
 * Purpose:     <name_value_pair> ::= ‘(‘ <identifier> <value> ’)’
 *
 *              Attempt to parse a name value pair from the input string, 
 *              and if successful, load the name and value into the supplied 
 *              instance of H5CL_nv_pair_t.
 *
 *                                                      JRM - 12/17/25
 *
 * Parameters:
 *
 *    nv_pair_ptr: Pointer to the instance of cl_nv_pair_t into which the 
 *              name value pair is to be loaded.  On failure, 
 *              nv_pair->val_type is set to CL_VAL_NONE.
 *
 *    lex_vars_ptr: Pointer to the instance of H5CL_lex_vars_t that contains
 *              the input string, lexer vars, and the instance of H5CL_tok_t
 *              to be loaded with the next token and returned to the caller.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 *  Changes:
 *
 *        - None.
 *
 *******************************************************************************/

herr_t
H5CL__parse_name_value_pair(H5CL_nv_pair_t *nv_pair_ptr, H5CL_lex_vars_t * lex_vars_ptr)
{
    char * name_ptr = NULL;
    char * qstr_ptr = NULL;
    uint8_t * bb_ptr = NULL;
    char * list_ptr = NULL;
    int val_type = H5CL_VAL_NONE;
    size_t len = 0; 
    int64_t int_val = 0;
    double f_val = 0.0;
    H5CL_token_t * token_ptr;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert( nv_pair_ptr );
    assert( H5CL_NV_PAIR_STRUCT_TAG == nv_pair_ptr->struct_tag );
    assert( NULL == nv_pair_ptr->name_ptr );
    assert( NULL == nv_pair_ptr->vlen_val_ptr );
    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);
    assert(H5CL_TOKEN_STRUCT_TAG == lex_vars_ptr->token.struct_tag);


    /* parse the left parentheses */
    if ( H5CL__lex_read_token(false, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- '(' expected.");

    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( H5CL_L_PAREN_TOK != token_ptr->code ) {

        assert( H5CL_L_PAREN_TOK == token_ptr->code );
    }


    /* parse the name in the name value pair, duplicate the string 
     * containing the name and store its address in name_ptr.
     */
    if ( H5CL__lex_read_token(false, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- <name> expected.");

    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( H5CL_SYMBOL_TOK != token_ptr->code ) {

        assert(false);
    }

    assert( NULL != token_ptr->str_ptr );
    assert( 0 < token_ptr->str_len );
    assert( token_ptr->str_len < token_ptr->max_str_len );

    if ( NULL == (name_ptr = (char *)H5MM_malloc(token_ptr->str_len + 1) ) )
        HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for name");

    strncpy(name_ptr, token_ptr->str_ptr, token_ptr->str_len);
    name_ptr[token_ptr->str_len] = '\0';

    assert( strlen(name_ptr) == token_ptr->str_len);


    /* parse the value associated with the name, and store it as appropriate 
     * in local variables.
     */
    if ( H5CL__lex_read_token(true, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- <value> expected.");

    assert( H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag );

    switch ( token_ptr->code ) {

        case H5CL_INT_TOK:
            val_type = H5CL_VAL_INT;
            int_val = token_ptr->int_val;
            break;

        case H5CL_FLOAT_TOK:
            val_type = H5CL_VAL_FLOAT;
            f_val = token_ptr->f_val;
            break;

        case H5CL_QSTRING_TOK:
            val_type = H5CL_VAL_QSTR;

            assert( 0 < token_ptr->str_len );
            assert( token_ptr->str_len < token_ptr->max_str_len );

            if ( NULL == (qstr_ptr = (char *)H5MM_malloc(token_ptr->str_len + 1) ) )
                HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for qstring");

            strncpy(qstr_ptr, token_ptr->str_ptr, token_ptr->str_len);
            qstr_ptr[token_ptr->str_len] = '\0';

            assert( strlen(qstr_ptr) == token_ptr->str_len);

            len = token_ptr->str_len;
            break;

        case H5CL_BIN_BLOB_TOK:
            val_type = H5CL_VAL_BB;

            assert( token_ptr->bb_ptr );
            assert( 0 < token_ptr->bb_len );
            assert( token_ptr->bb_len  < token_ptr->max_str_len );

            if ( NULL == (bb_ptr = (uint8_t *)H5MM_malloc(token_ptr->bb_len) ) )
                HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for binary blob");

            memcpy(bb_ptr, token_ptr->bb_ptr, (size_t)(token_ptr->bb_len));

            len = token_ptr->bb_len;
            break;

        case H5CL_LIST_TOK:
            val_type = H5CL_VAL_LIST;

            assert( 0 < token_ptr->str_len );
            assert( token_ptr->str_len < token_ptr->max_str_len );

            if ( NULL == (list_ptr = (char *)H5MM_malloc(token_ptr->str_len + 1) ) )
                HGOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "memory allocation failed for qstring");

            strncpy(list_ptr, token_ptr->str_ptr, token_ptr->str_len);
            list_ptr[token_ptr->str_len] = '\0';

            assert( strlen(list_ptr) == token_ptr->str_len);
            assert( 0 == strcmp(token_ptr->str_ptr, list_ptr) );

            len = token_ptr->str_len;
            break;

        default: 
            /* replace with unexpected token error message */ 
            assert(false);
            break;
    }


    /* parse the right parentheses */
    if ( H5CL__lex_read_token(false, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- ')' expected.");

    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( H5CL_R_PAREN_TOK != token_ptr->code ) {

        assert(false);
    }


    /* if all goes well, load the supplied instance of cl_nv_pair_t */

    nv_pair_ptr->name_ptr = name_ptr;
    nv_pair_ptr->val_type = val_type;
    nv_pair_ptr->int_val  = int_val;
    nv_pair_ptr->f_val    = f_val;
    
    switch(val_type) {
        case H5CL_VAL_QSTR:
            nv_pair_ptr->vlen_val_ptr = (void *)qstr_ptr;

            /* The quote string is now the property of *nv_pair_ptr -- set qstr_ptr to NULL */
            qstr_ptr = NULL;
            break;

        case H5CL_VAL_BB:
            nv_pair_ptr->vlen_val_ptr = (void *)bb_ptr;

            /* The binary blob buffer is now the property of *nv_pair_ptr -- set bb_ptr to NULL */
            bb_ptr = NULL;
            break;

        case H5CL_VAL_LIST:
            nv_pair_ptr->vlen_val_ptr = (void *)list_ptr;

            /* The list string is now the property of *nv_pair_ptr -- set list_ptr to NULL */
            list_ptr = NULL;
            break;

        default:
            nv_pair_ptr->vlen_val_ptr = NULL;
            break;
    }

    nv_pair_ptr->len = len;

done:

    /* add code to discard any buffers allocated on failure */
    if ( SUCCEED != ret_value ) {

        if ( qstr_ptr ) {

            H5MM_xfree(qstr_ptr);
        }

        if ( bb_ptr ) {

            H5MM_xfree(bb_ptr);
        }

        if ( list_ptr ) {

            H5MM_xfree(list_ptr);
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__parse_name_value_pair() */


/*******************************************************************************
 *
 * Function:    H5CL__parse_name_value_pair_list() 
 *
 * Purpose:     <name_value_pair_list> ::= ‘(‘ (<name_value_pair>)* ‘)’
 *
 *              Attempt to parse a name value pair list from the input string. 
 *              The length of name value pair list may not exceed max_nv_pairs.  
 *              If successful, load the name value pairs into the array of 
 *              instances of H5CL_nv_pair_t with base address nv_pairs.
 *
 *                                                      JRM - 12/17/25
 *
 * Parameters:
 *
 *    nv_pairs: Base address of an array of H5CL_nv_pair_t of length
 *              max_nv_pairs.  If successful, then names and values
 *              in the target name value pair list are loaded into 
 *              this array.
 *
 *    max_nv_pairs: Length of the array of H5CL_nv_pair_t whose
 *              base address is passed in nv_pairs.  Note that if the 
 *              number of name value pairs in the name value pair list 
 *              exceeds this value, the function will fail.
 *
 *    lex_vars_ptr: Pointer to the instance of H5CL_lex_vars_t that 
 *              contains the input string, the lexer vars, and 
 *              the instance of H5CL_token_t to be loaded with the 
 *              next token and returned to the caller.
 *
 * Return:      Success:        non-negative
 *              Failure:        negative
 *
 *  Changes:
 *
 *        - None.
 *
 *******************************************************************************/

herr_t
H5CL__parse_name_value_pair_list(H5CL_nv_pair_t * nv_pairs, int max_nv_pairs, 
                                   H5CL_lex_vars_t * lex_vars_ptr)
{
    char peeked_next_char;
    int i;
    H5CL_token_t * token_ptr = NULL;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    assert( nv_pairs );
    assert( max_nv_pairs > 0 );

    for ( i = 0; i < max_nv_pairs; i++ ) {

        assert( H5CL_NV_PAIR_STRUCT_TAG == nv_pairs[i].struct_tag );
        assert( NULL                      == nv_pairs[i].name_ptr );
        assert( H5CL_VAL_NONE           == nv_pairs[i].val_type );
        assert( 0                         == nv_pairs[i].int_val );
        assert( 0.0                       <= nv_pairs[i].f_val ); /* circumlocution to keep */
        assert( 0.0                       >= nv_pairs[i].f_val ); /* the compiler happy     */
        assert( NULL                      == nv_pairs[i].vlen_val_ptr );
        assert( 0                         == nv_pairs[i].len );
    }

    assert(lex_vars_ptr);
    assert(H5CL_LEX_VARS_STRUCT_TAG == lex_vars_ptr->struct_tag);


    /* parse the left parentheses */
    if ( H5CL__lex_read_token(false, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- '(' expected.");

    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( H5CL_L_PAREN_TOK != token_ptr->code ) {

        assert( H5CL_L_PAREN_TOK == token_ptr->code );
    }

    
    /* parse the list of name value pairs */
    i = 0;

    if ( H5CL__lex_peek_next_char(&peeked_next_char, lex_vars_ptr) < 0 ) 
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_peek_next_char() failed.");

    while ( ( '(' == peeked_next_char )  && ( i < max_nv_pairs ) ) {

        /* parse a name value pair and insert the name and 
         * value into nv_pairs[i].
         */
        if ( H5CL__parse_name_value_pair(&(nv_pairs[i]), lex_vars_ptr) < 0 ) 
            HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__parse_name_value_pair() failed.");

        i++;

        if ( H5CL__lex_peek_next_char(&peeked_next_char, lex_vars_ptr) < 0 ) 
            HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_peek_next_char() failed.");
    }

    if  ( ( '(' == peeked_next_char )  && ( i >= max_nv_pairs ) )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "max number of name value pairs exceeded.");

    /* parse the right parentheses */
    if ( H5CL__lex_read_token(false, &token_ptr, lex_vars_ptr) < 0 )
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "H5CL__lex_read_token() failed -- ')' expected.");

    assert(H5CL_TOKEN_STRUCT_TAG == token_ptr->struct_tag);

    if ( H5CL_R_PAREN_TOK != token_ptr->code ) {

        assert(false);
    }

done:

    if (ret_value < 0 ) {
 
        /* reset the supplied vector of H5CL_nv_pair_t to its original state before returning */
        for ( i = 0; i < max_nv_pairs; i++ ) {

            assert( H5CL_NV_PAIR_STRUCT_TAG == nv_pairs[i].struct_tag );

            if ( nv_pairs[i].name_ptr ) {

                H5MM_xfree(nv_pairs[i].name_ptr);
                nv_pairs[i].name_ptr = NULL;
            }
            nv_pairs[i].val_type = H5CL_VAL_NONE;
            nv_pairs[i].int_val  = 0;
            nv_pairs[i].f_val    = 0.0;
            if ( nv_pairs[i].vlen_val_ptr ) {

                H5MM_xfree(nv_pairs[i].vlen_val_ptr);
                nv_pairs[i].vlen_val_ptr = NULL;
            }
            nv_pairs[i].len      = 0;
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)

} /* H5CL__parse_name_value_pair_list() */

