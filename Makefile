## SPDX-License-Identifier: GPL-2.0-only

##
# Main Makefile for mcc
##

# Compiler flags
CFLAGS := -Isrc -std=c99 -D_GNU_SOURCE -Wall -Wextra -g -O2

# Preprocessor objects
PP_OBJ := src/pp/io.o src/pp/token.o src/pp/lex.o \
		  src/pp/cexpr.o src/pp/search.o src/pp/pp.o

# Parser objects
PARSE_OBJ := src/parse/parse.o src/parse/dump.o src/parse/type.o

# Standalone pre-processor
CPP_BIN := cpp
CPP_OBJ := $(PP_OBJ) src/cpp.o

# Compiler driver
MCC_BIN := mcc
MCC_OBJ := $(PP_OBJ) $(PARSE_OBJ) src/mcc.o

.PHONY: all
all: $(CPP_BIN) $(MCC_BIN)

$(CPP_BIN): $(CPP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(MCC_BIN): $(MCC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f $(CPP_BIN) $(CPP_OBJ) $(MCC_BIN) $(MCC_OBJ)
