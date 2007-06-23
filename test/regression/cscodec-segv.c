#include <stdio.h>

#include <hubbub/hubbub.h>

#include "charset/codec.h"

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	hubbub_charsetcodec *codec;

	if (argc != 2) {
		printf("Usage: %s <aliases_file>\n", argv[0]);
		return 1;
	}

	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	codec = hubbub_charsetcodec_create("ISO-8859-1", myrealloc, NULL);
	assert(codec != NULL);

	hubbub_charsetcodec_destroy(codec);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}
