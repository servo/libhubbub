/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdlib.h>
#include <string.h>

/* These two are for htonl / ntohl */
#include <arpa/inet.h>
#include <netinet/in.h>

#include "charset/aliases.h"
#include "utils/utf16.h"
#include "utils/utils.h"

#include "codec_impl.h"

/**
 * UTF-16 charset codec
 */
typedef struct hubbub_utf16_codec {
	hubbub_charsetcodec base;	/**< Base class */

#define INVAL_BUFSIZE (32)
	uint8_t inval_buf[INVAL_BUFSIZE];	/**< Buffer for fixing up
						 * incomplete input
						 * sequences */
	size_t inval_len;		/*< Byte length of inval_buf **/

#define READ_BUFSIZE (8)
	uint32_t read_buf[READ_BUFSIZE];	/**< Buffer for partial
						 * output sequences (decode)
						 * (host-endian) */
	size_t read_len;		/**< Character length of read_buf */

#define WRITE_BUFSIZE (8)
	uint32_t write_buf[WRITE_BUFSIZE];	/**< Buffer for partial
						 * output sequences (encode)
						 * (host-endian) */
	size_t write_len;		/**< Character length of write_buf */

} hubbub_utf16_codec;

static bool hubbub_utf16_codec_handles_charset(const char *charset);
static hubbub_charsetcodec *hubbub_utf16_codec_create(const char *charset,
		hubbub_alloc alloc, void *pw);
static void hubbub_utf16_codec_destroy (hubbub_charsetcodec *codec);
static hubbub_error hubbub_utf16_codec_encode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static hubbub_error hubbub_utf16_codec_decode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static hubbub_error hubbub_utf16_codec_reset(hubbub_charsetcodec *codec);
static hubbub_error hubbub_utf16_codec_read_char(hubbub_utf16_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static hubbub_error hubbub_utf16_codec_filter_decoded_char(
		hubbub_utf16_codec *c,
		uint32_t ucs4, uint8_t **dest, size_t *destlen);

/**
 * Determine whether this codec handles a specific charset
 *
 * \param charset  Charset to test
 * \return true if handleable, false otherwise
 */
bool hubbub_utf16_codec_handles_charset(const char *charset)
{
	return hubbub_mibenum_from_name(charset, strlen(charset)) ==
			hubbub_mibenum_from_name("UTF-16", SLEN("UTF-16"));
}

/**
 * Create a utf16 codec
 *
 * \param charset  The charset to read from / write to
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to codec, or NULL on failure
 */
hubbub_charsetcodec *hubbub_utf16_codec_create(const char *charset,
		hubbub_alloc alloc, void *pw)
{
	hubbub_utf16_codec *codec;

	UNUSED(charset);

	codec = alloc(NULL, sizeof(hubbub_utf16_codec), pw);
	if (codec == NULL)
		return NULL;

	codec->inval_buf[0] = '\0';
	codec->inval_len = 0;

	codec->read_buf[0] = 0;
	codec->read_len = 0;

	codec->write_buf[0] = 0;
	codec->write_len = 0;

	/* Finally, populate vtable */
	codec->base.handler.destroy = hubbub_utf16_codec_destroy;
	codec->base.handler.encode = hubbub_utf16_codec_encode;
	codec->base.handler.decode = hubbub_utf16_codec_decode;
	codec->base.handler.reset = hubbub_utf16_codec_reset;

	return (hubbub_charsetcodec *) codec;
}

/**
 * Destroy a utf16 codec
 *
 * \param codec  The codec to destroy
 */
void hubbub_utf16_codec_destroy (hubbub_charsetcodec *codec)
{
	UNUSED(codec);
}

/**
 * Encode a chunk of UCS4 data into utf16
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return HUBBUB_OK          on success,
 *         HUBBUB_NOMEM       if output buffer is too small,
 *         HUBBUB_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
 *         <any_other_error>  as a result of the failure of the
 *                            client-provided filter function.
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read. Any remaining output for the character will be buffered by the
 * codec for writing on the next call. This buffered data is post-filtering,
 * so will not be refiltered on the next call.
 *
 * In the case of the filter function failing, ::source will point _at_ the
 * last input character read; nothing will be written or buffered for the
 * failed character. It is up to the client to fix the cause of the failure
 * and retry the encoding process.
 *
 * Note that, if failure occurs whilst attempting to write any output
 * buffered by the last call, then ::source and ::sourcelen will remain
 * unchanged (as nothing more has been read).
 *
 * There is no way to determine the output character which caused a
 * failure (as it may be one in a filter-injected replacement sequence).
 * It is, however, possible to determine which source character caused it
 * (this being the character immediately before the location pointed to by
 * ::source on exit).
 *
 * [I.e. the process of filtering results in a potential one-to-many mapping
 * between source characters and output characters, and identification of
 * individual output characters is impossible.]
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 */
hubbub_error hubbub_utf16_codec_encode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	hubbub_utf16_codec *c = (hubbub_utf16_codec *) codec;
	uint32_t ucs4;
	uint32_t *towrite;
	size_t towritelen;
	hubbub_error error;

	/* Process any outstanding characters from the previous call */
	if (c->write_len > 0) {
		uint32_t *pwrite = c->write_buf;
		uint8_t buf[4];
		size_t len;

		while (c->write_len > 0) {
			error = hubbub_utf16_from_ucs4(pwrite[0], buf, &len);
			if (error != HUBBUB_OK)
				abort();

			if (*destlen < len) {
				/* Insufficient output buffer space */
				for (len = 0; len < c->write_len; len++)
					c->write_buf[len] = pwrite[len];

				return HUBBUB_NOMEM;
			}

			memcpy(*dest, buf, len);

			*dest += len;
			*destlen -= len;

			pwrite++;
			c->write_len--;
		}
	}

	/* Now process the characters for this call */
	while (*sourcelen > 0) {
		ucs4 = ntohl(*((uint32_t *) (void *) *source));
		towrite = &ucs4;
		towritelen = 1;

		/* Run character we're about to output through the
		 * registered filter, so it can replace it. */
		if (c->base.filter != NULL) {
			error = c->base.filter(ucs4,
					&towrite, &towritelen,
					c->base.filter_pw);
			if (error != HUBBUB_OK)
				return error;
		}

		/* Output current characters */
		while (towritelen > 0) {
			uint8_t buf[4];
			size_t len;

			error = hubbub_utf16_from_ucs4(towrite[0], buf, &len);
			if (error != HUBBUB_OK)
				abort();

			if (*destlen < len) {
				/* Insufficient output space */
				if (towritelen >= WRITE_BUFSIZE)
					abort();

				c->write_len = towritelen;

				/* Copy pending chars to save area, for
				 * processing next call. */
				for (len = 0; len < towritelen; len++)
					c->write_buf[len] = towrite[len];

				/* Claim character we've just buffered,
				 * so it's not reprocessed */
				*source += 4;
				*sourcelen -= 4;

				return HUBBUB_NOMEM;
			}

			memcpy(*dest, buf, len);

			*dest += len;
			*destlen -= len;

			towrite++;
			towritelen--;
		}

		*source += 4;
		*sourcelen -= 4;
	}

	return HUBBUB_OK;
}

/**
 * Decode a chunk of utf16 data into UCS4
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return HUBBUB_OK          on success,
 *         HUBBUB_NOMEM       if output buffer is too small,
 *         HUBBUB_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
 *         <any_other_error>  as a result of the failure of the
 *                            client-provided filter function.
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read, if the result is _OK or _NOMEM. Any remaining output for the
 * character will be buffered by the codec for writing on the next call.
 * This buffered data is post-filtering, so will not be refiltered on the
 * next call.
 *
 * In the case of the result being _INVALID or the filter function failing,
 * ::source will point _at_ the last input character read; nothing will be
 * written or buffered for the failed character. It is up to the client to
 * fix the cause of the failure and retry the decoding process.
 *
 * Note that, if failure occurs whilst attempting to write any output
 * buffered by the last call, then ::source and ::sourcelen will remain
 * unchanged (as nothing more has been read).
 *
 * There is no way to determine the output character which caused a
 * failure (as it may be one in a filter-injected replacement sequence).
 * It is, however, possible to determine which source character caused it
 * (this being the character immediately at or before the location pointed
 * to by ::source on exit).
 *
 * [I.e. the process of filtering results in a potential one-to-many mapping
 * between source characters and output characters, and identification of
 * individual output characters is impossible.]
 *
 * If STRICT error handling is configured and an illegal sequence is split
 * over two calls, then _INVALID will be returned from the second call,
 * but ::source will point mid-way through the invalid sequence (i.e. it
 * will be unmodified over the second call). In addition, the internal
 * incomplete-sequence buffer will be emptied, such that subsequent calls
 * will progress, rather than re-evaluating the same invalid sequence.
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 *
 * Call this with a source length of 0 to flush the output buffer.
 */
hubbub_error hubbub_utf16_codec_decode(hubbub_charsetcodec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	hubbub_utf16_codec *c = (hubbub_utf16_codec *) codec;
	hubbub_error error;

	if (c->read_len > 0) {
		/* Output left over from last decode */
		uint32_t *pread = c->read_buf;

		while (c->read_len > 0 && *destlen >= c->read_len * 4) {
			*((uint32_t *) (void *) *dest) = htonl(pread[0]);

			*dest += 4;
			*destlen -= 4;

			pread++;
			c->read_len--;
		}

		if (*destlen < c->read_len * 4) {
			/* Ran out of output buffer */
			size_t i;

			/* Shuffle remaining output down */
			for (i = 0; i < c->read_len; i++)
				c->read_buf[i] = pread[i];

			return HUBBUB_NOMEM;
		}
	}

	if (c->inval_len > 0) {
		/* The last decode ended in an incomplete sequence.
		 * Fill up inval_buf with data from the start of the
		 * new chunk and process it. */
		uint8_t *in = c->inval_buf;
		size_t ol = c->inval_len;
		size_t l = min(INVAL_BUFSIZE - ol - 1, *sourcelen);
		size_t orig_l = l;

		memcpy(c->inval_buf + ol, *source, l);

		l += c->inval_len;

		error = hubbub_utf16_codec_read_char(c,
				(const uint8_t **) &in, &l, dest, destlen);
		if (error != HUBBUB_OK && error != HUBBUB_NOMEM) {
			return error;
		}

		/* And now, fix up source pointers */
		*source += max((signed) (orig_l - l), 0);
		*sourcelen -= max((signed) (orig_l - l), 0);

		/* Failed to resolve an incomplete character and
		 * ran out of buffer space. No recovery strategy
		 * possible, so explode everywhere. */
		if ((orig_l + ol) - l == 0)
			abort();

		/* Report memory exhaustion case from above */
		if (error != HUBBUB_OK)
			return error;
	}

	/* Finally, the "normal" case; process all outstanding characters */
	while (*sourcelen > 0) {
		error = hubbub_utf16_codec_read_char(c,
				source, sourcelen, dest, destlen);
		if (error != HUBBUB_OK) {
			return error;
		}
	}

	return HUBBUB_OK;
}

/**
 * Clear a utf16 codec's encoding state
 *
 * \param codec  The codec to reset
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_utf16_codec_reset(hubbub_charsetcodec *codec)
{
	hubbub_utf16_codec *c = (hubbub_utf16_codec *) codec;

	c->inval_buf[0] = '\0';
	c->inval_len = 0;

	c->read_buf[0] = 0;
	c->read_len = 0;

	c->write_buf[0] = 0;
	c->write_len = 0;

	return HUBBUB_OK;
}


/**
 * Read a character from the UTF-16 to UCS4 (big endian)
 *
 * \param c          The codec
 * \param source     Pointer to pointer to source buffer (updated on exit)
 * \param sourcelen  Pointer to length of source buffer (updated on exit)
 * \param dest       Pointer to pointer to output buffer (updated on exit)
 * \param destlen    Pointer to length of output buffer (updated on exit)
 * \return HUBBUB_OK on success,
 *         HUBBUB_NOMEM       if output buffer is too small,
 *         HUBBUB_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
 *         <any_other_error>  as a result of the failure of the
 *                            client-provided filter function.
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read, if the result is _OK or _NOMEM. Any remaining output for the
 * character will be buffered by the codec for writing on the next call.
 * This buffered data is post-filtering, so will not be refiltered on the
 * next call.
 *
 * In the case of the result being _INVALID or the filter function failing,
 * ::source will point _at_ the last input character read; nothing will be
 * written or buffered for the failed character. It is up to the client to
 * fix the cause of the failure and retry the decoding process.
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 */
hubbub_error hubbub_utf16_codec_read_char(hubbub_utf16_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	uint32_t ucs4;
	size_t sucs4;
	hubbub_error error;

	/* Convert a single character */
	error = hubbub_utf16_to_ucs4(*source, *sourcelen, &ucs4, &sucs4);
	if (error == HUBBUB_OK) {
		/* Read a character */
		error = hubbub_utf16_codec_filter_decoded_char(c,
				ucs4, dest, destlen);
		if (error == HUBBUB_OK || error == HUBBUB_NOMEM) {
			/* filter function succeeded; update source pointers */
			*source += sucs4;
			*sourcelen -= sucs4;
		}

		/* Clear inval buffer */
		c->inval_buf[0] = '\0';
		c->inval_len = 0;

		return error;
	} else if (error == HUBBUB_NEEDDATA) {
		/* Incomplete input sequence */
		if (*sourcelen > INVAL_BUFSIZE)
			abort();

		memmove(c->inval_buf, (char *) *source, *sourcelen);
		c->inval_buf[*sourcelen] = '\0';
		c->inval_len = *sourcelen;

		*source += *sourcelen;
		*sourcelen = 0;

		return HUBBUB_OK;
	} else if (error == HUBBUB_INVALID) {
		/* Illegal input sequence */
		uint32_t nextchar;

		/* Clear inval buffer */
		c->inval_buf[0] = '\0';
		c->inval_len = 0;

		/* Strict errormode; simply flag invalid character */
		if (c->base.errormode == HUBBUB_CHARSETCODEC_ERROR_STRICT) {
			return HUBBUB_INVALID;
		}

		/* Find next valid UTF-16 sequence.
		 * We're processing client-provided data, so let's
		 * be paranoid about its validity. */
		error = hubbub_utf16_next_paranoid(*source, *sourcelen,
				0, &nextchar);
		if (error != HUBBUB_OK) {
			if (error == HUBBUB_NEEDDATA) {
				/* Need more data to be sure */
				if (*sourcelen > INVAL_BUFSIZE)
					abort();

				memmove(c->inval_buf, (char *) *source,
						*sourcelen);
				c->inval_buf[*sourcelen] = '\0';
				c->inval_len = *sourcelen;

				*source += *sourcelen;
				*sourcelen = 0;

				nextchar = 0;
			} else {
				return error;
			}
		}

		/* output U+FFFD and continue processing. */
		error = hubbub_utf16_codec_filter_decoded_char(c,
				0xFFFD, dest, destlen);
		if (error == HUBBUB_OK || error == HUBBUB_NOMEM) {
			/* filter function succeeded; update source pointers */
			*source += nextchar;
			*sourcelen -= nextchar;
		}

		return error;
	}

	return HUBBUB_OK;
}

/**
 * Feed a UCS4 character through the registered filter and output the result
 *
 * \param c        Codec to use
 * \param ucs4     UCS4 character (host endian)
 * \param dest     Pointer to pointer to output buffer
 * \param destlen  Pointer to output buffer length
 * \return HUBBUB_OK          on success,
 *         HUBBUB_NOMEM       if output buffer is too small,
 *         <any_other_error>  as a result of the failure of the
 *                            client-provided filter function.
 */
hubbub_error hubbub_utf16_codec_filter_decoded_char(hubbub_utf16_codec *c,
		uint32_t ucs4, uint8_t **dest, size_t *destlen)
{
	if (c->base.filter != NULL) {
		uint32_t *rep;
		size_t replen;
		hubbub_error error;

		error = c->base.filter(ucs4, &rep, &replen,
				c->base.filter_pw);
		if (error != HUBBUB_OK) {
			return error;
		}

		while (replen > 0 && *destlen >= replen * 4) {
			*((uint32_t *) (void *) *dest) = htonl(*rep);

			*dest += 4;
			*destlen -= 4;

			rep++;
			replen--;
		}

		if (*destlen < replen * 4) {
			/* Run out of output buffer */
			size_t i;

			/* Buffer remaining output */
			c->read_len = replen;

			for (i = 0; i < replen; i++) {
				c->read_buf[i] = rep[i];
			}

			return HUBBUB_NOMEM;
		}

	} else {
		if (*destlen < 4) {
			/* Run out of output buffer */
			c->read_len = 1;
			c->read_buf[0] = ucs4;

			return HUBBUB_NOMEM;
		}

		*((uint32_t *) (void *) *dest) = htonl(ucs4);
		*dest += 4;
		*destlen -= 4;
	}

	return HUBBUB_OK;
}


const hubbub_charsethandler hubbub_utf16_codec_handler = {
	hubbub_utf16_codec_handles_charset,
	hubbub_utf16_codec_create
};
