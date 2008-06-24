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
 * Handle token in "in head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_in_head(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	bool reprocess = false;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		reprocess = process_characters_expect_whitespace(treebuilder,
				token, true);
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
			process_tag_in_body(treebuilder, token);
		} else if (type == BASE || type == COMMAND ||
				type == EVENT_SOURCE || type == LINK) {
			process_base_link_meta_in_head(treebuilder,
					token, type);

			/** \todo ack sc flag */
		} else if (type == META) {
			process_base_link_meta_in_head(treebuilder,
					token, type);

			/** \todo ack sc flag */

			/** \todo detect charset */
		} else if (type == TITLE) {
			parse_generic_rcdata(treebuilder, token, true);
		} else if (type == NOFRAMES || type == STYLE) {
			parse_generic_rcdata(treebuilder, token, false);
		} else if (type == NOSCRIPT) {
			/** \todo determine if scripting is enabled */
			if (false /*scripting_is_enabled*/) {
				parse_generic_rcdata(treebuilder, token, false);
			} else {
				insert_element(treebuilder, &token->data.tag);
				treebuilder->context.mode = IN_HEAD_NOSCRIPT;
			}
		} else if (type == SCRIPT) {
			process_script_in_head(treebuilder, token);
		} else if (type == HEAD) {
			/** \todo parse error */
		} else {
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HEAD) {
			handled = true;
		} else if (type == BR) {
			reprocess = true;
		} /** \todo parse error */
	}
		break;
	case HUBBUB_TOKEN_EOF:
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

		treebuilder->context.mode = AFTER_HEAD;
	}

	return reprocess;
}



/**
 * Process a tag as if in the "in head" state.
 *
 * \param treebuilder	The treebuilder instance
 * \param token		The token to process
 * \return True to reprocess the token, false otherwise
 */
bool process_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	return handle_in_head(treebuilder, token);
}
