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
	hubbub_tree_create_comment create_comment;
	hubbub_tree_create_doctype create_doctype;
	hubbub_tree_create_element create_element;
	hubbub_tree_create_text create_text;
	hubbub_tree_ref_node ref_node;
	hubbub_tree_unref_node unref_node;
	hubbub_tree_append_child append_child;
	hubbub_tree_insert_before insert_before;
	hubbub_tree_remove_child remove_child;
	hubbub_tree_clone_node clone_node;
	hubbub_tree_reparent_children reparent_children;
	hubbub_tree_get_parent get_parent;
	hubbub_tree_has_children has_children;
	hubbub_tree_form_associate form_associate;
	hubbub_tree_add_attributes add_attributes;
	hubbub_tree_set_quirks_mode set_quirks_mode;
	void *ctx;
} hubbub_tree_handler;

#endif

