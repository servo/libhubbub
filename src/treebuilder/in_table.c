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
#include "utils/string.h"


/**
 * Clear the stack back to a table context: "the UA must, while the current
 * node is not a table element or an html element, pop elements from the stack
 * of open elements."
 */
static inline void clear_stack_table_context(hubbub_treebuilder *treebuilder)
{
	hubbub_ns ns;
	element_type type = current_node(treebuilder);
	void *node;

	while (type != TABLE && type != HTML) {
		element_stack_pop(treebuilder, &ns, &type, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		type = current_node(treebuilder);
	}

	return;
}


/**
 * Process an <input> tag in the "in table" insertion mode.
 */
static inline bool process_input_in_table(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	for (size_t i = 0; i < token->data.tag.n_attributes; i++) {
		hubbub_attribute *attr = &token->data.tag.attributes[i];

		if (!hubbub_string_match_ci(attr->value.ptr, attr->value.len,
				(uint8_t *) "hidden", SLEN("hidden"))) {
			continue;
		}

		/** \todo parse error */
		insert_element(treebuilder, &token->data.tag);

		if (treebuilder->context.form_element != NULL) {
			treebuilder->tree_handler->form_associate(
					treebuilder->tree_handler->ctx,
					treebuilder->context.form_element,
					treebuilder->context.element_stack[
					treebuilder->context.current_node].node);
		}

		return true;
	}

	return false;
}


/**
 * Handle token in "in table" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
hubbub_error handle_in_table(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = true;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		if (treebuilder->context.element_stack[
				current_table(treebuilder)
				].tainted) {
			handled = false;
		} else {
			handled = !process_characters_expect_whitespace(
					treebuilder, token, true);
		}
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
		bool tainted = treebuilder->context.element_stack[
					current_table(treebuilder)
					].tainted;

		if (type == CAPTION) {
			clear_stack_table_context(treebuilder);
			treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
			formatting_list_append(treebuilder, 
					token->data.tag.ns, type,
					treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
					treebuilder->context.current_node);

			insert_element(treebuilder, &token->data.tag);
			treebuilder->context.mode = IN_CAPTION;
		} else if (type == COLGROUP || type == COL) {
			hubbub_tag tag = token->data.tag;

			if (type == COL) {
				/* Insert colgroup and reprocess */
				tag.name.ptr = (const uint8_t *) "colgroup";
				tag.name.len = SLEN("colgroup");

				err = HUBBUB_REPROCESS;
			}

			clear_stack_table_context(treebuilder);
			insert_element(treebuilder, &tag);
			treebuilder->context.mode = IN_COLUMN_GROUP;
		} else if (type == TBODY || type == TFOOT || type == THEAD ||
				type == TD || type == TH || type == TR) {
			hubbub_tag tag = token->data.tag;

			if (type == TD || type == TH || type == TR) {
				/* Insert tbody and reprocess */
				tag.name.ptr = (const uint8_t *) "tbody";
				tag.name.len = SLEN("tbody");

				err = HUBBUB_REPROCESS;
			}

			clear_stack_table_context(treebuilder);
			insert_element(treebuilder, &tag);
			treebuilder->context.mode = IN_TABLE_BODY;
		} else if (type == TABLE) {
			/** \todo parse error */

			/* This should match "</table>" handling */
			element_stack_pop_until(treebuilder, TABLE);
			reset_insertion_mode(treebuilder);

			err = HUBBUB_REPROCESS;
		} else if (!tainted && (type == STYLE || type == SCRIPT)) {
			err = handle_in_head(treebuilder, token);
		} else if (!tainted && type == INPUT) {
			handled = process_input_in_table(treebuilder, token);
		} else {
			handled = false;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TABLE) {
			/** \todo fragment case */

			element_stack_pop_until(treebuilder, TABLE);
			reset_insertion_mode(treebuilder);
		} else if (type == BODY || type == CAPTION || type == COL ||
				type == COLGROUP || type == HTML ||
				type == TBODY || type == TD || type == TFOOT ||
				type == TH || type == THEAD || type == TR) {
			/** \todo parse error */
		} else {
			handled = false;
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		break;
	}

	if (!handled) {
		treebuilder->context.in_table_foster = true;

		/** \todo parse error */
		handle_in_body(treebuilder, token);

		treebuilder->context.in_table_foster = false;
	}


	return err;
}
