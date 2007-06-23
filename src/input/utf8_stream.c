/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdbool.h>
#include <string.h>

#include "charset/aliases.h"
#include "charset/detect.h"
#include "input/streamimpl.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#define BUFFER_CHUNK (4096)

static bool hubbub_utf8stream_uses_encoding(const char *int_enc);
static hubbub_inputstream *hubbub_utf8stream_create(const char *enc,
		const char *int_enc, hubbub_alloc alloc, void *pw);
static void hubbub_utf8stream_destroy(hubbub_inputstream *stream);
static hubbub_error hubbub_utf8stream_append(hubbub_inputstream *stream,
		const uint8_t *data, size_t len);
static hubbub_error hubbub_utf8stream_insert(hubbub_inputstream *stream,
		const uint8_t *data, size_t len);
static uint32_t hubbub_utf8stream_peek(hubbub_inputstream *stream);
static uint32_t hubbub_utf8stream_cur_pos(hubbub_inputstream *stream,
		size_t *len);
static void hubbub_utf8stream_lowercase(hubbub_inputstream *stream);
static void hubbub_utf8stream_uppercase(hubbub_inputstream *stream);
static void hubbub_utf8stream_advance(hubbub_inputstream *stream);
static hubbub_error hubbub_utf8stream_push_back(hubbub_inputstream *stream,
		uint32_t character);
static int hubbub_utf8stream_compare_range_ci(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len);
static int hubbub_utf8stream_compare_range_cs(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len);
static int hubbub_utf8stream_compare_range_ascii(hubbub_inputstream *stream,
		uint32_t off, size_t len, const char *data, size_t dlen);
static hubbub_error hubbub_utf8stream_replace_range(
		hubbub_inputstream *stream,
		uint32_t start, size_t len, uint32_t ucs4);

/**
 * Determine whether a stream implementation uses an internal encoding
 *
 * \param int_enc  The desired encoding
 * \return true if handled, false otherwise
 */
bool hubbub_utf8stream_uses_encoding(const char *int_enc)
{
	return (hubbub_mibenum_from_name(int_enc, strlen(int_enc)) ==
			hubbub_mibenum_from_name("UTF-8", SLEN("UTF-8")));
}

/**
 * Create an input stream
 *
 * \param enc      Document charset, or NULL if unknown
 * \param int_enc  Desired encoding of document
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to stream instance, or NULL on failure
 */
hubbub_inputstream *hubbub_utf8stream_create(const char *enc,
		const char *int_enc, hubbub_alloc alloc, void *pw)
{
	hubbub_inputstream *stream;

	if (hubbub_mibenum_from_name(int_enc, strlen(int_enc)) !=
			hubbub_mibenum_from_name("UTF-8", SLEN("UTF-8")))
		return NULL;

	stream = alloc(NULL, sizeof(hubbub_inputstream), pw);
	if (stream == NULL)
		return NULL;

	stream->buffer = alloc(NULL, BUFFER_CHUNK, pw);
	if (stream->buffer == NULL) {
		alloc(stream, 0, pw);
		return NULL;
	}

	stream->buffer_len = 0;
	stream->buffer_alloc = BUFFER_CHUNK;

	stream->cursor = 0;

	stream->had_eof = false;

	stream->input = hubbub_filter_create(int_enc, alloc, pw);
	if (stream->input == NULL) {
		alloc(stream->buffer, 0, pw);
		alloc(stream, 0, pw);
		return NULL;
	}

	if (enc != NULL) {
		hubbub_error error;
		hubbub_filter_optparams params;

		stream->mibenum = hubbub_mibenum_from_name(enc, strlen(enc));

		if (stream->mibenum != 0) {
			params.encoding.name = enc;

			error = hubbub_filter_setopt(stream->input,
					HUBBUB_FILTER_SET_ENCODING, &params);
			if (error != HUBBUB_OK && error != HUBBUB_INVALID) {
				hubbub_filter_destroy(stream->input);
				alloc(stream->buffer, 0, pw);
				alloc(stream, 0, pw);
				return NULL;
			}

			stream->encsrc = HUBBUB_CHARSET_DICTATED;
		}
	} else {
		stream->mibenum = 0;
		stream->encsrc = HUBBUB_CHARSET_UNKNOWN;
	}

	stream->destroy = hubbub_utf8stream_destroy;
	stream->append = hubbub_utf8stream_append;
	stream->insert = hubbub_utf8stream_insert;
	stream->peek = hubbub_utf8stream_peek;
	stream->cur_pos = hubbub_utf8stream_cur_pos;
	stream->lowercase = hubbub_utf8stream_lowercase;
	stream->uppercase = hubbub_utf8stream_uppercase;
	stream->advance = hubbub_utf8stream_advance;
	stream->push_back = hubbub_utf8stream_push_back;
	stream->cmp_range_ci = hubbub_utf8stream_compare_range_ci;
	stream->cmp_range_cs = hubbub_utf8stream_compare_range_cs;
	stream->cmp_range_ascii = hubbub_utf8stream_compare_range_ascii;
	stream->replace_range = hubbub_utf8stream_replace_range;

	return stream;
}

/**
 * Destroy an input stream
 *
 * \param stream  Input stream to destroy
 */
void hubbub_utf8stream_destroy(hubbub_inputstream *stream)
{
	if (stream->input != NULL) {
		hubbub_filter_destroy(stream->input);
	}

	if (stream->buffer != NULL) {
		stream->alloc(stream->buffer, 0, stream->pw);
	}

	stream->alloc(stream, 0, stream->pw);
}

/**
 * Append data to an input stream
 *
 * \param stream  Input stream to append data to
 * \param data    Data to append (in document charset), or NULL to flag EOF
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_utf8stream_append(hubbub_inputstream *stream,
		const uint8_t *data, size_t len)
{
	hubbub_error error;
	uint8_t *base;
	size_t space;

	if (data == NULL) {
		/* EOF indicated */
		size_t dummy_len = 0;
		uint8_t *dummy_data = (uint8_t *) &dummy_len;

		base = stream->buffer + stream->buffer_len;
		space = stream->buffer_alloc - stream->buffer_len;

		/* Forcibly flush through any remaining buffered data */
		while ((error = hubbub_filter_process_chunk(stream->input,
				(const uint8_t **) &dummy_data, &dummy_len,
				&base, &space)) == HUBBUB_NOMEM) {
			bool moved = false;
			uint8_t *temp = stream->alloc(stream->buffer,
					stream->buffer_alloc + BUFFER_CHUNK,
					stream->pw);

			if (temp == NULL) {
				return HUBBUB_NOMEM;
			}

			moved = (temp != stream->buffer);

			stream->buffer = temp;
			stream->buffer_len += stream->buffer_alloc -
					stream->buffer_len - space;
			stream->buffer_alloc += BUFFER_CHUNK;

			base = stream->buffer + stream->buffer_len;
			space = stream->buffer_alloc - stream->buffer_len;

			if (moved)
				hubbub_inputstream_buffer_moved(stream);
		}

		/* And fix up buffer length */
		stream->buffer_len += stream->buffer_alloc -
				stream->buffer_len - space;

		stream->had_eof = true;
	} else {
		/* Normal data chunk */

		if (stream->mibenum == 0) {
			/* Haven't found charset yet; detect it */
			error = hubbub_charset_extract(&data, &len,
					&stream->mibenum, &stream->encsrc);
			if (error) {
				return error;
			}

			/* We should always have a charset by now */
			if (stream->mibenum == 0)
				abort();
		}

		base = stream->buffer + stream->buffer_len;
		space = stream->buffer_alloc - stream->buffer_len;

		/* Convert chunk to UTF-8 */
		while ((error = hubbub_filter_process_chunk(stream->input,
				&data, &len,
				&base, &space)) == HUBBUB_NOMEM) {
			bool moved = false;
			uint8_t *temp = stream->alloc(stream->buffer,
					stream->buffer_alloc + BUFFER_CHUNK,
					stream->pw);

			if (temp == NULL) {
				return HUBBUB_NOMEM;
			}

			moved = (temp != stream->buffer);

			stream->buffer = temp;
			stream->buffer_len += stream->buffer_alloc -
					stream->buffer_len - space;
			stream->buffer_alloc += BUFFER_CHUNK;

			base = stream->buffer + stream->buffer_len;
			space = stream->buffer_alloc - stream->buffer_len -
					space;

			if (moved)
				hubbub_inputstream_buffer_moved(stream);
		}

		/* And fix up buffer length */
		stream->buffer_len += stream->buffer_alloc -
				stream->buffer_len - space;
	}

	return HUBBUB_OK;
}

/**
 * Insert data into stream at current location
 *
 * \param stream  Input stream to insert into
 * \param data    Data to insert (UTF-8 encoded)
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_utf8stream_insert(hubbub_inputstream *stream,
		const uint8_t *data, size_t len)
{
	size_t space;
	uint8_t *curpos;

	space = stream->buffer_alloc - stream->buffer_len;

	/* Need to grow buffer, if there's insufficient space */
	if (space <= len) {
		bool moved = false;
		uint8_t *temp = stream->alloc(stream->buffer,
				stream->buffer_alloc +
				((len + BUFFER_CHUNK - 1) & ~BUFFER_CHUNK) +
				BUFFER_CHUNK,
				stream->pw);

		if (temp == NULL)
			return HUBBUB_NOMEM;

		moved = (temp != stream->buffer);

		stream->buffer = temp;
		stream->buffer_alloc +=
				((len + BUFFER_CHUNK - 1) & ~BUFFER_CHUNK);

		if (moved)
			hubbub_inputstream_buffer_moved(stream);
	}

	/* Find the insertion point
	 * (just before the next character to be read) */
	curpos = stream->buffer + stream->cursor;

	/* Move data above this point up */
	memmove(curpos + len, curpos, stream->buffer_len - stream->cursor);

	/* Copy new data into gap created by memmove */
	memcpy(curpos, data, len);

	/* Fix up buffer length */
	stream->buffer_len += len;

	return HUBBUB_OK;
}

/**
 * Look at the next character in the stream
 *
 * \param stream  Stream to look in
 * \return UCS4 (host-endian) character code, or EOF or OOD.
 */
uint32_t hubbub_utf8stream_peek(hubbub_inputstream *stream)
{
	hubbub_error error;
	size_t len;
	uint32_t ret;

	if (stream->cursor == stream->buffer_len) {
		return stream->had_eof ? HUBBUB_INPUTSTREAM_EOF
					: HUBBUB_INPUTSTREAM_OOD;
	}

	error = hubbub_utf8_to_ucs4(stream->buffer + stream->cursor,
			stream->buffer_len - stream->cursor,
			&ret, &len);
	if (error != HUBBUB_OK && error != HUBBUB_NEEDDATA)
		return HUBBUB_INPUTSTREAM_OOD;

	if (error == HUBBUB_NEEDDATA) {
		if (stream->had_eof)
			return HUBBUB_INPUTSTREAM_EOF;
		else
			return HUBBUB_INPUTSTREAM_OOD;
	}

	return ret;
}

/**
 * Retrieve the byte index and length of the current character in the stream
 *
 * \param stream  Stream to look in
 * \param len     Pointer to location to receive byte length of character
 * \return Byte index of current character from start of stream,
 *         or (uint32_t) -1 on error
 */
uint32_t hubbub_utf8stream_cur_pos(hubbub_inputstream *stream,
		size_t *len)
{
	hubbub_utf8_char_byte_length(stream->buffer + stream->cursor, len);

	return stream->cursor;
}

/**
 * Convert the current character to lower case
 *
 * \param stream  Stream to look in
 */
void hubbub_utf8stream_lowercase(hubbub_inputstream *stream)
{
	if ('A' <= stream->buffer[stream->cursor] &&
			stream->buffer[stream->cursor] <= 'Z')
		stream->buffer[stream->cursor] += 0x0020;
}

/**
 * Convert the current character to upper case
 *
 * \param stream  Stream to look in
 */
void hubbub_utf8stream_uppercase(hubbub_inputstream *stream)
{
	if ('a' <= stream->buffer[stream->cursor] &&
			stream->buffer[stream->cursor] <= 'z')
		stream->buffer[stream->cursor] -= 0x0020;
}

/**
 * Advance the stream's current position
 *
 * \param stream  The stream whose position to advance
 */
void hubbub_utf8stream_advance(hubbub_inputstream *stream)
{
	hubbub_error error;
	uint32_t next;

	error = hubbub_utf8_next(stream->buffer, stream->buffer_len,
			stream->cursor, &next);

	if (error == HUBBUB_OK)
		stream->cursor = next;
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
hubbub_error hubbub_utf8stream_push_back(hubbub_inputstream *stream,
		uint32_t character)
{
	hubbub_error error;
	uint32_t prev;
	uint8_t buf[6];
	size_t len;

	error = hubbub_utf8_prev(stream->buffer, stream->cursor, &prev);
	if (error != HUBBUB_OK)
		return error;

	error = hubbub_utf8_from_ucs4(character, buf, &len);
	if (error != HUBBUB_OK)
		return error;

	if ((stream->cursor - prev) != len ||
			memcmp(stream->buffer + prev, buf, len) != 0)
		return HUBBUB_INVALID;

	stream->cursor = prev;

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
int hubbub_utf8stream_compare_range_ci(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len)
{
	return strncasecmp((const char *) (stream->buffer + r1),
			(const char *) (stream->buffer + r2), len);
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
int hubbub_utf8stream_compare_range_cs(hubbub_inputstream *stream,
		uint32_t r1, uint32_t r2, size_t len)
{
	return strncmp((const char *) (stream->buffer + r1),
			(const char *) (stream->buffer + r2), len);
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
int hubbub_utf8stream_compare_range_ascii(hubbub_inputstream *stream,
		uint32_t off, size_t len, const char *data, size_t dlen)
{
	/* Lengths don't match, so strings don't */
	if (len != dlen)
		return 1; /* arbitrary */

	return strncmp((const char *) (stream->buffer + off),
			data, len);
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
hubbub_error hubbub_utf8stream_replace_range(hubbub_inputstream *stream,
		uint32_t start, size_t len, uint32_t ucs4)
{
	uint8_t buf[6];
	size_t replen;
	int32_t diff;
	hubbub_error error;

	/* Get UTF8 version of replacement character */
	error = hubbub_utf8_from_ucs4(ucs4, buf, &replen);
	if (error)
		return error;

	diff = replen - len;

	if (stream->buffer_len + diff >= stream->buffer_alloc) {
		/* Need more buffer space */
		bool moved = false;
		uint8_t *temp = stream->alloc(stream->buffer,
				stream->buffer_alloc +
				((diff + BUFFER_CHUNK - 1) & ~BUFFER_CHUNK) +
				BUFFER_CHUNK,
				stream->pw);

		if (temp == NULL)
			return HUBBUB_NOMEM;

		moved = (temp != stream->buffer);

		stream->buffer = temp;
		stream->buffer_alloc +=
				((diff + BUFFER_CHUNK - 1) & ~BUFFER_CHUNK);

		if (moved)
			hubbub_inputstream_buffer_moved(stream);
	}

	/* Move subsequent input to correct location */
	memmove(stream->buffer + start + len + diff,
			stream->buffer + start + len,
			stream->buffer_len - (start + len));

	/* And fill the gap with the replacement character */
	memcpy(stream->buffer + start, buf, replen);

	/* Finally, update length */
	stream->buffer_len += diff;

	return HUBBUB_OK;
}

hubbub_streamhandler utf8stream = {
	hubbub_utf8stream_uses_encoding,
	hubbub_utf8stream_create
};
