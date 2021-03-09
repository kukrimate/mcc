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

.PHONY: all
all: test_lex

test_lex: $(TEST_LEX_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f test_lex $(TEST_LEX_OBJ)
