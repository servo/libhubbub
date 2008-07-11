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
 * Handle tokens in "script collect characters" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_script_collect_characters(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	bool done = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		if (treebuilder->context.collect.string.len == 0) {
			treebuilder->context.collect.string.data.off =
					token->data.character.data.off;
		}
		treebuilder->context.collect.string.len += 
				token->data.character.len;
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type != treebuilder->context.collect.type) {
			/** \todo parse error */
			/** \todo Mark script as "already executed" */
		}

		done = true;
	}
		break;
	case HUBBUB_TOKEN_EOF:
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_START_TAG:
		/** \todo parse error */
		/** \todo Mark script as "already executed" */
		done = reprocess = true;
		break;
	}

	if (done) {
		int success;
		void *text, *appended;

		if (treebuilder->context.collect.string.len) {
			success = treebuilder->tree_handler->create_text(
					treebuilder->tree_handler->ctx,
					&treebuilder->context.collect.string,
					&text);
			if (success != 0) {
				/** \todo errors */
			}

			/** \todo fragment case -- skip this lot entirely */

			success = treebuilder->tree_handler->append_child(
					treebuilder->tree_handler->ctx,
					treebuilder->context.collect.node,
					text, &appended);
			if (success != 0) {
				/** \todo errors */
				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						text);
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, appended);
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, text);
		}

		/** \todo insertion point manipulation */

		/* Append script node to current node */
		success = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node,
				treebuilder->context.collect.node, &appended);
		if (success != 0) {
			/** \todo errors */
		}

		/** \todo restore insertion point */

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				appended);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.collect.node);
		treebuilder->context.collect.node = NULL;

		/** \todo process any pending script */

		/* Return to previous insertion mode */
		treebuilder->context.mode =
				treebuilder->context.collect.mode;
	}

	return reprocess;
}

