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

#undef DEBUG_IN_BODY

typedef struct bookmark {
	formatting_list_entry *prev;
	formatting_list_entry *next;
} bookmark;

static void process_character(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static bool process_start_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static bool process_end_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

static void process_html_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_body_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_container_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token);
static void process_form_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type);
static void process_plaintext_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_a_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_presentational_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type);
static void process_nobr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_button_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_applet_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, const hubbub_token *token, 
		element_type type);
static void process_hr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_image_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_input_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_isindex_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_textarea_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_select_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static void process_phrasing_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

static bool process_0body_in_body(hubbub_treebuilder *treebuilder);
static void process_0container_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static void process_0p_in_body(hubbub_treebuilder *treebuilder);
static void process_0dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static void process_0h_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static void process_0presentational_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static void process_0applet_button_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, element_type type);
static void process_0br_in_body(hubbub_treebuilder *treebuilder);
static void process_0generic_in_body(hubbub_treebuilder *treebuilder, 
		element_type type);

static bool aa_find_and_validate_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type,
		formatting_list_entry **element);
static formatting_list_entry *aa_find_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type);
static bool aa_find_furthest_block(hubbub_treebuilder *treebuilder,
		formatting_list_entry *formatting_element, 
		uint32_t *furthest_block);
static void aa_remove_from_parent(hubbub_treebuilder *treebuilder, void *node);
static void aa_reparent_node(hubbub_treebuilder *treebuilder, void *node, 
		void *new_parent);
static void aa_find_bookmark_location_reparenting_misnested(
		hubbub_treebuilder *treebuilder, 
		uint32_t formatting_element, uint32_t *furthest_block,
		bookmark *bookmark, uint32_t *last_node);
static void aa_remove_element_stack_item(hubbub_treebuilder *treebuilder, 
		uint32_t index, uint32_t limit);
static void aa_clone_and_replace_entries(hubbub_treebuilder *treebuilder,
		formatting_list_entry *element);


/**
 * Handle tokens in "in body" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
bool handle_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

#if !defined(NDEBUG) && defined(DEBUG_IN_BODY)
	fprintf(stdout, "Processing token %d\n", token->type);
	element_stack_dump(treebuilder, stdout);
	formatting_list_dump(treebuilder, stdout);
#endif

	if (treebuilder->context.strip_leading_lr &&
			token->type != HUBBUB_TOKEN_CHARACTER) {
		/* Reset the LR stripping flag */
		treebuilder->context.strip_leading_lr = false;
	}

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		process_character(treebuilder, token);
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
		reprocess = process_start_tag(treebuilder, token);
		break;
	case HUBBUB_TOKEN_END_TAG:
		reprocess = process_end_tag(treebuilder, token);
		break;
	case HUBBUB_TOKEN_EOF:
		for (uint32_t i = treebuilder->context.current_node; 
				i > 0; i--) {
			element_type type = 
				treebuilder->context.element_stack[i].type;

			if (!(type == DD || type == DT || type == LI ||
					type == P || type == TBODY || 
					type == TD || type == TFOOT ||
					type == TH || type == THEAD ||
					type == TR || type == BODY)) {
				/** \todo parse error */
				break;
			}
		}
		break;
	}

#if !defined(NDEBUG) && defined(DEBUG_IN_BODY)
	fprintf(stdout, "Processed\n");
	element_stack_dump(treebuilder, stdout);
	formatting_list_dump(treebuilder, stdout);
#endif

	return reprocess;
}

/**
 * Process a character token
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_character(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_string dummy = token->data.character;

	reconstruct_active_formatting_list(treebuilder);

	if (treebuilder->context.strip_leading_lr) {
		const uint8_t *str =
				treebuilder->input_buffer + dummy.data.off;

		/** \todo UTF-16 */
		if (*str == '\n') {
			dummy.data.off++;
			dummy.len--;
		}

		treebuilder->context.strip_leading_lr = false;
	}

	if (dummy.len)
		append_text(treebuilder, &dummy);
}

/**
 * Process a start tag
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token
 */
bool process_start_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	element_type type = element_type_from_name(treebuilder,
			&token->data.tag.name);

	if (type == HTML) {
		process_html_in_body(treebuilder, token);
	} else if (type == BASE || type == COMMAND ||
			type == EVENT_SOURCE || type == LINK ||
			type == META || type == NOFRAMES || type == SCRIPT ||
			type == STYLE || type == TITLE) {
		/* Process as "in head" */
		process_in_head(treebuilder, token);
	} else if (type == BODY) {
		process_body_in_body(treebuilder, token);
	} else if (type == ADDRESS || type == ARTICLE || type == ASIDE ||
			type == BLOCKQUOTE || type == CENTER ||
			type == DATAGRID || type == DETAILS ||
			type == DIALOG || type == DIR ||
			type == DIV || type == DL || type == FIELDSET ||
			type == FIGURE || type == FOOTER ||
			type == H1 || type == H2 || type == H3 ||
			type == H4 || type == H5 || type == H6 ||
			type == HEADER || type == MENU || type == NAV ||
			type == OL || type == P || type == SECTION ||
			type == UL) {
		process_container_in_body(treebuilder, token);
	} else if (type == PRE || type == LISTING) {
		process_container_in_body(treebuilder, token);

		treebuilder->context.strip_leading_lr = true;
	} else if (type == FORM) {
		process_form_in_body(treebuilder, token);
	} else if (type == DD || type == DT || type == LI) {
		process_dd_dt_li_in_body(treebuilder, token, type);
	} else if (type == PLAINTEXT) {
		process_plaintext_in_body(treebuilder, token);
	} else if (type == A) {
		process_a_in_body(treebuilder, token);
	} else if (type == B || type == BIG || type == EM || 
			type == FONT || type == I || type == S || 
			type == SMALL || type == STRIKE || 
			type == STRONG || type == TT || type == U) {
		process_presentational_in_body(treebuilder, 
				token, type);
	} else if (type == NOBR) {
		process_nobr_in_body(treebuilder, token);
	} else if (type == BUTTON) {
		process_button_in_body(treebuilder, token);
	} else if (type == APPLET || type == MARQUEE || 
			type == OBJECT) {
		process_applet_marquee_object_in_body(treebuilder,
				token, type);
	} else if (type == XMP) {
		reconstruct_active_formatting_list(treebuilder);
		parse_generic_rcdata(treebuilder, token, false);
	} else if (type == TABLE) {
		process_container_in_body(treebuilder, token);

		/** \todo Section 9.2.3.1 is really vague
		 * Are we meant to reset the insertion mode all the time or 
		 * only when we're actually in body? I'd inferred the latter 
		 * interpretation from the spec, but that causes breakage on 
		 * real-world pages. */
/*		if (treebuilder->context.mode == IN_BODY) {*/
			treebuilder->context.element_stack[
					current_table(treebuilder)].
					tainted = false;
			treebuilder->context.mode = IN_TABLE;
/*		}*/
	} else if (type == AREA || type == BASEFONT || 
			type == BGSOUND || type == BR || 
			type == EMBED || type == IMG || type == PARAM ||
			type == SPACER || type == WBR) {
		reconstruct_active_formatting_list(treebuilder);
		insert_element_no_push(treebuilder, &token->data.tag);
	} else if (type == HR) {
		process_hr_in_body(treebuilder, token);
	} else if (type == IMAGE) {
		process_image_in_body(treebuilder, token);
	} else if (type == INPUT) {
		process_input_in_body(treebuilder, token);
	} else if (type == ISINDEX) {
		process_isindex_in_body(treebuilder, token);
	} else if (type == TEXTAREA) {
		process_textarea_in_body(treebuilder, token);
	} else if (type == IFRAME || type == NOEMBED || 
			type == NOFRAMES || 
			(false /* scripting */ && type == NOSCRIPT)) {
		parse_generic_rcdata(treebuilder, token, false);
	} else if (type == SELECT) {
		process_select_in_body(treebuilder, token);

		if (treebuilder->context.mode == IN_BODY) {
			treebuilder->context.mode = IN_SELECT;
		} else if (treebuilder->context.mode == IN_TABLE ||
				treebuilder->context.mode == IN_CAPTION ||
				treebuilder->context.mode == IN_COLUMN_GROUP ||
				treebuilder->context.mode == IN_TABLE_BODY ||
				treebuilder->context.mode == IN_ROW ||
				treebuilder->context.mode == IN_CELL) {
			treebuilder->context.mode = IN_SELECT_IN_TABLE;
		}
	} else if (type == RP || type == RT) {
		/** \todo ruby */
	} else if (type == MATH || type == SVG) {
		hubbub_tag tag = token->data.tag;

		reconstruct_active_formatting_list(treebuilder);
		adjust_foreign_attributes(treebuilder, &tag);

		if (type == SVG) {
			adjust_svg_attributes(treebuilder, &tag);
			tag.ns = HUBBUB_NS_SVG;
		} else {
			tag.ns = HUBBUB_NS_MATHML;
		}

		if (token->data.tag.self_closing) {
			insert_element_no_push(treebuilder, &tag);
			/** \todo ack sc flag */
		} else {
			insert_element(treebuilder, &tag);
			treebuilder->context.second_mode =
					treebuilder->context.mode;
			treebuilder->context.mode = IN_FOREIGN_CONTENT;
		}
	} else if (type == CAPTION || type == COL || type == COLGROUP ||
			type == FRAME || type == FRAMESET ||
			type == HEAD || type == TBODY ||
			type == TD || type == TFOOT || type == TH ||
			type == THEAD || type == TR) {
		/** \todo parse error */
	} else {
		process_phrasing_in_body(treebuilder, token);
	}

	return reprocess;
}

/**
 * Process an end tag
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token
 */
bool process_end_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;
	element_type type = element_type_from_name(treebuilder,
			&token->data.tag.name);

	if (type == BODY) {
		if (process_0body_in_body(treebuilder) &&
				treebuilder->context.mode == IN_BODY) {
			treebuilder->context.mode = AFTER_BODY;
		}
	} else if (type == HTML) {
		/* Act as if </body> has been seen then, if
		 * that wasn't ignored, reprocess this token */
		if (process_0body_in_body(treebuilder) &&
				treebuilder->context.mode == IN_BODY) {
			treebuilder->context.mode = AFTER_BODY;
		}
		reprocess = true;
	} else if (type == ADDRESS || type == BLOCKQUOTE || 
			type == CENTER || type == DIR || type == DIV ||
			type == DL || type == FIELDSET || 
			type == LISTING || type == MENU ||
			type == OL || type == PRE || type == UL ||
			type == FORM) {
		process_0container_in_body(treebuilder, type);
	} else if (type == P) {
		process_0p_in_body(treebuilder);
	} else if (type == DD || type == DT || type == LI) {
		process_0dd_dt_li_in_body(treebuilder, type);
	} else if (type == H1 || type == H2 || type == H3 || 
			type == H4 || type == H5 || type == H6) {
		process_0h_in_body(treebuilder, type);
	} else if (type == A || type == B || type == BIG || 
			type == EM || type == FONT || type == I ||
			type == NOBR || type == S || type == SMALL ||
			type == STRIKE || type == STRONG ||
			type == TT || type == U) {
		process_0presentational_in_body(treebuilder, type);
	} else if (type == APPLET || type == BUTTON ||
			type == MARQUEE || type == OBJECT) {
		process_0applet_button_marquee_object_in_body(
				treebuilder, type);
	} else if (type == BR) {
		process_0br_in_body(treebuilder);
	} else if (type == AREA || type == BASEFONT || 
			type == BGSOUND || type == EMBED || 
			type == HR || type == IFRAME ||
			type == IMAGE || type == IMG ||
			type == INPUT || type == ISINDEX ||
			type == NOEMBED || type == NOFRAMES ||
			type == PARAM || type == SELECT ||
			type == SPACER || type == TABLE ||
			type == TEXTAREA || type == WBR ||
			(false /* scripting enabled */ && 
					type == NOSCRIPT)) {
		/** \todo parse error */
/*	} else if (type == EVENT_SOURCE || type == SECTION ||
			type == NAV || type == ARTICLE ||
			type == ASIDE || type == HEADER ||
			type == FOOTER || type == DATAGRID ||
			type == COMMAND) {
*/	} else {
		process_0generic_in_body(treebuilder, type);
	}

	return reprocess;
}

/**
 * Process a <html> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_html_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	/** \todo parse error */

	treebuilder->tree_handler->add_attributes(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[0].node,
			token->data.tag.attributes, 
			token->data.tag.n_attributes);
}

/**
 * Process a <body> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_body_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	/** \todo parse error */

	if (treebuilder->context.current_node < 1 || 
			treebuilder->context.element_stack[1].type != BODY)
		return;

	treebuilder->tree_handler->add_attributes(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[1].node,
			token->data.tag.attributes,
			token->data.tag.n_attributes);
}

/**
 * Process a generic container start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_container_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	if (element_in_scope(treebuilder, P, false)) {
		process_0p_in_body(treebuilder);
	}

	insert_element(treebuilder, &token->data.tag);
}

/**
 * Process a <form> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_form_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	if (treebuilder->context.form_element != NULL) {
		/** \todo parse error */
	} else {
		if (element_in_scope(treebuilder, P, false)) {
			process_0p_in_body(treebuilder);
		}

		insert_element(treebuilder, &token->data.tag);

		/* Claim a reference on the node and 
		 * use it as the current form element */
		treebuilder->tree_handler->ref_node(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[
			treebuilder->context.current_node].node);

		treebuilder->context.form_element =
			treebuilder->context.element_stack[
			treebuilder->context.current_node].node;
	}
}

/**
 * Process a <dd>, <dt> or <li> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
void process_dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node;

	if (element_in_scope(treebuilder, P, false)) {
		process_0p_in_body(treebuilder);
	}

	/* Find last LI/(DD,DT) on stack, if any */
	for (node = treebuilder->context.current_node; node > 0; node--) {
		element_type ntype = stack[node].type;

		if (type == LI && ntype == LI)
			break;

		if (((type == DD || type == DT) && 
				(ntype == DD || ntype == DT)))
			break;

		if (!is_formatting_element(ntype) &&
				!is_phrasing_element(ntype) &&
				ntype != ADDRESS && 
				ntype != DIV)
			break;
	}

	/* If we found one, then pop all nodes up to and including it */
	if (stack[node].type == LI || stack[node].type == DD ||
			stack[node].type == DT) {
		/* Check that we're only popping one node 
		 * and emit a parse error if not */
		if (treebuilder->context.current_node > node) {
			/** \todo parse error */
		}

		do {
			hubbub_ns ns;
			element_type otype;
			void *node;

			if (!element_stack_pop(treebuilder, &ns,
					&otype, &node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);
		} while (treebuilder->context.current_node >= node);
	}

	insert_element(treebuilder, &token->data.tag);
}

/**
 * Process a <plaintext> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_plaintext_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_tokeniser_optparams params;

	if (element_in_scope(treebuilder, P, false)) {
		process_0p_in_body(treebuilder);
	}

	insert_element(treebuilder, &token->data.tag);

	params.content_model.model = HUBBUB_CONTENT_MODEL_PLAINTEXT;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_CONTENT_MODEL,
			&params);
}

/**
 * Process a <a> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_a_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	formatting_list_entry *entry = 
			aa_find_formatting_element(treebuilder, A);

	if (entry != NULL) {
		uint32_t index = entry->stack_index;
		void *node = entry->details.node;
		formatting_list_entry *entry2;

		/** \todo parse error */

		/* Act as if </a> were seen */
		process_0presentational_in_body(treebuilder, A);

		entry2 = aa_find_formatting_element(treebuilder, A);

		/* Remove from formatting list, if it's still there */
		if (entry2 == entry && entry2->details.node == node) {
			element_type otype;
			void *onode;
			uint32_t oindex;

			formatting_list_remove(treebuilder, entry,
					&otype, &onode, &oindex);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, onode);
				
		}

		/* Remove from the stack of open elements, if still there */
		if (index <= treebuilder->context.current_node &&
				treebuilder->context.element_stack[index].node 
				== node) {
			aa_remove_element_stack_item(treebuilder, index,
					treebuilder->context.current_node);
			treebuilder->context.current_node--;
		}
	}

	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	formatting_list_append(treebuilder, A, 
		treebuilder->context.element_stack[
			treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
}

/**
 * Process a <b>, <big>, <em>, <font>, <i>, <s>, <small>, 
 * <strike>, <strong>, <tt>, or <u> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
void process_presentational_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	formatting_list_append(treebuilder, type, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
}

/**
 * Process a <nobr> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_nobr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	reconstruct_active_formatting_list(treebuilder);

	if (element_in_scope(treebuilder, NOBR, false)) {
		/** \todo parse error */

		/* Act as if </nobr> were seen */
		process_0presentational_in_body(treebuilder, NOBR);

		/* Yes, again */
		reconstruct_active_formatting_list(treebuilder);
	}

	insert_element(treebuilder, &token->data.tag);

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	formatting_list_append(treebuilder, NOBR, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
}

/**
 * Process a <button> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_button_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	if (element_in_scope(treebuilder, BUTTON, false)) {
		/** \todo parse error */

		/* Act as if </button> has been seen */
		process_0applet_button_marquee_object_in_body(treebuilder, 
				BUTTON);
	}

	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	if (treebuilder->context.form_element != NULL) {
		treebuilder->tree_handler->form_associate(
			treebuilder->tree_handler->ctx,
			treebuilder->context.form_element,
			treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
	}

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	formatting_list_append(treebuilder, BUTTON, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node,
		treebuilder->context.current_node);
}

/**
 * Process an <applet>, <marquee> or <object> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
void process_applet_marquee_object_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	formatting_list_append(treebuilder, type, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
}

/**
 * Process an <hr> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_hr_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	if (element_in_scope(treebuilder, P, false)) {
		process_0p_in_body(treebuilder);
	}

	insert_element_no_push(treebuilder, &token->data.tag);
}

/**
 * Process an <image> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_image_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_tag tag;

	/** \todo UTF-16 */
	tag.ns = HUBBUB_NS_HTML;
	tag.name.type = HUBBUB_STRING_PTR;
	tag.name.data.ptr = (const uint8_t *) "img";
	tag.name.len = SLEN("img");

	tag.n_attributes = token->data.tag.n_attributes;
	tag.attributes = token->data.tag.attributes;

	reconstruct_active_formatting_list(treebuilder);

	insert_element_no_push(treebuilder, &tag);
}

/**
 * Process an <input> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_input_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_ns ns;
	element_type otype;
	void *node;

	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	if (treebuilder->context.form_element != NULL) {
		treebuilder->tree_handler->form_associate(
			treebuilder->tree_handler->ctx,
			treebuilder->context.form_element,
			treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
	}

	if (!element_stack_pop(treebuilder, &ns, &otype, &node)) {
		/** \todo errors */
	}

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			node);
}

/**
 * Process an <isindex> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_isindex_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_token dummy;
	hubbub_attribute *action = NULL;
	hubbub_attribute *prompt = NULL;
	hubbub_attribute *attrs = NULL;
	size_t n_attrs = 0;

	/** \todo parse error */

	if (treebuilder->context.form_element != NULL)
		return;

	/* First up, clone the token's attributes */
	if (token->data.tag.n_attributes > 0) {
		attrs = treebuilder->alloc(NULL, 
				(token->data.tag.n_attributes + 1) *
						sizeof(hubbub_attribute),
		       		treebuilder->alloc_pw);
		if (attrs == NULL) {
			/** \todo error handling */
			return;
		}

		for (uint32_t i = 0; i < token->data.tag.n_attributes; i++) {
			hubbub_attribute *attr = &token->data.tag.attributes[i];
			const uint8_t *name = treebuilder->input_buffer + 
					attr->name.data.off;

			if (strncmp((const char *) name, "action", 
					attr->name.len) == 0) {
				action = attr;	
			} else if (strncmp((const char *) name, "prompt",
					attr->name.len) == 0) {
				prompt = attr;
			} else if (strncmp((const char *) name, "name",
					attr->name.len) == 0) {
			} else {
				attrs[n_attrs++] = *attr;
			}
		}

		attrs[n_attrs].ns = HUBBUB_NS_HTML;
		attrs[n_attrs].name.type = HUBBUB_STRING_PTR;
		attrs[n_attrs].name.data.ptr = (const uint8_t *) "name";
		attrs[n_attrs].name.len = SLEN("name");
		attrs[n_attrs].value.type = HUBBUB_STRING_PTR;
		attrs[n_attrs].value.data.ptr = (const uint8_t *) "isindex";
		attrs[n_attrs].value.len = SLEN("isindex");
		n_attrs++;
	}

	/* isindex algorithm */

	/* Set up dummy as a start tag token */
	dummy.type = HUBBUB_TOKEN_START_TAG;
	dummy.data.tag.ns = HUBBUB_NS_HTML;
	dummy.data.tag.name.type = HUBBUB_STRING_PTR;

	/* Act as if <form> were seen */
	dummy.data.tag.name.data.ptr = (const uint8_t *) "form";
	dummy.data.tag.name.len = SLEN("form");

	dummy.data.tag.n_attributes = action != NULL ? 1 : 0;
	dummy.data.tag.attributes = action;

	process_form_in_body(treebuilder, &dummy);

	/* Act as if <hr> were seen */
	dummy.data.tag.name.data.ptr = (const uint8_t *) "hr";
	dummy.data.tag.name.len = SLEN("hr");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	process_hr_in_body(treebuilder, &dummy);

	/* Act as if <p> were seen */
	dummy.data.tag.name.data.ptr = (const uint8_t *) "p";
	dummy.data.tag.name.len = SLEN("p");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	process_container_in_body(treebuilder, &dummy);

	/* Act as if <label> were seen */
	dummy.data.tag.name.data.ptr = (const uint8_t *) "label";
	dummy.data.tag.name.len = SLEN("label");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	process_phrasing_in_body(treebuilder, &dummy);

	/* Act as if a stream of characters were seen */
	dummy.type = HUBBUB_TOKEN_CHARACTER;
	if (prompt != NULL) {
		dummy.data.character = prompt->value;
	} else {
		/** \todo Localisation */
#define PROMPT "This is a searchable index. Insert your search keywords here: "
		dummy.data.character.type = HUBBUB_STRING_PTR;
		dummy.data.character.data.ptr = (const uint8_t *) PROMPT;
		dummy.data.character.len = SLEN(PROMPT);
#undef PROMPT
	}
	
	process_character(treebuilder, &dummy);

	/* Act as if <input> was seen */
	dummy.type = HUBBUB_TOKEN_START_TAG;
	dummy.data.tag.name.type = HUBBUB_STRING_PTR;
	dummy.data.tag.name.data.ptr = (const uint8_t *) "input";
	dummy.data.tag.name.len = SLEN("input");

	dummy.data.tag.n_attributes = n_attrs;
	dummy.data.tag.attributes = attrs;

	process_input_in_body(treebuilder, &dummy);

	/* Act as if </label> was seen */
	process_0generic_in_body(treebuilder, LABEL);

	/* Act as if </p> was seen */
	process_0p_in_body(treebuilder);

	/* Act as if <hr> was seen */
	dummy.data.tag.name.data.ptr = (const uint8_t *) "hr";
	dummy.data.tag.name.len = SLEN("hr");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	process_hr_in_body(treebuilder, &dummy);

	/* Act as if </form> was seen */
	process_0container_in_body(treebuilder, FORM);

	/* Clean up */
	treebuilder->alloc(attrs, 0, treebuilder->alloc_pw);
}

/**
 * Process a <textarea> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_textarea_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	treebuilder->context.strip_leading_lr = true;
	parse_generic_rcdata(treebuilder, token, true);
}

/**
 * Process a <select> start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_select_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);

	if (treebuilder->context.form_element != NULL) {
		treebuilder->tree_handler->form_associate(
			treebuilder->tree_handler->ctx,
			treebuilder->context.form_element,
			treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
	}
}

/**
 * Process a phrasing start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
void process_phrasing_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	reconstruct_active_formatting_list(treebuilder);

	insert_element(treebuilder, &token->data.tag);
}

/**
 * Process a </body> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \return True if processed, false otherwise
 */
bool process_0body_in_body(hubbub_treebuilder *treebuilder)
{
	bool processed = true;

	if (!element_in_scope(treebuilder, BODY, false)) {
		/** \todo parse error */
		processed = true;
	} else {
		element_context *stack = treebuilder->context.element_stack;
		uint32_t node;

		for (node = treebuilder->context.current_node; 
				node > 0; node--) {
			element_type ntype = stack[node].type;

			if (ntype != DD && ntype != DT && ntype != LI && 
					ntype != P && ntype != TBODY &&
					ntype != TD && ntype != TFOOT &&
					ntype != TH && ntype != THEAD &&
					ntype != TR && ntype != BODY) {
				/** \todo parse error */
			}
		}
	}

	return processed;
}

/**
 * Process a container end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0container_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	if (type == FORM) {
		if (treebuilder->context.form_element != NULL)
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.form_element);
		treebuilder->context.form_element = NULL;
	}

	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			if (!element_stack_pop(treebuilder, &ns, &otype,
					&node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}
	}
}

/**
 * Process a </p> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 */
void process_0p_in_body(hubbub_treebuilder *treebuilder)
{
	uint32_t popped = 0;

	if (treebuilder->context.element_stack[
			treebuilder->context.current_node].type != P) {
		/** \todo parse error */
	}

	while (element_in_scope(treebuilder, P, false)) {
		hubbub_ns ns;
		element_type type;
		void *node;

		if (!element_stack_pop(treebuilder, &ns, &type, &node)) {
			/** \todo errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		popped++;
	}

	if (popped == 0) {
		hubbub_token dummy;

		dummy.type = HUBBUB_TOKEN_START_TAG;
		dummy.data.tag.ns = HUBBUB_NS_HTML;
		dummy.data.tag.name.type = HUBBUB_STRING_PTR;
		/** \todo UTF-16 */
		dummy.data.tag.name.data.ptr = (const uint8_t *) "p";
		dummy.data.tag.name.len = SLEN("p");
		dummy.data.tag.n_attributes = 0;
		dummy.data.tag.attributes = NULL;

		process_container_in_body(treebuilder, &dummy);

		/* Reprocess the end tag. This is safe as we've just 
		 * inserted a <p> into the current scope */
		process_0p_in_body(treebuilder);
	}
}

/**
 * Process a </dd>, </dt>, or </li> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, type);

		do {
			hubbub_ns ns;
			void *node;

			if (!element_stack_pop(treebuilder, &ns, &otype, &node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}
	}
}

/**
 * Process a </h1>, </h2>, </h3>, </h4>,
 * </h5>, or </h6> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0h_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	UNUSED(type);

	/** \todo optimise this */
	if (element_in_scope(treebuilder, H1, false) ||
			element_in_scope(treebuilder, H2, false) ||
			element_in_scope(treebuilder, H3, false) ||
			element_in_scope(treebuilder, H4, false) ||
			element_in_scope(treebuilder, H5, false) ||
			element_in_scope(treebuilder, H6, false)) {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			if (!element_stack_pop(treebuilder, &ns, &otype,
					&node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != H1 && otype != H2 &&
				otype != H3 && otype != H4 &&
				otype != H5 && otype != H6);

		if (popped > 1) {
			/** \todo parse error */
		}
	} else {
		/** \todo parse error */
	}
}

/**
 * Process a presentational end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0presentational_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	/* Welcome to the adoption agency */

	while (true) {
		element_context *stack = treebuilder->context.element_stack;

		/* 1 */
		formatting_list_entry *entry;
		uint32_t formatting_element;

		if (!aa_find_and_validate_formatting_element(treebuilder,
				type, &entry))
			return;

		assert(entry->details.type == type);

		/* Take a copy of the stack index for use
		 * during stack manipulation */
		formatting_element = entry->stack_index;

		/* 2 & 3 */
		uint32_t furthest_block;

		if (!aa_find_furthest_block(treebuilder,
				entry, &furthest_block))
			return;

		/* 4 */
		uint32_t common_ancestor = formatting_element - 1;

		/* 5 */
		aa_remove_from_parent(treebuilder, stack[furthest_block].node);

		/* 6 */
		bookmark bookmark;

		bookmark.prev = entry->prev;
		bookmark.next = entry->next;

		/* 7 */
		uint32_t last_node;

		aa_find_bookmark_location_reparenting_misnested(treebuilder,
				formatting_element, &furthest_block,
				&bookmark, &last_node);

		/* 8 */
		if (stack[common_ancestor].type == TABLE ||
				stack[common_ancestor].type == TBODY ||
				stack[common_ancestor].type == TFOOT ||
				stack[common_ancestor].type == THEAD ||
				stack[common_ancestor].type == TR) {
			aa_insert_into_foster_parent(treebuilder,
					stack[last_node].node);
		} else {
			aa_reparent_node(treebuilder, stack[last_node].node,
					stack[common_ancestor].node);
		}

		/* 9 */
		void *fe_clone = NULL;

		treebuilder->tree_handler->clone_node(
				treebuilder->tree_handler->ctx,
				entry->details.node, false, &fe_clone);

		/* 10 */
		treebuilder->tree_handler->reparent_children(
				treebuilder->tree_handler->ctx,
				stack[furthest_block].node, fe_clone);

		/* 11 */
		void *clone_appended = NULL;

		treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				stack[furthest_block].node, fe_clone,
				&clone_appended);

		/* 12 and 13 are reversed here so that we know the correct
		 * stack index to use when inserting into the formatting list */

		/* 13 */
		aa_remove_element_stack_item(treebuilder, formatting_element,
				furthest_block);

		/* Fix up furthest block index */
		furthest_block--;

		/* Now, in the gap after furthest block,
		 * we insert an entry for clone */
		stack[furthest_block + 1].type = entry->details.type;
		stack[furthest_block + 1].node = clone_appended;

		/* 12 */
		element_type otype;
		void *onode;
		uint32_t oindex;

		formatting_list_remove(treebuilder, entry,
				&otype, &onode, &oindex);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,	onode);

		formatting_list_insert(treebuilder,
				bookmark.prev, bookmark.next,
				otype, fe_clone, furthest_block + 1);

		/* 14 */
	}
}

/**
 * Adoption agency: find and validate the formatting element
 *
 * \param treebuilder  The treebuilder instance
 * \param type         Element type to search for
 * \param element      Pointer to location to receive list entry
 * \return True to continue processing, false to stop
 */
bool aa_find_and_validate_formatting_element(hubbub_treebuilder *treebuilder,
		element_type type, formatting_list_entry **element)
{
	formatting_list_entry *entry;

	entry = aa_find_formatting_element(treebuilder, type);

	if (entry == NULL || (entry->stack_index != 0 &&
			element_in_scope(treebuilder, entry->details.type,
					false) != entry->stack_index)) {
		/** \todo parse error */
		return false;
	}

	if (entry->stack_index == 0) {
		/* Not in element stack => remove from formatting list */
		element_type type;
		void *node;
		uint32_t index;

		/** \todo parse error */

		if (!formatting_list_remove(treebuilder, entry,
				&type, &node, &index)) {
			/** \todo errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return false;
	}

	if (entry->stack_index != treebuilder->context.current_node) {
		/** \todo parse error */
	}

	*element = entry;

	return true;
}

/**
 * Adoption agency: find formatting element
 *
 * \param treebuilder  The treebuilder instance
 * \param type         Type of element to search for
 * \return Pointer to formatting element, or NULL if none found
 */
formatting_list_entry *aa_find_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type)
{
	formatting_list_entry *entry;

	for (entry = treebuilder->context.formatting_list_end;
			entry != NULL; entry = entry->prev) {

		/* Assumption: HTML and TABLE elements are not in the list */
		if (is_scoping_element(entry->details.type) ||
				entry->details.type == type)
			break;
	}

	/* Check if we stopped on a marker, rather than a formatting element */
	if (entry != NULL && is_scoping_element(entry->details.type))
		entry = NULL;

	return entry;
}

/**
 * Adoption agency: find furthest block
 *
 * \param treebuilder         The treebuilder instance
 * \param formatting_element  The formatting element
 * \param furthest_block      Pointer to location to receive furthest block
 * \return True to continue processing (::furthest_block filled in).
 */
bool aa_find_furthest_block(hubbub_treebuilder *treebuilder,
		formatting_list_entry *formatting_element,
		uint32_t *furthest_block)
{
	uint32_t fe_index = formatting_element->stack_index;
	uint32_t fb;

	for (fb = fe_index + 1; fb <= treebuilder->context.current_node; fb++) {
		element_type type = treebuilder->context.element_stack[fb].type;

		if (!(is_phrasing_element(type) || is_formatting_element(type)))
			break;
	}

	if (fb > treebuilder->context.current_node) {
		hubbub_ns ns;
		element_type type;
		void *node;
		uint32_t index;

		/* Pop all elements off the stack up to,
		 * and including, the formatting element */
		do {
			if (!element_stack_pop(treebuilder, &ns, &type, &node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);
		} while (treebuilder->context.current_node >= fe_index);

		/* Remove the formatting element from the list */
		if (!formatting_list_remove(treebuilder, formatting_element,
				&type, &node, &index)) {
			/* \todo errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return false;
	}

	*furthest_block = fb;

	return true;
}

/**
 * Adoption agency: remove a node from its parent
 *
 * \param treebuilder  The treebuilder instance
 * \param node         Node to remove
 */
void aa_remove_from_parent(hubbub_treebuilder *treebuilder, void *node)
{
	/* Get parent */
	void *parent = NULL;

	treebuilder->tree_handler->get_parent(treebuilder->tree_handler->ctx,
			node, false, &parent);

	if (parent != NULL) {
		void *removed;

	 	treebuilder->tree_handler->remove_child(
				treebuilder->tree_handler->ctx,
				parent, node, &removed);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, removed);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, parent);
	}
}

/**
 * Adoption agency: reparent a node
 *
 * \param treebuilder  The treebuilder instance
 * \param node         The node to reparent
 * \param new_parent   The new parent
 */
void aa_reparent_node(hubbub_treebuilder *treebuilder, void *node,
		void *new_parent)
{
	void *appended;

	aa_remove_from_parent(treebuilder, node);

	treebuilder->tree_handler->append_child(treebuilder->tree_handler->ctx,
			new_parent, node, &appended);

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			appended);
}

/**
 * Adoption agency: this is step 7
 *
 * \param treebuilder         The treebuilder instance
 * \param formatting_element  The stack index of the formatting element
 * \param furthest_block      Pointer to index of furthest block in element
 *                            stack (updated on exit)
 * \param bookmark            Pointer to bookmark (pre-initialised)
 * \param last_node           Pointer to location to receive index of last node
 */
void aa_find_bookmark_location_reparenting_misnested(
		hubbub_treebuilder *treebuilder,
		uint32_t formatting_element, uint32_t *furthest_block,
		bookmark *bookmark, uint32_t *last_node)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node, last, fb;
	formatting_list_entry *node_entry;

	node = last = fb = *furthest_block;

	while (true) {
		/* i */
		node--;

		/* ii */
		for (node_entry = treebuilder->context.formatting_list_end;
				node_entry != NULL;
				node_entry = node_entry->prev) {
			if (node_entry->stack_index == node)
				break;
		}

		/* Node is not in list of active formatting elements */
		if (node_entry == NULL) {
			aa_remove_element_stack_item(treebuilder,
				node, treebuilder->context.current_node);

			/* Update furthest block index and the last node index,
			 * as these are always below node in the stack */
			fb--;
			last--;

			/* Fixup the current_node index */
			treebuilder->context.current_node--;

			/* Back to i */
			continue;
		}

		/* iii */
		if (node == formatting_element)
			break;

		/* iv */
		if (last == fb) {
			bookmark->prev = node_entry;
			bookmark->next = node_entry->next;
		}

		/* v */
		bool children = false;

		treebuilder->tree_handler->has_children(
				treebuilder->tree_handler->ctx,
				node_entry->details.node, &children);

		if (children) {
			aa_clone_and_replace_entries(treebuilder, node_entry);
		}

		/* vi */
		aa_reparent_node(treebuilder,
				stack[last].node, stack[node].node);

		/* vii */
		last = node;

		/* viii */
	}

	*furthest_block = fb;
	*last_node = last;
}

/**
 * Adoption agency: remove an entry from the stack at the given index
 *
 * \param treebuilder  The treebuilder instance
 * \param index        The index of the item to remove
 * \param limit        The index of the last item to move
 *
 * Preconditions: index < limit, limit <= current_node
 * Postcondition: stack[limit] is empty
 */
void aa_remove_element_stack_item(hubbub_treebuilder *treebuilder,
		uint32_t index, uint32_t limit)
{
	element_context *stack = treebuilder->context.element_stack;

	assert(index < limit);
	assert(limit <= treebuilder->context.current_node);

	/* First, scan over subsequent entries in the stack,
	 * searching for them in the list of active formatting
	 * entries. If found, update the corresponding
	 * formatting list entry's stack index to match the
	 * new stack location */
	for (uint32_t n = index + 1; n <= limit; n++) {
		if (is_formatting_element(stack[n].type) ||
				(is_scoping_element(stack[n].type) &&
				stack[n].type != HTML &&
				stack[n].type != TABLE)) {
			formatting_list_entry *e;

			for (e = treebuilder->context.formatting_list_end;
					e != NULL; e = e->prev) {
				if (e->stack_index == n)
					e->stack_index--;
			}
		}
	}

	/* Reduce node's reference count */
	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
					stack[index].node);

	/* Now, shuffle the stack up one, removing node in the process */
	memmove(&stack[index], &stack[index + 1],
			(limit - index) * sizeof(element_context));
}

/**
 * Adoption agency: shallow clone a node and replace its formatting list
 * and element stack entries
 *
 * \param treebuilder  The treebuilder instance
 * \param element      The item in the formatting list containing the node
 */
void aa_clone_and_replace_entries(hubbub_treebuilder *treebuilder,
		formatting_list_entry *element)
{
	element_type otype;
	uint32_t oindex;
	void *clone, *onode;

	/* Shallow clone of node */
	treebuilder->tree_handler->clone_node(treebuilder->tree_handler->ctx,
			element->details.node, false, &clone);

	/* Replace formatting list entry for node with clone */
	formatting_list_replace(treebuilder, element,
			element->details.type, clone, element->stack_index,
			&otype, &onode, &oindex);

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			onode);

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
			clone);

	/* Replace node's stack entry with clone */
	treebuilder->context.element_stack[element->stack_index].node = clone;

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			onode);
}

/**
 * Adoption agency: locate foster parent and insert node into it
 *
 * \param treebuilder  The treebuilder instance
 * \param node         The node to insert
 */
void aa_insert_into_foster_parent(hubbub_treebuilder *treebuilder, void *node)
{
	element_context *stack = treebuilder->context.element_stack;
	void *foster_parent = NULL;
	bool insert = false;
	void *inserted;

	uint32_t cur_table = current_table(treebuilder);

	stack[cur_table].tainted = true;

	if (cur_table == 0) {
		treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx,
				stack[0].node);

		foster_parent = stack[0].node;
	} else {
		void *t_parent = NULL;

		treebuilder->tree_handler->get_parent(
			treebuilder->tree_handler->ctx,
			stack[cur_table].node,
			true, &t_parent);

		if (t_parent != NULL) {
			foster_parent = t_parent;
			insert = true;
		} else {
			treebuilder->tree_handler->ref_node(
					treebuilder->tree_handler->ctx,
					stack[cur_table - 1].node);
			foster_parent = stack[cur_table - 1].node;
		}
	}

	if (insert) {
		treebuilder->tree_handler->insert_before(
				treebuilder->tree_handler->ctx,
				foster_parent, node,
				stack[cur_table].node,
				&inserted);
	} else {
		treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				foster_parent, node,
				&inserted);
	}

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			inserted);

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			foster_parent);
}


/**
 * Process an </applet>, <button>, <marquee>,
 * or <object> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0applet_button_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, element_type type)
{
	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			if (!element_stack_pop(treebuilder, &ns, &otype,
					&node)) {
				/** \todo errors */
			}

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}

		clear_active_formatting_list_to_marker(treebuilder);
	}
}

/**
 * Process a </br> end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 */
void process_0br_in_body(hubbub_treebuilder *treebuilder)
{
	hubbub_tag tag;

	/** \todo parse error */

	/* Act as if <br> has been seen. */

	/** \todo UTF-16 */
	tag.ns = HUBBUB_NS_HTML;
	tag.name.type = HUBBUB_STRING_PTR;
	tag.name.data.ptr = (const uint8_t *) "br";
	tag.name.len = SLEN("br");

	tag.n_attributes = 0;
	tag.attributes = NULL;

	reconstruct_active_formatting_list(treebuilder);

	insert_element_no_push(treebuilder, &tag);
}

/**
 * Process a generic end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
void process_0generic_in_body(hubbub_treebuilder *treebuilder, 
		element_type type)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node = treebuilder->context.current_node;

	do {
		if (stack[node].type == type) {
			uint32_t popped = 0;
			element_type otype;

			close_implied_end_tags(treebuilder, UNKNOWN);

			do {
				hubbub_ns ns;
				void *node;

				if (!element_stack_pop(treebuilder,
						&ns, &otype, &node)) {
					/** \todo errors */
				}

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);

				popped++;
			} while (otype != type);

			if (popped > 1) {
				/** \todo parse error */
			}

			break;
		} else if (!is_formatting_element(stack[node].type) && 
				!is_phrasing_element(stack[node].type)) {
			/** \todo parse error */
			break;
		}
	} while (--node > 0);
}

