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
 * Handle token in "before head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_before_head(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	bool reprocess = false;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		reprocess = process_characters_expect_whitespace(treebuilder,
				token, false);
		break;
	case HUBBUB_TOKEN_COMMENT:
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
			/* Process as if "in body" */
			handle_in_body(treebuilder, token);
		} else if (type == HEAD) {
			handled = true;
		} else {
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HEAD || type == BR) {
			reprocess = true;
		} else {
			/** \todo parse error */
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		reprocess = true;
		break;
	}

	if (handled || reprocess) {
		hubbub_tag tag;

		if (reprocess) {
			/* Manufacture head tag */
			tag.name.type = HUBBUB_STRING_PTR;
			tag.name.data.ptr = (const uint8_t *) "head";
			tag.name.len = SLEN("head");

			tag.n_attributes = 0;
			tag.attributes = NULL;
		} else {
			tag = token->data.tag;
		}

		insert_element(treebuilder, &tag);

		treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		treebuilder->context.head_element = 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node;

		treebuilder->context.mode = IN_HEAD;
	}

	return reprocess;
}

