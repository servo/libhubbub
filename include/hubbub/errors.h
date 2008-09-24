/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_errors_h_
#define hubbub_errors_h_

#include <stddef.h>

typedef enum hubbub_error {
	HUBBUB_OK               = 0,
	HUBBUB_OOD		= 1, /**< Out of data */
	HUBBUB_REPROCESS	= 2,
	HUBBUB_ENCODINGCHANGE	= 3,

	HUBBUB_NOMEM            = 5,
	HUBBUB_BADPARM          = 6,
	HUBBUB_INVALID          = 7,
	HUBBUB_FILENOTFOUND     = 8,
	HUBBUB_NEEDDATA         = 9,

	HUBBUB_UNKNOWN		= 10
} hubbub_error;

/* Convert a hubbub error value to a string */
const char *hubbub_error_to_string(hubbub_error error);
/* Convert a string to a hubbub error value */
hubbub_error hubbub_error_from_string(const char *str, size_t len);

#endif

