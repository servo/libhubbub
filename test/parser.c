#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <hubbub/hubbub.h>

#include <hubbub/parser.h>

#include "utils/utils.h"

#include "testutils.h"

static hubbub_error token_handler(const hubbub_token *token, void *pw);

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

static int run_test(int argc, char **argv, unsigned int CHUNK_SIZE)
{
	hubbub_parser *parser;
	hubbub_parser_optparams params;
	FILE *fp;
	size_t len, origlen;
	uint8_t buf[CHUNK_SIZE];
	const char *charset;
	hubbub_charset_source cssource;

	/* Initialise library */
	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	parser = hubbub_parser_create("UTF-8", myrealloc, NULL);
	assert(parser != NULL);

	params.token_handler.handler = token_handler;
	params.token_handler.pw = NULL;
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_TOKEN_HANDLER,
			&params) == HUBBUB_OK);

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

		assert(hubbub_parser_parse_chunk(parser,
				buf, CHUNK_SIZE) == HUBBUB_OK);

		len -= CHUNK_SIZE;
	}

	if (len > 0) {
		fread(buf, 1, len, fp);

		assert(hubbub_parser_parse_chunk(parser,
				buf, len) == HUBBUB_OK);

		len = 0;

		assert(hubbub_parser_completed(parser) == HUBBUB_OK);
	}

	fclose(fp);

	charset = hubbub_parser_read_charset(parser, &cssource);

	printf("Charset: %s (from %d)\n", charset, cssource);

	hubbub_parser_destroy(parser);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
        int shift;
        int offset;
	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}
#define DO_TEST(n) if ((ret = run_test(argc, argv, (n))) != 0) return ret
        for (shift = 0; (1 << shift) != 16384; shift++)
        	for (offset = 0; offset < 10; offset += 3)
	                DO_TEST((1 << shift) + offset);
        return 0;
#undef DO_TEST
}

hubbub_error token_handler(const hubbub_token *token, void *pw)
{
	static const char *token_names[] = {
		"DOCTYPE", "START TAG", "END TAG",
		"COMMENT", "CHARACTERS", "EOF"
	};
	size_t i;

	UNUSED(pw);

	printf("%s: ", token_names[token->type]);

	switch (token->type) {
	case HUBBUB_TOKEN_DOCTYPE:
		printf("'%.*s' %sids:\n",
				(int) token->data.doctype.name.len,
				token->data.doctype.name.ptr,
				token->data.doctype.force_quirks ?
						"(force-quirks) " : "");

		if (token->data.doctype.public_missing)
			printf("\tpublic: missing\n");
		else
			printf("\tpublic: '%.*s'\n",
				(int) token->data.doctype.public_id.len,
				token->data.doctype.public_id.ptr);

		if (token->data.doctype.system_missing)
			printf("\tsystem: missing\n");
		else
			printf("\tsystem: '%.*s'\n",
				(int) token->data.doctype.system_id.len,
				token->data.doctype.system_id.ptr);

		break;
	case HUBBUB_TOKEN_START_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				token->data.tag.name.ptr,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					token->data.tag.attributes[i].name.ptr,
					(int) token->data.tag.attributes[i].value.len,
					token->data.tag.attributes[i].value.ptr);
		}
		break;
	case HUBBUB_TOKEN_END_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				token->data.tag.name.ptr,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					token->data.tag.attributes[i].name.ptr,
					(int) token->data.tag.attributes[i].value.len,
					token->data.tag.attributes[i].value.ptr);
		}
		break;
	case HUBBUB_TOKEN_COMMENT:
		printf("'%.*s'\n", (int) token->data.comment.len,
				token->data.comment.ptr);
		break;
	case HUBBUB_TOKEN_CHARACTER:
		printf("'%.*s'\n", (int) token->data.character.len,
				token->data.character.ptr);
		break;
	case HUBBUB_TOKEN_EOF:
		printf("\n");
		break;
	}

	return HUBBUB_OK;
}
