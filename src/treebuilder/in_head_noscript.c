/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Handle tokens in "in head noscript" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_in_head_noscript(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		/* This should be equivalent to "in head" processing */
		reprocess = process_characters_expect_whitespace(treebuilder,
				token, true);
		break;
	case HUBBUB_TOKEN_COMMENT:
		/* This should be equivalent to "in head" processing */
		process_comment_append(treebuilder, token,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
		/** \todo parse error */
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/* Process as "in body" */
			process_tag_in_body(treebuilder, token);
		} else if (type == NOSCRIPT) {
			handled = true;
		} else if (type == LINK) {
			/* This should be equivalent to "in head" processing */
			process_base_link_meta_in_head(treebuilder,
					token, type);

			/** \todo ack sc flag */
		} else if (type == META) {
			/* This should be equivalent to "in head" processing */
			process_base_link_meta_in_head(treebuilder,
					token, type);

			/** \todo ack sc flag */

			/** \todo detect charset */
		} else if (type == NOFRAMES) {
			/* This should be equivalent to "in head" processing */
			parse_generic_rcdata(treebuilder, token, true);
		} else if (type == STYLE) {
			/* This should be equivalent to "in head" processing */
			parse_generic_rcdata(treebuilder, token, false);
		} else if (type == HEAD || type == NOSCRIPT) {
			/** \todo parse error */
		} else {
			/** \todo parse error */
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == NOSCRIPT) {
			handled = true;
		} else if (type == BR) {
			/** \todo parse error */
			reprocess = true;
		} else {
			/** \todo parse error */
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		/** \todo parse error */
		reprocess = true;
		break;
	}

	if (handled || reprocess) {
		element_type otype;
		void *node;

		if (!element_stack_pop(treebuilder, &otype, &node)) {
			/** \todo errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		treebuilder->context.mode = IN_HEAD;
	}

	return reprocess;
}

