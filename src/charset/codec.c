/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include "charset/aliases.h"

#include "codec_impl.h"

extern hubbub_charsethandler hubbub_iconv_codec_handler;
extern hubbub_charsethandler hubbub_utf8_codec_handler;
extern hubbub_charsethandler hubbub_utf16_codec_handler;

static hubbub_charsethandler *handler_table[] = {
	&hubbub_utf8_codec_handler,
	&hubbub_utf16_codec_handler,
	&hubbub_iconv_codec_handler,
	NULL,
};

/**
 * Create a charset codec
 *
 * \param charset  Target charset
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to codec instance, or NULL on failure
 */
hubbub_charsetcodec *hubbub_charsetcodec_create(const char *charset,
		hubbub_alloc alloc, void *pw)
{
	hubbub_charsetcodec *codec;
	hubbub_charsethandler **handler;
	const hubbub_aliases_canon * canon;

	if (charset == NULL || alloc == NULL)
		return NULL;

	/* Canonicalise charset name. */
	canon = hubbub_alias_canonicalise(charset, strlen(charset));
	if (canon == NULL)
		return NULL;

	/* Search for handler class */
	for (handler = handler_table; *handler != NULL; handler++) {
		if ((*handler)->handles_charset(canon->name))
			break;
	}

	/* None found */
	if ((*handler) == NULL)
		return NULL;

	/* Instantiate class */
	codec = (*handler)->create(canon->name, alloc, pw);
	if (codec == NULL)
		return NULL;

	/* and initialise it */
	codec->mibenum = canon->mib_enum;

	codec->filter = NULL;
	codec->filter_pw = NULL;

	codec->errormode = HUBBUB_CHARSETCODEC_ERROR_LOOSE;

	codec->alloc = alloc;
	codec->alloc_pw = pw;

	return codec;
}

/**
 * Destroy a charset codec
 *
 * \param codec  The codec to destroy
 */
void hubbub_charsetcodec_destroy(hubbub_charsetcodec *codec)
{
	if (codec == NULL)
		return;

	codec->handler.destroy(codec);

	codec->alloc(codec, 0, codec->alloc_pw);
}

/**
 * Configure a charset codec
 *
 * \param codec   The codec to configure
 * \parem type    The codec option type to configure
 * \param params  Option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_charsetcodec_setopt(hubbub_charsetcodec *codec,
		hubbub_charsetcodec_opttype type,
		hubbub_charsetcodec_optparams *params)
{
	if (codec == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_CHARSETCODEC_FILTER_FUNC:
		codec->filter = params->filter_func.filter;
		codec->filter_pw = params->filter_func.pw;
		break;

	case HUBBUB_CHARSETCODEC_ERROR_MODE:
		codec->errormode = params->error_mode.mode;
		break;
	}

	return HUBBUB_OK;
}

/**
 * Encode a chunk of UCS4 data into a codec's charset
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * source, sourcelen, dest and destlen will be updated appropriately on exit
 */
hubbub_error hubbub_charsetcodec_encode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	if (codec == NULL || source == NULL || *source == NULL ||
			sourcelen == NULL || dest == NULL || *dest == NULL ||
			destlen == NULL)
		return HUBBUB_BADPARM;

	return codec->handler.encode(codec, source, sourcelen, dest, destlen);
}

/**
 * Decode a chunk of data in a codec's charset into UCS4
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * source, sourcelen, dest and destlen will be updated appropriately on exit
 *
 * Call this with a source length of 0 to flush any buffers.
 */
hubbub_error hubbub_charsetcodec_decode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	if (codec == NULL || source == NULL || *source == NULL ||
			sourcelen == NULL || dest == NULL || *dest == NULL ||
			destlen == NULL)
		return HUBBUB_BADPARM;

	return codec->handler.decode(codec, source, sourcelen, dest, destlen);
}

/**
 * Clear a charset codec's encoding state
 *
 * \param codec  The codec to reset
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_charsetcodec_reset(hubbub_charsetcodec *codec)
{
	if (codec == NULL)
		return HUBBUB_BADPARM;

	/* Reset filter */
	if (codec->filter)
		codec->filter(HUBBUB_CHARSETCODEC_NULL, NULL, NULL, NULL);

	return codec->handler.reset(codec);
}

