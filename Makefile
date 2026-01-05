CFLAGS=-std=c17 -Wall -Wextra

all: chip8

chip8: chip8.c
	$(CC) chip8.c -o chip8 $(CFLAGS) `sdl2-config --cflags --libs`

clean:
	rm chip8