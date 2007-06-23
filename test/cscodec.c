#include <stdio.h>
#include <string.h>

#include <hubbub/hubbub.h>

#include "charset/codec.h"
#include "utils/utils.h"

#include "testutils.h"

typedef struct line_ctx {
	hubbub_charsetcodec *codec;

	size_t buflen;
	size_t bufused;
	uint8_t *buf;
	size_t explen;
	size_t expused;
	uint8_t *exp;

	bool indata;
	bool inexp;

	hubbub_error exp_ret;

	enum { ENCODE, DECODE } dir;
} line_ctx;

static bool handle_line(const char *data, size_t datalen, void *pw);
static void run_test(line_ctx *ctx);
static hubbub_error filter(uint32_t c, uint32_t **output,
		size_t *outputlen, void *pw);


static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	line_ctx ctx;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	assert(hubbub_initialise(argv[1], myrealloc, NULL) == HUBBUB_OK);

	assert(hubbub_charsetcodec_create("NATS-SEFI-ADD",
			myrealloc, NULL) == NULL);

	ctx.codec = hubbub_charsetcodec_create("UTF-8", myrealloc, NULL);
	assert(ctx.codec != NULL);

	ctx.buflen = parse_filesize(argv[2]);
	if (ctx.buflen == 0)
		return 1;

	ctx.buf = malloc(2 * ctx.buflen);
	if (ctx.buf == NULL) {
		printf("Failed allocating %u bytes\n",
				(unsigned int) ctx.buflen);
		return 1;
	}

	ctx.exp = ctx.buf + ctx.buflen;
	ctx.explen = ctx.buflen;

	ctx.buf[0] = '\0';
	ctx.exp[0] = '\0';
	ctx.bufused = 0;
	ctx.expused = 0;
	ctx.indata = false;
	ctx.inexp = false;
	ctx.exp_ret = HUBBUB_OK;

	assert(parse_testfile(argv[2], handle_line, &ctx) == true);

	/* and run final test */
	if (ctx.bufused > 0 && ctx.buf[ctx.bufused - 1] == '\n')
		ctx.bufused -= 1;

	if (ctx.expused > 0 && ctx.exp[ctx.expused - 1] == '\n')
		ctx.expused -= 1;

	run_test(&ctx);

	free(ctx.buf);

	hubbub_charsetcodec_destroy(ctx.codec);

	assert(hubbub_finalise(myrealloc, NULL) == HUBBUB_OK);

	printf("PASS\n");

	return 0;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;

	if (data[0] == '#') {
		if (ctx->inexp) {
			/* This marks end of testcase, so run it */

			if (ctx->buf[ctx->bufused - 1] == '\n')
				ctx->bufused -= 1;

			if (ctx->exp[ctx->expused - 1] == '\n')
				ctx->expused -= 1;

			run_test(ctx);

			ctx->buf[0] = '\0';
			ctx->exp[0] = '\0';
			ctx->bufused = 0;
			ctx->expused = 0;
			ctx->exp_ret = HUBBUB_OK;
		}

		if (strncasecmp(data+1, "data", 4) == 0) {
			hubbub_charsetcodec_optparams params;
			const char *ptr = data + 6;

			ctx->indata = true;
			ctx->inexp = false;

			if (strncasecmp(ptr, "decode", 6) == 0)
				ctx->dir = DECODE;
			else
				ctx->dir = ENCODE;

			ptr += 7;

			if (strncasecmp(ptr, "LOOSE", 5) == 0) {
				params.error_mode.mode =
					HUBBUB_CHARSETCODEC_ERROR_LOOSE;
				ptr += 6;
			} else if (strncasecmp(ptr, "STRICT", 6) == 0) {
				params.error_mode.mode =
					HUBBUB_CHARSETCODEC_ERROR_STRICT;
				ptr += 7;
			} else {
				params.error_mode.mode =
					HUBBUB_CHARSETCODEC_ERROR_TRANSLIT;
				ptr += 9;
			}

			assert(hubbub_charsetcodec_setopt(ctx->codec,
				HUBBUB_CHARSETCODEC_ERROR_MODE,
				(hubbub_charsetcodec_optparams *) &params)
				== HUBBUB_OK);

			if (strncasecmp(ptr, "filter", 6) == 0) {
				params.filter_func.filter = filter;
				params.filter_func.pw = ctx;

				assert(hubbub_charsetcodec_setopt(ctx->codec,
					HUBBUB_CHARSETCODEC_FILTER_FUNC,
					(hubbub_charsetcodec_optparams *)
						&params) == HUBBUB_OK);
			}
		} else if (strncasecmp(data+1, "expected", 8) == 0) {
			ctx->indata = false;
			ctx->inexp = true;

			ctx->exp_ret = hubbub_error_from_string(data + 10,
					datalen - 10 - 1 /* \n */);
		} else if (strncasecmp(data+1, "reset", 5) == 0) {
			ctx->indata = false;
			ctx->inexp = false;

			hubbub_charsetcodec_reset(ctx->codec);
		}
	} else {
		if (ctx->indata) {
			memcpy(ctx->buf + ctx->bufused, data, datalen);
			ctx->bufused += datalen;
		}
		if (ctx->inexp) {
			memcpy(ctx->exp + ctx->expused, data, datalen);
			ctx->expused += datalen;
		}
	}

	return true;
}

void run_test(line_ctx *ctx)
{
	static int testnum;
	size_t destlen = ctx->bufused * 4;
	uint8_t dest[destlen];
	uint8_t *pdest = dest;
	const uint8_t *psrc = ctx->buf;
	size_t srclen = ctx->bufused;
	size_t i;

	if (ctx->dir == DECODE) {
		assert(hubbub_charsetcodec_decode(ctx->codec,
				&psrc, &srclen,
				&pdest, &destlen) == ctx->exp_ret);
	} else {
		assert(hubbub_charsetcodec_encode(ctx->codec,
				&psrc, &srclen,
				&pdest, &destlen) == ctx->exp_ret);
	}

	printf("%d: Read '", ++testnum);
	for (i = 0; i < ctx->expused; i++) {
		printf("%c%c ", "0123456789abcdef"[(dest[i] >> 4) & 0xf],
				"0123456789abcdef"[dest[i] & 0xf]);
	}
	printf("' Expected '");
	for (i = 0; i < ctx->expused; i++) {
		printf("%c%c ", "0123456789abcdef"[(ctx->exp[i] >> 4) & 0xf],
				"0123456789abcdef"[ctx->exp[i] & 0xf]);
	}
	printf("'\n");

	assert(memcmp(dest, ctx->exp, ctx->expused) == 0);
}

hubbub_error filter(uint32_t c, uint32_t **output,
		size_t *outputlen, void *pw)
{
	static uint32_t outbuf;

	UNUSED(pw);

	if (c == HUBBUB_CHARSETCODEC_NULL) {
		outbuf = 0;
		return HUBBUB_OK;
	}

	outbuf = c;

	*output = &outbuf;
	*outputlen = 1;

	return HUBBUB_OK;
}
