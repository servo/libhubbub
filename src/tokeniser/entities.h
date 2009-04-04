/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_tokeniser_entities_h_
#define hubbub_tokeniser_entities_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>

/* Create the entities dictionary */
hubbub_error hubbub_entities_create(hubbub_allocator_fn alloc, void *pw);
/* Destroy the entities dictionary */
void hubbub_entities_destroy(hubbub_allocator_fn alloc, void *pw);

/* Step-wise search for an entity in the dictionary */
hubbub_error hubbub_entities_search_step(uint8_t c, uint32_t *result,
		void **context);

#endif
