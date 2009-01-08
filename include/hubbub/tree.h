/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_tree_h_
#define hubbub_tree_h_

#include <hubbub/functypes.h>

/**
 * Hubbub tree handler
 */
typedef struct hubbub_tree_handler {
	hubbub_tree_create_comment create_comment;	/**< Create comment */
	hubbub_tree_create_doctype create_doctype;	/**< Create doctype */
	hubbub_tree_create_element create_element;	/**< Create element */
	hubbub_tree_create_text create_text;		/**< Create text */
	hubbub_tree_ref_node ref_node;			/**< Reference node */
	hubbub_tree_unref_node unref_node;		/**< Unreference node */
	hubbub_tree_append_child append_child;		/**< Append child */
	hubbub_tree_insert_before insert_before;	/**< Insert before */
	hubbub_tree_remove_child remove_child;		/**< Remove child */
	hubbub_tree_clone_node clone_node;		/**< Clone node */
	hubbub_tree_reparent_children reparent_children;/**< Reparent children*/
	hubbub_tree_get_parent get_parent;		/**< Get parent */
	hubbub_tree_has_children has_children;		/**< Has children? */
	hubbub_tree_form_associate form_associate;	/**< Form associate */
	hubbub_tree_add_attributes add_attributes;	/**< Add attributes */
	hubbub_tree_set_quirks_mode set_quirks_mode;	/**< Set quirks mode */
	hubbub_tree_encoding_change encoding_change;	/**< Change encoding */
	void *ctx;					/**< Context pointer */
} hubbub_tree_handler;

#endif

