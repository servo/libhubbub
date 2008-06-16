#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <hubbub/hubbub.h>

#include <hubbub/parser.h>

#include "utils/utils.h"

#include "testutils.h"

static const uint8_t *pbuffer;

static void buffer_handler(const uint8_t *buffer, size_t len, void *pw);
static void token_handler(const hubbub_token *token, void *pw);

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	hubbub_parser *parser;
	hubbub_parser_optparams params;
	FILE *fp;
	size_t len, origlen;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	const char *charset;
	hubbub_charset_source cssource;
	uint8_t *buffer;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	/* Initialise library */
	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	parser = hubbub_parser_create("UTF-8", "UTF-8", myrealloc, NULL);
	assert(parser != NULL);

	params.buffer_handler.handler = buffer_handler;
	params.buffer_handler.pw = NULL;
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_BUFFER_HANDLER,
			&params) == HUBBUB_OK);

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

	assert(hubbub_parser_claim_buffer(parser, &buffer, &len) ==
			HUBBUB_OK);

	free(buffer);

	hubbub_parser_destroy(parser);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}

void buffer_handler(const uint8_t *buffer, size_t len, void *pw)
{
	UNUSED(len);
	UNUSED(pw);

	pbuffer = buffer;
}

void token_handler(const hubbub_token *token, void *pw)
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
				pbuffer + token->data.doctype.name.data.off,
				token->data.doctype.force_quirks ?
						"(force-quirks) " : "");

		if (token->data.doctype.public_missing)
			printf("\tpublic: missing\n");
		else
			printf("\tpublic: '%.*s'\n",
				(int) token->data.doctype.public_id.len,
				pbuffer + token->data.doctype.public_id.data.off);

		if (token->data.doctype.system_missing)
			printf("\tsystem: missing\n");
		else
			printf("\tsystem: '%.*s'\n",
				(int) token->data.doctype.system_id.len,
				pbuffer + token->data.doctype.system_id.data.off);

		break;
	case HUBBUB_TOKEN_START_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				pbuffer + token->data.tag.name.data.off,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					pbuffer + token->data.tag.attributes[i].name.data.off,
					(int) token->data.tag.attributes[i].value.len,
					pbuffer + token->data.tag.attributes[i].value.data.off);
		}
		break;
	case HUBBUB_TOKEN_END_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				pbuffer + token->data.tag.name.data.off,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					pbuffer + token->data.tag.attributes[i].name.data.off,
					(int) token->data.tag.attributes[i].value.len,
					pbuffer + token->data.tag.attributes[i].value.data.off);
		}
		break;
	case HUBBUB_TOKEN_COMMENT:
		printf("'%.*s'\n", (int) token->data.comment.len,
				pbuffer + token->data.comment.data.off);
		break;
	case HUBBUB_TOKEN_CHARACTER:
		printf("'%.*s'\n", (int) token->data.character.len,
				pbuffer + token->data.character.data.off);
		break;
	case HUBBUB_TOKEN_EOF:
		printf("\n");
		break;
	}
}
