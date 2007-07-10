/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <hubbub/parser.h>

#include "input/inputstream.h"
#include "tokeniser/tokeniser.h"

/**
 * Hubbub parser object
 */
struct hubbub_parser {
	hubbub_inputstream *stream;	/**< Input stream instance */
	hubbub_tokeniser *tok;		/**< Tokeniser instance */

	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *pw;			/**< Client data */
};

/**
 * Create a hubbub parser
 *
 * \param enc      Source document encoding, or NULL to autodetect
 * \param int_enc  Desired encoding of document
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to parser instance, or NULL on error
 */
hubbub_parser *hubbub_parser_create(const char *enc, const char *int_enc,
		hubbub_alloc alloc, void *pw)
{
	hubbub_parser *parser;

	if (alloc == NULL)
		return NULL;

	parser = alloc(NULL, sizeof(hubbub_parser), pw);
	if (parser == NULL)
		return NULL;

	parser->stream = hubbub_inputstream_create(enc, int_enc, alloc, pw);
	if (parser->stream == NULL) {
		alloc(parser, 0, pw);
		return NULL;
	}

	parser->tok = hubbub_tokeniser_create(parser->stream, alloc, pw);
	if (parser->tok == NULL) {
		hubbub_inputstream_destroy(parser->stream);
		alloc(parser, 0, pw);
		return NULL;
	}

	parser->alloc = alloc;
	parser->pw = pw;

	return parser;
}

/**
 * Destroy a hubbub parser
 *
 * \param parser  Parser instance to destroy
 */
void hubbub_parser_destroy(hubbub_parser *parser)
{
	if (parser == NULL)
		return;

	hubbub_tokeniser_destroy(parser->tok);

	hubbub_inputstream_destroy(parser->stream);

	parser->alloc(parser, 0, parser->pw);
}

/**
 * Configure a hubbub parser
 *
 * \param parser  Parser instance to configure
 * \param type    Option to set
 * \param params  Option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_setopt(hubbub_parser *parser,
		hubbub_parser_opttype type,
		hubbub_parser_optparams *params)
{
	hubbub_tokeniser_opttype toktype;

	if (parser == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_PARSER_TOKEN_HANDLER:
		toktype = HUBBUB_TOKENISER_TOKEN_HANDLER;
		break;
	case HUBBUB_PARSER_BUFFER_HANDLER:
		toktype = HUBBUB_TOKENISER_BUFFER_HANDLER;
		break;
	case HUBBUB_PARSER_ERROR_HANDLER:
		toktype = HUBBUB_TOKENISER_BUFFER_HANDLER;
		break;
	case HUBBUB_PARSER_CONTENT_MODEL:
		toktype = HUBBUB_TOKENISER_CONTENT_MODEL;
		break;
	default:
		return HUBBUB_INVALID;
	}

	return hubbub_tokeniser_setopt(parser->tok, toktype,
			(hubbub_tokeniser_optparams *) params);
}

/**
 * Pass a chunk of data to a hubbub parser for parsing
 *
 * \param parser  Parser instance to use
 * \param data    Data to parse (encoded in the input charset)
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_parse_chunk(hubbub_parser *parser,
		uint8_t *data, size_t len)
{
	hubbub_error error;

	if (parser == NULL || data == NULL)
		return HUBBUB_BADPARM;

	error = hubbub_inputstream_append(parser->stream, data, len);
	if (error != HUBBUB_OK)
		return error;

	error = hubbub_tokeniser_run(parser->tok);
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Pass a chunk of extraneous data to a hubbub parser for parsing
 *
 * \param parser  Parser instance to use
 * \param data    Data to parse (encoded in internal charset)
 * \param len     Length, in byte, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_parse_extraneous_chunk(hubbub_parser *parser,
		uint8_t *data, size_t len)
{
	hubbub_error error;

	/** \todo In some cases, we don't actually want script-inserted
	 * data to be parsed until later. We'll need some way of flagging
	 * this through the public API, and the inputstream API will need
	 * some way of marking the insertion point so that, when the
	 * tokeniser is run, only the inserted chunk is parsed. */

	if (parser == NULL || data == NULL)
		return HUBBUB_BADPARM;

	error = hubbub_inputstream_insert(parser->stream, data, len);
	if (error != HUBBUB_OK)
		return error;

	error = hubbub_tokeniser_run(parser->tok);
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Inform the parser that the last chunk of data has been parsed
 *
 * \param parser  Parser to inform
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_completed(hubbub_parser *parser)
{
	hubbub_error error;

	if (parser == NULL)
		return HUBBUB_BADPARM;

	error = hubbub_inputstream_append(parser->stream, NULL, 0);
	if (error != HUBBUB_OK)
		return error;

	error = hubbub_tokeniser_run(parser->tok);
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Read the document charset
 *
 * \param parser  Parser instance to query
 * \param source  Pointer to location to receive charset source
 * \return Pointer to charset name (constant; do not free), or NULL if unknown
 */
const char *hubbub_parser_read_charset(hubbub_parser *parser,
		hubbub_charset_source *source)
{
	if (parser == NULL || source == NULL)
		return NULL;

	return hubbub_inputstream_read_charset(parser->stream, source);
}

/**
 * Claim ownership of the document buffer
 *
 * \param parser  Parser whose buffer to claim
 * \param buffer  Pointer to location to receive buffer pointer
 * \param len     Pointer to location to receive byte length of buffer
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Once the buffer has been claimed by a client, the parser disclaims
 * all ownership rights (and invalidates any internal references it may have
 * to the buffer). Therefore, the only parser call which may be made
 * after calling this function is to destroy the parser.
  */
hubbub_error hubbub_parser_claim_buffer(hubbub_parser *parser,
		uint8_t **buffer, size_t *len)
{
	if (parser == NULL || buffer == NULL || len == NULL)
		return HUBBUB_BADPARM;

	return hubbub_inputstream_claim_buffer(parser->stream, buffer, len);
}
