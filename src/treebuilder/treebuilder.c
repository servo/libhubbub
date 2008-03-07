/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include "treebuilder/treebuilder.h"
#include "utils/utils.h"

struct hubbub_treebuilder
{
	hubbub_tokeniser *tokeniser;	/**< Underlying tokeniser */

	const uint8_t *input_buffer;	/**< Start of tokeniser's buffer */
	size_t input_buffer_len;	/**< Length of input buffer */

	hubbub_tree_handler tree_handler;

	hubbub_buffer_handler buffer_handler;
	void *buffer_pw;

	hubbub_error_handler error_handler;
	void *error_pw;

	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *alloc_pw;			/**< Client private data */
};

static void hubbub_treebuilder_buffer_handler(const uint8_t *data,
		size_t len, void *pw);
static void hubbub_treebuilder_token_handler(const hubbub_token *token, 
		void *pw);

/**
 * Create a hubbub treebuilder 
 *
 * \param tokeniser  Underlying tokeniser instance
 * \param alloc      Memory (de)allocation function
 * \param pw         Pointer to client-specific private data
 * \return Pointer to treebuilder instance, or NULL on error.
 */
hubbub_treebuilder *hubbub_treebuilder_create(hubbub_tokeniser *tokeniser,
		hubbub_alloc alloc, void *pw)
{
	hubbub_treebuilder *tb;
	hubbub_tokeniser_optparams tokparams;

	if (tokeniser == NULL || alloc == NULL)
		return NULL;

	tb = alloc(NULL, sizeof(hubbub_treebuilder), pw);
	if (tb == NULL)
		return NULL;

	tb->tokeniser = tokeniser;

	tb->input_buffer = NULL;
	tb->input_buffer_len = 0;

	memset(&tb->tree_handler, 0, sizeof(hubbub_tree_handler));

	tb->buffer_handler = NULL;
	tb->buffer_pw = NULL;

	tb->error_handler = NULL;
	tb->error_pw = NULL;

	tb->alloc = alloc;
	tb->alloc_pw = pw;

	tokparams.token_handler.handler = hubbub_treebuilder_token_handler;
	tokparams.token_handler.pw = tb;

	if (hubbub_tokeniser_setopt(tokeniser, HUBBUB_TOKENISER_TOKEN_HANDLER,
			&tokparams) != HUBBUB_OK) {
		alloc(tb, 0, pw);
		return NULL;
	}

	tokparams.buffer_handler.handler = hubbub_treebuilder_buffer_handler;
	tokparams.buffer_handler.pw = tb;

	if (hubbub_tokeniser_setopt(tokeniser, HUBBUB_TOKENISER_BUFFER_HANDLER,
			&tokparams) != HUBBUB_OK) {
		alloc(tb, 0, pw);
		return NULL;
	}

	return tb;	
}

/**
 * Destroy a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to destroy
 */
void hubbub_treebuilder_destroy(hubbub_treebuilder *treebuilder)
{
	hubbub_tokeniser_optparams tokparams;

	if (treebuilder == NULL)
		return;

	tokparams.buffer_handler.handler = treebuilder->buffer_handler;
	tokparams.buffer_handler.pw = treebuilder->buffer_pw;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_BUFFER_HANDLER, &tokparams);

	tokparams.token_handler.handler = NULL;
	tokparams.token_handler.pw = NULL;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_TOKEN_HANDLER, &tokparams);

	treebuilder->alloc(treebuilder, 0, treebuilder->alloc_pw);
}

/**
 * Configure a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to configure
 * \param type         The option type to configure
 * \param params       Pointer to option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error hubbub_treebuilder_setopt(hubbub_treebuilder *treebuilder,
		hubbub_treebuilder_opttype type,
		hubbub_treebuilder_optparams *params)
{
	if (treebuilder == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_TREEBUILDER_BUFFER_HANDLER:
		treebuilder->buffer_handler = params->buffer_handler.handler;
		treebuilder->buffer_pw = params->buffer_handler.pw;
		treebuilder->buffer_handler(treebuilder->input_buffer,
				treebuilder->input_buffer_len,
				treebuilder->buffer_pw);
		break;
	case HUBBUB_TREEBUILDER_ERROR_HANDLER:
		treebuilder->error_handler = params->error_handler.handler;
		treebuilder->error_pw = params->error_handler.pw;
		break;
	case HUBBUB_TREEBUILDER_TREE_HANDLER:
		treebuilder->tree_handler = params->tree_handler;
		break;
	}

	return HUBBUB_OK;
}

/**
 * Handle tokeniser buffer moving
 *
 * \param data  New location of buffer
 * \param len   Length of buffer in bytes
 * \param pw    Pointer to treebuilder instance
 */
void hubbub_treebuilder_buffer_handler(const uint8_t *data,
		size_t len, void *pw)
{
	hubbub_treebuilder *treebuilder = (hubbub_treebuilder *) pw;

	treebuilder->input_buffer = data;
	treebuilder->input_buffer_len = len;

	/* Inform client buffer handler, too (if there is one) */
	if (treebuilder->buffer_handler != NULL) {
		treebuilder->buffer_handler(treebuilder->input_buffer,
				treebuilder->input_buffer_len,
				treebuilder->buffer_pw);
	}
}

/**
 * Handle tokeniser emitting a token
 *
 * \param token  The emitted token
 * \param pw     Pointer to treebuilder instance
 */
void hubbub_treebuilder_token_handler(const hubbub_token *token, 
		void *pw)
{
	hubbub_treebuilder *treebuilder = (hubbub_treebuilder *) pw;

	UNUSED(treebuilder);
	UNUSED(token);

	/** \todo implement this */
}

