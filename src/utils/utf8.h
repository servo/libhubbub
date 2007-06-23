/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file
 * UTF-8 manipulation functions (interface).
 */

#ifndef hubbub_utils_utf8_h_
#define hubbub_utils_utf8_h

#include <inttypes.h>

#include <hubbub/errors.h>

inline hubbub_error hubbub_utf8_to_ucs4(const uint8_t *s, size_t len,
		uint32_t *ucs4, size_t *clen);
inline hubbub_error hubbub_utf8_from_ucs4(uint32_t ucs4, uint8_t *s,
		size_t *len);

inline hubbub_error hubbub_utf8_length(const uint8_t *s, size_t max,
		size_t *len);
inline hubbub_error hubbub_utf8_char_byte_length(const uint8_t *s,
		size_t *len);

inline hubbub_error hubbub_utf8_prev(const uint8_t *s, uint32_t off,
		uint32_t *prevoff);
inline hubbub_error hubbub_utf8_next(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff);

inline hubbub_error hubbub_utf8_next_paranoid(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff);

#endif

