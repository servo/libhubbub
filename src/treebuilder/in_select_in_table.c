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
 * Handle token in "in select in table" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_in_select_in_table(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool handled = false;
	bool reprocess = false;

	if (token->type == HUBBUB_TOKEN_END_TAG ||
			token->type == HUBBUB_TOKEN_START_TAG) {
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == CAPTION || type == TABLE || type == TBODY ||
				type == TFOOT || type == THEAD || type == TR ||
				type == TD || type == TH) {
			/** \todo parse error */

			handled = true;

			if ((token->type == HUBBUB_TOKEN_END_TAG &&
					element_in_scope(treebuilder, type,
							true)) ||
					token->type == HUBBUB_TOKEN_START_TAG) {
				/** \todo fragment case */

				element_stack_pop_until(treebuilder, SELECT);
				reset_insertion_mode(treebuilder);
				reprocess = true;
			}
		}
	}

	if (!handled) {
		reprocess = process_in_select(treebuilder, token);
	}

	return reprocess;
}
