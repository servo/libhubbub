/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell
 */

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "utils/string.h"


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

	return strncmp((const char *) a, (const char *) b, b_len) == 0;
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
	if (a_len != b_len)
		return false;

	return strncasecmp((const char *) a, (const char *) b, b_len) == 0;
}
