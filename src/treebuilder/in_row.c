/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Clear the stack back to a table body context.
 *
 * \param treebuilder	The treebuilder instance
 */
static void table_clear_stack(hubbub_treebuilder *treebuilder)
{
	element_type cur_node = treebuilder->context.element_stack[
			treebuilder->context.current_node].type;

	while (cur_node != TR && cur_node != HTML) {
		element_type type;
		void *node;

		if (!element_stack_pop(treebuilder, &type, &node)) {
			/** \todo errors */
		}

		cur_node = treebuilder->context.element_stack[
				treebuilder->context.current_node].type;
	}

	return;
}


/**
 * Handle </tr> and anything that acts "as if" </tr> was emitted.
 *
 * \param treebuilder	The treebuilder instance
 * \return True to reprocess the token, false otherwise
 */
static inline bool act_as_if_end_tag_tr(hubbub_treebuilder *treebuilder)
{
	element_type otype;
	void *node;

	/** \todo fragment case */

	table_clear_stack(treebuilder);
	if (!element_stack_pop(treebuilder, &otype, &node)) {
		/** \todo errors */
	}

	treebuilder->context.mode = IN_TABLE_BODY;

	return true;
}


/**
 * Handle tokens in "in row" insertion mode
 *
 * Up to date with the spec as of 25 June 2008
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_in_row(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

	switch (token->type) {
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TH || TD) {
			table_clear_stack(treebuilder);

			insert_element(treebuilder, &token->data.tag);
			treebuilder->context.mode = IN_CELL;

			formatting_list_append(treebuilder, type,
					treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
					treebuilder->context.current_node);
		} else if (type == CAPTION || type == COL ||
				type == COLGROUP || type == TBODY ||
				type == TFOOT || type == THEAD || type == TR) {
			reprocess = act_as_if_end_tag_tr(treebuilder);
		} else {
			reprocess = process_in_table(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TR) {
			(void)act_as_if_end_tag_tr(treebuilder);
		} else if (type == TABLE) {
			reprocess = act_as_if_end_tag_tr(treebuilder);
		} else if (type == BODY || type == CAPTION || type == COL ||
				type == COLGROUP || type == HTML ||
				type == TD || type == TH) {
			/** \todo parse error */
			/* Ignore the token */
		} else {
			reprocess = process_in_table(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_CHARACTER:
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_EOF:
		reprocess = process_in_table(treebuilder, token);
		break;
	}

	return reprocess;
}

