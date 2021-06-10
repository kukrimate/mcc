## SPDX-License-Identifier: GPL-2.0-only

##
# Main Makefile for mcc
##

# Compiler flags
CFLAGS := -Isrc -std=c99 -D_GNU_SOURCE -Wall -Wextra -g -O2

# Compiler filename
MCC_BIN := mcc

# Compiler objects
MCC_OBJ := src/pp/io.o src/pp/token.o src/pp/lex.o src/pp/cexpr.o \
		   src/pp/pp.o src/parse/parse.o src/parse/dump.o src/parse/type.o \
		   src/mcc.o

.PHONY: all
all: $(MCC_BIN)

$(MCC_BIN): $(MCC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f $(MCC_BIN) $(MCC_OBJ)
