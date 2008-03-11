/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_types_h_
#define hubbub_types_h_

#include <stdbool.h>
#include <inttypes.h>

/** Source of charset information, in order of importance
 * A client-dictated charset will override all others.
 * A document-specified charset will override autodetection or the default */
typedef enum hubbub_charset_source {
	HUBBUB_CHARSET_UNKNOWN          = 0,	/**< Unknown */
	HUBBUB_CHARSET_DEFAULT          = 1,	/**< Default setting */
	HUBBUB_CHARSET_DETECTED         = 2,	/**< Autodetected */
	HUBBUB_CHARSET_DOCUMENT         = 3,	/**< Defined in document */
	HUBBUB_CHARSET_DICTATED         = 4,	/**< Dictated by client */
} hubbub_charset_source;

/**
 * Content model flag
 */
typedef enum hubbub_content_model {
	HUBBUB_CONTENT_MODEL_PCDATA,
	HUBBUB_CONTENT_MODEL_RCDATA,
	HUBBUB_CONTENT_MODEL_CDATA,
	HUBBUB_CONTENT_MODEL_PLAINTEXT
} hubbub_content_model;

/**
 * Quirks mode flag
 */
typedef enum hubbub_quirks_mode {
	HUBBUB_QUIRKS_MODE_NONE,
	HUBBUB_QUIRKS_MODE_LIMITED,
	HUBBUB_QUIRKS_MODE_FULL
} hubbub_quirks_mode;

/**
 * Type of an emitted token
 */
typedef enum hubbub_token_type {
	HUBBUB_TOKEN_DOCTYPE,
	HUBBUB_TOKEN_START_TAG,
	HUBBUB_TOKEN_END_TAG,
	HUBBUB_TOKEN_COMMENT,
	HUBBUB_TOKEN_CHARACTER,
	HUBBUB_TOKEN_EOF
} hubbub_token_type;

/**
 * Tokeniser string type
 */
typedef struct hubbub_string {
	uint32_t data_off;		/**< Byte offset of string start */
	size_t len;			/**< Byte length of string */
} hubbub_string;

/**
 * Tag attribute data
 */
typedef struct hubbub_attribute {
	hubbub_string name;		/**< Attribute name */
	hubbub_string value;		/**< Attribute value */
} hubbub_attribute;

/**
 * Data for doctype token
 */
typedef struct hubbub_doctype {
	hubbub_string name;		/**< Doctype name */
	bool correct;			/**< Doctype validity flag */
} hubbub_doctype;

/**
 * Data for a tag
 */
typedef struct hubbub_tag {
	hubbub_string name;		/**< Tag name */
	uint32_t n_attributes;		/**< Count of attributes */
	hubbub_attribute *attributes;	/**< Array of attribute data */
} hubbub_tag;

/**
 * Token data
 */
typedef struct hubbub_token {
	hubbub_token_type type;

	union {
		hubbub_doctype doctype;

		hubbub_tag tag;

		hubbub_string comment;

		hubbub_string character;
	} data;
} hubbub_token;

#endif
