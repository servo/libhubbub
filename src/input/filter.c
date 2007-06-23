/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "charset/aliases.h"
#include "charset/codec.h"
#include "utils/utils.h"

#include "input/filter.h"


/** Input filter */
struct hubbub_filter {
	hubbub_charsetcodec *read_codec;	/**< Read codec */
	hubbub_charsetcodec *write_codec;	/**< Write codec */

	uint32_t filter_output[2];	/**< Filter output buffer */
	uint32_t last_filter_char;	/**< Last filtered character */

	uint32_t pivot_buf[64];		/**< Conversion pivot buffer */

	bool leftover;			/**< Data remains from last call */
	uint8_t *pivot_left;		/**< Remaining pivot to write */
	size_t pivot_len;		/**< Length of pivot remaining */

	struct {
		uint16_t encoding;	/**< Input encoding */
	} settings;			/**< Filter settings */

	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *pw;			/**< Client private data */
};

static hubbub_error hubbub_filter_set_defaults(hubbub_filter *input);
static hubbub_error hubbub_filter_set_encoding(hubbub_filter *input,
		const char *enc);
static hubbub_error read_character_filter(uint32_t c,
		uint32_t **output, size_t *outputlen, void *pw);

/**
 * Create an input filter
 *
 * \param int_enc  Desired encoding of document
 * \param alloc    Function used to (de)allocate data
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to filter instance, or NULL on failure
 */
hubbub_filter *hubbub_filter_create(const char *int_enc,
		hubbub_alloc alloc, void *pw)
{
	hubbub_filter *filter;

	if (alloc == NULL)
		return NULL;

	filter = alloc(NULL, sizeof(*filter), pw);
	if (!filter)
		return NULL;

	filter->last_filter_char = 0;

	filter->leftover = false;
	filter->pivot_left = NULL;
	filter->pivot_len = 0;

	filter->alloc = alloc;
	filter->pw = pw;

	if (hubbub_filter_set_defaults(filter) != HUBBUB_OK) {
		filter->alloc(filter, 0, pw);
		return NULL;
	}

	filter->write_codec = hubbub_charsetcodec_create(int_enc, alloc, pw);
	if (filter->write_codec == NULL) {
		if (filter->read_codec != NULL)
			hubbub_charsetcodec_destroy(filter->read_codec);
		filter->alloc(filter, 0, pw);
		return NULL;
	}

	return filter;
}

/**
 * Destroy an input filter
 *
 * \param input  Pointer to filter instance
 */
void hubbub_filter_destroy(hubbub_filter *input)
{
	if (input == NULL)
		return;

	if (input->read_codec != NULL)
		hubbub_charsetcodec_destroy(input->read_codec);

	if (input->write_codec != NULL)
		hubbub_charsetcodec_destroy(input->write_codec);

	input->alloc(input, 0, input->pw);

	return;
}

/**
 * Configure an input filter
 *
 * \param input   Pointer to filter instance
 * \param type    Input option type to configure
 * \param params  Option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_filter_setopt(hubbub_filter *input,
		hubbub_filter_opttype type,
		hubbub_filter_optparams *params)
{
	hubbub_error error = HUBBUB_OK;

	if (input == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_FILTER_SET_ENCODING:
		error = hubbub_filter_set_encoding(input,
				params->encoding.name);
		break;
	}

	return error;
}

/**
 * Process a chunk of data
 *
 * \param input   Pointer to filter instance
 * \param data    Pointer to pointer to input buffer
 * \param len     Pointer to length of input buffer
 * \param output  Pointer to pointer to output buffer
 * \param outlen  Pointer to length of output buffer
 * \return HUBBUB_OK on success, appropriate error otherwise
 *
 * Call this with an input buffer length of 0 to flush any buffers.
 */
hubbub_error hubbub_filter_process_chunk(hubbub_filter *input,
		const uint8_t **data, size_t *len,
		uint8_t **output, size_t *outlen)
{
	hubbub_error read_error, write_error;

	if (input == NULL || data == NULL || *data == NULL || len == NULL ||
			output == NULL || *output == NULL || outlen == NULL)
		return HUBBUB_BADPARM;

	if (input->leftover) {
		/* Some data left to be written from last call */

		/* Attempt to flush the remaining data. */
		write_error = hubbub_charsetcodec_encode(input->write_codec,
				(const uint8_t **) &input->pivot_left,
				&input->pivot_len,
				output, outlen);

		if (write_error != HUBBUB_OK) {
			return write_error;
		}

		/* And clear leftover */
		input->pivot_left = NULL;
		input->pivot_len = 0;
		input->leftover = false;
	}

	while (*len > 0) {
		size_t pivot_len = sizeof(input->pivot_buf);
		uint8_t *pivot = (uint8_t *) input->pivot_buf;

		read_error = hubbub_charsetcodec_decode(input->read_codec,
				data, len,
				(uint8_t **) &pivot, &pivot_len);

		pivot = (uint8_t *) input->pivot_buf;
		pivot_len = sizeof(input->pivot_buf) - pivot_len;

		if (pivot_len > 0) {
			write_error = hubbub_charsetcodec_encode(
					input->write_codec,
					(const uint8_t **) &pivot,
					&pivot_len,
					output, outlen);

			if (write_error != HUBBUB_OK) {
				input->leftover = true;
				input->pivot_left = pivot;
				input->pivot_len = pivot_len;

				return write_error;
			}
		}

		if (read_error != HUBBUB_OK && read_error != HUBBUB_NOMEM)
			return read_error;
	}

	return HUBBUB_OK;
}

/**
 * Reset an input filter's state
 *
 * \param input  The input filter to reset
 * \param HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_filter_reset(hubbub_filter *input)
{
	hubbub_error error;

	if (input == NULL)
		return HUBBUB_BADPARM;

	/* Clear pivot buffer leftovers */
	input->pivot_left = NULL;
	input->pivot_len = 0;
	input->leftover = false;

	/* Reset read codec */
	error = hubbub_charsetcodec_reset(input->read_codec);
	if (error != HUBBUB_OK)
		return error;

	/* Reset write codec */
	error = hubbub_charsetcodec_reset(input->write_codec);
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Set an input filter's default settings
 *
 * \param input  Input filter to configure
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_filter_set_defaults(hubbub_filter *input)
{
	hubbub_error error;

	if (input == NULL)
		return HUBBUB_BADPARM;

	input->read_codec = NULL;
	input->write_codec = NULL;
	input->settings.encoding = 0;
	error = hubbub_filter_set_encoding(input, "ISO-8859-1");
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Set an input filter's encoding
 *
 * \param input  Input filter to configure
 * \param enc    Encoding name
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_filter_set_encoding(hubbub_filter *input,
		const char *enc)
{
	const char *old_enc;
	uint16_t mibenum;
	hubbub_error error;
	hubbub_charsetcodec_optparams params;

	if (input == NULL || enc == NULL)
		return HUBBUB_BADPARM;

	mibenum = hubbub_mibenum_from_name(enc, strlen(enc));
	if (mibenum == 0)
		return HUBBUB_INVALID;

	/* Exit early if we're already using this encoding */
	if (input->settings.encoding == mibenum)
		return HUBBUB_OK;

	old_enc = hubbub_mibenum_to_name(input->settings.encoding);
	if (old_enc == NULL)
		old_enc = "ISO-8859-1";

	if (input->read_codec != NULL)
		hubbub_charsetcodec_destroy(input->read_codec);

	input->read_codec = hubbub_charsetcodec_create(enc, input->alloc,
			input->pw);
	if (input->read_codec == NULL)
		return HUBBUB_NOMEM;

	/* Register filter function */
	params.filter_func.filter = read_character_filter;
	params.filter_func.pw = (void *) input;
	error = hubbub_charsetcodec_setopt(input->read_codec,
			HUBBUB_CHARSETCODEC_FILTER_FUNC,
			(hubbub_charsetcodec_optparams *) &params);
	if (error != HUBBUB_OK)
		return error;

	input->settings.encoding = mibenum;

	return HUBBUB_OK;
}

/**
 * Character filter function for read characters
 *
 * \param c          The read character (UCS4 - host byte order)
 * \param output     Pointer to pointer to output buffer (filled on exit)
 * \param outputlen  Pointer to output buffer length (filled on exit)
 * \param pw         Pointer to client-specific private data.
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error read_character_filter(uint32_t c, uint32_t **output,
		size_t *outputlen, void *pw)
{
	hubbub_filter *input = (hubbub_filter *) pw;
	size_t len;

	if (output == NULL || outputlen == NULL || pw == NULL)
		return HUBBUB_BADPARM;

	/* Line ending normalisation:
	 *   CRLF -> LF  (trap CR and let LF through unmodified)
	 *   CR   -> LF  (trap CR and convert to LF if not CRLF)
	 *   LF   -> LF  (leave LF alone)
	 */

#define NUL (0x00000000)
#define CR  (0x0000000D)
#define LF  (0x0000000A)
#define REP (0x0000FFFD)

	if (c == NUL) {
		/* Replace NUL (U+0000) characters in input with U+FFFD */
		input->filter_output[0] = REP;
		len = 1;
	} else if (c == CR) {
		/* Trap CR characters */
		len = 0;
	} else if (input->last_filter_char == CR && c != LF) {
		/* Last char was CR and this isn't LF => CR -> LF */
		input->filter_output[0] = LF;
		input->filter_output[1] = c;
		len = 2;
	} else {
		/* Let character through unchanged */
		input->filter_output[0] = c;
		len = 1;
	}

#undef NUL
#undef CR
#undef LF
#undef REP

	input->last_filter_char = c;

	*output = input->filter_output;
	*outputlen = len;

	return HUBBUB_OK;
}
