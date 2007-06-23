/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_functypes_h_
#define hubbub_functypes_h_

#include <stdlib.h>

#include <hubbub/types.h>

/* Type of allocation function for hubbub */
typedef void *(*hubbub_alloc)(void *ptr, size_t size, void *pw);

/**
 * Type of token handling function
 */
typedef void (*hubbub_token_handler)(const hubbub_token *token, void *pw);

/**
 * Type of document buffer handling function
 */
typedef void (*hubbub_buffer_handler)(const uint8_t *data,
		size_t len, void *pw);

/**
 * Type of parse error handling function
 */
typedef void (*hubbub_error_handler)(uint32_t line, uint32_t col,
		const char *message, void *pw);


#endif

