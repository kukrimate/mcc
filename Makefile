# Turn on all warnings and disable insane ones explicitly
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wno-implicit-fallthrough -g

MCC := mcc
OBJ := src/lex.o src/cpp.o

all: $(MCC)

$(MCC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(MCC) $(OBJ)
