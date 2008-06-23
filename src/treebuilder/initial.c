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
 * Handle token in initial insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_initial(hubbub_treebuilder *treebuilder, const hubbub_token *token)
{
	bool reprocess = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		if (process_characters_expect_whitespace(treebuilder, token,
				false)) {
			/** \todo parse error */

			treebuilder->tree_handler->set_quirks_mode(
					treebuilder->tree_handler->ctx,
					HUBBUB_QUIRKS_MODE_FULL);
			treebuilder->context.mode = BEFORE_HTML;
			reprocess = true;
		}
		break;
	case HUBBUB_TOKEN_COMMENT:
		process_comment_append(treebuilder, token,
				treebuilder->context.document);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
	{
		int success;
		void *doctype, *appended;

		/** \todo parse error */

		/** \todo need public and system ids from tokeniser */
		success = treebuilder->tree_handler->create_doctype(
				treebuilder->tree_handler->ctx,
				&token->data.doctype.name,
				&token->data.doctype.public_id,
				&token->data.doctype.system_id, &doctype);
		if (success != 0) {
			/** \todo errors */
		}

		/* Append to Document node */
		success = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.document,
				doctype, &appended);
		if (success != 0) {
			/** \todo errors */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					doctype);
		}

		/* \todo look up the doctype in a catalog */

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, appended);
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, doctype);

		treebuilder->context.mode = BEFORE_HTML;
	}
		break;
	case HUBBUB_TOKEN_START_TAG:
	case HUBBUB_TOKEN_END_TAG:
	case HUBBUB_TOKEN_EOF:
		/** \todo parse error */
		treebuilder->tree_handler->set_quirks_mode(
				treebuilder->tree_handler->ctx,
				HUBBUB_QUIRKS_MODE_FULL);
		reprocess = true;
		break;
	}

	if (reprocess) {
		treebuilder->context.mode = BEFORE_HTML;
	}

	return reprocess;
}

