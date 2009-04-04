/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_h_
#define hubbub_h_

#include <hubbub/errors.h>
#include <hubbub/functypes.h>
#include <hubbub/types.h>

/* Initialise the Hubbub library for use */
hubbub_error hubbub_initialise(const char *aliases_file,
		hubbub_allocator_fn alloc, void *pw);

/* Clean up after Hubbub */
hubbub_error hubbub_finalise(hubbub_allocator_fn alloc, void *pw);

#endif

