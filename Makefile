CFLAGS := -Isrc -std=c99 -D_GNU_SOURCE -Wall -Wextra -Wdeclaration-after-statement -g

MCC := mcc
OBJ := src/lib/str.o \
	   src/pp/io.o src/pp/token.o src/pp/cexpr.o src/pp/lex.o src/pp/pp.o \
	   src/parse/parse.o src/main.o

all: $(MCC)

$(MCC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(MCC) $(OBJ)
