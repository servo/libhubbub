/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_input_inputstream_h_
#define hubbub_input_inputstream_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>
#include <hubbub/types.h>

typedef struct hubbub_inputstream hubbub_inputstream;

/* EOF pseudo-character */
#define HUBBUB_INPUTSTREAM_EOF (0xFFFFFFFFU)
/* Out-of-data indicator */
#define HUBBUB_INPUTSTREAM_OOD (0xFFFFFFFEU)

/* Type of input stream buffer moved handler function */
typedef void (*hubbub_inputstream_buffermoved)(const uint8_t *buffer,
		size_t len, void *pw);

/* Create an input stream */
hubbub_inputstream *hubbub_inputstream_create(const char *enc,
		const char *int_enc, hubbub_alloc alloc, void *pw);
/* Destroy an input stream */
void hubbub_inputstream_destroy(hubbub_inputstream *stream);

/* Append data to an input stream */
hubbub_error hubbub_inputstream_append(hubbub_inputstream *stream,
		const uint8_t *data, size_t len);
/* Insert data into stream at current location */
hubbub_error hubbub_inputstream_insert(hubbub_inputstream *stream,
		const uint8_t *data, size_t len);

/* Look at the next character in the stream */
uint32_t hubbub_inputstream_peek(hubbub_inputstream *stream);

/* Retrieve the byte index and length of the current character in the stream */
uint32_t hubbub_inputstream_cur_pos(hubbub_inputstream *stream, size_t *len);

/* Convert the current character to lowercase */
void hubbub_inputstream_lowercase(hubbub_inputstream *stream);

/* Convert the current character to uppercase */
void hubbub_inputstream_uppercase(hubbub_inputstream *stream);

/* Advance the stream's current position */
void hubbub_inputstream_advance(hubbub_inputstream *stream);

/* Push a character back onto the stream */
hubbub_error hubbub_inputstream_push_back(hubbub_inputstream *stream,
		uint32_t character);

/* Rewind the input stream by a number of bytes */
hubbub_error hubbub_inputstream_rewind(hubbub_inputstream *stream, size_t n);

/* Claim ownership of an input stream's buffer */
hubbub_error hubbub_inputstream_claim_buffer(hubbub_inputstream *stream,
		uint8_t **buffer, size_t *len);

/* Register interest in buffer moved events */
hubbub_error hubbub_inputstream_register_movehandler(
		hubbub_inputstream *stream,
		hubbub_inputstream_buffermoved handler, void *pw);

/* Deregister interest in buffer moved events */
hubbub_error hubbub_inputstream_deregister_movehandler(
		hubbub_inputstream *stream,
		hubbub_inputstream_buffermoved handler, void *pw);

/* Case insensitively compare a pair of ranges in the input stream */
int hubbub_inputstream_compare_range_ci(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len);

/* Case sensitively compare a pair of ranges in the input stream */
int hubbub_inputstream_compare_range_cs(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len);

/* Case sensitively compare a range of input stream against an ASCII string */
int hubbub_inputstream_compare_range_ascii(hubbub_inputstream *stream,
		uint32_t off, size_t len, const char *data, size_t dlen);

/* Replace a range of bytes in the input stream with a single character */
hubbub_error hubbub_inputstream_replace_range(hubbub_inputstream *stream,
		uint32_t start, size_t len, uint32_t ucs4);

/* Read the document charset */
const char *hubbub_inputstream_read_charset(hubbub_inputstream *stream,
		hubbub_charset_source *source);

#endif

