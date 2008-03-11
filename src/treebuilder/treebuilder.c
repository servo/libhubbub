/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/treebuilder.h"
#include "utils/utils.h"

typedef enum
{
	INITIAL,
	BEFORE_HTML,
	BEFORE_HEAD,
	IN_HEAD,
	IN_HEAD_NOSCRIPT,
	AFTER_HEAD,
	IN_BODY,
	IN_TABLE,
	IN_CAPTION,
	IN_COLUMN_GROUP,
	IN_TABLE_BODY,
	IN_ROW,
	IN_CELL,
	IN_SELECT,
	IN_SELECT_IN_TABLE,
	AFTER_BODY,
	IN_FRAMESET,
	AFTER_FRAMESET,
	AFTER_AFTER_BODY,
	AFTER_AFTER_FRAMESET,
} insertion_mode;

typedef enum
{
/* Special */
	ADDRESS, AREA, BASE, BASEFONT, BGSOUND, BLOCKQUOTE, BODY, BR, CENTER,
	COL, COLGROUP, DD, DIR, DIV, DL, DT, EMBED, FIELDSET, FORM, FRAME,
	FRAMESET, H1, H2, H3, H4, H5, H6, HEAD, HR, IFRAME, IMAGE, IMG, INPUT,
	ISINDEX, LI, LINK, LISTING, MENU, META, NOEMBED, NOFRAMES, NOSCRIPT,
	OL, OPTGROUP, OPTION, P, PARAM, PLAINTEXT, PRE, SCRIPT, SELECT, SPACER,
	STYLE, TBODY, TEXTAREA, TFOOT, THEAD, TITLE, TR, UL, WBR,
/* Scoping */
	APPLET, BUTTON, CAPTION, HTML, MARQUEE, OBJECT, TABLE, TD, TH,
/* Formatting */
	A, B, BIG, EM, FONT, I, NOBR, S, SMALL, STRIKE, STRONG, TT, U,
/* Phrasing */
	/**< \todo Enumerate phrasing elements */
} element_type;

typedef struct element_context
{
	element_type type;
	void *node;
} element_context;

typedef struct formatting_list_entry
{
	element_context details;	/**< Entry details */

	uint32_t stack_index;		/**< Index into element stack */

	struct formatting_list_entry *prev;	/**< Previous in list */
	struct formatting_list_entry *next;	/**< Next in list */
} formatting_list_entry;

typedef struct hubbub_treebuilder_context
{
	insertion_mode mode;		/**< The current insertion mode */

#define ELEMENT_STACK_CHUNK 128
	element_context *element_stack;	/**< Stack of open elements */
	uint32_t stack_alloc;		/**< Number of stack slots allocated */
	uint32_t current_node;		/**< Index of current node in stack */
	uint32_t current_table;		/**< Index of current table in stack */

	formatting_list_entry *formatting_list;	/**< List of active formatting 
						 * elements */
	formatting_list_entry *formatting_list_end;	/**< End of active 
							 * formatting list */

	void *head_element;		/**< Pointer to HEAD element */

	void *form_element;		/**< Pointer to most recently 
					 * opened FORM element */

	void *document;			/**< Pointer to the document node */
} hubbub_treebuilder_context;

struct hubbub_treebuilder
{
	hubbub_tokeniser *tokeniser;	/**< Underlying tokeniser */

	const uint8_t *input_buffer;	/**< Start of tokeniser's buffer */
	size_t input_buffer_len;	/**< Length of input buffer */

	hubbub_treebuilder_context context;

	hubbub_tree_handler *tree_handler;

	hubbub_buffer_handler buffer_handler;
	void *buffer_pw;

	hubbub_error_handler error_handler;
	void *error_pw;

	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *alloc_pw;			/**< Client private data */
};

static void hubbub_treebuilder_buffer_handler(const uint8_t *data,
		size_t len, void *pw);
static void hubbub_treebuilder_token_handler(const hubbub_token *token, 
		void *pw);

static bool handle_initial(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

/** \todo Uncomment the static keyword here once these functions are actually used */

/*static*/ bool element_in_scope(hubbub_treebuilder *treebuilder,
		element_type type, bool in_table);
/*static*/ void reconstruct_active_formatting_list(hubbub_treebuilder *treebuilder);
/*static*/ void clear_active_formatting_list_to_marker(
		hubbub_treebuilder *treebuilder);
/*static*/ void insert_element(hubbub_treebuilder *treebuilder, 
		const hubbub_tag *tag_name);
/*static*/ void close_implied_end_tags(hubbub_treebuilder *treebuilder, 
		element_type except);
/*static*/ void reset_insertion_mode(hubbub_treebuilder *treebuilder);

static element_type element_type_from_name(hubbub_treebuilder *treebuilder,
		const hubbub_string *tag_name);

static inline bool is_special_element(element_type type);
static inline bool is_scoping_element(element_type type);
static inline bool is_formatting_element(element_type type);
static inline bool is_phrasing_element(element_type type);

static bool element_stack_push(hubbub_treebuilder *treebuilder,
		element_type type, void *node);
static bool element_stack_pop(hubbub_treebuilder *treebuilder,
		element_type *type, void **node);

/*static*/ bool formatting_list_insert(hubbub_treebuilder *treebuilder,
		element_type type, void *node, uint32_t stack_index);
static bool formatting_list_remove(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type *type, void **node, uint32_t *stack_index);
static bool formatting_list_replace(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type type, void *node, uint32_t stack_index,
		element_type *otype, void **onode, uint32_t *ostack_index);

/**
 * Create a hubbub treebuilder 
 *
 * \param tokeniser  Underlying tokeniser instance
 * \param alloc      Memory (de)allocation function
 * \param pw         Pointer to client-specific private data
 * \return Pointer to treebuilder instance, or NULL on error.
 */
hubbub_treebuilder *hubbub_treebuilder_create(hubbub_tokeniser *tokeniser,
		hubbub_alloc alloc, void *pw)
{
	hubbub_treebuilder *tb;
	hubbub_tokeniser_optparams tokparams;

	if (tokeniser == NULL || alloc == NULL)
		return NULL;

	tb = alloc(NULL, sizeof(hubbub_treebuilder), pw);
	if (tb == NULL)
		return NULL;

	tb->tokeniser = tokeniser;

	tb->input_buffer = NULL;
	tb->input_buffer_len = 0;

	tb->tree_handler = NULL;

	memset(&tb->context, 0, sizeof(hubbub_treebuilder_context));
	tb->context.mode = INITIAL;

	tb->context.element_stack = alloc(NULL,
			ELEMENT_STACK_CHUNK * sizeof(element_context),
			pw);
	if (tb->context.element_stack == NULL) {
		alloc(tb, 0, pw);
		return NULL;
	}
	tb->context.stack_alloc = ELEMENT_STACK_CHUNK;
	/* We rely on HTML not being equal to zero to determine 
	 * if the first item in the stack is in use. Assert this here. */
	assert(HTML != 0);
	tb->context.element_stack[0].type = 0;

	tb->buffer_handler = NULL;
	tb->buffer_pw = NULL;

	tb->error_handler = NULL;
	tb->error_pw = NULL;

	tb->alloc = alloc;
	tb->alloc_pw = pw;

	tokparams.token_handler.handler = hubbub_treebuilder_token_handler;
	tokparams.token_handler.pw = tb;

	if (hubbub_tokeniser_setopt(tokeniser, HUBBUB_TOKENISER_TOKEN_HANDLER,
			&tokparams) != HUBBUB_OK) {
		alloc(tb->context.element_stack, 0, pw);
		alloc(tb, 0, pw);
		return NULL;
	}

	tokparams.buffer_handler.handler = hubbub_treebuilder_buffer_handler;
	tokparams.buffer_handler.pw = tb;

	if (hubbub_tokeniser_setopt(tokeniser, HUBBUB_TOKENISER_BUFFER_HANDLER,
			&tokparams) != HUBBUB_OK) {
		alloc(tb->context.element_stack, 0, pw);
		alloc(tb, 0, pw);
		return NULL;
	}

	return tb;	
}

/**
 * Destroy a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to destroy
 */
void hubbub_treebuilder_destroy(hubbub_treebuilder *treebuilder)
{
	formatting_list_entry *entry, *next;
	hubbub_tokeniser_optparams tokparams;

	if (treebuilder == NULL)
		return;

	tokparams.buffer_handler.handler = treebuilder->buffer_handler;
	tokparams.buffer_handler.pw = treebuilder->buffer_pw;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_BUFFER_HANDLER, &tokparams);

	tokparams.token_handler.handler = NULL;
	tokparams.token_handler.pw = NULL;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_TOKEN_HANDLER, &tokparams);

	/* Clean up context */
	if (treebuilder->tree_handler != NULL) {
		if (treebuilder->context.head_element != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.head_element);
		}

		if (treebuilder->context.form_element != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.form_element);
		}

		if (treebuilder->context.document != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.document);
		}

		for (uint32_t n = treebuilder->context.current_node; 
				n > 0; n--) {
			treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[n].node);
		}
		if (treebuilder->context.element_stack[0].type == HTML) {
			treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[0].node);
		}
	}
	treebuilder->alloc(treebuilder->context.element_stack, 0, 
			treebuilder->alloc_pw);
	treebuilder->context.element_stack = NULL;

	for (entry = treebuilder->context.formatting_list; entry != NULL;
			entry = next) {
		next = entry->next;

		if (treebuilder->tree_handler != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					entry->details.node);
		}

		treebuilder->alloc(entry, 0, treebuilder->alloc_pw);
	}

	treebuilder->alloc(treebuilder, 0, treebuilder->alloc_pw);
}

/**
 * Configure a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to configure
 * \param type         The option type to configure
 * \param params       Pointer to option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error hubbub_treebuilder_setopt(hubbub_treebuilder *treebuilder,
		hubbub_treebuilder_opttype type,
		hubbub_treebuilder_optparams *params)
{
	if (treebuilder == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_TREEBUILDER_BUFFER_HANDLER:
		treebuilder->buffer_handler = params->buffer_handler.handler;
		treebuilder->buffer_pw = params->buffer_handler.pw;
		treebuilder->buffer_handler(treebuilder->input_buffer,
				treebuilder->input_buffer_len,
				treebuilder->buffer_pw);
		break;
	case HUBBUB_TREEBUILDER_ERROR_HANDLER:
		treebuilder->error_handler = params->error_handler.handler;
		treebuilder->error_pw = params->error_handler.pw;
		break;
	case HUBBUB_TREEBUILDER_TREE_HANDLER:
		treebuilder->tree_handler = params->tree_handler;
		break;
	case HUBBUB_TREEBUILDER_DOCUMENT_NODE:
		treebuilder->context.document = params->document_node;
		break;
	}

	return HUBBUB_OK;
}

/**
 * Handle tokeniser buffer moving
 *
 * \param data  New location of buffer
 * \param len   Length of buffer in bytes
 * \param pw    Pointer to treebuilder instance
 */
void hubbub_treebuilder_buffer_handler(const uint8_t *data,
		size_t len, void *pw)
{
	hubbub_treebuilder *treebuilder = (hubbub_treebuilder *) pw;

	treebuilder->input_buffer = data;
	treebuilder->input_buffer_len = len;

	/* Inform client buffer handler, too (if there is one) */
	if (treebuilder->buffer_handler != NULL) {
		treebuilder->buffer_handler(treebuilder->input_buffer,
				treebuilder->input_buffer_len,
				treebuilder->buffer_pw);
	}
}

/**
 * Handle tokeniser emitting a token
 *
 * \param token  The emitted token
 * \param pw     Pointer to treebuilder instance
 */
void hubbub_treebuilder_token_handler(const hubbub_token *token, 
		void *pw)
{
	hubbub_treebuilder *treebuilder = (hubbub_treebuilder *) pw;
	bool reprocess = true;

	/* Do nothing if we have no document node or there's no tree handler */
	if (treebuilder->context.document == NULL ||
			treebuilder->tree_handler == NULL)
		return;

	while (reprocess == true) {
		switch (treebuilder->context.mode) {
		case INITIAL:
			reprocess = handle_initial(treebuilder, token);
			break;
		case BEFORE_HTML:
		case BEFORE_HEAD:
		case IN_HEAD:
		case IN_HEAD_NOSCRIPT:
		case AFTER_HEAD:
		case IN_BODY:
		case IN_TABLE:
		case IN_CAPTION:
		case IN_COLUMN_GROUP:
		case IN_TABLE_BODY:
		case IN_ROW:
		case IN_CELL:
		case IN_SELECT:
		case IN_SELECT_IN_TABLE:
		case AFTER_BODY:
		case IN_FRAMESET:
		case AFTER_FRAMESET:
		case AFTER_AFTER_BODY:
		case AFTER_AFTER_FRAMESET:
			reprocess = false;
			break;
		}
	}
}

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
	{
		const uint8_t *data = treebuilder->input_buffer + 
				token->data.character.data_off;
		size_t len = token->data.character.len;
		size_t c;

		/** \todo UTF-16 */

		for (c = 0; c < len; c++) {
			if (data[c] != 0x09 && data[c] != 0x0A && 
					data[c] != 0x0B && data[c] != 0x0C &&
					data[c] != 0x20)
				break;
		}
		/* Non-whitespace characters in token, so reprocess */
		if (c != len) {
			/* Update token data to strip leading whitespace */
			((hubbub_token *) token)->data.character.data_off += 
					len - c;
			((hubbub_token *) token)->data.character.len -= c;

			/** \todo parse error */

			treebuilder->tree_handler->set_quirks_mode(
					treebuilder->tree_handler->ctx,
					HUBBUB_QUIRKS_MODE_FULL);

			treebuilder->context.mode = BEFORE_HTML;
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_COMMENT:
	{
		int success;
		void *comment, *appended;

		success = treebuilder->tree_handler->create_comment(
				treebuilder->tree_handler->ctx,
				&token->data.comment, &comment);
		if (success != 0) {
			/** \todo errors */
		}

		/* Append to Document node */
		success = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.document,
				comment, &appended);
		if (success != 0) {
			/** \todo errors */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					comment);
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, appended);
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, comment);
	}
		break;
	case HUBBUB_TOKEN_DOCTYPE:
	{
		int success;
		void *doctype, *appended;

		/** \todo need public and system ids from tokeniser */
		success = treebuilder->tree_handler->create_doctype(
				treebuilder->tree_handler->ctx,
				&token->data.doctype.name,
				NULL, NULL, &doctype);
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

		/** \todo doctype processing */

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
		treebuilder->context.mode = BEFORE_HTML;
		reprocess = true;
		break;
	}

	return reprocess;
}

/**
 * Determine if an element is in (table) scope
 *
 * \param treebuilder  Treebuilder to look in
 * \param type         Element type to find
 * \param in_table     Whether we're looking in table scope
 * \return True iff element is in scope, false otherwise
 */
bool element_in_scope(hubbub_treebuilder *treebuilder,
		element_type type, bool in_table)
{
	uint32_t node;

	if (treebuilder->context.element_stack == NULL)
		return false;

	for (node = treebuilder->context.current_node; node > 0; node --) {
		element_type node_type = 
				treebuilder->context.element_stack[node].type;

		if (node_type == type)
			return true;

		if (node_type == TABLE)
			break;

		/* The list of element types given in the spec here are the
		 * scoping elements excluding TABLE and HTML. TABLE is handled
		 * in the previous conditional and HTML should only occur
		 * as the first node in the stack, which is never processed
		 * in this loop. */
		if (!in_table && is_scoping_element(node_type))
			break;
	}

	return false;
}

/**
 * Reconstruct the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 */
void reconstruct_active_formatting_list(hubbub_treebuilder *treebuilder)
{
	formatting_list_entry *entry;

	if (treebuilder->context.formatting_list == NULL)
		return;

	entry = treebuilder->context.formatting_list_end;

	/* Assumption: HTML and TABLE elements are not inserted into the list */
	if (is_scoping_element(entry->details.type) || entry->stack_index != 0)
		return;

	while (entry->prev != NULL) {
		entry = entry->prev;

		if (is_scoping_element(entry->details.type) ||
				entry->stack_index != 0) {
			entry = entry->next;
			break;
		}
	}

	while (1) {
		int success;
		void *clone, *appended;
		element_type prev_type;
		void *prev_node;
		uint32_t prev_stack_index;

		success = treebuilder->tree_handler->clone_node(
				treebuilder->tree_handler->ctx,
				entry->details.node,
				false,
				&clone);
		if (success != 0) {
			/** \todo handle errors */
			return;
		}

		success = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
				clone,
				&appended);
		if (success != 0) {
			/** \todo handle errors */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					clone);
			return;
		}

		if (element_stack_push(treebuilder,
				entry->details.type, 
				appended) == false) {
			/** \todo handle memory exhaustion */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					appended);
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					clone);
		}

		if (formatting_list_replace(treebuilder, entry, 
				entry->details.type, clone, 
				treebuilder->context.current_node,
				&prev_type, &prev_node, 
				&prev_stack_index) == false) {
			/** \todo handle errors */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					clone);
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				prev_node);

		if (entry->next != NULL)
			entry = entry->next;
	}
}

/**
 * Clear the list of active formatting elements up to the last marker
 *
 * \param treebuilder  The treebuilder instance containing the list
 */
void clear_active_formatting_list_to_marker(hubbub_treebuilder *treebuilder)
{
	formatting_list_entry *entry;
	bool done = false;

	while ((entry = treebuilder->context.formatting_list_end) != NULL) {
		element_type type;
		void *node;
		uint32_t stack_index;

		if (is_scoping_element(entry->details.type))
			done = true;

		if (formatting_list_remove(treebuilder, entry, 
				&type, &node, &stack_index) == false) {
			/** \todo handle errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		if (done == true)
			break;
	}
}

/**
 * Create element and insert it into the DOM, pushing it on the stack
 *
 * \param treebuilder  The treebuilder instance
 * \param tag          The element to insert
 */
void insert_element(hubbub_treebuilder *treebuilder, const hubbub_tag *tag)
{
	int success;
	void *node, *appended;

	success = treebuilder->tree_handler->create_element(
			treebuilder->tree_handler->ctx, tag, &node);
	if (success != 0) {
		/** \todo errors */
	}

	success = treebuilder->tree_handler->append_child(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[
				treebuilder->context.current_node].node,
			node, &appended);
	if (success != 0) {
		/** \todo errors */
	}

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			appended);

	if (element_stack_push(treebuilder, 
			element_type_from_name(treebuilder, &tag->name), 
			node) == false) {
		/** \todo errors */
	}
}

/**
 * Close implied end tags
 *
 * \param treebuilder  The treebuilder instance
 * \param except       Tag type to exclude from processing [DD,DT,LI,P]
 */
void close_implied_end_tags(hubbub_treebuilder *treebuilder, 
		element_type except)
{
	element_type type;

	type = treebuilder->context.element_stack[
			treebuilder->context.current_node].type;
	
	while (type == DD || type == DT || type == LI || type == P) {
		element_type otype;
		void *node;

		if (type == except)
			break;

		if (element_stack_pop(treebuilder, &otype, &node) == false) {
			/** \todo errors */
		}

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		type = treebuilder->context.element_stack[
				treebuilder->context.current_node].type;
	}
}

/**
 * Reset the insertion mode
 *
 * \param treebuilder  The treebuilder to reset
 */
void reset_insertion_mode(hubbub_treebuilder *treebuilder)
{
	uint32_t node;
	element_context *stack = treebuilder->context.element_stack;

	/** \todo fragment parsing algorithm */

	for (node = treebuilder->context.current_node; node > 0; node--) {
		switch (stack[node].type) {
		case SELECT:
			/* fragment case */
			break;
		case TD:
		case TH:
			treebuilder->context.mode = IN_CELL;
			return;
		case TR:
			treebuilder->context.mode = IN_ROW;
			return;
		case TBODY:
		case TFOOT:
		case THEAD:
			treebuilder->context.mode = IN_TABLE_BODY;
			return;
		case CAPTION:
			treebuilder->context.mode = IN_CAPTION;
			return;
		case COLGROUP:
			/* fragment case */
			break;
		case TABLE:
			treebuilder->context.mode = IN_TABLE;
			return;
		case HEAD:
			/* fragment case */
			break;
		case BODY:
			treebuilder->context.mode = IN_BODY;
			return;
		case FRAMESET:
			/* fragment case */
			break;
		case HTML:
			/* fragment case */
			break;
		default:
			break;
		}
	}
}

/**
 * Convert an element name into an element type
 *
 * \param treebuilder  The treebuilder instance
 * \param tag_name     The tag name to consider
 * \return The corresponding element type
 */
element_type element_type_from_name(hubbub_treebuilder *treebuilder,
		const hubbub_string *tag_name)
{
	static const struct {
		const char *name;
		element_type type;
	} name_type_map[] = {
		{ "ADDRESS", ADDRESS },	{ "AREA", AREA },
		{ "BASE", BASE },	{ "BASEFONT", BASEFONT },
		{ "BGSOUND", BGSOUND },	{ "BLOCKQUOTE", BLOCKQUOTE },
		{ "BODY", BODY },	{ "BR", BR }, 
		{ "CENTER", CENTER },	{ "COL", COL },
		{ "COLGROUP", COLGROUP },	{ "DD", DD },
		{ "DIR", DIR },		{ "DIV", DIV },
		{ "DL", DL },		{ "DT", DT },
		{ "EMBED", EMBED },	{ "FIELDSET", FIELDSET },
		{ "FORM", FORM },	{ "FRAME", FRAME },
		{ "FRAMESET", FRAMESET },	{ "H1", H1 },
		{ "H2", H2 },		{ "H3", H3 },
		{ "H4", H4 },		{ "H5", H5 },
		{ "H6", H6 },		{ "HEAD", HEAD },
		{ "HR", HR },		{ "IFRAME", IFRAME },
		{ "IMAGE", IMAGE },	{ "IMG", IMG },
		{ "INPUT", INPUT },	{ "ISINDEX", ISINDEX },
		{ "LI", LI },		{ "LINK", LINK },
		{ "LISTING", LISTING },	{ "MENU", MENU },
		{ "META", META },	{ "NOEMBED", NOEMBED },
		{ "NOFRAMES", NOFRAMES },	{ "NOSCRIPT", NOSCRIPT },
		{ "OL", OL },		{ "OPTGROUP", OPTGROUP },
		{ "OPTION", OPTION },	{ "P", P },
		{ "PARAM", PARAM },	{ "PLAINTEXT", PLAINTEXT },
		{ "PRE", PRE },		{ "SCRIPT", SCRIPT },
		{ "SELECT", SELECT },	{ "SPACER", SPACER },
		{ "STYLE", STYLE }, 	{ "TBODY", TBODY },
		{ "TEXTAREA", TEXTAREA },	{ "TFOOT", TFOOT },
		{ "THEAD", THEAD },	{ "TITLE", TITLE },
		{ "TR", TR },		{ "UL", UL },
		{ "WBR", WBR },
		{ "APPLET", APPLET },	{ "BUTTON", BUTTON },
		{ "CAPTION", CAPTION },	{ "HTML", HTML },
		{ "MARQUEE", MARQUEE },	{ "OBJECT", OBJECT },
		{ "TABLE", TABLE },	{ "TD", TD },
		{ "TH", TH },
		{ "A", A },		{ "B", B },
		{ "BIG", BIG },		{ "EM", EM },
		{ "FONT", FONT },	{ "I", I },
		{ "NOBR", NOBR },	{ "S", S },
		{ "SMALL", SMALL },	{ "STRIKE", STRIKE },
		{ "STRONG", STRONG },	{ "TT", TT },
		{ "U", U },
	};
	const char *data = 
		(const char *) treebuilder->input_buffer + tag_name->data_off;

	/** \todo UTF-16 support */
	/** \todo optimise this */

	for (uint32_t i = 0; 
			i < sizeof(name_type_map) / sizeof(name_type_map[0]);
			i++) {
		if (strlen(name_type_map[i].name) != tag_name->len)
			continue;

		if (strncasecmp(name_type_map[i].name, 
				data, tag_name->len) == 0)
			return name_type_map[i].type;
	}

	/** \todo produce type values for unknown tags */
	return U + 1;
}

/**
 * Determine if a node is a special element
 *
 * \param type  Node type to consider
 * \return True iff node is a special element
 */
inline bool is_special_element(element_type type)
{
	return (type <= WBR);
}

/**
 * Determine if a node is a scoping element
 *
 * \param type  Node type to consider
 * \return True iff node is a scoping element
 */
inline bool is_scoping_element(element_type type)
{
	return (type >= APPLET && type <= TH);
}

/**
 * Determine if a node is a formatting element
 *
 * \param type  Node type to consider
 * \return True iff node is a formatting element
 */
inline bool is_formatting_element(element_type type)
{
	return (type >= A && type <= U);
}

/**
 * Determine if a node is a phrasing element
 *
 * \param type  Node type to consider
 * \return True iff node is a phrasing element
 */
inline bool is_phrasing_element(element_type type)
{
	return (type > U);
}

/**
 * Push an element onto the stack of open elements
 *
 * \param treebuilder  The treebuilder instance containing the stack
 * \param type         The type of element being pushed
 * \param node         The node to push
 * \return True on success, false on memory exhaustion
 */
bool element_stack_push(hubbub_treebuilder *treebuilder,
		element_type type, void *node)
{
	uint32_t slot = treebuilder->context.current_node + 1;

	if (slot > treebuilder->context.stack_alloc) {
		element_context *temp = treebuilder->alloc(
				treebuilder->context.element_stack, 
				(treebuilder->context.stack_alloc + 
					ELEMENT_STACK_CHUNK) * 
					sizeof(element_context), 
				treebuilder->alloc_pw);

		if (temp == NULL)
			return false;

		treebuilder->context.element_stack = temp;
		treebuilder->context.stack_alloc += ELEMENT_STACK_CHUNK;
	}

	treebuilder->context.element_stack[slot].type = type;
	treebuilder->context.element_stack[slot].node = node;

	treebuilder->context.current_node = slot;

	/* Update current table index */
	if (type == TABLE)
		treebuilder->context.current_table = slot;

	return true;
}

/**
 * Pop an element off the stack of open elements
 *
 * \param treebuilder  The treebuilder instance containing the stack
 * \param type         Pointer to location to receive element type
 * \param node         Pointer to location to receive node
 * \return True on success, false on memory exhaustion.
 */
bool element_stack_pop(hubbub_treebuilder *treebuilder,
		element_type *type, void **node)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t slot = treebuilder->context.current_node;
	formatting_list_entry *entry;

	/* We're popping a table, find previous */
	if (stack[slot].type == TABLE) {
		uint32_t t;
		for (t = slot - 1; t > 0; t--) {
			if (stack[t].type == TABLE)
				break;
		}

		treebuilder->context.current_table = t;
	}

	if (is_formatting_element(stack[slot].type) || 
			(is_scoping_element(stack[slot].type) && 
			stack[slot].type != HTML && 
			stack[slot].type != TABLE)) {
		/* Find occurrences of the node we're about to pop in the list 
		 * of active formatting elements. We need to invalidate their 
		 * stack index information. */
		for (entry = treebuilder->context.formatting_list_end; 
				entry != NULL; entry = entry->prev) {
			/** \todo Can we optimise this? 
			 * (i.e. by not traversing the entire list) */
			if (entry->stack_index == slot)
				entry->stack_index = 0;
		}
	}

	*type = stack[slot].type;
	*node = stack[slot].node;

	/** \todo reduce allocated stack size once there's enough free */

	treebuilder->context.current_node = slot - 1;

	return true;
}

/**
 * Insert an element into the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \param type         Type of node being inserted
 * \param node         Node being inserted
 * \param stack_index  Index into stack of open elements
 * \return True on success, false on memory exhaustion
 */
bool formatting_list_insert(hubbub_treebuilder *treebuilder,
		element_type type, void *node, uint32_t stack_index)
{
	formatting_list_entry *entry;

	entry = treebuilder->alloc(NULL, sizeof(formatting_list_entry),
			treebuilder->alloc_pw);
	if (entry == NULL)
		return false;

	entry->details.type = type;
	entry->details.node = node;
	entry->stack_index = stack_index;

	entry->prev = treebuilder->context.formatting_list_end;
	entry->next = NULL;

	if (entry->prev != NULL)
		entry->prev->next = entry;
	else
		treebuilder->context.formatting_list = entry;

	treebuilder->context.formatting_list_end = entry;

	return true;
}

/**
 * Remove an element from the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \param entry        The item to remove
 * \param type         Pointer to location to receive type of node
 * \param node         Pointer to location to receive node
 * \param stack_index  Pointer to location to receive stack index
 * \return True on success, false on memory exhaustion
 */
bool formatting_list_remove(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type *type, void **node, uint32_t *stack_index)
{
	*type = entry->details.type;
	*node = entry->details.node;
	*stack_index = entry->stack_index;

	if (entry->prev == NULL)
		treebuilder->context.formatting_list = entry->next;
	else
		entry->prev->next = entry->next;

	if (entry->next == NULL)
		treebuilder->context.formatting_list_end = entry->prev;
	else
		entry->next->prev = entry->prev;

	treebuilder->alloc(entry, 0, treebuilder->alloc_pw);

	return true;
}

/**
 * Remove an element from the list of active formatting elements
 *
 * \param treebuilder   Treebuilder instance containing list
 * \param entry         The item to replace
 * \param type          Replacement node type
 * \param node          Replacement node
 * \param stack_index   Replacement stack index
 * \param otype         Pointer to location to receive old type
 * \param onode         Pointer to location to receive old node
 * \param ostack_index  Pointer to location to receive old stack index
 * \return True on success, false on memory exhaustion
 */
bool formatting_list_replace(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type type, void *node, uint32_t stack_index,
		element_type *otype, void **onode, uint32_t *ostack_index)
{
	UNUSED(treebuilder);

	*otype = entry->details.type;
	*onode = entry->details.node;
	*ostack_index = entry->stack_index;

	entry->details.type = type;
	entry->details.node = node;
	entry->stack_index = stack_index;

	return true;
}

