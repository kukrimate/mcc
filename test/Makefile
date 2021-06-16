## SPDX-License-Identifier: GPL-2.0-only

##
# Makefile for mcc tests
##

# Library directory
LIBDIR := ../src

# C compiler flags
CFLAGS := -I$(LIBDIR) -std=c99 -D_GNU_SOURCE -Wall -Wextra -O1 -g

# Lexer test objects
TEST_LEX_OBJ := $(LIBDIR)/lex/token.o $(LIBDIR)/lex/lex.o test_lex.o

# Preprocessor test objects
TEST_PP_OBJ  := $(LIBDIR)/lex/token.o $(LIBDIR)/lex/lex.o \
				$(LIBDIR)/pp/core.o $(LIBDIR)/pp/eval.o  $(LIBDIR)/pp/dir.o \
				$(LIBDIR)/pp/exp.o test_pp.o

.PHONY: all
all: test_lex test_pp

test_lex: $(TEST_LEX_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_pp: $(TEST_PP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f test_lex $(TEST_LEX_OBJ) test_pp $(TEST_PP_OBJ)
