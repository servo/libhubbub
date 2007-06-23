#include <stdio.h>
#include <stdlib.h>

#include <hubbub/hubbub.h>

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	assert (hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}
