/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"



static bool element_in_scope_in_non_html_ns(hubbub_treebuilder *treebuilder)
{
	uint32_t node;

	if (treebuilder->context.element_stack == NULL)
		return false;

	for (node = treebuilder->context.current_node; node > 0; node--) {
		element_type node_ns =
				treebuilder->context.element_stack[node].ns;

		if (node_ns != HTML)
			return true;
	}

	return false;
}



/**
 * Handle tokens in "in foreign content" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_in_foreign_content(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

	element_type type = element_type_from_name(treebuilder,
			&token->data.tag.name);

	element_type cur_node = current_node(treebuilder);
	hubbub_ns cur_node_ns = current_node_ns(treebuilder);


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
		if (cur_node_ns == HUBBUB_NS_HTML ||
				(cur_node_ns == HUBBUB_NS_MATHML &&
				(type != MGLYPH && type != MALIGNMARK) &&
				(cur_node == MI || cur_node == MO ||
				cur_node == MN || cur_node == MS ||
				cur_node == MTEXT))) {
			treebuilder->context.mode =
					treebuilder->context.second_mode;
			hubbub_treebuilder_token_handler(token, treebuilder);

			if (treebuilder->context.mode == IN_FOREIGN_CONTENT &&
					!element_in_scope_in_non_html_ns(treebuilder)) {
				treebuilder->context.mode =
						treebuilder->context.second_mode;
			}
		} else if (type == B || type ==  BIG || type == BLOCKQUOTE ||
				type == BODY || type == BR || type == CENTER ||
				type == CODE || type == DD || type == DIV ||
				type == DL || type == DT || type == EM ||
				type == EMBED || type == FONT || type == H1 ||
				type == H2 || type == H3 || type == H4 ||
				type == H5 || type == H6 || type == HEAD ||
				type == HR || type == I || type == IMG ||
				type == LI || type == LISTING ||
				type == MENU || type == META || type == NOBR ||
				type == OL || type == P || type == PRE ||
				type == RUBY || type == S || type == SMALL ||
				type == SPAN || type == STRONG ||
				type == STRIKE || type == SUB || type == SUP ||
				type == TABLE || type == TT || type == U ||
				type == UL || type == VAR) {
			/** \todo parse error */

			while (cur_node_ns != HUBBUB_NS_HTML) {
				void *node;
				element_stack_pop(treebuilder, &cur_node_ns,
						&cur_node, &node);
				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
				cur_node_ns = current_node_ns(treebuilder);
			}

			treebuilder->context.mode =
					treebuilder->context.second_mode;
		} else {
			hubbub_tag tag = token->data.tag;

			adjust_foreign_attributes(treebuilder, &tag);

			/* Set to the right namespace and insert */
			tag.ns = cur_node_ns;

			if (token->data.tag.self_closing) {
				insert_element_no_push(treebuilder, &tag);
				/** \todo ack sc flag */
			} else {
				insert_element(treebuilder, &tag);
			}
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
		/** \parse error */
		break;
	case HUBBUB_TOKEN_EOF:
		while (cur_node_ns != HUBBUB_NS_HTML) {
			void *node;
			element_stack_pop(treebuilder, &cur_node_ns,
					&cur_node, &node);
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);
			cur_node_ns = current_node_ns(treebuilder);
		}

		treebuilder->context.mode =
				treebuilder->context.second_mode;

		reprocess = true;
		break;
	}

	return reprocess;
}

