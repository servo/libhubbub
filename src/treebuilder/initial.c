/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * Up-to-date with 19 June 2008 spec.
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"
#include "utils/string.h"


#define S(s)	{ s, sizeof s }

struct {
	const char *name;
	size_t len;
} public_doctypes[] = {
	S("+//Silmaril//dtd html Pro v0r11 19970101//"),
	S("-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//"),
	S("-//AS//DTD HTML 3.0 asWedit + extensions//"),
	S("-//IETF//DTD HTML 2.0 Level 1//"),
	S("-//IETF//DTD HTML 2.0 Level 2//"),
	S("-//IETF//DTD HTML 2.0 Strict Level 1//"),
	S("-//IETF//DTD HTML 2.0 Strict Level 2//"),
	S("-//IETF//DTD HTML 2.0 Strict//"),
	S("-//IETF//DTD HTML 2.0//"),
	S("-//IETF//DTD HTML 2.1E//"),
	S("-//IETF//DTD HTML 3.0//"),
	S("-//IETF//DTD HTML 3.2 Final//"),
	S("-//IETF//DTD HTML 3.2//"),
	S("-//IETF//DTD HTML 3//"),
	S("-//IETF//DTD HTML Level 0//"),
	S("-//IETF//DTD HTML Level 1//"),
	S("-//IETF//DTD HTML Level 2//"),
	S("-//IETF//DTD HTML Level 3//"),
	S("-//IETF//DTD HTML Strict Level 0//"),
	S("-//IETF//DTD HTML Strict Level 1//"),
	S("-//IETF//DTD HTML Strict Level 2//"),
	S("-//IETF//DTD HTML Strict Level 3//"),
	S("-//IETF//DTD HTML Strict//"),
	S("-//IETF//DTD HTML//"),
	S("-//Metrius//DTD Metrius Presentational//"),
	S("-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//"),
	S("-//Microsoft//DTD Internet Explorer 2.0 HTML//"),
	S("-//Microsoft//DTD Internet Explorer 2.0 Tables//"),
	S("-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//"),
	S("-//Microsoft//DTD Internet Explorer 3.0 HTML//"),
	S("-//Microsoft//DTD Internet Explorer 3.0 Tables//"),
	S("-//Netscape Comm. Corp.//DTD HTML//"),
	S("-//Netscape Comm. Corp.//DTD Strict HTML//"),
	S("-//O'Reilly and Associates//DTD HTML 2.0//"),
	S("-//O'Reilly and Associates//DTD HTML Extended 1.0//"),
	S("-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//"),
	S("-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//"),
	S("-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//"),
	S("-//Spyglass//DTD HTML 2.0 Extended//"),
	S("-//SQ//DTD HTML 2.0 HoTMetaL + extensions//"),
	S("-//Sun Microsystems Corp.//DTD HotJava HTML//"),
	S("-//Sun Microsystems Corp.//DTD HotJava Strict HTML//"),
	S("-//W3C//DTD HTML 3 1995-03-24//"),
	S("-//W3C//DTD HTML 3.2 Draft//"),
	S("-//W3C//DTD HTML 3.2 Final//"),
	S("-//W3C//DTD HTML 3.2//"),
	S("-//W3C//DTD HTML 3.2S Draft//"),
	S("-//W3C//DTD HTML 4.0 Frameset//"),
	S("-//W3C//DTD HTML 4.0 Transitional//"),
	S("-//W3C//DTD HTML Experimental 19960712//"),
	S("-//W3C//DTD HTML Experimental 970421//"),
	S("-//W3C//DTD W3 HTML//"),
	S("-//W3O//DTD W3 HTML 3.0//"),
};

#undef S


/**
 * Check if one string starts with another.
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
static bool starts_with(const uint8_t *a, size_t a_len, const uint8_t *b,
		size_t b_len)
{
	uint8_t z1, z2;

	if (a_len < b_len)
		return false;

	for (const uint8_t *s1 = a, *s2 = b; b_len > 0; s1++, s2++, b_len--)
	{
		z1 = (*s1 & ~0x20);
		z2 = (*s2 & ~0x20);
		if (z1 != z2) return false;
		if (!z1) return true;
	}

	return true;
}


/**
 * Determine whether this doctype triggers full quirks mode
 *
 * \param cdoc        The doctype to examine
 * \return True to trigger quirks, false otherwise
 */
static bool lookup_full_quirks(hubbub_treebuilder *treebuilder,
		const hubbub_doctype *cdoc)
{
	size_t i;

	const uint8_t *name = treebuilder->input_buffer + cdoc->name.data.off;
	size_t name_len = cdoc->name.len;

	const uint8_t *public_id = treebuilder->input_buffer +
			cdoc->public_id.data.off;
	size_t public_id_len = cdoc->public_id.len;

	const uint8_t *system_id = treebuilder->input_buffer +
			cdoc->system_id.data.off;
	size_t system_id_len = cdoc->system_id.len;

#define S(s)	(uint8_t *) s, sizeof s

	/* Check the name is "HTML" (case-insensitively) */
	if (!hubbub_string_match_ci(name, name_len, S("HTML")))
		return true;

	/* No public id means not-quirks */
	if (cdoc->public_missing) return false;

	for (i = 0; i < sizeof public_doctypes / sizeof public_doctypes[0]; i++)
	{
		if (starts_with(public_id, public_id_len,
				(uint8_t *) public_doctypes[i].name,
				public_doctypes[i].len)) {
			return true;
		}
	}

	if (hubbub_string_match_ci(public_id, public_id_len,
					S("-//W3O//DTD W3 HTML Strict 3.0//EN//")) ||
			hubbub_string_match_ci(public_id, public_id_len,
					S("-/W3C/DTD HTML 4.0 Transitional/EN")) ||
			hubbub_string_match_ci(public_id, public_id_len,
					S("HTML")) ||
			hubbub_string_match_ci(system_id, system_id_len,
					S("http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"))) {
		return true;
	}

	if (cdoc->system_missing == true &&
			(starts_with(public_id, public_id_len,
					S("-//W3C//DTD HTML 4.01 Frameset//")) ||
			starts_with(public_id, public_id_len,
					S("-//W3C//DTD HTML 4.01 Transitional//")))) {
		return true;
	}

#undef S

	return false;
}


/**
 * Determine whether this doctype triggers limited quirks mode
 *
 * \param cdoc        The doctype to examine
 * \return True to trigger quirks, false otherwise
 */
static bool lookup_limited_quirks(hubbub_treebuilder *treebuilder,
		const hubbub_doctype *cdoc)
{
	const uint8_t *public_id = treebuilder->input_buffer +
			cdoc->public_id.data.off;
	size_t public_id_len = cdoc->public_id.len;

#define S(s)	(uint8_t *) s, sizeof s

	if (starts_with(public_id, public_id_len,
					S("-//W3C//DTD XHTML 1.0 Frameset//")) ||
			starts_with(public_id, public_id_len,
					S("-//W3C//DTD XHTML 1.0 Transitional//"))) {
		return true;
	}

	if (cdoc->system_missing == false &&
			(starts_with(public_id, public_id_len,
					S("-//W3C//DTD HTML 4.01 Frameset//")) ||
			starts_with(public_id, public_id_len,
					S("-//W3C//DTD HTML 4.01 Transitional//")))) {
		return true;
	}

#undef S

	return false;
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
		if (process_characters_expect_whitespace(treebuilder, token,
				false)) {
			/** \todo parse error */

			treebuilder->tree_handler->set_quirks_mode(
					treebuilder->tree_handler->ctx,
					HUBBUB_QUIRKS_MODE_FULL);
			treebuilder->context.mode = BEFORE_HTML;
			reprocess = true;
		}
		break;
	case HUBBUB_TOKEN_COMMENT:
		process_comment_append(treebuilder, token,
				treebuilder->context.document);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
	{
		int success;
		void *doctype, *appended;

		/** \todo parse error */

		/** \todo need public and system ids from tokeniser */
		success = treebuilder->tree_handler->create_doctype(
				treebuilder->tree_handler->ctx,
				&token->data.doctype,
				&doctype);
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

		const hubbub_doctype *cdoc = &token->data.doctype;

		/* Work out whether we need quirks mode or not */
		if (cdoc->force_quirks == true ||
				lookup_full_quirks(treebuilder, cdoc)) {
			treebuilder->tree_handler->set_quirks_mode(
					treebuilder->tree_handler->ctx,
					HUBBUB_QUIRKS_MODE_FULL);
		} else if (lookup_limited_quirks(treebuilder, cdoc)) {
			treebuilder->tree_handler->set_quirks_mode(
					treebuilder->tree_handler->ctx,
					HUBBUB_QUIRKS_MODE_LIMITED);
		}

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
		reprocess = true;
		break;
	}

	if (reprocess) {
		treebuilder->context.mode = BEFORE_HTML;
	}

	return reprocess;
}

