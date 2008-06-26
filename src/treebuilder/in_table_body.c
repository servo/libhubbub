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

	while (cur_node != TBODY && cur_node != TFOOT &&
			cur_node != THEAD && cur_node != HTML) {
		hubbub_ns ns;
		element_type type;
		void *node;

		if (!element_stack_pop(treebuilder, &ns, &type, &node)) {
			/** \todo errors */
		}

		cur_node = treebuilder->context.element_stack[
				treebuilder->context.current_node].type;
	}

	return;
}


/**
 * Handle the case common to some start tag and the table end tag cases.
 *
 * \param treebuilder	The treebuilder instance
 */
static bool table_sub_start_or_table_end(hubbub_treebuilder *treebuilder)
{
	if (element_in_scope(treebuilder, TBODY, true) ||
			element_in_scope(treebuilder, THEAD, true) ||
			element_in_scope(treebuilder, TFOOT, true)) {
		hubbub_ns ns;
		element_type otype;
		void *node;

		table_clear_stack(treebuilder);

		/* "Act as if an end tag with the same name as the current
		 * node had been seen" -- this behaviour should be identical
		 * to handling for (tbody/tfoot/thead) end tags in this mode */
		if (!element_stack_pop(treebuilder, &ns, &otype, &node)) {
			/** \todo errors */
		}

		treebuilder->context.mode = IN_TABLE;

		return true;
	} else {
		/** \todo parse error */
	}

	return false;
}


/**
 * Handle tokens in "in column group" insertion mode
 *
 * Up to date with the spec as of 25 June 2008
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_in_column_group(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

	switch (token->type) {
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TR) {
			table_clear_stack(treebuilder);
			insert_element(treebuilder, &token->data.tag);
			treebuilder->context.mode = IN_ROW;
		} else if (type == TH || TD) {
			hubbub_tag tag;

			/** \todo parse error */

			/* Manufacture tr tag */
			tag.name.type = HUBBUB_STRING_PTR;
			tag.name.data.ptr = (const uint8_t *) "tr";
			tag.name.len = SLEN("tr");

			tag.n_attributes = 0;
			tag.attributes = NULL;

			insert_element(treebuilder, &tag);
			treebuilder->context.mode = IN_ROW;

			reprocess = true;
		} else if (type == CAPTION || type == COL ||
				type == COLGROUP || type == TBODY ||
				type == TFOOT || type == THEAD) {
			table_sub_start_or_table_end(treebuilder);
		} else {
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TBODY || type == TFOOT || type == THEAD) {
			if (!element_in_scope(treebuilder, type, true)) {
				/** \todo parse error */
				/* Ignore the token */
			} else {
				hubbub_ns ns;
				element_type otype;
				void *node;

				table_clear_stack(treebuilder);
				if (!element_stack_pop(treebuilder, &ns,
						&otype, &node)) {
					/** \todo errors */
				}

				treebuilder->context.mode = IN_TABLE;
			}
		} else if (type == TABLE) {
			table_sub_start_or_table_end(treebuilder);
		} else if (type == BODY || type == CAPTION || type == COL ||
				type == COLGROUP || type == HTML ||
				type == TD || type == TH || type == TR) {
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

