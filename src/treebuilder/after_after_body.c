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


/**
 * Handle token in "after after body" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_after_after_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

	switch (token->type) {
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_CHARACTER:
		handle_in_body(treebuilder, token);
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/* Process as if "in body" */
			process_tag_in_body(treebuilder, token);
		} else {
			/** \todo parse error */
			treebuilder->context.mode = IN_BODY;
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
		/** \todo parse error */
		treebuilder->context.mode = IN_BODY;
		reprocess = true;
		break;
	case HUBBUB_TOKEN_EOF:
		break;
	}

	return reprocess;
}

