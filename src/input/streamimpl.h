/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_input_streamimpl_h_
#define hubbub_input_streamimpl_h_

#include <stdbool.h>

#include <hubbub/types.h>

#include "input/filter.h"
#include "input/inputstream.h"

typedef struct hubbub_inputstream_bm_handler hubbub_inputstream_bm_handler;

/**
 * Input stream definition: implementations extend this
 */
struct hubbub_inputstream {
	uint8_t *buffer;		/**< Document buffer */
	size_t buffer_len;		/**< Amount of data in buffer */
	size_t buffer_alloc;		/**< Allocated size of buffer */

	uint32_t cursor;		/**< Byte offset of current position */

	bool had_eof;			/**< Whether EOF has been reached */

	uint16_t mibenum;		/**< MIB enum for charset, or 0 */
	hubbub_charset_source encsrc;	/**< Charset source */

	hubbub_filter *input;		/**< Charset conversion filter */

	hubbub_inputstream_bm_handler *handlers;	/**< List of buffer
							 * moved handlers */
	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *pw;			/**< Client private data */

	void (*destroy)(hubbub_inputstream *stream);
	hubbub_error (*append)(hubbub_inputstream *stream,
			const uint8_t *data, size_t len);
	hubbub_error (*insert)(hubbub_inputstream *stream,
			const uint8_t *data, size_t len);
	uint32_t (*peek)(hubbub_inputstream *stream);
	uint32_t (*cur_pos)(hubbub_inputstream *stream, size_t *len);
	void (*lowercase)(hubbub_inputstream *stream);
	void (*uppercase)(hubbub_inputstream *stream);
	void (*advance)(hubbub_inputstream *stream);
	hubbub_error (*push_back)(hubbub_inputstream *stream,
			uint32_t character);
	int (*cmp_range_ci)(hubbub_inputstream *stream, uint32_t r1,
			uint32_t r2, size_t len);
	int (*cmp_range_cs)(hubbub_inputstream *stream, uint32_t r1,
			uint32_t r2, size_t len);
	int (*cmp_range_ascii)(hubbub_inputstream *stream,
			uint32_t off, size_t len,
			const char *data, size_t dlen);
	hubbub_error (*replace_range)(hubbub_inputstream *stream,
			uint32_t start, size_t len, uint32_t ucs4);
};

/**
 * Input stream factory component definition
 */
typedef struct hubbub_streamhandler {
	bool (*uses_encoding)(const char *int_enc);
	hubbub_inputstream *(*create)(const char *enc, const char *int_enc,
			hubbub_alloc alloc, void *pw);
} hubbub_streamhandler;

/* Notification of stream buffer moving */
void hubbub_inputstream_buffer_moved(hubbub_inputstream *stream);

#endif
