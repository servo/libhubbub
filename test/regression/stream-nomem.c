#include <stdio.h>
#include <string.h>

#include <hubbub/hubbub.h>

#include "utils/utils.h"

#include "input/inputstream.h"

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	hubbub_inputstream *stream;

	/* This is specially calculated so that the inputstream is forced to 
	 * reallocate (it assumes that the inputstream's buffer chunk size 
	 * is 4k) */
#define BUFFER_SIZE (4096 + 4)
	uint8_t input_buffer[BUFFER_SIZE];
	uint8_t *buffer;
	size_t buflen;
	uint32_t c;

	if (argc != 2) {
		printf("Usage: %s <aliases_file>\n", argv[0]);
		return 1;
	}

	/* Populate the buffer with something sane */
	memset(input_buffer, 'a', BUFFER_SIZE);
	/* Now, set up our test data */
	input_buffer[BUFFER_SIZE - 1] = '5';
	input_buffer[BUFFER_SIZE - 2] = '4';
	input_buffer[BUFFER_SIZE - 3] = '\xbd';
	input_buffer[BUFFER_SIZE - 4] = '\xbf';
	/* This byte will occupy the 4095th byte in the buffer and
	 * thus cause the entirety of U+FFFD to be buffered until after
	 * the buffer has been enlarged */
	input_buffer[BUFFER_SIZE - 5] = '\xef';
	input_buffer[BUFFER_SIZE - 6] = '3';
	input_buffer[BUFFER_SIZE - 7] = '2';
	input_buffer[BUFFER_SIZE - 8] = '1';

	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	stream = hubbub_inputstream_create("UTF-8", "UTF-8", myrealloc, NULL);
	assert(stream != NULL);

	assert(hubbub_inputstream_append(stream, input_buffer, BUFFER_SIZE) == 
			HUBBUB_OK);

	assert(hubbub_inputstream_append(stream, NULL, 0) == HUBBUB_OK);

	while ((c = hubbub_inputstream_peek(stream)) != HUBBUB_INPUTSTREAM_EOF)
		hubbub_inputstream_advance(stream);

	assert(hubbub_inputstream_claim_buffer(stream, &buffer, &buflen) == 
			HUBBUB_OK);

	assert(buflen == BUFFER_SIZE);

	printf("Buffer: '%.*s'\n", 8, buffer + (BUFFER_SIZE - 8));

	assert( buffer[BUFFER_SIZE - 6] == '3' && 
		buffer[BUFFER_SIZE - 5] == (uint8_t) '\xef' && 
		buffer[BUFFER_SIZE - 4] == (uint8_t) '\xbf' && 
		buffer[BUFFER_SIZE - 3] == (uint8_t) '\xbd' && 
		buffer[BUFFER_SIZE - 2] == '4');

	free(buffer);

	hubbub_inputstream_destroy(stream);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}

