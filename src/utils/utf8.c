/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file
 * UTF-8 manipulation functions (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/utf8.h"

/** Number of continuation bytes for a given start byte */
static const uint8_t numContinuations[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
};

/**
 * Convert a UTF-8 multibyte sequence into a single UCS4 character
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param s     The sequence to process
 * \param len   Length of sequence
 * \param ucs4  Pointer to location to receive UCS4 character (host endian)
 * \param clen  Pointer to location to receive byte length of UTF-8 sequence
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_to_ucs4(const uint8_t *s, size_t len,
		uint32_t *ucs4, size_t *clen)
{
	if (s == NULL || ucs4 == NULL || clen == NULL)
		return HUBBUB_BADPARM;

	if (len == 0)
		return HUBBUB_NEEDDATA;

	if (*s < 0x80) {
		*ucs4 = *s;
		*clen = 1;
	} else if ((*s & 0xE0) == 0xC0) {
		if (len < 2)
			return HUBBUB_NEEDDATA;
		else if ((*(s+1) & 0xC0) != 0x80)
			return HUBBUB_INVALID;
		else {
			*ucs4 = ((*s & 0x1F) << 6) | (*(s+1) & 0x3F);
			*clen = 2;
		}
	} else if ((*s & 0xF0) == 0xE0) {
		if (len < 3)
			return HUBBUB_NEEDDATA;
		else if ((*(s+1) & 0xC0) != 0x80 ||
				(*(s+2) & 0xC0) != 0x80)
			return HUBBUB_INVALID;
		else {
			*ucs4 = ((*s & 0x0F) << 12) |
				((*(s+1) & 0x3F) << 6) |
				(*(s+2) & 0x3F);
			*clen = 3;
		}
	} else if ((*s & 0xF8) == 0xF0) {
		if (len < 4)
			return HUBBUB_NEEDDATA;
		else if ((*(s+1) & 0xC0) != 0x80 ||
				(*(s+2) & 0xC0) != 0x80 ||
				(*(s+3) & 0xC0) != 0x80)
			return HUBBUB_INVALID;
		else {
			*ucs4 = ((*s & 0x0F) << 18) |
				((*(s+1) & 0x3F) << 12) |
				((*(s+2) & 0x3F) << 6) |
				(*(s+3) & 0x3F);
			*clen = 4;
		}
	} else if ((*s & 0xFC) == 0xF8) {
		if (len < 5)
			return HUBBUB_NEEDDATA;
		else if ((*(s+1) & 0xC0) != 0x80 ||
				(*(s+2) & 0xC0) != 0x80 ||
				(*(s+3) & 0xC0) != 0x80 ||
				(*(s+4) & 0xC0) != 0x80)
			return HUBBUB_INVALID;
		else {
			*ucs4 = ((*s & 0x0F) << 24) |
				((*(s+1) & 0x3F) << 18) |
				((*(s+2) & 0x3F) << 12) |
				((*(s+3) & 0x3F) << 6) |
				(*(s+4) & 0x3F);
			*clen = 5;
		}
	} else if ((*s & 0xFE) == 0xFC) {
		if (len < 6)
			return HUBBUB_NEEDDATA;
		else if ((*(s+1) & 0xC0) != 0x80 ||
				(*(s+2) & 0xC0) != 0x80 ||
				(*(s+3) & 0xC0) != 0x80 ||
				(*(s+4) & 0xC0) != 0x80 ||
				(*(s+5) & 0xC0) != 0x80)
			return HUBBUB_INVALID;
		else {
			*ucs4 = ((*s & 0x0F) << 28) |
				((*(s+1) & 0x3F) << 24) |
				((*(s+2) & 0x3F) << 18) |
				((*(s+3) & 0x3F) << 12) |
				((*(s+4) & 0x3F) << 6) |
				(*(s+5) & 0x3F);
			*clen = 6;
		}
	} else {
		return HUBBUB_INVALID;
	}

	return HUBBUB_OK;
}

/**
 * Convert a single UCS4 character into a UTF-8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param ucs4  The character to process (0 <= c <= 0x7FFFFFFF) (host endian)
 * \param s     Pointer to 6 byte long output buffer
 * \param len   Pointer to location to receive length of multibyte sequence
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_from_ucs4(uint32_t ucs4, uint8_t *s,
		size_t *len)
{
	uint32_t l = 0;

	if (s == NULL || len == NULL)
		return HUBBUB_BADPARM;
	else if (ucs4 < 0x80) {
		*s = (uint8_t) ucs4;
		l = 1;
	} else if (ucs4 < 0x800) {
		*s = 0xC0 | ((ucs4 >> 6) & 0x1F);
		*(s+1) = 0x80 | (ucs4 & 0x3F);
		l = 2;
	} else if (ucs4 < 0x10000) {
		*s = 0xE0 | ((ucs4 >> 12) & 0xF);
		*(s+1) = 0x80 | ((ucs4 >> 6) & 0x3F);
		*(s+2) = 0x80 | (ucs4 & 0x3F);
		l = 3;
	} else if (ucs4 < 0x200000) {
		*s = 0xF0 | ((ucs4 >> 18) & 0x7);
		*(s+1) = 0x80 | ((ucs4 >> 12) & 0x3F);
		*(s+2) = 0x80 | ((ucs4 >> 6) & 0x3F);
		*(s+3) = 0x80 | (ucs4 & 0x3F);
		l = 4;
	} else if (ucs4 < 0x4000000) {
		*s = 0xF8 | ((ucs4 >> 24) & 0x3);
		*(s+1) = 0x80 | ((ucs4 >> 18) & 0x3F);
		*(s+2) = 0x80 | ((ucs4 >> 12) & 0x3F);
		*(s+3) = 0x80 | ((ucs4 >> 6) & 0x3F);
		*(s+4) = 0x80 | (ucs4 & 0x3F);
		l = 5;
	} else if (ucs4 <= 0x7FFFFFFF) {
		*s = 0xFC | ((ucs4 >> 30) & 0x1);
		*(s+1) = 0x80 | ((ucs4 >> 24) & 0x3F);
		*(s+2) = 0x80 | ((ucs4 >> 18) & 0x3F);
		*(s+3) = 0x80 | ((ucs4 >> 12) & 0x3F);
		*(s+4) = 0x80 | ((ucs4 >> 6) & 0x3F);
		*(s+5) = 0x80 | (ucs4 & 0x3F);
		l = 6;
	} else {
		return HUBBUB_INVALID;
	}

	*len = l;

	return HUBBUB_OK;
}

/**
 * Calculate the length (in characters) of a bounded UTF-8 string
 *
 * \param s    The string
 * \param max  Maximum length
 * \param len  Pointer to location to receive length of string
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_length(const uint8_t *s, size_t max,
		size_t *len)
{
	const uint8_t *end = s + max;
	int l = 0;

	if (s == NULL || len == NULL)
		return HUBBUB_BADPARM;

	while (s < end) {
		if ((*s & 0x80) == 0x00)
			s += 1;
		else if ((*s & 0xE0) == 0xC0)
			s += 2;
		else if ((*s & 0xF0) == 0xE0)
			s += 3;
		else if ((*s & 0xF8) == 0xF0)
			s += 4;
		else if ((*s & 0xFC) == 0xF8)
			s += 5;
		else if ((*s & 0xFE) == 0xFC)
			s += 6;
		else
			return HUBBUB_INVALID;
		l++;
	}

	*len = l;

	return HUBBUB_OK;
}

/**
 * Calculate the length (in bytes) of a UTF-8 character
 *
 * \param s    Pointer to start of character
 * \param len  Pointer to location to receive length
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_char_byte_length(const uint8_t *s,
		size_t *len)
{
	if (s == NULL || len == NULL)
		return HUBBUB_BADPARM;

	*len = numContinuations[s[0]] + 1 /* Start byte */;

	return HUBBUB_OK;
}

/**
 * Find previous legal UTF-8 char in string
 *
 * \param s        The string
 * \param off      Offset in the string to start at
 * \param prevoff  Pointer to location to receive offset of first byte of
 *                 previous legal character
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_prev(const uint8_t *s, uint32_t off,
		uint32_t *prevoff)
{
	if (s == NULL || prevoff == NULL)
		return HUBBUB_BADPARM;

	while (off != 0 && (s[--off] & 0xC0) == 0x80)
		/* do nothing */;

	*prevoff = off;

	return HUBBUB_OK;
}

/**
 * Find next legal UTF-8 char in string
 *
 * \param s        The string (assumed valid)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_next(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff)
{
	if (s == NULL || off >= len || nextoff == NULL)
		return HUBBUB_BADPARM;

	/* Skip current start byte (if present - may be mid-sequence) */
	if (s[off] < 0x80 || (s[off] & 0xC0) == 0xC0)
		off++;

	while (off < len && (s[off] & 0xC0) == 0x80)
		off++;

	*nextoff = off;

	return HUBBUB_OK;
}

/**
 * Find next legal UTF-8 char in string
 *
 * \param s        The string (assumed to be of dubious validity)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
inline hubbub_error hubbub_utf8_next_paranoid(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff)
{
	bool valid;

	if (s == NULL || off >= len || nextoff == NULL)
		return HUBBUB_BADPARM;

	/* Skip current start byte (if present - may be mid-sequence) */
	if (s[off] < 0x80 || (s[off] & 0xC0) == 0xC0)
		off++;

	while (1) {
		/* Find next possible start byte */
		while (off < len && (s[off] & 0xC0) == 0x80)
			off++;

		/* Ran off end of data */
		if (off == len || off + numContinuations[s[off]] >= len)
			return HUBBUB_NEEDDATA;

		/* Found if start byte is ascii,
		 * or next n bytes are valid continuations */
		valid = true;

		switch (numContinuations[s[off]]) {
		case 5:
			valid &= ((s[off + 5] & 0xC0) == 0x80);
		case 4:
			valid &= ((s[off + 4] & 0xC0) == 0x80);
		case 3:
			valid &= ((s[off + 3] & 0xC0) == 0x80);
		case 2:
			valid &= ((s[off + 2] & 0xC0) == 0x80);
		case 1:
			valid &= ((s[off + 1] & 0xC0) == 0x80);
		case 0:
			valid &= (s[off + 0] < 0x80);
		}

		if (valid)
			break;

		/* Otherwise, skip this (invalid) start byte and try again */
		off++;
	}

	*nextoff = off;

	return HUBBUB_OK;
}

