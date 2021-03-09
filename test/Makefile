##
# Makefile for mcc tests
##

# Library directory
LIBDIR := ../lib

# C compiler flags
CFLAGS := -I$(LIBDIR) -std=c99 -D_GNU_SOURCE \
			-Wall -Wextra -Wdeclaration-after-statement

# Lexer test objects
TEST_LEX_OBJ := $(LIBDIR)/str.o $(LIBDIR)/pp/io.o $(LIBDIR)/pp/token.o \
				$(LIBDIR)/pp/lex.o test_lex.o

# Preprocessor test objects
TEST_PP_OBJ := $(LIBDIR)/str.o $(LIBDIR)/pp/io.o $(LIBDIR)/pp/token.o \
				$(LIBDIR)/pp/lex.o $(LIBDIR)/pp/cexpr.o $(LIBDIR)/pp/search.o \
				$(LIBDIR)/pp/pp.o test_pp.o


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
