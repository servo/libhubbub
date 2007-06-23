/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_charset_codec_h_
#define hubbub_charset_codec_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>

typedef struct hubbub_charsetcodec hubbub_charsetcodec;

#define HUBBUB_CHARSETCODEC_NULL (0xffffffffU)

/**
 * Type of charset codec filter function
 *
 * \param c          UCS4 character (in host byte order) or
 *                   HUBBUB_CHARSETCODEC_NULL to reset
 * \param output     Pointer to location to store output buffer location
 * \param outputlen  Pointer to location to store output buffer length
 * \param pw         Pointer to client-specific private data
 * \return HUBBUB_OK on success, or appropriate error otherwise.
 *
 * The output buffer is owned by the filter code and will not be freed by
 * any charset codec. It should contain the replacement UCS4 character(s)
 * for the input. The replacement characters should be in host byte order.
 * The contents of *output and *outputlen on entry are ignored and these
 * will be filled in by the filter code.
 *
 * Filters may elect to replace the input character with no output. In this
 * case, *output should be set to NULL and *outputlen should be set to 0 and
 * HUBBUB_OK should be returned.
 *
 * The output length is in terms of the number of UCS4 characters in the
 * output buffer. i.e.:
 *
 * for (size_t i = 0; i < outputlen; i++) {
 *   dest[curchar++] = output[i];
 * }
 *
 * would copy the contents of the filter output buffer to the codec's output
 * buffer.
 */
typedef hubbub_error (*hubbub_charsetcodec_filter)(uint32_t c,
		uint32_t **output, size_t *outputlen, void *pw);

/**
 * Charset codec error mode
 *
 * A codec's error mode determines its behaviour in the face of:
 *
 * + characters which are unrepresentable in the destination charset (if
 *   encoding data) or which cannot be converted to UCS4 (if decoding data).
 * + invalid byte sequences (both encoding and decoding)
 *
 * The options provide a choice between the following approaches:
 *
 * + draconian, "stop processing" ("strict")
 * + "replace the unrepresentable character with something else" ("loose")
 * + "attempt to transliterate, or replace if unable" ("translit")
 *
 * The default error mode is "loose".
 *
 *
 * In the "loose" case, the replacement character will depend upon:
 *
 * + Whether the operation was encoding or decoding
 * + If encoding, what the destination charset is.
 *
 * If decoding, the replacement character will be:
 *
 *     U+FFFD (REPLACEMENT CHARACTER)
 *
 * If encoding, the replacement character will be:
 *
 *     U+003F (QUESTION MARK) if the destination charset is not UTF-(8|16|32)
 *     U+FFFD (REPLACEMENT CHARACTER) otherwise.
 *
 *
 * In the "translit" case, the codec will attempt to transliterate into
 * the destination charset, if encoding. If decoding, or if transliteration
 * fails, this option is identical to "loose".
 */
typedef enum hubbub_charsetcodec_errormode {
	/** Abort processing if unrepresentable character encountered */
	HUBBUB_CHARSETCODEC_ERROR_STRICT   = 0,
	/** Replace unrepresentable characters with single alternate */
	HUBBUB_CHARSETCODEC_ERROR_LOOSE    = 1,
	/** Transliterate unrepresentable characters, if possible */
	HUBBUB_CHARSETCODEC_ERROR_TRANSLIT = 2,
} hubbub_charsetcodec_errormode;

/**
 * Charset codec option types
 */
typedef enum hubbub_charsetcodec_opttype {
	/** Register codec filter function */
	HUBBUB_CHARSETCODEC_FILTER_FUNC = 0,
	/** Set codec error mode */
	HUBBUB_CHARSETCODEC_ERROR_MODE  = 1,
} hubbub_charsetcodec_opttype;

/**
 * Charset codec option parameters
 */
typedef union hubbub_charsetcodec_optparams {
	/** Parameters for filter function setting */
	struct {
		/** Filter function */
		hubbub_charsetcodec_filter filter;
		/** Client-specific private data */
		void *pw;
	} filter_func;

	/** Parameters for error mode setting */
	struct {
		/** The desired error handling mode */
		hubbub_charsetcodec_errormode mode;
	} error_mode;
} hubbub_charsetcodec_optparams;


/* Create a charset codec */
hubbub_charsetcodec *hubbub_charsetcodec_create(const char *charset,
		hubbub_alloc alloc, void *pw);
/* Destroy a charset codec */
void hubbub_charsetcodec_destroy(hubbub_charsetcodec *codec);

/* Configure a charset codec */
hubbub_error hubbub_charsetcodec_setopt(hubbub_charsetcodec *codec,
		hubbub_charsetcodec_opttype type,
		hubbub_charsetcodec_optparams *params);

/* Encode a chunk of UCS4 data into a codec's charset */
hubbub_error hubbub_charsetcodec_encode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);

/* Decode a chunk of data in a codec's charset into UCS4 */
hubbub_error hubbub_charsetcodec_decode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);

/* Reset a charset codec */
hubbub_error hubbub_charsetcodec_reset(hubbub_charsetcodec *codec);

#endif
