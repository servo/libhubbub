/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_treebuilder_internal_h_
#define hubbub_treebuilder_internal_h_

#include "treebuilder/treebuilder.h"

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
	GENERIC_RCDATA,
	SCRIPT_COLLECT_CHARACTERS,
} insertion_mode;

typedef enum
{
/* Special */
	ADDRESS, AREA, ARTICLE, ASIDE, BASE, BASEFONT, BGSOUND, BLOCKQUOTE,
	BODY, BR, CENTER, COL, COLGROUP, COMMAND, DATAGRID, DD, DETAILS,
	DIALOG, DIR, DIV, DL, DT, EMBED, EVENT_SOURCE, FIELDSET, FIGURE,
	FOOTER, FORM, FRAME, FRAMESET, H1, H2, H3, H4, H5, H6, HEAD, HEADER,
	HR, IFRAME, IMAGE, IMG, INPUT, ISINDEX, LI, LINK, LISTING, MENU, META,
	NAV, NOEMBED, NOFRAMES, NOSCRIPT, OL, OPTGROUP, OPTION, P, PARAM,
	PLAINTEXT, PRE, SCRIPT, SECTION, SELECT, SPACER, STYLE, TBODY,
	TEXTAREA, TFOOT, THEAD, TITLE, TR, UL, WBR,
/* Scoping */
	APPLET, BUTTON, CAPTION, HTML, MARQUEE, OBJECT, TABLE, TD, TH,
/* Formatting */
	A, B, BIG, EM, FONT, I, NOBR, S, SMALL, STRIKE, STRONG, TT, U,
/* Phrasing */
	/**< \todo Enumerate phrasing elements */
	LABEL, MATH, RP, RT, XMP,
	UNKNOWN,
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

	struct {
		insertion_mode mode;	/**< Insertion mode to return to */
		void *node;		/**< Node to attach Text child to */
		element_type type;	/**< Type of node */
		hubbub_string string;	/**< Text data */
	} collect;			/**< Context for character collecting */

	bool strip_leading_lr;		/**< Whether to strip a LR from the 
					 * start of the next character sequence
					 * received */
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

bool process_characters_expect_whitespace(
		hubbub_treebuilder *treebuilder, const hubbub_token *token,
		bool insert_into_current_node);
void process_comment_append(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, void *parent);
void parse_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, bool rcdata);
void process_base_link_meta_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type);
void process_script_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

uint32_t element_in_scope(hubbub_treebuilder *treebuilder,
		element_type type, bool in_table);
void reconstruct_active_formatting_list(hubbub_treebuilder *treebuilder);
void clear_active_formatting_list_to_marker(
		hubbub_treebuilder *treebuilder);
void insert_element(hubbub_treebuilder *treebuilder, 
		const hubbub_tag *tag_name);
void insert_element_no_push(hubbub_treebuilder *treebuilder,
		const hubbub_tag *tag_name);
void close_implied_end_tags(hubbub_treebuilder *treebuilder, 
		element_type except);
void reset_insertion_mode(hubbub_treebuilder *treebuilder);
void append_text(hubbub_treebuilder *treebuilder,
		const hubbub_string *string);

element_type element_type_from_name(hubbub_treebuilder *treebuilder,
		const hubbub_string *tag_name);

bool is_special_element(element_type type);
bool is_scoping_element(element_type type);
bool is_formatting_element(element_type type);
bool is_phrasing_element(element_type type);

bool element_stack_push(hubbub_treebuilder *treebuilder,
		element_type type, void *node);
bool element_stack_pop(hubbub_treebuilder *treebuilder,
		element_type *type, void **node);

bool formatting_list_append(hubbub_treebuilder *treebuilder,
		element_type type, void *node, uint32_t stack_index);
bool formatting_list_insert(hubbub_treebuilder *treebuilder,
		formatting_list_entry *prev, formatting_list_entry *next,
		element_type type, void *node, uint32_t stack_index);
bool formatting_list_remove(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type *type, void **node, uint32_t *stack_index);
bool formatting_list_replace(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		element_type type, void *node, uint32_t stack_index,
		element_type *otype, void **onode, uint32_t *ostack_index);

#ifndef NDEBUG
#include <stdio.h>

void element_stack_dump(hubbub_treebuilder *treebuilder, FILE *fp);
void formatting_list_dump(hubbub_treebuilder *treebuilder, FILE *fp);
#endif

#endif

