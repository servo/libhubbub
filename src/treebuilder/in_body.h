/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_treebuilder_in_body_h_
#define hubbub_treebuilder_in_body_h_

#include "treebuilder/internal.h"

bool handle_in_body(hubbub_treebuilder *treebuilder, const hubbub_token *token);
bool process_tag_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token);

#endif

