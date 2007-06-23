/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_charset_codecimpl_h_
#define hubbub_charset_codecimpl_h_

#include <stdbool.h>
#include <inttypes.h>

#include "codec.h"

/**
 * Core charset codec definition; implementations extend this
 */
struct hubbub_charsetcodec {
	uint16_t mibenum;			/**< MIB enum for charset */

	hubbub_charsetcodec_filter filter;	/**< filter function */
	void *filter_pw;			/**< filter private word */

	hubbub_charsetcodec_errormode errormode;	/**< error mode */

	hubbub_alloc alloc;			/**< allocation function */
	void *alloc_pw;				/**< private word */

	struct {
		void (*destroy)(hubbub_charsetcodec *codec);
		hubbub_error (*encode)(hubbub_charsetcodec *codec,
				const uint8_t **source, size_t *sourcelen,
				uint8_t **dest, size_t *destlen);
		hubbub_error (*decode)(hubbub_charsetcodec *codec,
				const uint8_t **source, size_t *sourcelen,
				uint8_t **dest, size_t *destlen);
		hubbub_error (*reset)(hubbub_charsetcodec *codec);
	} handler; /**< Vtable for handler code */
};

/**
 * Codec factory component definition
 */
typedef struct hubbub_charsethandler {
	bool (*handles_charset)(const char *charset);
	hubbub_charsetcodec *(*create)(const char *charset,
			hubbub_alloc alloc, void *pw);
} hubbub_charsethandler;

#endif
