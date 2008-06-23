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
 * Handle tokens in "generic rcdata" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	bool done = false;

	if (treebuilder->context.strip_leading_lr &&
			token->type != HUBBUB_TOKEN_CHARACTER) {
		/* Reset the LR stripping flag */
		treebuilder->context.strip_leading_lr = false;
	}

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		if (treebuilder->context.collect.string.len == 0) {
			treebuilder->context.collect.string.data.off =
					token->data.character.data.off;
		}
		treebuilder->context.collect.string.len += 
				token->data.character.len;

		if (treebuilder->context.strip_leading_lr) {
			const uint8_t *str = treebuilder->input_buffer + 
				treebuilder->context.collect.string.data.off;

			/** \todo UTF-16 */
			if (*str == '\n') {
				treebuilder->context.collect.string.data.off++;
				treebuilder->context.collect.string.len--;
			}

			treebuilder->context.strip_leading_lr = false;
		}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type != treebuilder->context.collect.type) {
			/** \todo parse error */
		}

		done = true;
	}
		break;
	case HUBBUB_TOKEN_EOF:
		/** \todo parse error */
		done = reprocess = true;
		break;
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_START_TAG:
		/* Should never happen */
		assert(0);
		break;
	}

	if (done) {
		int success;
		void *text, *appended;

		success = treebuilder->tree_handler->create_text(
				treebuilder->tree_handler->ctx,
				&treebuilder->context.collect.string,
				&text);
		if (success != 0) {
			/** \todo errors */
		}

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

		/* Clean up context */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.collect.node);
		treebuilder->context.collect.node = NULL;

		/* Return to previous insertion mode */
		treebuilder->context.mode =
				treebuilder->context.collect.mode;
	}

	return reprocess;
}

