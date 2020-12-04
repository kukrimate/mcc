/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include <vec.h>
#include "lex.h"

typedef enum {
	FT_FILE,
} frame_type;

typedef struct {
	frame_type type;
	union {
		FILE *fp;
	};
} frame;

VEC_GEN(frame, f)

static void
next_token(struct fvec *frames, token *token)
{
	frame *frame;

recurse:
	// Signal end of file if no frames are left
	if (!frames->n) {
		token->type = TK_END_FILE;
		return;
	}

	// Get most current frame
	frame = frames->arr + (frames->n - 1);

	// Seperate handling for each frame type
	switch (frame->type) {
	case FT_FILE:
		// Call lexer on the file
		lex_next_token(frame->fp, token);
		break;
	}

	// See if we need to recurse
	if (token->type == TK_END_FILE) {
		--frames->n;
		goto recurse;
	}
}

static void
preprocess(FILE *fp)
{
	struct fvec frames;
	token token;

	// Initialize stack
	fvec_init(&frames);
	// Push input file as a start
	fvec_add(&frames, (frame) { .type = FT_FILE, .fp = fp });

	for (;;) {
		next_token(&frames, &token);

		// Exit loop on end of file
		if (token.type == TK_END_FILE)
			break;

		switch (token.type) {
		case TK_END_LINE:
			break;
		case TK_IDENTIFIER:
		case TK_PP_NUMBER:
		case TK_CHAR_CONST:
		case TK_STRING_LIT:
			printf("%s\n", token.data);
			break;
		default:
			printf("Punct: %x\n", token.type);
		}
	}

}

int
main(int argc, char *argv[])
{
	char *path;
	FILE *fp;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		return 1;
	}

	path = argv[1];

	if (!(fp = fopen(path, "r"))) {
		perror(path);
		return 1;
	}

	preprocess(fp);
	fclose(fp);
	return 0;
}
