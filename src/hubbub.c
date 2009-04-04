/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <parserutils/parserutils.h>

#include <hubbub/hubbub.h>

#include "utils/parserutilserror.h"
#include "tokeniser/entities.h"

/**
 * Initialise the Hubbub library for use.
 *
 * This _must_ be called before using any hubbub functions
 *
 * \param aliases_file  Pointer to name of file containing encoding alias data
 * \param alloc         Pointer to (de)allocation function
 * \param pw            Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, applicable error otherwise.
 */
hubbub_error hubbub_initialise(const char *aliases_file,
		hubbub_allocator_fn alloc, void *pw)
{
	hubbub_error error;

	if (aliases_file == NULL || alloc == NULL)
		return HUBBUB_BADPARM;

	error = hubbub_error_from_parserutils_error(
			parserutils_initialise(aliases_file, alloc, pw));
	if (error != HUBBUB_OK)
		return error;

	error = hubbub_entities_create(alloc, pw);
	if (error != HUBBUB_OK) {
		parserutils_finalise(alloc, pw);
		return error;
	}

	return HUBBUB_OK;
}

/**
 * Clean up after Hubbub
 *
 * \param alloc  Pointer to (de)allocation function
 * \param pw     Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, applicable error otherwise.
 */
hubbub_error hubbub_finalise(hubbub_allocator_fn alloc, void *pw)
{
	if (alloc == NULL)
		return HUBBUB_BADPARM;

	hubbub_entities_destroy(alloc, pw);

	parserutils_finalise(alloc, pw);

	return HUBBUB_OK;
}

