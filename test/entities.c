#include "tokeniser/entities.h"

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	uint32_t result;
	void *context = NULL;

	UNUSED(argc);
	UNUSED(argv);

	assert(hubbub_entities_create(myrealloc, NULL) == HUBBUB_OK);

	assert(hubbub_entities_search_step('o', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('r', &result, &context) ==
			HUBBUB_OK);

	assert(hubbub_entities_search_step('d', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('f', &result, &context) ==
			HUBBUB_OK);

	assert(hubbub_entities_search_step('z', &result, &context) ==
			HUBBUB_INVALID);

	hubbub_entities_destroy(myrealloc, NULL);

	printf("PASS\n");

	return 0;
}
