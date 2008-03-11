#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hubbub/hubbub.h>
#include <hubbub/parser.h>
#include <hubbub/tree.h>

#include "utils/utils.h"

#include "testutils.h"

#define NODE_REF_CHUNK 1024
static uint16_t *node_ref;
static uintptr_t node_ref_alloc;
static uintptr_t node_counter;

#define GROW_REF							\
	if (node_counter >= node_ref_alloc) {				\
		uint16_t *temp = realloc(node_ref,			\
				(node_ref_alloc + NODE_REF_CHUNK) *	\
				sizeof(uint16_t));			\
		if (temp == NULL) {					\
			printf("FAIL - no memory\n");			\
			exit(1);					\
		}							\
		node_ref = temp;					\
		node_ref_alloc += NODE_REF_CHUNK;			\
	}

static const uint8_t *pbuffer;

static void buffer_handler(const uint8_t *buffer, size_t len, void *pw);
static int create_comment(void *ctx, const hubbub_string *data, void **result);
static int create_doctype(void *ctx, const hubbub_string *qname,
		const hubbub_string *public_id, const hubbub_string *system_id,
		void **result);
static int create_element(void *ctx, const hubbub_tag *tag, void **result);
static int create_text(void *ctx, const hubbub_string *data, void **result);
static int ref_node(void *ctx, void *node);
static int unref_node(void *ctx, void *node);
static int append_child(void *ctx, void *parent, void *child, void **result);
static int insert_before(void *ctx, void *parent, void *child, void *ref_child,
		void **result);
static int remove_child(void *ctx, void *parent, void *child, void **result);
static int clone_node(void *ctx, void *node, bool deep, void **result);
static int set_quirks_mode(void *ctx, hubbub_quirks_mode mode);

static hubbub_tree_handler tree_handler = {
	create_comment,
	create_doctype,
	create_element,
	create_text,
	ref_node,
	unref_node,
	append_child,
	insert_before,
	remove_child,
	clone_node,
	set_quirks_mode,
	NULL
};

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
	bool passed = true;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	node_ref = calloc(NODE_REF_CHUNK, sizeof(uint16_t));
	if (node_ref == NULL) {
		printf("Failed allocating node_ref\n");
		return 1;
	}
	node_ref_alloc = NODE_REF_CHUNK;

	/* Initialise library */
	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	parser = hubbub_parser_create("UTF-8", "UTF-8", myrealloc, NULL);
	assert(parser != NULL);

	params.buffer_handler.handler = buffer_handler;
	params.buffer_handler.pw = NULL;
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_BUFFER_HANDLER,
			&params) == HUBBUB_OK);

	params.tree_handler = &tree_handler;
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_TREE_HANDLER,
			&params) == HUBBUB_OK);

	params.document_node = (void *) ++node_counter;
	ref_node(NULL, (void *) node_counter);
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_DOCUMENT_NODE,
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

	/* Ensure that all nodes have been released by the treebuilder */
	for (uintptr_t n = 1; n <= node_counter; n++) {
		if (node_ref[n] != 0) {
			printf("%u still referenced (=%u)\n", n, node_ref[n]);
			passed = false;
		}
	}

	free(node_ref);

	printf("%s\n", passed ? "PASS" : "FAIL");

	return 0;
}

void buffer_handler(const uint8_t *buffer, size_t len, void *pw)
{
	UNUSED(len);
	UNUSED(pw);

	pbuffer = buffer;
}

int create_comment(void *ctx, const hubbub_string *data, void **result)
{
	printf("Creating (%u) [comment '%.*s']\n", ++node_counter,
			data->len, pbuffer + data->data_off);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return 0;
}

int create_doctype(void *ctx, const hubbub_string *qname,
		const hubbub_string *public_id, const hubbub_string *system_id,
		void **result)
{
	UNUSED(public_id);
	UNUSED(system_id);

	printf("Creating (%u) [doctype '%.*s']\n", ++node_counter,
			qname->len, pbuffer + qname->data_off);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return 0;
}

int create_element(void *ctx, const hubbub_tag *tag, void **result)
{
	printf("Creating (%u) [element '%.*s']\n", ++node_counter,
			tag->name.len, pbuffer + tag->name.data_off);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return 0;
}

int create_text(void *ctx, const hubbub_string *data, void **result)
{
	printf("Creating (%u) [text '%.*s']\n", ++node_counter,
			data->len, pbuffer + data->data_off);

	GROW_REF
	node_ref[node_counter] = 0;
	
	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return 0;
}

int ref_node(void *ctx, void *node)
{
	UNUSED(ctx);

	printf("Referencing %u (=%u)\n", 
			(uintptr_t) node, ++node_ref[(uintptr_t) node]);

	return 0;
}

int unref_node(void *ctx, void *node)
{
	UNUSED(ctx);

	printf("Unreferencing %u (=%u)\n", 
			(uintptr_t) node, --node_ref[(uintptr_t) node]);

	return 0;
}

int append_child(void *ctx, void *parent, void *child, void **result)
{
	printf("Appending %u to %u\n", (uintptr_t) child, (uintptr_t) parent);
	ref_node(ctx, child);

	*result = (void *) child;

	return 0;
}

int insert_before(void *ctx, void *parent, void *child, void *ref_child,
		void **result)
{
	printf("Inserting %u in %u before %u\n", (uintptr_t) child, 
			(uintptr_t) parent, (uintptr_t) ref_child);
	ref_node(ctx, child);

	*result = (void *) child;

	return 0;
}

int remove_child(void *ctx, void *parent, void *child, void **result)
{
	printf("Removing %u from %u\n", (uintptr_t) child, (uintptr_t) parent);
	ref_node(ctx, child);

	*result = (void *) child;

	return 0;
}

int clone_node(void *ctx, void *node, bool deep, void **result)
{
	printf("%sCloning %u -> %u\n", deep ? "Deep-" : "",
			(uintptr_t) node, ++node_counter);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return 0;
}

int set_quirks_mode(void *ctx, hubbub_quirks_mode mode)
{
	UNUSED(ctx);

	printf("Quirks mode = %u\n", mode);

	return 0;
}

