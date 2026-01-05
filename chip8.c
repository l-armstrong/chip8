#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

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
    uint8_t width;
    uint8_t height;
} display_t;

typedef enum {
    OFF = 0,
    ON  = 1,
} chip8_power_t;

struct chip8_t;
typedef void(*chip8_instruction)(struct chip8_t*, uint16_t);
typedef struct chip8_instructions_t {
    chip8_instruction primary[16];
    chip8_instruction op0[256];
    chip8_instruction op8[16];
    chip8_instruction opE[256];
    chip8_instruction opF[256];
} chip8_instructions_t;

typedef struct chip8_t {
    memory_t             mem;
    cpu_t                cpu;
    stack16_t            stack;
    display_t            display;
    keyboard_t           keyboard;
    uint8_t              delay_timer;
    uint8_t              sound_timer;
    chip8_power_t        power;
    chip8_instructions_t *instrs;
} chip8_t;

SDL_Window   *window;
SDL_Renderer *renderer;

void launch_window(chip8_t *chip8) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow(
        "CHIP8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        chip8->display.width * 10,
        chip8->display.height * 10,
        0
    );

    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    renderer = SDL_CreateRenderer(
        window,
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }

}

void destroy_window(void) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void clear_window(void) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void handle_input(chip8_t *chip8) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            chip8->power = OFF;
        }
    }
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

void chip8_init(chip8_t *c) {
    memset(c, 0, sizeof(*c));
    c->instrs = NULL;
    c->cpu.PC = 0x200;
    c->power = ON;
    c->display.width = 64;
    c->display.height = 32;
}

void chip8_load_program(chip8_t *chip8, uint8_t *chip8_program, size_t file_size) {
    memcpy(&chip8->mem.map[0x200], chip8_program, file_size);
}

static void op_unknown(chip8_t *chip8, uint16_t op) {
    printf("unknown opcode: %04X\n", op);
    exit(1);
}

static void sys_ignored(chip8_t *chip8, uint16_t op) {
    chip8->cpu.PC += 2;
}

/* GET Opcode Helpers */
#define X(op)      (((op) >> 8) & 0xF)
#define Y(op)      (((op) >> 4) & 0xF)
#define KK(op)     ((op) & 0xFF)
#define NNN(op)    ((op) & 0x0FFF)

/* 00e0 CLS */
static void cls(chip8_t *chip8, uint16_t op) {
    printf("CLS: clear screen\n");
    memset(chip8->display.buf, 0, sizeof(chip8->display.buf));
    chip8->cpu.PC += 2;
}

/* 6xkk (load) */
static void ld_vx_kk(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE 6xkk\n");
    VX(&chip8->cpu, X(op)) = KK(op);
    chip8->cpu.PC += 2;
}

/* 7xkk (add) */
static void add_vx_kk(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE 7xkk\n");
    VX(&chip8->cpu, X(op)) = (VX(&chip8->cpu, X(op)) + KK(op)) & 255;
    chip8->cpu.PC += 2;
}
/* 1NNN (jump) */
static void jp_nnn(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE 1nnn\n");
    chip8->cpu.PC = NNN(op);
}

/* Annn */
static void ld_i_nnn(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE Annn\n");
    chip8->cpu.I = NNN(op);
    chip8->cpu.PC += 2;
}

/* Dxyn */
static void drw_vx_vy_n(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE Dxyn\n");
    chip8->cpu.PC += 2;
}

static void route_0(chip8_t *chip8, uint16_t op) {
    printf("Dispatching route 0\n"); 
    chip8->instrs->op0[op & 0xFF](chip8, op);
}

void chip8_load_instructions(chip8_instructions_t *chip8_instructions) {
    for (int i = 0; i < 16;  i++) chip8_instructions->primary[i] = op_unknown; 
    for (int i = 0; i < 256; i++) chip8_instructions->op0[i] = sys_ignored;
    for (int i = 0; i < 16;  i++) chip8_instructions->op8[i] = op_unknown;
    for (int i = 0; i < 256; i++) chip8_instructions->opE[i] = op_unknown;
    for (int i = 0; i < 256; i++) chip8_instructions->opF[i] = op_unknown;

    chip8_instructions->primary[0x0] = route_0;
    chip8_instructions->primary[0x1] = jp_nnn;
    chip8_instructions->primary[0x6] = ld_vx_kk;
    chip8_instructions->primary[0x7] = add_vx_kk;
    chip8_instructions->primary[0xA] = ld_i_nnn;
    chip8_instructions->primary[0xD] = drw_vx_vy_n;

    chip8_instructions->op0[0xE0] = cls;
}

void chip8_exec(chip8_t *chip8) {
    uint16_t opcode = (chip8->mem.map[chip8->cpu.PC] << 8) | chip8->mem.map[chip8->cpu.PC+1];
    chip8->instrs->primary[(opcode >> 12) & 0xF](chip8, opcode);
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
    size_t file_size = ftell(fp);
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

    chip8_t chip8;
    chip8_instructions_t chip8_instructions;
    chip8_init(&chip8);
    chip8_load_program(&chip8, chip8_program, file_size);
    chip8_load_instructions(&chip8_instructions);
    chip8.instrs = &chip8_instructions;

    launch_window(&chip8);
    clear_window();

    while(chip8.power == ON) {
        handle_input(&chip8);
        chip8_exec(&chip8);
    }

    destroy_window();
    free(chip8_program);
    return 0;
}