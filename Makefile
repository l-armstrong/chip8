all: chip8

chip8: chip8.c
	$(CC) chip8.c -Wall -W -O2 -o chip8

clean:
	rm chip8