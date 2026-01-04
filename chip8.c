#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct memory_t {
    uint8_t map[4096];
} memory_t;

typedef struct stack16_t {
    uint16_t data[16]; // used to store the address that the interpreter should return to
    uint8_t  sp;
} stack16_t;

typedef struct cpu_t {
    uint8_t  V[16]; // 0xVF // used as a flag by some instructions
    uint16_t I;  // used to store memory addresses
    uint16_t PC;
} cpu_t;

/* Register access helpers */
#define VX(cpu, x) ((cpu)->V[(x) & 0xF])
#define VF(cpu)    ((cpu)->V[0xF])
 
typedef struct keyboard_t {
    uint8_t keys[16]; // 0x0-0xF
} keyboard_t;

typedef struct display_t {
    uint8_t buf[64*32];
} display_t;

typedef struct chip8_t {
    memory_t    mem;
    cpu_t       cpu;
    stack16_t   stack;
    display_t   display;
    keyboard_t  keyboard;
    uint8_t     delay_timer;
    uint8_t     sound_timer;
} chip8_t;

// dispatch table

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program.ch8>\n", argv[0]);
        return 1;
    }

    /* Read the program into memory */
    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("Opening Chip8 program");
        return 1;
    }
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    // Set cursor back to beginning
    fseek(fp,0,SEEK_SET);

    /* Read in chip8 program */
    uint8_t *chip8_program = xmalloc(file_size+1);
    size_t read = fread(chip8_program, 1, file_size, fp);

    if (read != file_size) {
        fprintf(stderr, "Failed to read full ROM.\n");
        exit(1);
    }
    chip8_program[file_size] = 0;
    fclose(fp);

    for (int i = 0; i < file_size; i++) {
        printf("%02X\n", chip8_program[i]);
    }
    return 0;
}