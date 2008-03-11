/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_treebuilder_treebuilder_h_
#define hubbub_treebuilder_treebuilder_h_

#include <stdbool.h>
#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>
#include <hubbub/tree.h>
#include <hubbub/types.h>

#include "tokeniser/tokeniser.h"

typedef struct hubbub_treebuilder hubbub_treebuilder;

/**
 * Hubbub treebuilder option types
 */
typedef enum hubbub_treebuilder_opttype {
	HUBBUB_TREEBUILDER_BUFFER_HANDLER,
	HUBBUB_TREEBUILDER_ERROR_HANDLER,
	HUBBUB_TREEBUILDER_TREE_HANDLER,
	HUBBUB_TREEBUILDER_DOCUMENT_NODE,
} hubbub_treebuilder_opttype;

/**
 * Hubbub treebuilder option parameters
 */
typedef union hubbub_treebuilder_optparams {
	struct {
		hubbub_buffer_handler handler;
		void *pw;
	} buffer_handler;

	struct {
		hubbub_error_handler handler;
		void *pw;
	} error_handler;

	hubbub_tree_handler *tree_handler;

	void *document_node;
} hubbub_treebuilder_optparams;

/* Create a hubbub treebuilder */
hubbub_treebuilder *hubbub_treebuilder_create(hubbub_tokeniser *tokeniser,
		hubbub_alloc alloc, void *pw);

/* Destroy a hubbub treebuilder */
void hubbub_treebuilder_destroy(hubbub_treebuilder *treebuilder);

/* Configure a hubbub treebuilder */
hubbub_error hubbub_treebuilder_setopt(hubbub_treebuilder *treebuilder,
		hubbub_treebuilder_opttype type,
		hubbub_treebuilder_optparams *params);

#endif

