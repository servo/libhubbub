/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_utils_dict_h_
#define hubbub_utils_dict_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/hubbub.h>

typedef struct hubbub_dict hubbub_dict;

/* Create a dictionary */
hubbub_error hubbub_dict_create(hubbub_allocator_fn alloc, void *pw, 
		hubbub_dict **dict);
/* Destroy a dictionary */
hubbub_error hubbub_dict_destroy(hubbub_dict *dict);

/* Insert a key-value pair into a dictionary */
hubbub_error hubbub_dict_insert(hubbub_dict *dict, const char *key,
		const void *value);

/* Step-wise search for a key in a dictionary */
hubbub_error hubbub_dict_search_step(hubbub_dict *dict, uint8_t c,
		const void **result, void **context);

#endif
