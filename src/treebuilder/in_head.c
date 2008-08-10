/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"

#include "charset/detect.h"

#include "utils/utils.h"
#include "utils/string.h"


/**
 * Process a <meta> tag as if "in head".
 *
 * \param treebuilder	The treebuilder instance
 * \param token        The token to process
 */
static hubbub_error process_meta_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	insert_element_no_push(treebuilder, &token->data.tag);

	/** \todo ack sc flag */

#if 0
	if (confidence == certain)
		return HUBBUB_OK;
#endif

	uint16_t charset_enc = 0;
	uint16_t content_type_enc = 0;

	for (size_t i = 0; i < token->data.tag.n_attributes; i++) {
		hubbub_attribute *attr = &token->data.tag.attributes[i];

		if (hubbub_string_match(attr->name.ptr, attr->name.len,
				(const uint8_t *) "charset",
				SLEN("charset")) == true) {
			/* Extract charset */
			charset_enc = parserutils_charset_mibenum_from_name(
					(const char *) attr->value.ptr,
					attr->value.len);
		} else if (hubbub_string_match(attr->name.ptr, attr->name.len,
				(const uint8_t *) "content",
				SLEN("content")) == true) {
			/* Extract charset from Content-Type */
			content_type_enc = hubbub_charset_parse_content(
					attr->value.ptr, attr->value.len);
		}
	}

	if (charset_enc != 0) {
		if (treebuilder->tree_handler->encoding_change) {
			treebuilder->tree_handler->encoding_change(
					treebuilder->tree_handler->ctx,
					charset_enc);
		}
		return HUBBUB_ENCODINGCHANGE;
	} else if (content_type_enc != 0) {
		if (treebuilder->tree_handler->encoding_change) {
			treebuilder->tree_handler->encoding_change(
					treebuilder->tree_handler->ctx,
					content_type_enc);
		}
		return HUBBUB_ENCODINGCHANGE;
	}

	return HUBBUB_OK;
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
	treebuilder->context.collect.string.ptr = NULL;
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
hubbub_error handle_in_head(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = process_characters_expect_whitespace(treebuilder,
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
			insert_element_no_push(treebuilder, &token->data.tag);

			/** \todo ack sc flag */
		} else if (type == META) {
			err = process_meta_in_head(treebuilder, token);
		} else if (type == TITLE) {
			parse_generic_rcdata(treebuilder, token, true);
		} else if (type == NOFRAMES || type == STYLE) {
			parse_generic_rcdata(treebuilder, token, false);
		} else if (type == NOSCRIPT) {
			if (treebuilder->context.enable_scripting) {
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
			err = HUBBUB_REPROCESS;
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
			err = HUBBUB_REPROCESS;
		} /** \todo parse error */
	}
		break;
	case HUBBUB_TOKEN_EOF:
		err = HUBBUB_REPROCESS;
		break;
	}

	if (handled || err == HUBBUB_REPROCESS) {
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

	return err;
}
