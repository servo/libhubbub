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
 * Process a <base>, <link>, or <meta> start tag as if in "in head"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The type of element (BASE, LINK, or META)
 */
static void process_base_link_meta_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	insert_element_no_push(treebuilder, &token->data.tag);

	if (type == META) {
		/** \todo charset extraction */
	}
}


/**
 * Process a <script> start tag as if in "in head"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
static void process_script_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	int success;
	void *script;
	hubbub_tokeniser_optparams params;

	success = treebuilder->tree_handler->create_element(
			treebuilder->tree_handler->ctx,
			&token->data.tag, &script);
	if (success != 0) {
		/** \todo errors */
	}

	/** \todo mark script as parser-inserted */

	/* It would be nice to be able to re-use the generic
	 * rcdata character collector here. Unfortunately, we
	 * can't as we need to do special processing after the
	 * script data has been collected, so we use an almost
	 * identical insertion mode which does the right magic
	 * at the end. */
	params.content_model.model = HUBBUB_CONTENT_MODEL_CDATA;
	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_CONTENT_MODEL, 
			&params);

	treebuilder->context.collect.mode = treebuilder->context.mode;
	treebuilder->context.collect.node = script;
	treebuilder->context.collect.type = SCRIPT;
	treebuilder->context.collect.string.data.off = 0;
	treebuilder->context.collect.string.len = 0;

	treebuilder->context.mode = SCRIPT_COLLECT_CHARACTERS;
}





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
			handle_in_body(treebuilder, token);
		} else if (type == BASE || type == COMMAND ||
				type == EVENTSOURCE || type == LINK) {
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
		hubbub_ns ns;
		element_type otype;
		void *node;

		if (!element_stack_pop(treebuilder, &ns, &otype, &node)) {
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
