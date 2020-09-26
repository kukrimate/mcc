CFLAGS := -std=c99 -Wall -Wextra -Wpedantic

MCC := mcc
OBJ := src/mcc.o src/cpp/lex.o

all: $(MCC)

$(MCC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(MCC) $(OBJ)
