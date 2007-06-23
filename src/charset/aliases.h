/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_charset_aliases_h_
#define hubbub_charset_aliases_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>

typedef struct hubbub_aliases_canon {
	struct hubbub_aliases_canon *next;
	uint16_t mib_enum;
	uint16_t name_len;
	char name[1];
} hubbub_aliases_canon;

/* Load encoding aliases from file */
hubbub_error hubbub_aliases_create(const char *filename,
		hubbub_alloc alloc, void *pw);
/* Destroy encoding aliases */
void hubbub_aliases_destroy(hubbub_alloc alloc, void *pw);

/* Convert an encoding alias to a MIB enum value */
uint16_t hubbub_mibenum_from_name(const char *alias, size_t len);
/* Convert a MIB enum value into an encoding alias */
const char *hubbub_mibenum_to_name(uint16_t mibenum);

/* Canonicalise an alias name */
hubbub_aliases_canon *hubbub_alias_canonicalise(const char *alias,
		size_t len);

#ifndef NDEBUG
void hubbub_aliases_dump(void);
#endif

#endif
