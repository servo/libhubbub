/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdbool.h>

#include "utils/dict.h"

/** Node in a dictionary tree */
typedef struct hubbub_dict_node {
	uint8_t split;			/**< Data to split on */
	struct hubbub_dict_node *lt;	/**< Subtree for data less than
					 * split */
	struct hubbub_dict_node *eq;	/**< Subtree for data equal to split */
	struct hubbub_dict_node *gt;	/**< Subtree for data greater than
					 * split */

	const void *value;		/**< Data for this node */
} hubbub_dict_node;

/** Dictionary object */
struct hubbub_dict {
	hubbub_dict_node *dict;		/**< Root of tree */

	hubbub_allocator_fn alloc;	/**< Memory (de)allocation function */
	void *pw;			/**< Pointer to client data */
};

static void hubbub_dict_destroy_internal(hubbub_dict *dict,
		hubbub_dict_node *root);
static hubbub_dict_node *hubbub_dict_insert_internal(hubbub_dict *dict,
		hubbub_dict_node *parent, const char *key,
		const void *value);


/**
 * Create a dictionary
 *
 * \param alloc  Memory (de)allocation function
 * \param pw     Pointer to client-specific private data (may be NULL)
 * \param dict   Pointer to location to receive dictionary instance
 * \return HUBBUB_OK on success,
 *         HUBBUB_BADPARM on bad parameters,
 *         HUBBUB_NOMEM on memory exhaustion
 */
hubbub_error hubbub_dict_create(hubbub_allocator_fn alloc, void *pw, 
		hubbub_dict **dict)
{
	hubbub_dict *d;

	if (alloc == NULL || dict == NULL)
		return HUBBUB_BADPARM;

	d = alloc(NULL, sizeof(hubbub_dict), pw);
	if (d == NULL)
		return HUBBUB_NOMEM;

	d->dict = NULL;

	d->alloc = alloc;
	d->pw = pw;

	*dict = d;

	return HUBBUB_OK;
}

/**
 * Destroy a dictionary
 *
 * \param dict  Dictionary to destroy
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_dict_destroy(hubbub_dict *dict)
{
	if (dict == NULL)
		return HUBBUB_BADPARM;

	hubbub_dict_destroy_internal(dict, dict->dict);

	dict->alloc(dict, 0, dict->pw);

	return HUBBUB_OK;
}

/**
 * Helper routine for dictionary destruction
 *
 * \param dict  Dictionary being destroyed
 * \param root  Root node of dictionary (sub)tree to destroy
 */
void hubbub_dict_destroy_internal(hubbub_dict *dict, hubbub_dict_node *root)
{
	if (root == NULL)
		return;

	hubbub_dict_destroy_internal(dict, root->lt);
	if (root->split != '\0')
		hubbub_dict_destroy_internal(dict, root->eq);
	hubbub_dict_destroy_internal(dict, root->gt);

	dict->alloc(root, 0, dict->pw);
}

/**
 * Insert a key-value pair into a dictionary
 *
 * \param dict   Dictionary to insert into
 * \param key    Key string
 * \param value  Value to associate with key (may be NULL)
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_dict_insert(hubbub_dict *dict, const char *key,
		const void *value)
{
	if (dict == NULL || key == NULL)
		return HUBBUB_BADPARM;

	dict->dict = hubbub_dict_insert_internal(dict, dict->dict,
			key, value);

	return HUBBUB_OK;
}

/**
 * Helper routine for insertion into dictionary
 *
 * \param dict    Dictionary being inserted into
 * \param parent  Parent node of subtree to insert into
 * \param key     Key string
 * \param value   Value to associate with key
 * \return Pointer to root of tree created
 */
hubbub_dict_node *hubbub_dict_insert_internal(hubbub_dict *dict,
		hubbub_dict_node *parent, const char *key, const void *value)
{
	if (parent == NULL) {
		parent = dict->alloc(NULL,
				sizeof(hubbub_dict_node), dict->pw);
		if (parent == NULL)
			return NULL;
		parent->split = (uint8_t) key[0];
		parent->lt = parent->eq = parent->gt = NULL;
		parent->value = NULL;
	}

	if ((uint8_t) key[0] < parent->split) {
		parent->lt = hubbub_dict_insert_internal(dict,
				parent->lt, key, value);
	} else if ((uint8_t) key[0] == parent->split) {
		if (key[0] == '\0') {
			parent->value = value;
		} else if (key[1] == '\0') {
			parent->value = value;
			parent->eq = hubbub_dict_insert_internal(dict,
					parent->eq, key + 1, value);
		} else {
			parent->eq = hubbub_dict_insert_internal(dict,
					parent->eq, key + 1, value);
		}
	} else  {
		parent->gt = hubbub_dict_insert_internal(dict,
				parent->gt, key, value);
	}

	return parent;
}

/**
 * Step-wise search for a key in a dictionary
 *
 * \param dict     Dictionary to search
 * \param c        Character to look for
 * \param result   Pointer to location for result
 * \param context  Pointer to location for search context
 * \return HUBBUB_OK if key found,
 *         HUBBUB_NEEDDATA if more steps are required
 *         HUBBUB_INVALID if nothing matches
 *
 * The value pointed to by ::context must be NULL for the first call.
 * Thereafter, pass in the same value as returned by the previous call.
 * The context is opaque to the caller and should not be inspected.
 *
 * The location pointed to by ::result will be set to NULL unless a match
 * is found.
 */
hubbub_error hubbub_dict_search_step(hubbub_dict *dict, uint8_t c,
		const void **result, void **context)
{
	bool match = false;
	hubbub_dict_node *p;

	if (dict == NULL || result == NULL || context == NULL)
		return HUBBUB_BADPARM;

	*result = NULL;

	if (*context == NULL) {
		p = dict->dict;
	} else {
		p = (hubbub_dict_node *) *context;
	}

	while (p != NULL) {
		if (c < p->split) {
			p = p->lt;
		} else if (c == p->split) {
			if (p->split == '\0') {
				match = true;
				p = NULL;
			} else if (p->eq != NULL && p->eq->split == '\0') {
				match = true;
				*result = p->eq->value;
				p = p->eq;
			} else if (p->value) {
				match = true;
				*result = p->value;
				p = p->eq;
			} else {
				p = p->eq;
			}

			break;
		} else {
			p = p->gt;
		}
	}

	*context = (void *) p;

	return (match) ? HUBBUB_OK :
			(p == NULL) ? HUBBUB_INVALID : HUBBUB_NEEDDATA;
}
