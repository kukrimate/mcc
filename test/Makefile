##
# Makefile for mcc tests
##

# Library directory
LIBDIR := ../src

# C compiler flags
CFLAGS := -I$(LIBDIR) -std=c99 -D_GNU_SOURCE -Wall -Wextra -O3 -g

# Lexer test objects
TEST_LEX_OBJ := $(LIBDIR)/lib/str.o $(LIBDIR)/pp/io.o $(LIBDIR)/pp/token.o \
				$(LIBDIR)/pp/lex.o test_lex.o

# Preprocessor test objects
TEST_PP_OBJ := $(LIBDIR)/lib/str.o $(LIBDIR)/pp/io.o $(LIBDIR)/pp/token.o \
				$(LIBDIR)/pp/lex.o $(LIBDIR)/pp/cexpr.o $(LIBDIR)/pp/search.o \
				$(LIBDIR)/pp/pp.o test_pp.o

# Hash table test objects
TESH_HASH_OBJ := $(LIBDIR)/lib/hash.o test_hash.o

.PHONY: all
all: test_lex test_pp test_hash

test_lex: $(TEST_LEX_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_pp: $(TEST_PP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_hash: $(TESH_HASH_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f test_hash $(TESH_HASH_OBJ) test_lex $(TEST_LEX_OBJ) \
			test_pp $(TEST_PP_OBJ)
