CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -O2 -Wall -Wextra -std=gnu99

BINARY = psx_emu

SOURCES = \
	src/main.c \
	src/psx.c \
	src/r3000.c \
	src/r3000_disassembler.c \
	src/r3000_interpreter.c

OBJECTS = $(patsubst %.c, %.o, $(SOURCES))

$(BINARY): $(OBJECTS)
		$(CC) -o $@ $^ -lgcc

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)