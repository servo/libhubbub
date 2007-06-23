/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_charset_detect_h_
#define hubbub_charset_detect_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>
#include <hubbub/types.h>

/* Extract a charset from a chunk of data */
hubbub_error hubbub_charset_extract(const uint8_t **data, size_t *len,
		uint16_t *mibenum, hubbub_charset_source *source);

#endif

