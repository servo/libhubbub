/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell
 */

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include "utils/string.h"


/**
 * Check if one string starts with another.
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
bool hubbub_string_starts(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len)
{
	uint8_t z1, z2;

	if (a_len < b_len)
		return false;

	for (const uint8_t *s1 = a, *s2 = b; b_len > 0; s1++, s2++, b_len--)
	{
		z1 = *s1;
		z2 = *s2;
		if (z1 != z2) return false;
		if (!z1) return true;
	}

	return true;
}

/**
 * Check that one string is exactly equal to another
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
bool hubbub_string_match(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len)
{
	if (a_len != b_len)
		return false;

	for (const uint8_t *s1 = a, *s2 = b; b_len > 0; s1++, s2++, b_len--)
	{
		if (*s1 != *s2) return false;
	}

	return true;
}

/**
 * Check that one string is case-insensitively equal to another
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
bool hubbub_string_match_ci(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len)
{
	uint8_t z1, z2;

	if (a_len != b_len)
		return false;

	for (const uint8_t *s1 = a, *s2 = b; b_len > 0; s1++, s2++, b_len--)
	{
		z1 = (*s1 & ~0x20);
		z2 = (*s2 & ~0x20);
		if (z1 != z2) return false;
	}

	return true;
}
