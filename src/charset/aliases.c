/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charset/aliases.h"

struct alias {
	struct alias *next;
	hubbub_aliases_canon *canon;
	uint16_t name_len;
	char name[1];
};

#define HASH_SIZE (43)
static hubbub_aliases_canon *canon_tab[HASH_SIZE];
static struct alias *alias_tab[HASH_SIZE];

static hubbub_error hubbub_create_alias(const char *alias,
		hubbub_aliases_canon *c, hubbub_alloc alloc, void *pw);
static hubbub_aliases_canon *hubbub_create_canon(const char *canon,
		uint16_t mibenum, hubbub_alloc alloc, void *pw);
static uint32_t hubbub_hash_val(const char *alias, size_t len);

/**
 * Create alias data from Aliases file
 *
 * \param filename  The path to the Aliases file
 * \param alloc     Memory (de)allocation function
 * \param pw        Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error hubbub_aliases_create(const char *filename,
		hubbub_alloc alloc, void *pw)
{
	char buf[300];
	FILE *fp;

	if (filename == NULL || alloc == NULL)
		return HUBBUB_BADPARM;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return HUBBUB_FILENOTFOUND;

	while (fgets(buf, sizeof buf, fp)) {
		char *p, *aliases = 0, *mib, *end;
		hubbub_aliases_canon *cf;

		if (buf[0] == 0 || buf[0] == '#')
			/* skip blank lines or comments */
			continue;

		buf[strlen(buf) - 1] = 0; /* lose terminating newline */
		end = buf + strlen(buf);

		/* find end of canonical form */
		for (p = buf; *p && !isspace(*p) && !iscntrl(*p); p++)
			; /* do nothing */
		if (p >= end)
			continue;
		*p++ = '\0'; /* terminate canonical form */

		/* skip whitespace */
		for (; *p && isspace(*p); p++)
			; /* do nothing */
		if (p >= end)
			continue;
		mib = p;

		/* find end of mibenum */
		for (; *p && !isspace(*p) && !iscntrl(*p); p++)
			; /* do nothing */
		if (p < end)
			*p++ = '\0'; /* terminate mibenum */

		cf = hubbub_create_canon(buf, atoi(mib), alloc, pw);
		if (cf == NULL)
			continue;

		/* skip whitespace */
		for (; p < end && *p && isspace(*p); p++)
			; /* do nothing */
		if (p >= end)
			continue;
		aliases = p;

		while (p < end) {
			/* find end of alias */
			for (; *p && !isspace(*p) && !iscntrl(*p); p++)
				; /* do nothing */
			if (p > end)
				/* stop if we've gone past the end */
				break;
			/* terminate current alias */
			*p++ = '\0';

			if (hubbub_create_alias(aliases, cf,
					alloc, pw) != HUBBUB_OK)
				break;

			/* in terminating, we may have advanced
			 * past the end - check this here */
			if (p >= end)
				break;

			/* skip whitespace */
			for (; *p && isspace(*p); p++)
				; /* do nothing */

			if (p >= end)
				/* gone past end => stop */
				break;

			/* update pointer to current alias */
			aliases = p;
		}
	}

	fclose(fp);

	return HUBBUB_OK;
}

/**
 * Free all alias data
 *
 * \param alloc  Memory (de)allocation function
 * \param pw     Pointer to client-specific private data
 */
void hubbub_aliases_destroy(hubbub_alloc alloc, void *pw)
{
	hubbub_aliases_canon *c, *d;
	struct alias *a, *b;
	int i;

	for (i = 0; i != HASH_SIZE; i++) {
		for (c = canon_tab[i]; c; c = d) {
			d = c->next;
			alloc(c, 0, pw);
		}
		canon_tab[i] = NULL;

		for (a = alias_tab[i]; a; a = b) {
			b = a->next;
			alloc(a, 0, pw);
		}
		alias_tab[i] = NULL;
	}
}

/**
 * Retrieve the MIB enum value assigned to an encoding name
 *
 * \param alias  The alias to lookup
 * \param len    The length of the alias string
 * \return The MIB enum value, or 0 if not found
 */
uint16_t hubbub_mibenum_from_name(const char *alias, size_t len)
{
	hubbub_aliases_canon *c;

	if (alias == NULL)
		return 0;

	c = hubbub_alias_canonicalise(alias, len);
	if (c == NULL)
		return 0;

	return c->mib_enum;
}

/**
 * Retrieve the canonical name of an encoding from the MIB enum
 *
 * \param mibenum The MIB enum value
 * \return Pointer to canonical name, or NULL if not found
 */
const char *hubbub_mibenum_to_name(uint16_t mibenum)
{
	int i;
	hubbub_aliases_canon *c;

	for (i = 0; i != HASH_SIZE; i++)
		for (c = canon_tab[i]; c; c = c->next)
			if (c->mib_enum == mibenum)
				return c->name;

	return NULL;
}


/**
 * Retrieve the canonical form of an alias name
 *
 * \param alias  The alias name
 * \param len    The length of the alias name
 * \return Pointer to canonical form or NULL if not found
 */
hubbub_aliases_canon *hubbub_alias_canonicalise(const char *alias,
		size_t len)
{
	uint32_t hash;
	hubbub_aliases_canon *c;
	struct alias *a;

	if (alias == NULL)
		return NULL;

	hash = hubbub_hash_val(alias, len);

	for (c = canon_tab[hash]; c; c = c->next)
		if (c->name_len == len &&
				strncasecmp(c->name, alias, len) == 0)
			break;
	if (c)
		return c;

	for (a = alias_tab[hash]; a; a = a->next)
		if (a->name_len == len &&
				strncasecmp(a->name, alias, len) == 0)
			break;
	if (a)
		return a->canon;

	return NULL;
}


/**
 * Create an alias
 *
 * \param alias  The alias name
 * \param c      The canonical form
 * \param alloc  Memory (de)allocation function
 * \param pw     Pointer to client-specific private data (may be NULL)
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_create_alias(const char *alias, hubbub_aliases_canon *c,
		hubbub_alloc alloc, void *pw)
{
	struct alias *a;
	uint32_t hash;

	if (alias == NULL || c == NULL || alloc == NULL)
		return HUBBUB_BADPARM;

	a = alloc(NULL, sizeof(struct alias) + strlen(alias) + 1, pw);
	if (a == NULL)
		return HUBBUB_NOMEM;

	a->canon = c;
	a->name_len = strlen(alias);
	strcpy(a->name, alias);
	a->name[a->name_len] = '\0';

	hash = hubbub_hash_val(alias, a->name_len);

	a->next = alias_tab[hash];
	alias_tab[hash] = a;

	return HUBBUB_OK;
}

/**
 * Create a canonical form
 *
 * \param canon    The canonical name
 * \param mibenum  The MIB enum value
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data (may be NULL)
 * \return Pointer to canonical form or NULL on error
 */
hubbub_aliases_canon *hubbub_create_canon(const char *canon,
		uint16_t mibenum, hubbub_alloc alloc, void *pw)
{
	hubbub_aliases_canon *c;
	uint32_t hash, len;

	if (canon == NULL || alloc == NULL)
		return NULL;

	len = strlen(canon);

	c = alloc(NULL, sizeof(hubbub_aliases_canon) + len + 1, pw);
	if (c == NULL)
		return NULL;

	c->mib_enum = mibenum;
	c->name_len = len;
	strcpy(c->name, canon);
	c->name[len] = '\0';

	hash = hubbub_hash_val(canon, len);

	c->next = canon_tab[hash];
	canon_tab[hash] = c;

	return c;
}

/**
 * Hash function
 *
 * \param alias String to hash
 * \return The hashed value
 */
uint32_t hubbub_hash_val(const char *alias, size_t len)
{
	const char *s = alias;
	uint32_t h = 5381;

	if (alias == NULL)
		return 0;

	while (len--)
		h = (h * 33) ^ (*s++ & ~0x20); /* case insensitive */

	return h % HASH_SIZE;
}


#ifndef NDEBUG
/**
 * Dump all alias data to stdout
 */
void hubbub_aliases_dump(void)
{
	hubbub_aliases_canon *c;
	struct alias *a;
	int i;
	size_t size = 0;

	for (i = 0; i != HASH_SIZE; i++) {
		for (c = canon_tab[i]; c; c = c->next) {
			printf("%d %s\n", i, c->name);
			size += offsetof(hubbub_aliases_canon, name) +
					c->name_len;
		}

		for (a = alias_tab[i]; a; a = a->next) {
			printf("%d %s\n", i, a->name);
			size += offsetof(struct alias, name) + a->name_len;
		}
	}

	size += (sizeof(canon_tab) / sizeof(canon_tab[0]));
	size += (sizeof(alias_tab) / sizeof(alias_tab[0]));

	printf("%u\n", (unsigned int) size);
}
#endif
