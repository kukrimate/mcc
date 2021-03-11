CFLAGS := -Isrc -std=c99 -D_GNU_SOURCE -Wall -Wextra -Wdeclaration-after-statement -g

# Library objects
LIB_OBJ := src/lib/str.o \
		   src/pp/io.o src/pp/token.o src/pp/lex.o \
		   src/pp/cexpr.o src/pp/search.o src/pp/pp.o
# Standalone pre-processor
CPP_BIN := cpp
CPP_OBJ := $(LIB_OBJ) src/cpp.o

all: $(CPP_BIN)

$(CPP_BIN): $(CPP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(CPP_BIN) $(CPP_OBJ)
