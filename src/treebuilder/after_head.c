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
 * Handle tokens in "after head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_after_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		append_text(treebuilder, &token->data.character);
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
		} else if (type == BODY) {
			handled = true;
		} else if (type == FRAMESET) {
			insert_element(treebuilder, &token->data.tag);
			treebuilder->context.mode = IN_FRAMESET;
		} else if (type == BASE || type == LINK || type == META ||
				type == NOFRAMES || type == SCRIPT ||
				type == STYLE || type == TITLE) {
			element_type otype;
			void *node;

			/** \todo parse error */

			if (!element_stack_push(treebuilder, 
					HEAD, 
					treebuilder->context.head_element)) {
				/** \todo errors */
			}


			/* This should be identical to handling "in head" */
			if (type == BASE || type == LINK || type == META) {
				/** \todo ack sc flag */

				process_base_link_meta_in_head(treebuilder, 
						token, type);
			} else if (type == SCRIPT) {
				process_script_in_head(treebuilder, token);
			} else if (type == STYLE || type == NOFRAMES) {
				parse_generic_rcdata(treebuilder, token, false);
			} else if (type == TITLE) {
				parse_generic_rcdata(treebuilder, token, true);
			}

			if (!element_stack_pop(treebuilder, &otype, &node)) {
				/** \todo errors */
			}

			/* No need to unref node as we never increased 
			 * its reference count when pushing it on the stack */
		} else if (type == HEAD) {
			/** \todo parse error */
		} else {
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
		/** \parse error */
		break;
	case HUBBUB_TOKEN_EOF:
		reprocess = true;
		break;
	}

	if (handled || reprocess) {
		hubbub_tag tag;

		if (reprocess) {
			/* Manufacture body */
			tag.name.type = HUBBUB_STRING_PTR;
			tag.name.data.ptr = (const uint8_t *) "body";
			tag.name.len = SLEN("body");

			tag.n_attributes = 0;
			tag.attributes = NULL;
		} else {
			tag = token->data.tag;
		}

		insert_element(treebuilder, &tag);

		treebuilder->context.mode = IN_BODY;
	}

	return reprocess;
}
