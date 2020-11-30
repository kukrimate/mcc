CFLAGS := -Ilibkm -std=c99 -D_GNU_SOURCE -Wall -Wextra -g

MCC := mcc
OBJ := src/io.o src/lex.o src/cpp.o

all: $(MCC)

$(MCC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(MCC) $(OBJ)
