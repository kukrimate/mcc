CFLAGS := -Isrc/lib -Isrc/cpp -std=c99 -Wall -Wextra -Wpedantic

MCC := mcc
OBJ := src/lib/djb2.o \
	src/lib/hset.o \
	src/lib/htab.o \
	src/cpp/lex.o \
	src/cpp/cpp.o \
	src/mcc.o

all: $(MCC)

$(MCC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(MCC) $(OBJ)
