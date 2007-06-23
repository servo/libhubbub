/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_input_filter_h_
#define hubbub_input_filter_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>

typedef struct hubbub_filter hubbub_filter;

/**
 * Input filter option types
 */
typedef enum hubbub_filter_opttype {
	HUBBUB_FILTER_SET_ENCODING       = 0,
} hubbub_filter_opttype;

/**
 * Input filter option parameters
 */
typedef union hubbub_filter_optparams {
	/** Parameters for encoding setting */
	struct {
		/** Encoding name */
		const char *name;
	} encoding;
} hubbub_filter_optparams;


/* Create an input filter */
hubbub_filter *hubbub_filter_create(const char *int_enc,
		hubbub_alloc alloc, void *pw);
/* Destroy an input filter */
void hubbub_filter_destroy(hubbub_filter *input);

/* Configure an input filter */
hubbub_error hubbub_filter_setopt(hubbub_filter *input,
		hubbub_filter_opttype type,
		hubbub_filter_optparams *params);

/* Process a chunk of data */
hubbub_error hubbub_filter_process_chunk(hubbub_filter *input,
		const uint8_t **data, size_t *len,
		uint8_t **output, size_t *outlen);

/* Reset an input filter's state */
hubbub_error hubbub_filter_reset(hubbub_filter *input);

#endif

