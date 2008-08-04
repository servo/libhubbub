/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org>
 */
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <stdio.h>

#include <parserutils/charset/utf8.h>

#include "utils/utils.h"

#include "tokeniser/entities.h"
#include "tokeniser/tokeniser.h"

/**
 * Table of mappings between Windows-1252 codepoints 128-159 and UCS4
 */
static const uint32_t cp1252Table[32] = {
	0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
};

/**
 * UTF-8 encoding of U+FFFD REPLACEMENT CHARACTER
 */
static const uint8_t u_fffd[3] = { '\xEF', '\xBF', '\xBD' };
static const hubbub_string u_fffd_str = { u_fffd, sizeof(u_fffd) };


/**
 * String for when we want to emit newlines
 */
static const uint8_t lf = '\n';
static const hubbub_string lf_str = { &lf, 1 };


/**
 * Tokeniser states
 */
typedef enum hubbub_tokeniser_state {
	STATE_DATA,
	STATE_CHARACTER_REFERENCE_DATA,
	STATE_TAG_OPEN,
	STATE_CLOSE_TAG_OPEN,
	STATE_TAG_NAME,
	STATE_BEFORE_ATTRIBUTE_NAME,
	STATE_ATTRIBUTE_NAME,
	STATE_AFTER_ATTRIBUTE_NAME,
	STATE_BEFORE_ATTRIBUTE_VALUE,
	STATE_ATTRIBUTE_VALUE_DQ,
	STATE_ATTRIBUTE_VALUE_SQ,
	STATE_ATTRIBUTE_VALUE_UQ,
	STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE,
	STATE_AFTER_ATTRIBUTE_VALUE_Q,
	STATE_SELF_CLOSING_START_TAG,
	STATE_BOGUS_COMMENT,
	STATE_MARKUP_DECLARATION_OPEN,
	STATE_MATCH_COMMENT,
	STATE_COMMENT_START,
	STATE_COMMENT_START_DASH,
	STATE_COMMENT,
	STATE_COMMENT_END_DASH,
	STATE_COMMENT_END,
	STATE_MATCH_DOCTYPE,
	STATE_DOCTYPE,
	STATE_BEFORE_DOCTYPE_NAME,
	STATE_DOCTYPE_NAME,
	STATE_AFTER_DOCTYPE_NAME,
	STATE_MATCH_PUBLIC,
	STATE_BEFORE_DOCTYPE_PUBLIC,
	STATE_DOCTYPE_PUBLIC_DQ,
	STATE_DOCTYPE_PUBLIC_SQ,
	STATE_AFTER_DOCTYPE_PUBLIC,
	STATE_MATCH_SYSTEM,
	STATE_BEFORE_DOCTYPE_SYSTEM,
	STATE_DOCTYPE_SYSTEM_DQ,
	STATE_DOCTYPE_SYSTEM_SQ,
	STATE_AFTER_DOCTYPE_SYSTEM,
	STATE_BOGUS_DOCTYPE,
	STATE_MATCH_CDATA,
	STATE_CDATA_BLOCK,
	STATE_NUMBERED_ENTITY,
	STATE_NAMED_ENTITY
} hubbub_tokeniser_state;

/**
 * Context for tokeniser
 */
typedef struct hubbub_tokeniser_context {
	size_t pending;				/**< Count of pending chars */

	hubbub_string current_comment;		/**< Current comment text */

	hubbub_token_type current_tag_type;	/**< Type of current_tag */
	hubbub_tag current_tag;			/**< Current tag */
	hubbub_doctype current_doctype;		/**< Current doctype */
	hubbub_tokeniser_state prev_state;	/**< Previous state */

	uint8_t last_start_tag_name[10];	/**< Name of the last start tag
						 * emitted */
	size_t last_start_tag_len;

	struct {
		uint32_t count;
		bool match;
	} close_tag_match;

	struct {
		uint32_t count;			/**< Index into "DOCTYPE" */
	} match_doctype;

	struct {
		uint32_t count;			/**< Index into "[CDATA[" */
		uint32_t end;			/**< Index into "]]>" */
	} match_cdata;

	struct {
		size_t offset;			/**< Offset in buffer */
		uint32_t length;		/**< Length of entity */
		uint32_t codepoint;		/**< UCS4 codepoint */
		bool complete;			/**< True if match complete */

		uint32_t poss_length;		/**< Optimistic length
						 * when matching named
						 * character references */
		uint8_t base;			/**< Base for numeric
						 * entities */
		void *context;			/**< Context for named
						 * entity search */
		size_t prev_len;		/**< Previous byte length
						 * of str */
		bool had_data;			/**< Whether we read
						 * anything after &#(x)? */
		bool overflow;			/**< Whether this entity has
						 * has overflowed the maximum
						 * numeric entity value */
		hubbub_tokeniser_state return_state;	/**< State we were
							 * called from */
	} match_entity;

	struct {
		uint32_t line;			/**< Current line of input */
		uint32_t col;			/**< Current character in
						 * line */
	} position;

	uint32_t allowed_char;
} hubbub_tokeniser_context;

/**
 * Tokeniser data structure
 */
struct hubbub_tokeniser {
	hubbub_tokeniser_state state;	/**< Current tokeniser state */
	hubbub_content_model content_model;	/**< Current content
						 * model flag */
	bool escape_flag;		/**< Escape flag **/
	bool process_cdata_section;

	parserutils_inputstream *input;	/**< Input stream */
	parserutils_buffer *buffer;	/**< Input buffer */

	hubbub_tokeniser_context context;	/**< Tokeniser context */

	hubbub_token_handler token_handler;
	void *token_pw;

	hubbub_error_handler error_handler;
	void *error_pw;

	hubbub_alloc alloc;		/**< Memory (de)allocation function */
	void *alloc_pw;			/**< Client private data */
};

static bool hubbub_tokeniser_handle_data(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_character_reference_data(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_tag_open(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_close_tag_open(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_tag_name(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_before_attribute_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_attribute_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_after_attribute_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_before_attribute_value(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_attribute_value_dq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_attribute_value_sq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_attribute_value_uq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_character_reference_in_attribute_value(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_after_attribute_value_q(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_self_closing_start_tag(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_bogus_comment(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_markup_declaration_open(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_match_comment(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_comment(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_match_doctype(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_before_doctype_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_after_doctype_name(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_match_public(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_before_doctype_public(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype_public_dq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype_public_sq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_after_doctype_public(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_match_system(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_before_doctype_system(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype_system_dq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_doctype_system_sq(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_after_doctype_system(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_bogus_doctype(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_match_cdata(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_cdata_block(hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_consume_character_reference(
		hubbub_tokeniser *tokeniser, size_t off);
static bool hubbub_tokeniser_handle_numbered_entity(
		hubbub_tokeniser *tokeniser);
static bool hubbub_tokeniser_handle_named_entity(
		hubbub_tokeniser *tokeniser);

static inline bool emit_character_token(hubbub_tokeniser *tokeniser,
		const hubbub_string *chars);
static inline bool emit_current_chars(hubbub_tokeniser *tokeniser);
static inline bool emit_current_tag(hubbub_tokeniser *tokeniser);
static inline bool emit_current_comment(hubbub_tokeniser *tokeniser);
static inline bool emit_current_doctype(hubbub_tokeniser *tokeniser,
		bool force_quirks);
static void hubbub_tokeniser_emit_token(hubbub_tokeniser *tokeniser,
		hubbub_token *token);

/**
 * Create a hubbub tokeniser
 *
 * \param input  Input stream instance
 * \param alloc  Memory (de)allocation function
 * \param pw     Pointer to client-specific private data (may be NULL)
 * \return Pointer to tokeniser instance, or NULL on failure
 */
hubbub_tokeniser *hubbub_tokeniser_create(parserutils_inputstream *input,
		hubbub_alloc alloc, void *pw)
{
	hubbub_tokeniser *tok;

	if (input == NULL || alloc == NULL)
		return NULL;

	tok = alloc(NULL, sizeof(hubbub_tokeniser), pw);
	if (tok == NULL)
		return NULL;

	tok->buffer = parserutils_buffer_create(alloc, pw);
	if (tok->buffer == NULL) {
		alloc(tok, 0, pw);
		return NULL;
	}

	tok->state = STATE_DATA;
	tok->content_model = HUBBUB_CONTENT_MODEL_PCDATA;

	tok->escape_flag = false;
	tok->process_cdata_section = false;

	tok->input = input;

	tok->token_handler = NULL;
	tok->token_pw = NULL;

	tok->error_handler = NULL;
	tok->error_pw = NULL;

	tok->alloc = alloc;
	tok->alloc_pw = pw;

	memset(&tok->context, 0, sizeof(hubbub_tokeniser_context));

	return tok;
}

/**
 * Destroy a hubbub tokeniser
 *
 * \param tokeniser  The tokeniser instance to destroy
 */
void hubbub_tokeniser_destroy(hubbub_tokeniser *tokeniser)
{
	if (tokeniser == NULL)
		return;

	if (tokeniser->context.current_tag.attributes != NULL) {
		tokeniser->alloc(tokeniser->context.current_tag.attributes,
				0, tokeniser->alloc_pw);
	}

	tokeniser->alloc(tokeniser, 0, tokeniser->alloc_pw);
}

/**
 * Configure a hubbub tokeniser
 *
 * \param tokeniser  The tokeniser instance to configure
 * \param type       The option type to set
 * \param params     Option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_tokeniser_setopt(hubbub_tokeniser *tokeniser,
		hubbub_tokeniser_opttype type,
		hubbub_tokeniser_optparams *params)
{
	if (tokeniser == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_TOKENISER_TOKEN_HANDLER:
		tokeniser->token_handler = params->token_handler.handler;
		tokeniser->token_pw = params->token_handler.pw;
		break;
	case HUBBUB_TOKENISER_ERROR_HANDLER:
		tokeniser->error_handler = params->error_handler.handler;
		tokeniser->error_pw = params->error_handler.pw;
		break;
	case HUBBUB_TOKENISER_CONTENT_MODEL:
		tokeniser->content_model = params->content_model.model;
		break;
	case HUBBUB_TOKENISER_PROCESS_CDATA:
		tokeniser->process_cdata_section = params->process_cdata;
		break;
	}

	return HUBBUB_OK;
}

/**
 * Process remaining data in the input stream
 *
 * \param tokeniser  The tokeniser instance to invoke
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_tokeniser_run(hubbub_tokeniser *tokeniser)
{
	bool cont = true;

	if (tokeniser == NULL)
		return HUBBUB_BADPARM;

#if 0
#define state(x) \
		case x: \
			printf( #x "\n");
#else
#define state(x) \
		case x:
#endif

	while (cont) {
		switch (tokeniser->state) {
		state(STATE_DATA)
			cont = hubbub_tokeniser_handle_data(tokeniser);
			break;
		state(STATE_CHARACTER_REFERENCE_DATA)
			cont = hubbub_tokeniser_handle_character_reference_data(
					tokeniser);
			break;
		state(STATE_TAG_OPEN)
			cont = hubbub_tokeniser_handle_tag_open(tokeniser);
			break;
		state(STATE_CLOSE_TAG_OPEN)
			cont = hubbub_tokeniser_handle_close_tag_open(
					tokeniser);
			break;
		state(STATE_TAG_NAME)
			cont = hubbub_tokeniser_handle_tag_name(tokeniser);
			break;
		state(STATE_BEFORE_ATTRIBUTE_NAME)
			cont = hubbub_tokeniser_handle_before_attribute_name(
					tokeniser);
			break;
		state(STATE_ATTRIBUTE_NAME)
			cont = hubbub_tokeniser_handle_attribute_name(
					tokeniser);
			break;
		state(STATE_AFTER_ATTRIBUTE_NAME)
			cont = hubbub_tokeniser_handle_after_attribute_name(
					tokeniser);
			break;
		state(STATE_BEFORE_ATTRIBUTE_VALUE)
			cont = hubbub_tokeniser_handle_before_attribute_value(
					tokeniser);
			break;
		state(STATE_ATTRIBUTE_VALUE_DQ)
			cont = hubbub_tokeniser_handle_attribute_value_dq(
					tokeniser);
			break;
		state(STATE_ATTRIBUTE_VALUE_SQ)
			cont = hubbub_tokeniser_handle_attribute_value_sq(
					tokeniser);
			break;
		state(STATE_ATTRIBUTE_VALUE_UQ)
			cont = hubbub_tokeniser_handle_attribute_value_uq(
					tokeniser);
			break;
		state(STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE)
			cont = hubbub_tokeniser_handle_character_reference_in_attribute_value(
					tokeniser);
			break;
		state(STATE_AFTER_ATTRIBUTE_VALUE_Q)
			cont = hubbub_tokeniser_handle_after_attribute_value_q(
					tokeniser);
			break;
		state(STATE_SELF_CLOSING_START_TAG)
			cont = hubbub_tokeniser_handle_self_closing_start_tag(
					tokeniser);
			break;
		state(STATE_BOGUS_COMMENT)
			cont = hubbub_tokeniser_handle_bogus_comment(
					tokeniser);
			break;
		state(STATE_MARKUP_DECLARATION_OPEN)
			cont = hubbub_tokeniser_handle_markup_declaration_open(
					tokeniser);
			break;
		state(STATE_MATCH_COMMENT)
			cont = hubbub_tokeniser_handle_match_comment(
					tokeniser);
			break;
		case STATE_COMMENT_START:
		case STATE_COMMENT_START_DASH:
		case STATE_COMMENT:
		case STATE_COMMENT_END_DASH:
		case STATE_COMMENT_END:
#if 0
			printf("COMMENT %d\n",
					tokeniser->state - STATE_COMMENT_START + 1);
#endif
			cont = hubbub_tokeniser_handle_comment(tokeniser);
			break;
		state(STATE_MATCH_DOCTYPE)
			cont = hubbub_tokeniser_handle_match_doctype(
					tokeniser);
			break;
		state(STATE_DOCTYPE)
			cont = hubbub_tokeniser_handle_doctype(tokeniser);
			break;
		state(STATE_BEFORE_DOCTYPE_NAME)
			cont = hubbub_tokeniser_handle_before_doctype_name(
					tokeniser);
			break;
		state(STATE_DOCTYPE_NAME)
			cont = hubbub_tokeniser_handle_doctype_name(
					tokeniser);
			break;
		state(STATE_AFTER_DOCTYPE_NAME)
			cont = hubbub_tokeniser_handle_after_doctype_name(
					tokeniser);
			break;

		state(STATE_MATCH_PUBLIC)
			cont = hubbub_tokeniser_handle_match_public(
					tokeniser);
			break;
		state(STATE_BEFORE_DOCTYPE_PUBLIC)
			cont = hubbub_tokeniser_handle_before_doctype_public(
					tokeniser);
			break;
		state(STATE_DOCTYPE_PUBLIC_DQ)
			cont = hubbub_tokeniser_handle_doctype_public_dq(
					tokeniser);
			break;
		state(STATE_DOCTYPE_PUBLIC_SQ)
			cont = hubbub_tokeniser_handle_doctype_public_sq(
					tokeniser);
			break;
		state(STATE_AFTER_DOCTYPE_PUBLIC)
			cont = hubbub_tokeniser_handle_after_doctype_public(
					tokeniser);
			break;
		state(STATE_MATCH_SYSTEM)
			cont = hubbub_tokeniser_handle_match_system(
					tokeniser);
			break;
		state(STATE_BEFORE_DOCTYPE_SYSTEM)
			cont = hubbub_tokeniser_handle_before_doctype_system(
					tokeniser);
			break;
		state(STATE_DOCTYPE_SYSTEM_DQ)
			cont = hubbub_tokeniser_handle_doctype_system_dq(
					tokeniser);
			break;
		state(STATE_DOCTYPE_SYSTEM_SQ)
			cont = hubbub_tokeniser_handle_doctype_system_sq(
					tokeniser);
			break;
		state(STATE_AFTER_DOCTYPE_SYSTEM)
			cont = hubbub_tokeniser_handle_after_doctype_system(
					tokeniser);
			break;
		state(STATE_BOGUS_DOCTYPE)
			cont = hubbub_tokeniser_handle_bogus_doctype(
					tokeniser);
			break;
		state(STATE_MATCH_CDATA)
			cont = hubbub_tokeniser_handle_match_cdata(
					tokeniser);
			break;
		state(STATE_CDATA_BLOCK)
			cont = hubbub_tokeniser_handle_cdata_block(
					tokeniser);
			break;
		state(STATE_NUMBERED_ENTITY)
			cont = hubbub_tokeniser_handle_numbered_entity(
					tokeniser);
			break;
		state(STATE_NAMED_ENTITY)
			cont = hubbub_tokeniser_handle_named_entity(
					tokeniser);
			break;
		}
	}

	return HUBBUB_OK;
}


/**
 * Macro to obtain the current character from the pointer "cptr".
 *
 * To be eliminated as soon as checks for EOF always happen before we want
 * the current character.
 */
#define CHAR(cptr) \
	(((cptr) == PARSERUTILS_INPUTSTREAM_EOF) ? 0 : (*((uint8_t *) cptr)))


/**
 * Various macros for manipulating buffers.
 *
 * \todo make some of these inline functions (type-safety)
 * \todo document them properly here
 */

#define START_BUF(str, cptr, lengt) \
	do { \
		uint8_t *data = tokeniser->buffer->data + \
				tokeniser->buffer->length; \
		parserutils_buffer_append( \
				tokeniser->buffer, \
				cptr, (lengt)); \
		(str).ptr = data; \
		(str).len = (lengt); \
	} while (0)

#define COLLECT(str, cptr, length) \
	do { \
		assert(str.len != 0); \
		parserutils_buffer_append(tokeniser->buffer, \
				(uint8_t *) cptr, (length)); \
		(str).len += (length); \
	} while (0)

#define COLLECT_MS(str, cptr, length) \
	do { \
		if ((str).len == 0) { \
			START_BUF(str, (uint8_t *)cptr, length); \
		} else { \
			COLLECT(str, cptr, length); \
		} \
	} while (0)

#define FINISH(str) \
	/* no-op */






/* this should always be called with an empty "chars" buffer */
bool hubbub_tokeniser_handle_data(hubbub_tokeniser *tokeniser)
{
	hubbub_token token;
	uintptr_t cptr;
	size_t len;

	while ((cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len)) !=
					PARSERUTILS_INPUTSTREAM_EOF &&
			cptr != PARSERUTILS_INPUTSTREAM_OOD) {
		uint8_t c = CHAR(cptr);

		if (c == '&' &&
				(tokeniser->content_model == HUBBUB_CONTENT_MODEL_PCDATA ||
				tokeniser->content_model == HUBBUB_CONTENT_MODEL_RCDATA) &&
				tokeniser->escape_flag == false) {
			tokeniser->state =
					STATE_CHARACTER_REFERENCE_DATA;
			/* Don't eat the '&'; it'll be handled by entity
			 * consumption */
			break;

		} else if (c == '-' &&
				tokeniser->escape_flag == false &&
				(tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_RCDATA ||
				tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_CDATA) &&
				tokeniser->context.pending >= 3) {

			cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending - 3,
					&len);

			if (strncmp((char *)cptr,
					"<!--", SLEN("<!--")) == 0) {
				tokeniser->escape_flag = true;
			}

			tokeniser->context.pending += len;
		} else if (c == '<' && (tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_PCDATA ||
					((tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_RCDATA ||
					tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_CDATA) &&
				tokeniser->escape_flag == false))) {
			if (tokeniser->context.pending > 0) {
				/* Emit any pending characters */
				emit_current_chars(tokeniser);
			}

			/* Buffer '<' */
			tokeniser->context.pending = len;
			tokeniser->state = STATE_TAG_OPEN;
			break;
		} else if (c == '>' && tokeniser->escape_flag == true &&
				(tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_RCDATA ||
				tokeniser->content_model ==
						HUBBUB_CONTENT_MODEL_CDATA)) {
			/* no need to check that there are enough characters,
			 * since you can only run into this if the flag is
			 * true in the first place, which requires four
			 * characters. */
			cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending - 2,
					&len);

			if (strncmp((char *)cptr, "-->", SLEN("-->")) == 0) {
				tokeniser->escape_flag = false;
			}

			tokeniser->context.pending += len;
		} else if (c == '\0') {
			if (tokeniser->context.pending > 0) {
				/* Emit any pending characters */
				emit_current_chars(tokeniser);
			}

			/* Emit a replacement character */
			emit_character_token(tokeniser, &u_fffd_str);

			/* Advance past NUL */
			parserutils_inputstream_advance(tokeniser->input, 1);
		} else if (c == '\r') {
			cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending + len,
					&len);

			if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
				break;
			}

			if (tokeniser->context.pending > 0) {
				/* Emit any pending characters */
				emit_current_chars(tokeniser);
			}

			c = CHAR(cptr);
			if (c != '\n') {
				/* Emit newline */
				emit_character_token(tokeniser, &lf_str);
			}

			/* Advance over */
			parserutils_inputstream_advance(tokeniser->input, 1);
		} else {
			/* Just collect into buffer */
			tokeniser->context.pending += len;
		}
	}

	if (tokeniser->state != STATE_TAG_OPEN &&
			(tokeniser->state != STATE_DATA ||
					cptr == PARSERUTILS_INPUTSTREAM_EOF) &&
			tokeniser->context.pending > 0) {
		/* Emit any pending characters */
		emit_current_chars(tokeniser);
	}

	if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		token.type = HUBBUB_TOKEN_EOF;
		hubbub_tokeniser_emit_token(tokeniser, &token);
	}

	return (cptr != PARSERUTILS_INPUTSTREAM_EOF && cptr != PARSERUTILS_INPUTSTREAM_OOD);
}

/* emit any pending tokens before calling */
bool hubbub_tokeniser_handle_character_reference_data(hubbub_tokeniser *tokeniser)
{
	assert(tokeniser->context.pending == 0);

	if (tokeniser->context.match_entity.complete == false) {
		return hubbub_tokeniser_consume_character_reference(tokeniser,
				tokeniser->context.pending);
	} else {
		hubbub_token token;

		uint8_t utf8[6];
		uint8_t *utf8ptr = utf8;
		size_t len = sizeof(utf8);

		token.type = HUBBUB_TOKEN_CHARACTER;

		if (tokeniser->context.match_entity.codepoint) {
			parserutils_charset_utf8_from_ucs4(
				tokeniser->context.match_entity.codepoint,
				&utf8ptr, &len);

			token.data.character.ptr = utf8;
			token.data.character.len = sizeof(utf8) - len;

			hubbub_tokeniser_emit_token(tokeniser, &token);

			/* +1 for ampersand */
			parserutils_inputstream_advance(tokeniser->input,
					tokeniser->context.match_entity.length
							+ 1);
		} else {
			uintptr_t cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending,
					&len);

			token.data.character.ptr = (uint8_t *)cptr;
			token.data.character.len = len;

			hubbub_tokeniser_emit_token(tokeniser, &token);
			parserutils_inputstream_advance(tokeniser->input, len);
		}

		/* Reset for next time */
		tokeniser->context.match_entity.complete = false;

		tokeniser->state = STATE_DATA;
	}

	return true;
}

/* this state always switches to another state straight away */
/* this state expects the current character to be '<' */
bool hubbub_tokeniser_handle_tag_open(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	assert(tokeniser->context.pending == 1);
/*	assert(tokeniser->context.chars.ptr[0] == '<'); */

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		/* Return to data state with '<' still in "chars" */
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);

	if (c == '/') {
		tokeniser->context.pending += len;

		tokeniser->context.close_tag_match.match = false;
		tokeniser->context.close_tag_match.count = 0;

		tokeniser->state = STATE_CLOSE_TAG_OPEN;
	} else if (tokeniser->content_model == HUBBUB_CONTENT_MODEL_RCDATA ||
			tokeniser->content_model ==
					HUBBUB_CONTENT_MODEL_CDATA) {
		/* Return to data state with '<' still in "chars" */
		tokeniser->state = STATE_DATA;
	} else if (tokeniser->content_model == HUBBUB_CONTENT_MODEL_PCDATA) {
		if (c == '!') {
			parserutils_inputstream_advance(tokeniser->input,
					SLEN("<!"));

			tokeniser->context.pending = 0;
			tokeniser->state = STATE_MARKUP_DECLARATION_OPEN;
		} else if ('A' <= c && c <= 'Z') {
			tokeniser->context.pending += len;
			tokeniser->context.current_tag_type =
					HUBBUB_TOKEN_START_TAG;

			uint8_t lc = (c + 0x20);
			START_BUF(ctag->name, &lc, len);
			ctag->n_attributes = 0;

			tokeniser->state = STATE_TAG_NAME;
		} else if ('a' <= c && c <= 'z') {
			tokeniser->context.pending += len;
			tokeniser->context.current_tag_type =
					HUBBUB_TOKEN_START_TAG;

			START_BUF(ctag->name, (uint8_t *)cptr, len);
			ctag->n_attributes = 0;

			tokeniser->state = STATE_TAG_NAME;
		} else if (c == '\0') {
			tokeniser->context.pending += len;
			tokeniser->context.current_tag_type =
					HUBBUB_TOKEN_START_TAG;

			START_BUF(ctag->name, u_fffd, sizeof(u_fffd));
			ctag->n_attributes = 0;

			tokeniser->state = STATE_TAG_NAME;
		} else if (c == '>') {
			/** \todo parse error */

			tokeniser->context.pending += len;
			tokeniser->state = STATE_DATA;
		} else if (c == '?') {
			/** \todo parse error */

			/* Cursor still at "<", need to advance past it */
			parserutils_inputstream_advance(
					tokeniser->input, SLEN("<"));
			tokeniser->context.pending = 0;

			tokeniser->state = STATE_BOGUS_COMMENT;
		} else {
			/* Return to data state with '<' still in "chars" */
			tokeniser->state = STATE_DATA;
		}
	}

	return true;
}

/* this state expects tokeniser->context.chars to be "</" */
/* this state never stays in this state for more than one character */
bool hubbub_tokeniser_handle_close_tag_open(hubbub_tokeniser *tokeniser)
{
	hubbub_tokeniser_context *ctx = &tokeniser->context;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	assert(tokeniser->context.pending == 2);
/*	assert(tokeniser->context.chars.ptr[0] == '<'); */
/*	assert(tokeniser->context.chars.ptr[1] == '/'); */

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_chars(tokeniser);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);

	/**\todo fragment case */

	if (tokeniser->content_model == HUBBUB_CONTENT_MODEL_RCDATA ||
			tokeniser->content_model ==
					HUBBUB_CONTENT_MODEL_CDATA) {
		uint8_t *start_tag_name =
			tokeniser->context.last_start_tag_name;
		size_t start_tag_len =
			tokeniser->context.last_start_tag_len;

		while ((cptr = parserutils_inputstream_peek(tokeniser->input,
					ctx->pending +
						ctx->close_tag_match.count,
					&len)) !=
				PARSERUTILS_INPUTSTREAM_EOF &&
				cptr != PARSERUTILS_INPUTSTREAM_OOD) {
			c = CHAR(cptr);

			if ((start_tag_name[ctx->close_tag_match.count] & ~0x20)
					!= (c & ~0x20)) {
				break;
			}

			ctx->close_tag_match.count += len;

			if (ctx->close_tag_match.count == start_tag_len) {
				ctx->close_tag_match.match = true;
				break;
			}
		}

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		}

		if (ctx->close_tag_match.match == true) {
			cptr = parserutils_inputstream_peek(
			 		tokeniser->input,
			 		ctx->pending +
				 		ctx->close_tag_match.count,
			 		&len);

			if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
				return false;
			} else if (cptr != PARSERUTILS_INPUTSTREAM_EOF) {
				c = CHAR(cptr);

				if (c != '\t' && c != '\n' && c != '\f' &&
						c != ' ' && c != '>' &&
						c != '/') {
					ctx->close_tag_match.match = false;
				}
			}
		}
	}

	if (ctx->close_tag_match.match == false &&
			tokeniser->content_model !=
					HUBBUB_CONTENT_MODEL_PCDATA) {
		/* We should emit "</" here, but instead we leave it in the
		 * buffer so the data state emits it with any characters
		 * following it */
		tokeniser->state = STATE_DATA;
	} else {
		cptr = parserutils_inputstream_peek(tokeniser->input,
				tokeniser->context.pending, &len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/** \todo parse error */

			/* Return to data state with "</" pending */
			tokeniser->state = STATE_DATA;
			return true;
		}

		c = CHAR(cptr);

		if ('A' <= c && c <= 'Z') {
			tokeniser->context.pending += len;

			tokeniser->context.current_tag_type =
					HUBBUB_TOKEN_END_TAG;

			uint8_t lc = (c + 0x20);
			START_BUF(tokeniser->context.current_tag.name,
					&lc, len);
			tokeniser->context.current_tag.n_attributes = 0;

			tokeniser->state = STATE_TAG_NAME;
		} else if ('a' <= c && c <= 'z') {
			tokeniser->context.pending += len;

			tokeniser->context.current_tag_type =
					HUBBUB_TOKEN_END_TAG;
			START_BUF(tokeniser->context.current_tag.name,
					(uint8_t *) cptr, len);
			tokeniser->context.current_tag.n_attributes = 0;

			tokeniser->state = STATE_TAG_NAME;
		} else if (c == '>') {
			/* Cursor still at "</", need to collect ">" */
			tokeniser->context.pending += len;

			/* Now need to advance past "</>" */
			parserutils_inputstream_advance(tokeniser->input,
					tokeniser->context.pending);
			tokeniser->context.pending = 0;

			/** \todo parse error */
			tokeniser->state = STATE_DATA;
		} else {
			/** \todo parse error */

			/* Cursor still at "</", need to advance past it */
			parserutils_inputstream_advance(tokeniser->input,
					tokeniser->context.pending);
			tokeniser->context.pending = 0;

			tokeniser->state = STATE_BOGUS_COMMENT;
		}
	}

	return true;
}

/* this state expects tokeniser->context.current_tag to already have its
   first character set */
bool hubbub_tokeniser_handle_tag_name(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	assert(tokeniser->context.pending > 0);
/*	assert(tokeniser->context.chars.ptr[0] == '<'); */
	assert(ctag->name.len > 0);
	assert(ctag->name.ptr);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		FINISH(ctag->name);

		tokeniser->state = STATE_BEFORE_ATTRIBUTE_NAME;
	} else if (c == '>') {
		FINISH(ctag->name);

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		COLLECT(ctag->name, u_fffd, sizeof(u_fffd));
	} else if (c == '/') {
		FINISH(ctag->name);
		tokeniser->state = STATE_SELF_CLOSING_START_TAG;
	} else if ('A' <= c && c <= 'Z') {
		uint8_t lc = (c + 0x20);
		COLLECT(ctag->name, &lc, len);
	} else {
		COLLECT(ctag->name, cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_before_attribute_name(
		hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '>') {
		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '/') {
		tokeniser->state = STATE_SELF_CLOSING_START_TAG;
	} else {
		hubbub_attribute *attr;

		if (c == '"' || c == '\'' || c == '=') {
			/** \todo parse error */
		}

		attr = tokeniser->alloc(ctag->attributes,
				(ctag->n_attributes + 1) *
					sizeof(hubbub_attribute),
				tokeniser->alloc_pw);
		if (attr == NULL) {
			/** \todo handle memory exhaustion */
		}

		ctag->attributes = attr;

		if ('A' <= c && c <= 'Z') {
			uint8_t lc = (c + 0x20);
			START_BUF(attr[ctag->n_attributes].name, &lc, len);
		} else if (c == '\0') {
			START_BUF(attr[ctag->n_attributes].name,
					u_fffd, sizeof(u_fffd));
		} else {
			START_BUF(attr[ctag->n_attributes].name,
					(uint8_t *) cptr, len);
		}

		attr[ctag->n_attributes].ns = HUBBUB_NS_NULL;
		attr[ctag->n_attributes].value.ptr = NULL;
		attr[ctag->n_attributes].value.len = 0;

		ctag->n_attributes++;

		tokeniser->state = STATE_ATTRIBUTE_NAME;
	}

	return true;
}

bool hubbub_tokeniser_handle_attribute_name(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	assert(ctag->attributes[ctag->n_attributes - 1].name.len > 0);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		FINISH(ctag->attributes[ctag->n_attributes - 1].name);
		tokeniser->state = STATE_AFTER_ATTRIBUTE_NAME;
	} else if (c == '=') {
		FINISH(ctag->attributes[ctag->n_attributes - 1].name);
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_VALUE;
	} else if (c == '>') {
		FINISH(ctag->attributes[ctag->n_attributes - 1].name);

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '/') {
		FINISH(ctag->attributes[ctag->n_attributes - 1].name);
		tokeniser->state = STATE_SELF_CLOSING_START_TAG;
	} else if (c == '\0') {
		COLLECT(ctag->attributes[ctag->n_attributes - 1].name,
				u_fffd, sizeof(u_fffd));
	} else if ('A' <= c && c <= 'Z') {
		uint8_t lc = (c + 0x20);
		COLLECT(ctag->attributes[ctag->n_attributes - 1].name,
				&lc, len);
	} else {
		COLLECT(ctag->attributes[ctag->n_attributes - 1].name,
				cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_after_attribute_name(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		tokeniser->context.pending += len;
	} else if (c == '=') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_VALUE;
	} else if (c == '>') {
		tokeniser->context.pending += len;

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '/') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_SELF_CLOSING_START_TAG;
	} else {
		hubbub_attribute *attr;

		if (c == '"' || c == '\'' || c == '=') {
			/** \todo parse error */
		}

		attr = tokeniser->alloc(ctag->attributes,
				(ctag->n_attributes + 1) *
					sizeof(hubbub_attribute),
				tokeniser->alloc_pw);
		if (attr == NULL) {
			/** \todo handle memory exhaustion */
		}

		ctag->attributes = attr;

		if ('A' <= c && c <= 'Z') {
			uint8_t lc = (c + 0x20);
			START_BUF(attr[ctag->n_attributes].name, &lc, len);
		} else if (c == '\0') {
			START_BUF(attr[ctag->n_attributes].name,
					u_fffd, sizeof(u_fffd));
		} else {
			START_BUF(attr[ctag->n_attributes].name,
					(uint8_t *)cptr, len);
		}

		attr[ctag->n_attributes].ns = HUBBUB_NS_NULL;
		attr[ctag->n_attributes].value.ptr = NULL;
		attr[ctag->n_attributes].value.len = 0;

		ctag->n_attributes++;

		tokeniser->context.pending += len;
		tokeniser->state = STATE_ATTRIBUTE_NAME;
	}

	return true;
}

/* this state is only ever triggered by an '=' */
bool hubbub_tokeniser_handle_before_attribute_value(
		hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		tokeniser->context.pending += len;
	} else if (c == '"') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_ATTRIBUTE_VALUE_DQ;
	} else if (c == '&') {
		tokeniser->state = STATE_ATTRIBUTE_VALUE_UQ;
	} else if (c == '\'') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_ATTRIBUTE_VALUE_SQ;
	} else if (c == '>') {
		tokeniser->context.pending += len;

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		tokeniser->context.pending += len;
		START_BUF(ctag->attributes[ctag->n_attributes - 1].value,
				u_fffd, sizeof(u_fffd));
		tokeniser->state = STATE_ATTRIBUTE_VALUE_UQ;
	} else {
		tokeniser->context.pending += len;
		START_BUF(ctag->attributes[ctag->n_attributes - 1].value,
				(uint8_t *)cptr, len);
		tokeniser->state = STATE_ATTRIBUTE_VALUE_UQ;
	}

	return true;
}

bool hubbub_tokeniser_handle_attribute_value_dq(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);

		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '"') {
		tokeniser->context.pending += len;
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);
		tokeniser->state = STATE_AFTER_ATTRIBUTE_VALUE_Q;
	} else if (c == '&') {
		tokeniser->context.prev_state = tokeniser->state;
		tokeniser->state = STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
		tokeniser->context.allowed_char = '"';
		/* Don't eat the '&'; it'll be handled by entity consumption */
	} else if (c == '\0') {
		tokeniser->context.pending += len;
		COLLECT(ctag->attributes[ctag->n_attributes - 1].value,
				u_fffd, sizeof(u_fffd));
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(ctag->attributes[
					ctag->n_attributes - 1].value,
					&lf, sizeof(lf));
		}

		tokeniser->context.pending += len;
	} else {
		tokeniser->context.pending += len;
		COLLECT_MS(ctag->attributes[ctag->n_attributes - 1].value,
				cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_attribute_value_sq(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);

		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '\'') {
		tokeniser->context.pending += len;
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);
		tokeniser->state =
				STATE_AFTER_ATTRIBUTE_VALUE_Q;
	} else if (c == '&') {
		tokeniser->context.prev_state = tokeniser->state;
		tokeniser->state = STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
		tokeniser->context.allowed_char = '\'';
		/* Don't eat the '&'; it'll be handled by entity consumption */
	} else if (c == '\0') {
		tokeniser->context.pending += len;
		COLLECT(ctag->attributes[ctag->n_attributes - 1].value,
				u_fffd, sizeof(u_fffd));
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(ctag->attributes[
					ctag->n_attributes - 1].value,
					&lf, sizeof(lf));
		}

		tokeniser->context.pending += len;
	} else {
		tokeniser->context.pending += len;
		COLLECT_MS(ctag->attributes[ctag->n_attributes - 1].value,
				cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_attribute_value_uq(hubbub_tokeniser *tokeniser)
{
	hubbub_tag *ctag = &tokeniser->context.current_tag;
	uint8_t c;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);

		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	c = CHAR(cptr);

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		tokeniser->context.pending += len;
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_NAME;
	} else if (c == '&') {
		tokeniser->context.prev_state = tokeniser->state;
		tokeniser->state = STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE;
		/* Don't eat the '&'; it'll be handled by entity consumption */
	} else if (c == '>') {
		tokeniser->context.pending += len;
		FINISH(ctag->attributes[ctag->n_attributes - 1].value);

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		tokeniser->context.pending += len;
		COLLECT(ctag->attributes[ctag->n_attributes - 1].value,
				u_fffd, sizeof(u_fffd));
	} else {
		if (c == '"' || c == '\'' || c == '=') {
			/** \todo parse error */
		}

		tokeniser->context.pending += len;
		COLLECT(ctag->attributes[ctag->n_attributes - 1].value,
				cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_character_reference_in_attribute_value(
		hubbub_tokeniser *tokeniser)
{
	if (tokeniser->context.match_entity.complete == false) {
		return hubbub_tokeniser_consume_character_reference(tokeniser,
				tokeniser->context.pending);
	} else {
		hubbub_tag *ctag = &tokeniser->context.current_tag;
		hubbub_attribute *attr = &ctag->attributes[
				ctag->n_attributes - 1];

		uint8_t utf8[6];
		uint8_t *utf8ptr = utf8;
		size_t len = sizeof(utf8);

		if (tokeniser->context.match_entity.codepoint) {
			parserutils_charset_utf8_from_ucs4(
				tokeniser->context.match_entity.codepoint,
				&utf8ptr, &len);

			/* +1 for the ampersand */
			tokeniser->context.pending +=
					tokeniser->context.match_entity.length
					+ 1;

			if (attr->value.len == 0) {
				START_BUF(attr->value,
						utf8, sizeof(utf8) - len);
			} else {
				COLLECT(attr->value, utf8, sizeof(utf8) - len);
			}
		} else {
			size_t len;
			uintptr_t cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending, &len);

			/* Insert the ampersand */
			tokeniser->context.pending += len;
			COLLECT_MS(attr->value, cptr, len);
		}

		/* Reset for next time */
		tokeniser->context.match_entity.complete = false;

		/* And back to the previous state */
		tokeniser->state = tokeniser->context.prev_state;
	}

	return true;
}

/* always switches state */
bool hubbub_tokeniser_handle_after_attribute_value_q(
		hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_NAME;
	} else if (c == '>') {
		tokeniser->context.pending += len;

		emit_current_tag(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '/') {
		tokeniser->context.pending += len;
		tokeniser->state = STATE_SELF_CLOSING_START_TAG;
	} else {
		/** \todo parse error */
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_NAME;
	}

	return true;
}

bool hubbub_tokeniser_handle_self_closing_start_tag(
		hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		return emit_current_tag(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	if (c == '>') {
		tokeniser->context.pending += len;

		tokeniser->context.current_tag.self_closing = true;
		emit_current_tag(tokeniser);

		tokeniser->state = STATE_DATA;
	} else {
		tokeniser->state = STATE_BEFORE_ATTRIBUTE_NAME;
	}

	return true;
}

/* this state expects tokeniser->context.chars to be empty on first entry */
bool hubbub_tokeniser_handle_bogus_comment(hubbub_tokeniser *tokeniser)
{
	hubbub_string *comment = &tokeniser->context.current_comment;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_DATA;
		tokeniser->context.current_comment.ptr =
					tokeniser->buffer->data;
		return emit_current_comment(tokeniser);
	}

	uint8_t c = CHAR(cptr);

	tokeniser->context.pending += len;

	if (c == '>') {
		tokeniser->context.current_comment.ptr =
					tokeniser->buffer->data;
		emit_current_comment(tokeniser);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		parserutils_buffer_append(tokeniser->buffer,
				u_fffd, sizeof(u_fffd));
		comment->len += sizeof(u_fffd);
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			parserutils_buffer_append(tokeniser->buffer,
					&lf, sizeof(lf));
			comment->len += sizeof(lf);
		}

		tokeniser->context.pending += len;
	} else {
		parserutils_buffer_append(tokeniser->buffer,
				(uint8_t *)cptr, len);
		comment->len += len;
	}

	return true;
}

/* this state always switches to another state straight away */
bool hubbub_tokeniser_handle_markup_declaration_open(
		hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			0, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	uint8_t c = CHAR(cptr);

	if (c == '-') {
		tokeniser->context.pending = len;
		tokeniser->state = STATE_MATCH_COMMENT;
	} else if ((c & ~0x20) == 'D') {
		tokeniser->context.pending = len;
		tokeniser->context.match_doctype.count = len;
		tokeniser->state = STATE_MATCH_DOCTYPE;
	} else if (tokeniser->process_cdata_section == true && c == '[') {
		tokeniser->context.pending = len;
		tokeniser->context.match_cdata.count = len;
		tokeniser->state = STATE_MATCH_CDATA;
	} else {
		tokeniser->state = STATE_BOGUS_COMMENT;
	}

	return true;
}


bool hubbub_tokeniser_handle_match_comment(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.pending =
				tokeniser->context.current_comment.len =
				0;
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	tokeniser->context.pending =
			tokeniser->context.current_comment.len =
			0;

	if (CHAR(cptr) == '-') {
		parserutils_inputstream_advance(tokeniser->input, SLEN("--"));
		tokeniser->state = STATE_COMMENT_START;
	} else {
		tokeniser->state = STATE_BOGUS_COMMENT;
	}

	return true;
}


bool hubbub_tokeniser_handle_comment(hubbub_tokeniser *tokeniser)
{
	hubbub_string *comment = &tokeniser->context.current_comment;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.current_comment.ptr =
					tokeniser->buffer->data;
		emit_current_comment(tokeniser);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);

	if (c == '>' && (tokeniser->state == STATE_COMMENT_START_DASH ||
			tokeniser->state == STATE_COMMENT_START ||
			tokeniser->state == STATE_COMMENT_END)) {
		tokeniser->context.pending += len;

		/** \todo parse error if state != COMMENT_END */
		tokeniser->context.current_comment.ptr =
					tokeniser->buffer->data;
		emit_current_comment(tokeniser);

		tokeniser->state = STATE_DATA;
	} else if (c == '-') {
		if (tokeniser->state == STATE_COMMENT_START) {
			tokeniser->state = STATE_COMMENT_START_DASH;
		} else if (tokeniser->state == STATE_COMMENT_START_DASH) {
			tokeniser->state = STATE_COMMENT_END;
		} else if (tokeniser->state == STATE_COMMENT) {
			tokeniser->state = STATE_COMMENT_END_DASH;
		} else if (tokeniser->state == STATE_COMMENT_END_DASH) {
			tokeniser->state = STATE_COMMENT_END;
		} else if (tokeniser->state == STATE_COMMENT_END) {
			parserutils_buffer_append(tokeniser->buffer,
					(uint8_t *) "-", SLEN("-"));
			comment->len += SLEN("-");
		}

		tokeniser->context.pending += len;
	} else {
		if (tokeniser->state == STATE_COMMENT_START_DASH ||
				tokeniser->state == STATE_COMMENT_END_DASH) {
			parserutils_buffer_append(tokeniser->buffer,
					(uint8_t *) "-", SLEN("-"));
			comment->len += SLEN("-");
		} else if (tokeniser->state == STATE_COMMENT_END) {
			parserutils_buffer_append(tokeniser->buffer,
					(uint8_t *) "--", SLEN("--"));
			comment->len += SLEN("--");
		}

		if (c == '\0') {
			parserutils_buffer_append(tokeniser->buffer,
					u_fffd, sizeof(u_fffd));
			comment->len += sizeof(u_fffd);
		} else if (c == '\r') {
			cptr = parserutils_inputstream_peek(
					tokeniser->input,
					tokeniser->context.pending + len,
					&len);
			if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
				return false;
			} else if (cptr != PARSERUTILS_INPUTSTREAM_EOF &&
					CHAR(cptr) != '\n') {
				parserutils_buffer_append(tokeniser->buffer,
						&lf, sizeof(lf));
				comment->len += sizeof(lf);
			}
		} else {
			parserutils_buffer_append(tokeniser->buffer,
					(uint8_t *)cptr, len);
			comment->len += len;
		}

		tokeniser->context.pending += len;
		tokeniser->state = STATE_COMMENT;
	}

	return true;
}




#define DOCTYPE		"DOCTYPE"
#define DOCTYPE_LEN	(SLEN(DOCTYPE) - 1)

bool hubbub_tokeniser_handle_match_doctype(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.match_doctype.count, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.current_comment.len =
				tokeniser->context.pending =
				0;
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	uint8_t c = CHAR(cptr);

	assert(tokeniser->context.match_doctype.count <= DOCTYPE_LEN);

	if (DOCTYPE[tokeniser->context.match_doctype.count] != (c & ~0x20)) {
		tokeniser->context.current_comment.len =
				tokeniser->context.pending =
				0;
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	tokeniser->context.pending += len;

	if (tokeniser->context.match_doctype.count == DOCTYPE_LEN) {
		/* Skip over the DOCTYPE bit */
		parserutils_inputstream_advance(tokeniser->input,
				tokeniser->context.pending);

		memset(&tokeniser->context.current_doctype, 0,
				sizeof tokeniser->context.current_doctype);
		tokeniser->context.current_doctype.public_missing = true;
		tokeniser->context.current_doctype.system_missing = true;
		tokeniser->context.pending = 0;

		tokeniser->state = STATE_DOCTYPE;
	}

	tokeniser->context.match_doctype.count++;

	return true;
}

#undef DOCTYPE
#undef DOCTYPE_LEN

bool hubbub_tokeniser_handle_doctype(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->state = STATE_BEFORE_DOCTYPE_NAME;
		return true;
	}

	uint8_t c = CHAR(cptr);

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		tokeniser->context.pending += len;
	}

	tokeniser->state = STATE_BEFORE_DOCTYPE_NAME;

	return true;
}

bool hubbub_tokeniser_handle_before_doctype_name(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		/* Emit current doctype, force-quirks on */
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '>') {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else {
		if (c == '\0') {
			START_BUF(cdoc->name, u_fffd, sizeof(u_fffd));
		} else {
			START_BUF(cdoc->name, (uint8_t *) cptr, len);
		}

		tokeniser->state = STATE_DOCTYPE_NAME;
	}

	return true;
}

bool hubbub_tokeniser_handle_doctype_name(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(cdoc->name);

		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		FINISH(cdoc->name);
		tokeniser->state = STATE_AFTER_DOCTYPE_NAME;
	} else if (c == '>') {
		FINISH(cdoc->name);
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		COLLECT(cdoc->name, u_fffd, sizeof(u_fffd));
	} else {
		COLLECT(cdoc->name, cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_after_doctype_name(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '>') {
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
	} else if ((c & ~0x20) == 'P') {
		tokeniser->context.match_doctype.count = 1;
		tokeniser->state = STATE_MATCH_PUBLIC;
	} else if ((c & ~0x20) == 'S') {
		tokeniser->context.match_doctype.count = 1;
		tokeniser->state = STATE_MATCH_SYSTEM;
	} else {
		tokeniser->state = STATE_BOGUS_DOCTYPE;
		tokeniser->context.current_doctype.force_quirks = true;
	}

	return true;
}

#define PUBLIC		"PUBLIC"
#define PUBLIC_LEN	(SLEN(PUBLIC) - 1)

bool hubbub_tokeniser_handle_match_public(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.current_doctype.force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
		return true;
	}

	uint8_t c = CHAR(cptr);

	assert(tokeniser->context.match_doctype.count <= PUBLIC_LEN);

	if (PUBLIC[tokeniser->context.match_doctype.count] != (c & ~0x20)) {
		tokeniser->context.current_doctype.force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
		return true;
	}

	tokeniser->context.pending += len;

	if (tokeniser->context.match_doctype.count == PUBLIC_LEN) {
		tokeniser->state = STATE_BEFORE_DOCTYPE_PUBLIC;
	}

	tokeniser->context.match_doctype.count++;

	return true;
}

#undef PUBLIC
#undef PUBLIC_LEN

bool hubbub_tokeniser_handle_before_doctype_public(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '"') {
		cdoc->public_missing = false;
		cdoc->public_id.len = 0;
		tokeniser->state = STATE_DOCTYPE_PUBLIC_DQ;
	} else if (c == '\'') {
		cdoc->public_missing = false;
		cdoc->public_id.len = 0;
		tokeniser->state = STATE_DOCTYPE_PUBLIC_SQ;
	} else if (c == '>') {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else {
		cdoc->force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
	}

	return true;
}

bool hubbub_tokeniser_handle_doctype_public_dq(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(cdoc->public_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '"') {
		FINISH(cdoc->public_id);
		tokeniser->state = STATE_AFTER_DOCTYPE_PUBLIC;
	} else if (c == '>') {
		FINISH(cdoc->public_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		if (cdoc->public_id.len == 0) {
			START_BUF(cdoc->public_id, u_fffd, sizeof(u_fffd));
		} else {
			COLLECT(cdoc->public_id, u_fffd, sizeof(u_fffd));
		}
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(cdoc->public_id, &lf, sizeof(lf));
		}
	} else {
		COLLECT_MS(cdoc->public_id, cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_doctype_public_sq(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(cdoc->public_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\'') {
		FINISH(cdoc->public_id);
		tokeniser->state = STATE_AFTER_DOCTYPE_PUBLIC;
	} else if (c == '>') {
		FINISH(cdoc->public_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		if (cdoc->public_id.len == 0) {
			START_BUF(cdoc->public_id,
					u_fffd, sizeof(u_fffd));
		} else {
			COLLECT(cdoc->public_id,
					u_fffd, sizeof(u_fffd));
		}
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(cdoc->public_id, &lf, sizeof(lf));
		}
	} else {
		COLLECT_MS(cdoc->public_id, cptr, len);
	}

	return true;
}


bool hubbub_tokeniser_handle_after_doctype_public(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '"') {
		cdoc->system_missing = false;
		cdoc->system_id.len = 0;

		tokeniser->state = STATE_DOCTYPE_SYSTEM_DQ;
	} else if (c == '\'') {
		cdoc->system_missing = false;
		cdoc->system_id.len = 0;

		tokeniser->state = STATE_DOCTYPE_SYSTEM_SQ;
	} else if (c == '>') {
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
	} else {
		cdoc->force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
	}

	return true;
}



#define SYSTEM		"SYSTEM"
#define SYSTEM_LEN	(SLEN(SYSTEM) - 1)

bool hubbub_tokeniser_handle_match_system(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.current_doctype.force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
		return true;
	}

	uint8_t c = CHAR(cptr);

	assert(tokeniser->context.match_doctype.count <= SYSTEM_LEN);

	if (SYSTEM[tokeniser->context.match_doctype.count] != (c & ~0x20)) {
		tokeniser->context.current_doctype.force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
		return true;
	}

	tokeniser->context.pending += len;

	if (tokeniser->context.match_doctype.count == SYSTEM_LEN) {
		tokeniser->state = STATE_BEFORE_DOCTYPE_SYSTEM;
	}

	tokeniser->context.match_doctype.count++;

	return true;
}

#undef SYSTEM
#undef SYSTEM_LEN

bool hubbub_tokeniser_handle_before_doctype_system(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
	} else if (c == '"') {
		cdoc->system_missing = false;
		cdoc->system_id.len = 0;

		tokeniser->state = STATE_DOCTYPE_SYSTEM_DQ;
	} else if (c == '\'') {
		cdoc->system_missing = false;
		cdoc->system_id.len = 0;

		tokeniser->state = STATE_DOCTYPE_SYSTEM_SQ;
	} else if (c == '>') {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else {
		cdoc->force_quirks = true;
		tokeniser->state = STATE_BOGUS_DOCTYPE;
	}

	return true;
}

bool hubbub_tokeniser_handle_doctype_system_dq(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(cdoc->system_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '"') {
		FINISH(cdoc->system_id);
		tokeniser->state = STATE_AFTER_DOCTYPE_SYSTEM;
	} else if (c == '>') {
		FINISH(cdoc->system_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		if (cdoc->public_id.len == 0) {
			START_BUF(cdoc->system_id, u_fffd, sizeof(u_fffd));
		} else {
			COLLECT(cdoc->system_id,
					u_fffd, sizeof(u_fffd));
		}
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(cdoc->system_id, &lf, sizeof(lf));
		}
	} else {
		COLLECT_MS(cdoc->system_id, cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_doctype_system_sq(hubbub_tokeniser *tokeniser)
{
	hubbub_doctype *cdoc = &tokeniser->context.current_doctype;
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		FINISH(cdoc->system_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\'') {
		FINISH(cdoc->system_id);
		tokeniser->state = STATE_AFTER_DOCTYPE_SYSTEM;
	} else if (c == '>') {
		FINISH(cdoc->system_id);
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		if (cdoc->public_id.len == 0) {
			START_BUF(cdoc->system_id, u_fffd, sizeof(u_fffd));
		} else {
			COLLECT(cdoc->system_id,
					u_fffd, sizeof(u_fffd));
		}
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF ||
				CHAR(cptr) != '\n') {
			COLLECT(cdoc->system_id, &lf, sizeof(lf));
		}
	} else {
		COLLECT_MS(cdoc->system_id, cptr, len);
	}

	return true;
}

bool hubbub_tokeniser_handle_after_doctype_system(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, true);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r') {
		/* pass over in silence */
	} else if (c == '>') {
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
	} else {
		tokeniser->state = STATE_BOGUS_DOCTYPE;
	}

	return true;
}


bool hubbub_tokeniser_handle_bogus_doctype(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);
	tokeniser->context.pending += len;

	if (c == '>') {
		emit_current_doctype(tokeniser, false);
		tokeniser->state = STATE_DATA;
	}

	return true;
}



#define CDATA		"[CDATA["
#define CDATA_LEN	(SLEN(CDATA) - 1)

bool hubbub_tokeniser_handle_match_cdata(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		tokeniser->context.current_comment.len =
				tokeniser->context.pending =
				0;
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	uint8_t c = CHAR(cptr);

	assert(tokeniser->context.match_cdata.count <= CDATA_LEN);

	if (CDATA[tokeniser->context.match_cdata.count] != (c & ~0x20)) {
		tokeniser->context.current_comment.len =
				tokeniser->context.pending =
				0;
		tokeniser->state = STATE_BOGUS_COMMENT;
		return true;
	}

	tokeniser->context.pending += len;

	if (tokeniser->context.match_cdata.count == CDATA_LEN) {
		parserutils_inputstream_advance(tokeniser->input,
				tokeniser->context.match_cdata.count + len);
		tokeniser->context.pending = 0;
		tokeniser->context.match_cdata.end = 0;
		tokeniser->state = STATE_CDATA_BLOCK;
	}

	tokeniser->context.match_cdata.count += len;

	return true;
}

#undef CDATA
#undef CDATA_LEN


bool hubbub_tokeniser_handle_cdata_block(hubbub_tokeniser *tokeniser)
{
	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			tokeniser->context.pending, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
		return false;
	} else if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		emit_current_chars(tokeniser);
		tokeniser->state = STATE_DATA;
		return true;
	}

	uint8_t c = CHAR(cptr);

	if (c == ']' && (tokeniser->context.match_cdata.end == 0 ||
			tokeniser->context.match_cdata.end == 1)) {
		tokeniser->context.pending += len;
		tokeniser->context.match_cdata.end += len;
	} else if (c == '>' && tokeniser->context.match_cdata.end == 2) {
		/* Remove the previous two "]]" */
		tokeniser->context.pending -= 2;

		/* Emit any pending characters */
		emit_current_chars(tokeniser);

		/* Now move past the "]]>" bit */
		parserutils_inputstream_advance(tokeniser->input, SLEN("]]>"));

		tokeniser->state = STATE_DATA;
	} else if (c == '\0') {
		if (tokeniser->context.pending > 0) {
			/* Emit any pending characters */
			emit_current_chars(tokeniser);
		}

		/* Perform NUL-byte replacement */
		emit_character_token(tokeniser, &u_fffd_str);

		parserutils_inputstream_advance(tokeniser->input, len);
		tokeniser->context.match_cdata.end = 0;
	} else if (c == '\r') {
		cptr = parserutils_inputstream_peek(
				tokeniser->input,
				tokeniser->context.pending + len,
				&len);

		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			return false;
		}

		if (tokeniser->context.pending > 0) {
			/* Emit any pending characters */
			emit_current_chars(tokeniser);
		}

		c = CHAR(cptr);
		if (c != '\n') {
			/* Emit newline */
			emit_character_token(tokeniser, &lf_str);
		}

		/* Advance over */
		parserutils_inputstream_advance(tokeniser->input, len);
		tokeniser->context.match_cdata.end = 0;
	} else {
		tokeniser->context.pending += len;
		tokeniser->context.match_cdata.end = 0;
	}

	return true;
}


bool hubbub_tokeniser_consume_character_reference(hubbub_tokeniser *tokeniser, size_t pos)
{
	uint32_t allowed_char = tokeniser->context.allowed_char;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			pos, &len);

	/* We should always started on a non-OOD character */
	assert(cptr != PARSERUTILS_INPUTSTREAM_OOD);

	size_t off = pos + len;

	/* Look at the character after the ampersand */
	cptr = parserutils_inputstream_peek(tokeniser->input, off, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return false;

	uint8_t c = CHAR(cptr);

	/* Set things up */
	tokeniser->context.match_entity.offset = off;
	tokeniser->context.match_entity.poss_length = 0;
	tokeniser->context.match_entity.length = 0;
	tokeniser->context.match_entity.base = 0;
	tokeniser->context.match_entity.codepoint = 0;
	tokeniser->context.match_entity.had_data = false;
	tokeniser->context.match_entity.return_state = tokeniser->state;
	tokeniser->context.match_entity.complete = false;
	tokeniser->context.match_entity.overflow = false;
	tokeniser->context.match_entity.context = NULL;
	tokeniser->context.match_entity.prev_len = len;

	/* Reset allowed character for future calls */
	tokeniser->context.allowed_char = '\0';

	if (c == '\t' || c == '\n' || c == '\f' || c == ' ' ||
			c == '<' || c == '&' ||
			cptr == PARSERUTILS_INPUTSTREAM_EOF ||
			(allowed_char && c == allowed_char)) {
		tokeniser->context.match_entity.complete = true;
		tokeniser->context.match_entity.codepoint = 0;
	} else if (c == '#') {
		tokeniser->context.match_entity.length += len;
		tokeniser->state = STATE_NUMBERED_ENTITY;
	} else {
		tokeniser->state = STATE_NAMED_ENTITY;
	}

	return true;
}


bool hubbub_tokeniser_handle_numbered_entity(hubbub_tokeniser *tokeniser)
{
	hubbub_tokeniser_context *ctx = &tokeniser->context;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			ctx->match_entity.offset + ctx->match_entity.length,
			&len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return false;

	uint8_t c = CHAR(cptr);

	if (ctx->match_entity.base == 0) {
		if ((c & ~0x20) == 'X') {
			ctx->match_entity.base = 16;
			ctx->match_entity.length += len;
		} else {
			ctx->match_entity.base = 10;
		}
	}

	while ((cptr = parserutils_inputstream_peek(tokeniser->input,
			ctx->match_entity.offset + ctx->match_entity.length,
			&len)) != PARSERUTILS_INPUTSTREAM_EOF &&
			cptr != PARSERUTILS_INPUTSTREAM_OOD) {
		c = CHAR(cptr);

		if (ctx->match_entity.base == 10 &&
				('0' <= c && c <= '9')) {
			ctx->match_entity.had_data = true;
			ctx->match_entity.codepoint =
				ctx->match_entity.codepoint * 10 + (c - '0');

			ctx->match_entity.length += len;
		} else if (ctx->match_entity.base == 16 &&
				(('0' <= c && c <= '9') ||
				('A' <= (c & ~0x20) &&
						(c & ~0x20) <= 'F'))) {
			ctx->match_entity.had_data = true;
			ctx->match_entity.codepoint *= 16;

			if ('0' <= c && c <= '9') {
				ctx->match_entity.codepoint += (c - '0');
			} else {
				ctx->match_entity.codepoint +=
						((c & ~0x20) - 'A' + 10);
			}

			ctx->match_entity.length += len;
		} else {
			break;
		}

		if (ctx->match_entity.codepoint >= 0x10FFFF) {
			ctx->match_entity.overflow = true;
		}
	}

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return false;

	c = CHAR(cptr);

	/* Eat trailing semicolon, if any */
	if (c == ';') {
		ctx->match_entity.length += len;
	}

	/* Had data, so calculate final codepoint */
	if (ctx->match_entity.had_data) {
		uint32_t cp = ctx->match_entity.codepoint;

		if (0x80 <= cp && cp <= 0x9F) {
			cp = cp1252Table[cp - 0x80];
		} else if (cp == 0x0D) {
			cp = 0x000A;
		} else if (ctx->match_entity.overflow || cp <= 0x0008 ||
				(0x000E <= cp && cp <= 0x001F) ||
				(0x007F <= cp && cp <= 0x009F) ||
				(0xD800 <= cp && cp <= 0xDFFF) ||
				(0xFDD0 <= cp && cp <= 0xFDDF) ||
				(cp & 0xFFFE) == 0xFFFE) {
			/* the check for cp > 0x10FFFF per spec is performed
			 * in the loop above to avoid overflow */
			cp = 0xFFFD;
		}

		ctx->match_entity.codepoint = cp;
	}

	/* Flag completion */
	ctx->match_entity.complete = true;

	/* And back to the state we were entered in */
	tokeniser->state = ctx->match_entity.return_state;

	return true;
}

bool hubbub_tokeniser_handle_named_entity(hubbub_tokeniser *tokeniser)
{
	hubbub_tokeniser_context *ctx = &tokeniser->context;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(tokeniser->input,
			ctx->match_entity.offset, &len);

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return false;

	uint8_t c = CHAR(cptr);

	while ((cptr = parserutils_inputstream_peek(tokeniser->input,
			ctx->match_entity.offset +
					ctx->match_entity.poss_length,
			&len)) !=
					PARSERUTILS_INPUTSTREAM_EOF &&
			cptr != PARSERUTILS_INPUTSTREAM_OOD) {
		uint32_t cp;

		c = CHAR(cptr);

		if (c > 0x7F) {
			/* Entity names are ASCII only */
			break;
		}

		hubbub_error error = hubbub_entities_search_step(c, &cp,
				&ctx->match_entity.context);
		if (error == HUBBUB_OK) {
			/* Had a match - store it for later */
			ctx->match_entity.codepoint = cp;

			ctx->match_entity.length =
					ctx->match_entity.poss_length + len;
			ctx->match_entity.poss_length =
					ctx->match_entity.length;
		} else if (error == HUBBUB_INVALID) {
			/* No further matches - use last found */
			break;
		} else {
			/* Need more data */
			ctx->match_entity.poss_length += len;
		}
	}

	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return false;

	cptr = parserutils_inputstream_peek(tokeniser->input,
			ctx->match_entity.offset + ctx->match_entity.length,
			&len);
	c = CHAR(cptr);

	if ((tokeniser->context.match_entity.return_state ==
			STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE) &&
			(c != ';')) {

		cptr = parserutils_inputstream_peek(tokeniser->input,
				ctx->match_entity.offset +
						ctx->match_entity.length,
				&len);
		c = CHAR(cptr);

		if ((0x0030 <= c && c <= 0x0039) ||
				(0x0041 <= c && c <= 0x005A) ||
				(0x0061 <= c && c <= 0x007A)) {
			ctx->match_entity.codepoint = 0;
		}
	}

	/* Flag completion */
	ctx->match_entity.complete = true;

	/* And back to the state from whence we came */
	tokeniser->state = ctx->match_entity.return_state;

	return true;
}



/*** Token emitting bits ***/

/**
 * Emit a character token.
 *
 * \param tokeniser	Tokeniser instance
 * \param chars		Pointer to hubbub_string to emit
 * \return	true
 */
static inline bool emit_character_token(hubbub_tokeniser *tokeniser,
		const hubbub_string *chars)
{
	hubbub_token token;

	token.type = HUBBUB_TOKEN_CHARACTER;
	token.data.character = *chars;

	hubbub_tokeniser_emit_token(tokeniser, &token);

	return true;
}

/**
 * Emit the current pending characters being stored in the tokeniser context.
 *
 * \param tokeniser	Tokeniser instance
 * \return	true
 */
static inline bool emit_current_chars(hubbub_tokeniser *tokeniser)
{
	hubbub_token token;

	size_t len;
	uintptr_t cptr = parserutils_inputstream_peek(
			tokeniser->input, 0, &len);

	token.type = HUBBUB_TOKEN_CHARACTER;
	token.data.character.ptr = (uint8_t *) cptr;
	token.data.character.len = tokeniser->context.pending;

	hubbub_tokeniser_emit_token(tokeniser, &token);

	return true;
}

/**
 * Emit the current tag token being stored in the tokeniser context.
 *
 * \param tokeniser	Tokeniser instance
 * \return	true
 */
static inline bool emit_current_tag(hubbub_tokeniser *tokeniser)
{
	hubbub_token token;

	/* Emit current tag */
	token.type = tokeniser->context.current_tag_type;
	token.data.tag = tokeniser->context.current_tag;
	token.data.tag.ns = HUBBUB_NS_HTML;

	/* Discard duplicate attributes */
	uint32_t i, j;
	uint32_t n_attributes = token.data.tag.n_attributes;
	hubbub_attribute *attrs = token.data.tag.attributes;

	/* Discard duplicate attributes */
	for (i = 0; i < n_attributes; i++) {
		for (j = 0; j < n_attributes; j++) {
			uint32_t move;

			if (j == i ||
				attrs[i].name.len !=
						attrs[j].name.len ||
				strncmp((char *)attrs[i].name.ptr,
					(char *)attrs[j].name.ptr,
					attrs[i].name.len) != 0) {
				/* Attributes don't match */
				continue;
			}

			/* Calculate amount to move */
			move = (n_attributes - 1 -
					((i < j) ? j : i)) *
					sizeof(hubbub_attribute);

			if (move > 0) {
				memmove((i < j) ? &attrs[j]
						: &attrs[i],
					(i < j) ? &attrs[j+1]
						: &attrs[i+1],
					move);
			}

			/* And reduce the number of attributes */
			n_attributes--;
		}
	}

	token.data.tag.n_attributes = n_attributes;

	hubbub_tokeniser_emit_token(tokeniser, &token);

	if (token.type == HUBBUB_TOKEN_START_TAG) {
		/* Save start tag name for R?CDATA */
		if (token.data.tag.name.len <
			sizeof(tokeniser->context.last_start_tag_name)) {
			strncpy((char *)tokeniser->context.last_start_tag_name,
				(const char *)token.data.tag.name.ptr,
				token.data.tag.name.len);
			tokeniser->context.last_start_tag_len =
					token.data.tag.name.len;
		} else {
			tokeniser->context.last_start_tag_name[0] = '\0';
			tokeniser->context.last_start_tag_len = 0;
		}
	} else /* if (token->type == HUBBUB_TOKEN_END_TAG) */ {
		/* Reset content model after R?CDATA elements */
		tokeniser->content_model = HUBBUB_CONTENT_MODEL_PCDATA;
	}

	return true;
}

/**
 * Emit the current comment token being stored in the tokeniser context.
 *
 * \param tokeniser	Tokeniser instance
 * \return	true
 */
static inline bool emit_current_comment(hubbub_tokeniser *tokeniser)
{
	hubbub_token token;

	token.type = HUBBUB_TOKEN_COMMENT;
	token.data.comment = tokeniser->context.current_comment;

	hubbub_tokeniser_emit_token(tokeniser, &token);

	return true;
}

/**
 * Emit the current doctype token being stored in the tokeniser context.
 *
 * \param tokeniser	Tokeniser instance
 * \param force_qurirks	Force quirks mode on this document
 * \return	true
 */
static inline bool emit_current_doctype(hubbub_tokeniser *tokeniser,
		bool force_quirks)
{
	hubbub_token token;

	/* Emit doctype */
	token.type = HUBBUB_TOKEN_DOCTYPE;
	token.data.doctype = tokeniser->context.current_doctype;
	if (force_quirks == true)
		token.data.doctype.force_quirks = true;

	hubbub_tokeniser_emit_token(tokeniser, &token);

	return true;
}

/**
 * Emit a token, performing sanity checks if necessary
 *
 * \param tokeniser  Tokeniser instance
 * \param token      Token to emit
 */
void hubbub_tokeniser_emit_token(hubbub_tokeniser *tokeniser,
		hubbub_token *token)
{
	assert(tokeniser != NULL);
	assert(token != NULL);

	/* Emit the token */
	if (tokeniser->token_handler) {
		tokeniser->token_handler(token, tokeniser->token_pw);
	}

	/* Discard current buffer */
	if (tokeniser->buffer->length) {
		parserutils_buffer_discard(tokeniser->buffer, 0,
				tokeniser->buffer->length);
	}

	/* Advance the pointer */
	if (tokeniser->context.pending) {
		parserutils_inputstream_advance(tokeniser->input,
				tokeniser->context.pending);
		tokeniser->context.pending = 0;
	}
}
