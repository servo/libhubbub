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
hubbub_error handle_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool done = false;

	if (treebuilder->context.strip_leading_lr &&
			token->type != HUBBUB_TOKEN_CHARACTER) {
		/* Reset the LR stripping flag */
		treebuilder->context.strip_leading_lr = false;
	}

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
	{
		hubbub_string chars = token->data.character;
		int success;
		void *text, *appended;

		if (treebuilder->context.strip_leading_lr) {
			if (chars.ptr[0] == '\n') {
				chars.ptr++;
				chars.len--;
			}

			treebuilder->context.strip_leading_lr = false;
		}

		if (chars.len == 0)
			break;


		success = treebuilder->tree_handler->create_text(
				treebuilder->tree_handler->ctx,
				&chars,
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
		done = true;
		err = HUBBUB_REPROCESS;
		break;
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_START_TAG:
		/* Should never happen */
		assert(0);
		break;
	}

	if (done) {
		/* Clean up context */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.collect.node);
		treebuilder->context.collect.node = NULL;

		/* Return to previous insertion mode */
		treebuilder->context.mode = treebuilder->context.collect.mode;
	}

	return err;
}

