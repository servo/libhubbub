/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include <hubbub/errors.h>

/**
 * Convert a hubbub error code to a string
 *
 * \param error  The error code to convert
 * \return Pointer to string representation of error, or NULL if unknown.
 */
const char *hubbub_error_to_string(hubbub_error error)
{
	const char *result = NULL;

	switch (error) {
	case HUBBUB_OK:
		result = "No error";
		break;
	case HUBBUB_OOD:
		result = "Out of data";
		break;
	case HUBBUB_NOMEM:
		result = "Insufficient memory";
		break;
	case HUBBUB_BADPARM:
		result = "Bad parameter";
		break;
	case HUBBUB_INVALID:
		result = "Invalid input";
		break;
	case HUBBUB_FILENOTFOUND:
		result = "File not found";
		break;
	case HUBBUB_NEEDDATA:
		result = "Insufficient data";
		break;
	}

	return result;
}

/**
 * Convert a string representation of an error name to a hubbub error code
 *
 * \param str  String containing error name
 * \param len  Length of string (bytes)
 * \return Hubbub error code, or HUBBUB_OK if unknown
 */
hubbub_error hubbub_error_from_string(const char *str, size_t len)
{
	if (strncmp(str, "HUBBUB_OK", len) == 0) {
		return HUBBUB_OK;
	} else if (strncmp(str, "HUBBUB_NOMEM", len) == 0) {
		return HUBBUB_NOMEM;
	} else if (strncmp(str, "HUBBUB_BADPARM", len) == 0) {
		return HUBBUB_BADPARM;
	} else if (strncmp(str, "HUBBUB_INVALID", len) == 0) {
		return HUBBUB_INVALID;
	} else if (strncmp(str, "HUBBUB_FILENOTFOUND", len) == 0) {
		return HUBBUB_FILENOTFOUND;
	} else if (strncmp(str, "HUBBUB_NEEDDATA", len) == 0) {
		return HUBBUB_NEEDDATA;
	}

	return HUBBUB_OK;
}
