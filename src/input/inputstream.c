/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdlib.h>

#include "charset/aliases.h"
#include "input/streamimpl.h"

/**
 * Buffer moving claimant context
 */
struct hubbub_inputstream_bm_handler {
	hubbub_inputstream_buffermoved handler;	/**< Handler function */
	void *pw;				/**< Client private data */

	struct hubbub_inputstream_bm_handler *next;
	struct hubbub_inputstream_bm_handler *prev;
};

extern hubbub_streamhandler utf8stream;
extern hubbub_streamhandler utf16stream;

static hubbub_streamhandler *handler_table[] = {
	&utf8stream,
	&utf16stream,
	NULL
};

/**
 * Create an input stream
 *
 * \param enc      Document charset, or NULL to autodetect
 * \param int_enc  Desired encoding of document
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to stream instance, or NULL on failure
 */
hubbub_inputstream *hubbub_inputstream_create(const char *enc,
		const char *int_enc, hubbub_alloc alloc, void *pw)
{
	hubbub_inputstream *stream;
	hubbub_streamhandler **handler;

	if (int_enc == NULL || alloc == NULL)
		return NULL;

	/* Search for handler class */
	for (handler = handler_table; *handler != NULL; handler++) {
		if ((*handler)->uses_encoding(int_enc))
			break;
	}

	/* None found */
	if ((*handler) == NULL)
		return NULL;

	stream = (*handler)->create(enc, int_enc, alloc, pw);
	if (stream == NULL)
		return NULL;

	stream->handlers = NULL;

	stream->alloc = alloc;
	stream->pw = pw;

	return stream;
}

/**
 * Destroy an input stream
 *
 * \param stream  Input stream to destroy
 */
void hubbub_inputstream_destroy(hubbub_inputstream *stream)
{
	hubbub_inputstream_bm_handler *h, *i;

	if (stream == NULL)
		return;

	for (h = stream->handlers; h; h = i) {
		i = h->next;

		stream->alloc(h, 0, stream->pw);
	}

	stream->destroy(stream);
}

/**
 * Append data to an input stream
 *
 * \param stream  Input stream to append data to
 * \param data    Data to append (in document charset), or NULL to flag EOF
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_append(hubbub_inputstream *stream,
		const uint8_t *data, size_t len)
{
	if (stream == NULL)
		return HUBBUB_BADPARM;

	/* Calling this if we've disowned the buffer is foolish */
	if (stream->buffer == NULL)
		return HUBBUB_INVALID;

	return stream->append(stream, data, len);
}

/**
 * Insert data into stream at current location
 *
 * \param stream  Input stream to insert into
 * \param data    Data to insert (UTF-8 encoded)
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_insert(hubbub_inputstream *stream,
		const uint8_t *data, size_t len)
{
	if (stream == NULL || data == NULL)
		return HUBBUB_BADPARM;

	/* Calling this if we've disowned the buffer is foolish */
	if (stream->buffer == NULL)
		return HUBBUB_INVALID;

	return stream->insert(stream, data, len);
}

/**
 * Look at the next character in the stream
 *
 * \param stream  Stream to look in
 * \return UCS4 (host-endian) character code, or EOF or OOD.
 */
uint32_t hubbub_inputstream_peek(hubbub_inputstream *stream)
{
	/* It is illegal to call this after the buffer has been disowned */
	if (stream == NULL || stream->buffer == NULL)
		return HUBBUB_INPUTSTREAM_OOD;

	return stream->peek(stream);;
}

/**
 * Retrieve the byte index and length of the current character in the stream
 *
 * \param stream  Stream to look in
 * \param len     Pointer to location to receive byte length of character
 * \return Byte index of current character from start of stream,
 *         or (uint32_t) -1 on error
 */
uint32_t hubbub_inputstream_cur_pos(hubbub_inputstream *stream,
		size_t *len)
{
	/* It is illegal to call this after the buffer has been disowned */
	if (stream == NULL || len == NULL || stream->buffer == NULL)
		return (uint32_t) -1;

	return stream->cur_pos(stream, len);
}

/**
 * Convert the current character to lower case
 *
 * \param stream  Stream to look in
 */
void hubbub_inputstream_lowercase(hubbub_inputstream *stream)
{
	if (stream == NULL || stream->buffer == NULL)
		return;

	stream->lowercase(stream);
}

/**
 * Convert the current character to upper case
 *
 * \param stream  Stream to look in
 */
void hubbub_inputstream_uppercase(hubbub_inputstream *stream)
{
	if (stream == NULL || stream->buffer == NULL)
		return;

	stream->uppercase(stream);
}

/**
 * Advance the stream's current position
 *
 * \param stream  The stream whose position to advance
 */
void hubbub_inputstream_advance(hubbub_inputstream *stream)
{
	/* It is illegal to call this after the buffer has been disowned */
	if (stream == NULL || stream->buffer == NULL)
		return;

	if (stream->cursor == stream->buffer_len)
		return;

	stream->advance(stream);
}

/**
 * Push a character back onto the stream
 *
 * \param stream     Stream to push back to
 * \param character  UCS4 (host-endian) codepoint to push back
 * \return HUBBUB_OK on success, appropriate error otherwise
 *
 * Note that this doesn't actually modify the data in the stream.
 * It works by ensuring that the character located just before the
 * current stream location is the same as ::character. If it is,
 * then the stream pointer is moved back. If it is not, then an
 * error is returned and the stream pointer remains unmodified.
 */
hubbub_error hubbub_inputstream_push_back(hubbub_inputstream *stream,
		uint32_t character)
{
	/* It is illegal to call this after the buffer has been disowned */
	if (stream == NULL || stream->buffer == NULL)
		return HUBBUB_BADPARM;

	if (stream->cursor == 0)
		return HUBBUB_INVALID;

	return stream->push_back(stream, character);
}

/**
 * Rewind the input stream by a number of bytes
 *
 * \param stream  Stream to rewind
 * \param n       Number of bytes to go back
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_rewind(hubbub_inputstream *stream, size_t n)
{
	if (stream == NULL || stream->buffer == NULL)
		return HUBBUB_BADPARM;

	if (stream->cursor < n)
		return HUBBUB_INVALID;

	stream->cursor -= n;

	return HUBBUB_OK;
}

/**
 * Claim ownership of an input stream's buffer
 *
 * \param stream  Input stream whose buffer to claim
 * \param buffer  Pointer to location to receive buffer pointer
 * \param len     Pointer to location to receive byte length of buffer
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Once the buffer has been claimed by a client, the input stream disclaims
 * all ownership rights (and invalidates any internal references it may have
 * to the buffer). Therefore, the only input stream call which may be made
 * after calling this function is to destroy the input stream. Therefore,
 * unless the stream pointer is located at EOF, this call will return an
 * error.
 */
hubbub_error hubbub_inputstream_claim_buffer(hubbub_inputstream *stream,
		uint8_t **buffer, size_t *len)
{
	if (stream == NULL || buffer == NULL || len == NULL)
		return HUBBUB_BADPARM;

	if (stream->had_eof == false ||
			stream->cursor != stream->buffer_len)
		return HUBBUB_INVALID;

	*buffer = stream->buffer;
	*len = stream->buffer_len;

	stream->buffer = NULL;

	return HUBBUB_OK;
}

/**
 * Register interest in buffer moved events
 *
 * \param stream   Input stream to register interest with
 * \param handler  Pointer to handler function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_register_movehandler(
		hubbub_inputstream *stream,
		hubbub_inputstream_buffermoved handler, void *pw)
{
	hubbub_inputstream_bm_handler *h;

	if (stream == NULL || handler == NULL)
		return HUBBUB_BADPARM;

	h = stream->alloc(NULL, sizeof(hubbub_inputstream_bm_handler),
			stream->pw);
	if (h == NULL)
		return HUBBUB_NOMEM;

	h->handler = handler;
	h->pw = pw;

	h->prev = NULL;
	h->next = stream->handlers;

	if (stream->handlers)
		stream->handlers->prev = h;
	stream->handlers = h;

	/* And notify claimant of current buffer location */
	handler(stream->buffer, stream->buffer_len, pw);

	return HUBBUB_OK;
}

/**
 * Deregister interest in buffer moved events
 *
 * \param stream   Input stream to deregister from
 * \param handler  Pointer to handler function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_deregister_movehandler(
		hubbub_inputstream *stream,
		hubbub_inputstream_buffermoved handler, void *pw)
{
	hubbub_inputstream_bm_handler *h;

	if (stream == NULL || handler == NULL)
		return HUBBUB_BADPARM;

	for (h = stream->handlers; h; h = h->next) {
		if (h->handler == handler && h->pw == pw)
			break;
	}

	if (h == NULL)
		return HUBBUB_INVALID;

	if (h->next)
		h->next->prev = h->prev;
	if (h->prev)
		h->prev->next = h->next;
	else
		stream->handlers = h->next;

	stream->alloc(h, 0, stream->pw);

	return HUBBUB_OK;
}

/**
 * Case insensitively compare a pair of ranges in the input stream
 *
 * \param stream  Input stream to look in
 * \param r1      Offset of start of first range
 * \param r2      Offset of start of second range
 * \param len     Byte length of ranges
 * \return 0 if ranges match, non-zero otherwise
 */
int hubbub_inputstream_compare_range_ci(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len)
{
	if (stream == NULL || stream->buffer == NULL)
		return 1; /* arbitrary */

	return stream->cmp_range_ci(stream, r1, r2, len);
}

/**
 * Case sensitively compare a pair of ranges in the input stream
 *
 * \param stream  Input stream to look in
 * \param r1      Offset of start of first range
 * \param r2      Offset of start of second range
 * \param len     Byte length of ranges
 * \return 0 if ranges match, non-zero otherwise
 */
int hubbub_inputstream_compare_range_cs(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len)
{
	if (stream == NULL || stream->buffer == NULL)
		return 1; /* arbitrary */

	return stream->cmp_range_cs(stream, r1, r2, len);
}

/**
 * Case sensitively compare a range of input stream against an ASCII string
 *
 * \param stream  Input stream to look in
 * \param off     Offset of range start
 * \param len     Byte length of range
 * \param data    Comparison string
 * \param dlen    Byte length of comparison string
 * \return 0 if match, non-zero otherwise
 */
int hubbub_inputstream_compare_range_ascii(hubbub_inputstream *stream,
		uint32_t off, size_t len, const char *data, size_t dlen)
{
	if (stream == NULL || stream->buffer == NULL)
		return 1; /* arbitrary */

	return stream->cmp_range_ascii(stream, off, len, data, dlen);
}

/**
 * Replace a range of bytes in the input stream with a single character
 *
 * \param stream  Input stream containing data
 * \param start   Offset of start of range to replace
 * \param len     Length (in bytes) of range to replace
 * \param ucs4    UCS4 (host endian) encoded replacement character
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_inputstream_replace_range(hubbub_inputstream *stream,
		uint32_t start, size_t len, uint32_t ucs4)
{
	if (stream == NULL || stream->buffer == NULL)
		return HUBBUB_BADPARM;

	if (start >= stream->buffer_len)
		return HUBBUB_INVALID;

	if (start < stream->cursor)
		return HUBBUB_INVALID;

	return stream->replace_range(stream, start, len, ucs4);
}

/**
 * Read the document charset
 *
 * \param stream  Input stream to query
 * \param source  Pointer to location to receive charset source
 * \return Pointer to charset name (constant; do not free), or NULL if unknown
 */
const char *hubbub_inputstream_read_charset(hubbub_inputstream *stream,
		hubbub_charset_source *source)
{
	if (stream == NULL || source == NULL)
		return NULL;

	*source = stream->encsrc;

	if (stream->encsrc == HUBBUB_CHARSET_UNKNOWN)
		return NULL;

	return hubbub_mibenum_to_name(stream->mibenum);
}

/**
 * Inform interested parties that the buffer has moved
 *
 * \param stream  Input stream
 */
void hubbub_inputstream_buffer_moved(hubbub_inputstream *stream)
{
	hubbub_inputstream_bm_handler *h;

	if (stream == NULL)
		return;

	for (h = stream->handlers; h; h = h->next)
		h->handler(stream->buffer, stream->buffer_len, h->pw);
}

