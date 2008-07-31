/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-8 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_parser_h_
#define hubbub_parser_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>
#include <hubbub/tree.h>
#include <hubbub/types.h>

typedef struct hubbub_parser hubbub_parser;

/**
 * Hubbub parser option types
 */
typedef enum hubbub_parser_opttype {
	HUBBUB_PARSER_TOKEN_HANDLER,
	HUBBUB_PARSER_ERROR_HANDLER,
	HUBBUB_PARSER_CONTENT_MODEL,
	HUBBUB_PARSER_TREE_HANDLER,
	HUBBUB_PARSER_DOCUMENT_NODE,
} hubbub_parser_opttype;

/**
 * Hubbub parser option parameters
 */
typedef union hubbub_parser_optparams {
	struct {
		hubbub_token_handler handler;
		void *pw;
	} token_handler;

	struct {
		hubbub_error_handler handler;
		void *pw;
	} error_handler;

	struct {
		hubbub_content_model model;
	} content_model;

	hubbub_tree_handler *tree_handler;

	void *document_node;
} hubbub_parser_optparams;

/* Create a hubbub parser */
hubbub_parser *hubbub_parser_create(const char *enc, const char *int_enc,
		hubbub_alloc alloc, void *pw);
/* Destroy a hubbub parser */
void hubbub_parser_destroy(hubbub_parser *parser);

/* Configure a hubbub parser */
hubbub_error hubbub_parser_setopt(hubbub_parser *parser,
		hubbub_parser_opttype type,
		hubbub_parser_optparams *params);

/* Pass a chunk of data to a hubbub parser for parsing */
/* This data is encoded in the input charset */
hubbub_error hubbub_parser_parse_chunk(hubbub_parser *parser,
		uint8_t *data, size_t len);
/* Pass a chunk of extraneous data to a hubbub parser for parsing */
/* This data is UTF-8 encoded */
hubbub_error hubbub_parser_parse_extraneous_chunk(hubbub_parser *parser,
		uint8_t *data, size_t len);
/* Inform the parser that the last chunk of data has been parsed */
hubbub_error hubbub_parser_completed(hubbub_parser *parser);

/* Read the document charset */
const char *hubbub_parser_read_charset(hubbub_parser *parser,
		hubbub_charset_source *source);

/* Claim ownership of the document buffer */
hubbub_error hubbub_parser_claim_buffer(hubbub_parser *parser,
		uint8_t **buffer, size_t *len);

#endif

