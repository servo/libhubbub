#include <stdio.h>
#include <string.h>

#include "charset/aliases.h"

#include "testutils.h"

extern void hubbub_aliases_dump(void);

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main (int argc, char **argv)
{
	hubbub_aliases_canon *c;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	hubbub_aliases_create(argv[1], myrealloc, NULL);

	hubbub_aliases_dump();

	c = hubbub_alias_canonicalise("moose", 5);
	if (c) {
		printf("FAIL - found invalid encoding 'moose'\n");
		return 1;
	}

	c = hubbub_alias_canonicalise("csinvariant", 11);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'csinvariant'\n");
		return 1;
	}

	c = hubbub_alias_canonicalise("nats-sefi-add", 13);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'nats-sefi-add'\n");
		return 1;
	}

	printf("%d\n", hubbub_mibenum_from_name(c->name, strlen(c->name)));

	printf("%s\n", hubbub_mibenum_to_name(c->mib_enum));

	hubbub_aliases_destroy(myrealloc, NULL);

	printf("PASS\n");

	return 0;
}
