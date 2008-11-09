#include "utils/dict.h"

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	hubbub_dict *dict;
	const void *result;
	void *context = NULL;

	UNUSED(argc);
	UNUSED(argv);

	assert(hubbub_dict_create(myrealloc, NULL, &dict) == HUBBUB_OK);

	assert(hubbub_dict_insert(dict, "Hello", (const void *) 123) ==
			HUBBUB_OK);
	assert(hubbub_dict_insert(dict, "Hello1", (const void *) 456) ==
			HUBBUB_OK);

	assert(hubbub_dict_search_step(dict, 'H', &result, &context) ==
			HUBBUB_NEEDDATA);
	assert(hubbub_dict_search_step(dict, 'e', &result, &context) ==
			HUBBUB_NEEDDATA);
	assert(hubbub_dict_search_step(dict, 'l', &result, &context) ==
			HUBBUB_NEEDDATA);
	assert(hubbub_dict_search_step(dict, 'l', &result, &context) ==
			HUBBUB_NEEDDATA);
	assert(hubbub_dict_search_step(dict, 'o', &result, &context) ==
			HUBBUB_OK);
	assert(result == (const void *) 123);
	assert(hubbub_dict_search_step(dict, '1', &result, &context) ==
			HUBBUB_OK);
	assert(result == (const void *) 456);
	assert(hubbub_dict_search_step(dict, '\0', &result, &context) ==
			HUBBUB_OK);
	assert(hubbub_dict_search_step(dict, 'x', &result, &context) ==
			HUBBUB_INVALID);

	hubbub_dict_destroy(dict);

	printf("PASS\n");

	return 0;
}
