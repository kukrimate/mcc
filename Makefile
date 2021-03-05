CFLAGS := -Ilib -std=c99 -D_GNU_SOURCE -Wall -Wextra -Wdeclaration-after-statement -g

# Library objects
LIB_OBJ := lib/str.o \
		   lib/pp/io.o lib/pp/token.o lib/pp/cexpr.o lib/pp/lex.o lib/pp/pp.o
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
