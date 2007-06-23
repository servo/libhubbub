#include <inttypes.h>
#include <stdio.h>

#include <hubbub/hubbub.h>

#include "utils/utils.h"

#include "input/inputstream.h"

#include "testutils.h"

static void buffer_moved_handler(const uint8_t *buffer, size_t len,
		void *pw);

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	hubbub_inputstream *stream;
	FILE *fp;
	size_t len, origlen;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	uint8_t *isb;
	size_t isblen;
	uint32_t c;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	/* Initialise library */
	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	stream = hubbub_inputstream_create("UTF-8", "UTF-8", myrealloc, NULL);
	assert(stream != NULL);

	assert(hubbub_inputstream_register_movehandler(stream,
			buffer_moved_handler, NULL) == HUBBUB_OK);

	fp = fopen(argv[2], "rb");
	if (fp == NULL) {
		printf("Failed opening %s\n", argv[2]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	origlen = len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	while (len >= CHUNK_SIZE) {
		fread(buf, 1, CHUNK_SIZE, fp);

		assert(hubbub_inputstream_append(stream,
				buf, CHUNK_SIZE) == HUBBUB_OK);

		len -= CHUNK_SIZE;

		while ((c = hubbub_inputstream_peek(stream)) !=
				HUBBUB_INPUTSTREAM_OOD) {
			size_t len;
			hubbub_inputstream_cur_pos(stream, &len);
			hubbub_inputstream_advance(stream);
			assert(hubbub_inputstream_push_back(stream, c) ==
					HUBBUB_OK);
			hubbub_inputstream_advance(stream);
		}
	}

	if (len > 0) {
		fread(buf, 1, len, fp);

		assert(hubbub_inputstream_append(stream,
				buf, len) == HUBBUB_OK);

		len = 0;
	}

	fclose(fp);

	assert(hubbub_inputstream_insert(stream,
			(const uint8_t *) "hello!!!",
			SLEN("hello!!!")) == HUBBUB_OK);

	assert(hubbub_inputstream_append(stream, NULL, 0) == HUBBUB_OK);

	while (hubbub_inputstream_peek(stream) !=
			HUBBUB_INPUTSTREAM_EOF) {
		size_t len;
		hubbub_inputstream_cur_pos(stream, &len);
		hubbub_inputstream_advance(stream);
	}

	assert(hubbub_inputstream_claim_buffer(stream, &isb, &isblen) ==
			HUBBUB_OK);

	printf("Input size: %zu, Output size: %zu\n", origlen, isblen);
	printf("Buffer at %p\n", isb);

	free(isb);

	assert(hubbub_inputstream_deregister_movehandler(stream,
			buffer_moved_handler, NULL) == HUBBUB_OK);

	hubbub_inputstream_destroy(stream);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}

void buffer_moved_handler(const uint8_t *buffer, size_t len,
		void *pw)
{
	UNUSED(pw);

	printf("Buffer moved to: %p (%zu)\n", buffer, len);
}
