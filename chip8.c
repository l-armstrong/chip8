#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

/*=========================================== Data Structures ==================================================== */
typedef struct memory_t {
    uint8_t  map[4096];
} memory_t;

typedef struct stack16_t {
    uint16_t data[16];       /* used to store the address that the interpreter should return to */
    uint8_t  sp;
} stack16_t;

typedef struct cpu_t {
    uint8_t  V[16];          /* V0xF is used as a flag by some instructions */
    uint16_t I;              /* used to store memory addresses */
    uint16_t PC;
} cpu_t;
 
typedef struct keyboard_t {
    uint8_t      keys[16];   /* 0x0 => 0xF */
} keyboard_t;

typedef struct display_t {
    uint8_t      buf[64*32];
    uint8_t      width;
    uint8_t      height;
    uint16_t     scale;
    SDL_Window   *window;
    SDL_Renderer *renderer;
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


/*============================================  Window  ==================================================== */
void launch_window(chip8_t *chip8) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        exit(1);
    }

    chip8->display.window = SDL_CreateWindow(
        "CHIP8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        chip8->display.width * chip8->display.scale,
        chip8->display.height * chip8->display.scale,
        0
    );

    if (chip8->display.window == NULL) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    chip8->display.renderer = SDL_CreateRenderer(
        chip8->display.window,
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (chip8->display.renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(chip8->display.window);
        SDL_Quit();
        exit(1);
    }

}

void destroy_window(chip8_t *chip8) {
    SDL_DestroyRenderer(chip8->display.renderer);
    SDL_DestroyWindow(chip8->display.window);
    SDL_Quit();
}

void clear_window(chip8_t *chip8) {
    SDL_SetRenderDrawColor(chip8->display.renderer, 0, 0, 0, 255);
    SDL_RenderClear(chip8->display.renderer);
    SDL_RenderPresent(chip8->display.renderer);
}

void update_window(chip8_t *chip8) {
    SDL_SetRenderDrawColor(chip8->display.renderer, 0, 0, 0, 255);
    SDL_RenderClear(chip8->display.renderer);
    SDL_SetRenderDrawColor(chip8->display.renderer, 255, 255, 255, 255);
    for (int y = 0; y < chip8->display.height; y++) {
        for (int x = 0; x < chip8->display.width; x++) {
            if (chip8->display.buf[y * chip8->display.width + x]) {
                SDL_Rect r = {
                    x * chip8->display.scale,
                    y * chip8->display.scale,
                    chip8->display.scale,
                    chip8->display.scale
                };
                SDL_RenderFillRect(chip8->display.renderer, &r);
            }
        }
    }

    SDL_RenderPresent(chip8->display.renderer);
}

/*============================================  User Input  ==================================================== */
static int sdl_key_to_chip8(SDL_Keycode key) {
    switch (key) {
        case SDLK_x: return 0x0;
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_z: return 0xA;
        case SDLK_c: return 0xB;
        case SDLK_4: return 0xC;
        case SDLK_r: return 0xD;
        case SDLK_f: return 0xE;
        case SDLK_v: return 0xF;
        default:     return -1;
    }
}

#define KEY_UP   0
#define KEY_DOWN 1
void handle_input(chip8_t *chip8) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                chip8->power = OFF;
                break;
            case SDL_KEYDOWN: {
                int k = sdl_key_to_chip8(e.key.keysym.sym);
                if (k != -1)
                    chip8->keyboard.keys[k] = KEY_DOWN;
                break;
            }
            case SDL_KEYUP: {
                int k = sdl_key_to_chip8(e.key.keysym.sym);
                if (k != -1) {
                    chip8->keyboard.keys[k] = 0;
                }
                break;
            }
        }
    }
}

/*============================================ Utility Methods  ==================================================== */
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

/*=========================================== Chip8 Instructions ==================================================== */
/* GET Opcode Helpers */
#define X(op)         (((op) >> 8) & 0xF)
#define Y(op)         (((op) >> 4) & 0xF)
#define KK(op)        ((op) & 0xFF)
#define NNN(op)       ((op) & 0x0FFF)
#define N(op)         ((op) & 0xF)
#define LO(op)        ((op) & 0xFF)

/* State access helpers */
#define V(c, i)       ((c)->cpu.V[(i) & 0xF])
#define I(c)          ((c)->cpu.I)
#define PC(c)         ((c)->cpu.PC)

/* Register access helpers */
#define VX(cpu, x)    ((cpu)->V[(x) & 0xF])
#define VF(cpu)       ((cpu)->V[0xF])

/* Flow helpers */
#define NEXT(c)        (PC(c) += 2)
#define SKIP(c)        (PC(c) += 4)
#define JUMP(c,a)      (PC(c) = (uint16_t)((a) & 0x0FFF))

/* Display helpers */
#define W(c)           ((c)->display.width)
#define H(c)           ((c)->display.height)
#define PIXIDX(c,x,y)  ((uint16_t)((y) * W(c) + (x)))
#define PIXEL(c,x,y)   ((c)->display.buf[PIXIDX((c),(x),(y))])

/* Memory helpers */
#define MEM(c,a)       ((c)->mem.map[(a) & 0x0FFF])
#define BIT(byte,i)    (((byte) >> (i)) & 1u)

/* Operand Helpers */
#define Vx(c,op)       VX(&(c)->cpu, X(op))
#define Vy(c,op)       VX(&(c)->cpu, Y(op))
#define VF_(c)         VF(&(c)->cpu)

/* 0nnn - sys addr 
 * jump to a machine routine at nnn
 * note: this instruction is ignored by modern interpreters */
static void sys_ignored(chip8_t *chip8, uint16_t op) {
    NEXT(chip8);
}

/* 00e0 - CLS 
 * clear the display */
static void cls(chip8_t *chip8, uint16_t op) {
    printf("CLS: clear screen\n");
    memset(chip8->display.buf, 0, sizeof(chip8->display.buf));
    NEXT(chip8);
}

/* 00ee - RET
 * return from a subroutine */
static void ret(chip8_t *chip8, uint16_t op) {
    chip8->cpu.PC = chip8->stack.data[chip8->stack.sp - 1];
    chip8->stack.sp--;
}

/* 1nnn - JP addr
 * jump to location nnn */
static void jp_addr(chip8_t *chip8, uint16_t op) {
    PC(chip8) = NNN(op);
}

/* 2nnn - Call addr 
 * call subroutine at nnn */
static void call_addr(chip8_t *chip8, uint16_t op) {
    chip8->stack.data[chip8->stack.sp++] = chip8->cpu.PC;
    PC(chip8) = NNN(op);
}

/* 3xkk - SE Vx, byte
 * skip next instruction if Vx = kk */
static void se_vx_byte(chip8_t *chip8, uint16_t op) {
    if (Vx(chip8, op) == KK(op)) SKIP(chip8);
    else NEXT(chip8);
}

/* 4xkk - SNE Vx, byte 
 * skip next instruction if Vx != kk */
static void sne_vx_byte(chip8_t *chip8, uint16_t op) {
    if (Vx(chip8, op) != KK(op)) SKIP(chip8);
    else NEXT(chip8);
}

/* 5xy0 - SE Vx, Vy 
 * skip next instruction if Vx = Vy */
static void se_vx_vy(chip8_t *chip8, uint16_t op) {
    if (Vx(chip8, op) == Vy(chip8, op)) SKIP(chip8);
    else NEXT(chip8);
}

/* 6xkk - LD Vx, byte 
 * set Vx = kk */
static void ld_vx_kk(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE 6xkk\n");
    Vx(chip8, op) = KK(op);
    NEXT(chip8);
}

/* 7xkk - ADD Vx, byte 
 * set Vx = Vx + kk */
static void add_vx_kk(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE 7xkk\n");
    Vx(chip8, op) = (Vx(chip8, op) + KK(op)) & 255;
    NEXT(chip8);
}

/* 8xy0 - LD Vx, Vy
 * set Vx = Vy */
static void ld_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = Vy(chip8, op);
    NEXT(chip8);
}

/* 8xy1 - OR Vx, Vy
 * set Vx = Vx OR Vy */
static void or_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vx(chip8, op) | Vy(chip8, op)) & 255;
    NEXT(chip8);
}

/* 8xy2 - AND Vx, Vy
 * set Vx = Vx AND Vy */
static void and_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vx(chip8, op) & Vy(chip8, op)) & 255;
    NEXT(chip8);
}

/* 8xy3 - XOR Vx, Vy
 * set Vx = Vx XOR Vy */
static void xor_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vx(chip8, op) ^ Vy(chip8, op)) & 255;
    NEXT(chip8);
}

/* 8xy4 - ADD Vx, Xy 
 * set Vx = Vx + Vy, set VF = carry */
static void add_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vx(chip8, op) + Vy(chip8, op)) & 255;
    VF_(chip8) = (Vx(chip8, op) + Vy(chip8, op)) > 255;
    NEXT(chip8);
}

/* 8xy5 - SUB Vx, Vy 
 * set Vx = Vx - Vy, set VF = NOT borrow */
static void sub_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vx(chip8, op) - Vy(chip8, op)) & 255;
    VF_(chip8) = Vx(chip8, op) > Vy(chip8, op);
    NEXT(chip8);
}

/* 8xy6 - SHR Vx, {, Vy}
 * set Vx = SHR 1 */
static void shr_vx(chip8_t *chip8, uint16_t op) {
    VF_(chip8) = Vx(chip8, op) & 0x01;
    Vx(chip8, op) = Vx(chip8, op) >> 1;
    NEXT(chip8);
}

/* 8xy7 - SUBN Vx, Vy 
 * set Vx = Vy - Vx, set VF = NOT borrow */
static void subn_vx_vy(chip8_t *chip8, uint16_t op) {
    Vx(chip8, op) = (Vy(chip8, op) - Vx(chip8, op)) & 255;
    VF_(chip8) = Vy(chip8, op) > Vx(chip8, op);
    NEXT(chip8);
}

/* 8xyE - SHL Vx, {, Vy}
 * set Vx = SHL 1 */
static void shl_vx(chip8_t *chip8, uint16_t op) {
    VF_(chip8) = Vx(chip8, op) & 0x08;
    Vx(chip8, op) = Vx(chip8, op) << 1;
    NEXT(chip8);
}

/* 9xy0 - SNE Vx, Vy 
 * skip next instruction if Vx != Vy */
static void sne_vx_vy(chip8_t *chip8, uint16_t op) {
    if (Vx(chip8, op) != Vy(chip8, op)) SKIP(chip8);
    else NEXT(chip8);
}

/* Annn - LD I, addr 
 * set I = nnn */
static void ld_i_nnn(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE Annn\n");
    I(chip8) = NNN(op);
    NEXT(chip8);
}

/* Bnnn - JP V0, addr 
 * jump to location nnn + V0 */
static void jp_v0_addr(chip8_t *chip8, uint16_t op) {
    PC(chip8) = NNN(op) + Vx(chip8, V(chip8, 0));
}

/* Cxkk - RND Vx, byte 
 * set Vx = random byte AND kk */
static void rnd_vx_byte(chip8_t *chip8, uint16_t op) {
    uint8_t r = rand() & 0xFF;
    Vx(chip8, op) = r & KK(op);
    NEXT(chip8);
}

/* Dxyn - DRW Vx, Vy, nibble
 * display n-byte sprite starting at memory location I at (Vx, Vy), set VV = collision */
static void drw_vx_vy_n(chip8_t *chip8, uint16_t op) {
    printf("RUNNING OPCODE Dxyn\n");
    VF(&chip8->cpu) = 0;

    const uint8_t x0 = VX(&chip8->cpu, X(op));
    const uint8_t y0 = VX(&chip8->cpu, Y(op));
    const uint8_t n =  N(op);

    for (uint8_t row = 0; row < n; row++) {
        uint16_t addr = chip8->cpu.I + row;
        if (addr >= 4096) break;
        uint8_t sprite_byte = chip8->mem.map[addr];

        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t sprite_pixel = (sprite_byte >> (7 - bit)) & 0x1;
            if (sprite_pixel == 0) continue;

            uint8_t x = (uint8_t)((x0 + bit) % chip8->display.width);
            uint8_t y = (uint8_t)((y0 + row) % chip8->display.height);

            uint16_t idx = (uint16_t)(y * chip8->display.width + x);

            if (chip8->display.buf[idx] == 1) {
                VF(&chip8->cpu) = 1;
            }

            chip8->display.buf[idx] ^= 1;
        }
    }
    chip8->cpu.PC += 2;
}

/* Ex9E - SKP Vx
 * skip next instruction if key with the value of Vx is pressed */
static void skp_vx(chip8_t *chip8, uint16_t op) {

}

/* ExA1 - SKNP Vx 
 * skip next instruction if key with the value of Vx is not pressed. */
static void sknp_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx07 - LD Vx, DT 
 * set Vx = delay timer value */
static void ld_vx_dt(chip8_t *chip8, uint16_t op) {

}

/* Fx0A - LD Vx, K
 * wait for a key press, store the value of they key in Vx */
static void ld_vx_k(chip8_t *chip8, uint16_t op) {

}

/* Fx15 - LD DT, Vx 
 * set delay timer = Vx */
static void ld_dt_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx18 - LD ST, Vx 
 * set sound timer = Vx */
static void ld_st_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx1E - ADD I, Vx 
 * set I = I + Vx */
static void add_i_vx(chip8_t *chip8, uint16_t op) {
    I(chip8) = I(chip8) + Vx(chip8, op);
    NEXT(chip8);
}

/* Fx29 - LD F, Vx 
 * set I = location of sprite for digit Vx */
static void ld_f_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx33 - LD B, Vx
 * store BCD representation of Vx in memory location I, I +1, I+2 */
static void ld_b_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx55 - LD [I], Vx 
 * store registers V0 through Vx in memory starting at location I */
static void ld_loc_i_vx(chip8_t *chip8, uint16_t op) {

}

/* Fx65 - LD Vx, [I]
 * read registers V0 through Vx from memory starting at location I */
static void ld_vx_loc_i(chip8_t *chip8, uint16_t op) {

}

/* Used to fill in function table for unknown opcodes */
static void op_unknown(chip8_t *chip8, uint16_t op) {
    printf("unknown opcode: %04X\n", op);
    exit(1);
}

/*====================================== Instruction Routers ================================================ */
static void route_0(chip8_t *chip8, uint16_t op) {
    printf("Dispatching route 0\n"); 
    chip8->instrs->op0[op & 0xFF](chip8, op);
}

static void route_8(chip8_t *chip8, uint16_t op) {
    printf("Dispatching route 0\n"); 
    chip8->instrs->op8[op & 0xFF](chip8, op);
}

static void route_E(chip8_t *chip8, uint16_t op) {
    printf("Dispatching route 0\n"); 
    chip8->instrs->opE[op & 0xFF](chip8, op);
}

static void route_F(chip8_t *chip8, uint16_t op) {
    printf("Dispatching route 0\n"); 
    chip8->instrs->opF[op & 0xFF](chip8, op);
}

/*=========================================== Chip8 Init ==================================================== */
void chip8_init(chip8_t *c) {
    memset(c, 0, sizeof(*c));
    c->instrs = NULL;
    c->cpu.PC = 0x200;
    c->power = ON;
    c->display.scale = 10;
    c->display.width = 64;
    c->display.height = 32;
}

void chip8_load_program(chip8_t *chip8, uint8_t *chip8_program, size_t file_size) {
    if (file_size > (sizeof(chip8->mem.map) - 0x200)) {
        fprintf(stderr, "chip8 ROM too large: %zu bytes\n", file_size);
        exit(1); 
    }
    memcpy(&chip8->mem.map[0x200], chip8_program, file_size);
}

void chip8_load_instructions(chip8_instructions_t *chip8_instructions) {
    for (int i = 0; i < 16;  i++) chip8_instructions->primary[i] = op_unknown; 
    for (int i = 0; i < 256; i++) chip8_instructions->op0[i] = sys_ignored;
    for (int i = 0; i < 16;  i++) chip8_instructions->op8[i] = op_unknown;
    for (int i = 0; i < 256; i++) chip8_instructions->opE[i] = op_unknown;
    for (int i = 0; i < 256; i++) chip8_instructions->opF[i] = op_unknown;

    /* primary dispatch */
    chip8_instructions->primary[0x0] = route_0;
    chip8_instructions->primary[0x1] = jp_addr;
    chip8_instructions->primary[0x2] = call_addr;
    chip8_instructions->primary[0x3] = se_vx_byte;
    chip8_instructions->primary[0x4] = sne_vx_byte;
    chip8_instructions->primary[0x5] = se_vx_vy;     
    chip8_instructions->primary[0x6] = ld_vx_kk;
    chip8_instructions->primary[0x7] = add_vx_kk;
    chip8_instructions->primary[0x8] = route_8;
    chip8_instructions->primary[0x9] = sne_vx_vy;    
    chip8_instructions->primary[0xA] = ld_i_nnn;
    chip8_instructions->primary[0xB] = jp_v0_addr;   
    chip8_instructions->primary[0xC] = rnd_vx_byte;
    chip8_instructions->primary[0xD] = drw_vx_vy_n;
    chip8_instructions->primary[0xE] = route_E;
    chip8_instructions->primary[0xF] = route_F;

    /* 0x00** group */
    chip8_instructions->op0[0xE0] = cls;
    chip8_instructions->op0[0xEE] = ret;

    /* 0x8xy* group */
    chip8_instructions->op8[0x0] = ld_vx_vy;
    chip8_instructions->op8[0x1] = or_vx_vy;
    chip8_instructions->op8[0x2] = and_vx_vy;
    chip8_instructions->op8[0x3] = xor_vx_vy;
    chip8_instructions->op8[0x4] = add_vx_vy;
    chip8_instructions->op8[0x5] = sub_vx_vy;
    chip8_instructions->op8[0x6] = shr_vx;
    chip8_instructions->op8[0x7] = subn_vx_vy;
    chip8_instructions->op8[0xE] = shl_vx;

    /* 0xEx** group */
    chip8_instructions->opE[0x9E] = skp_vx;
    chip8_instructions->opE[0xA1] = sknp_vx;

    /* 0xFx** group */
    chip8_instructions->opF[0x07] = ld_vx_dt;
    chip8_instructions->opF[0x0A] = ld_vx_k;
    chip8_instructions->opF[0x15] = ld_dt_vx;
    chip8_instructions->opF[0x18] = ld_st_vx;
    chip8_instructions->opF[0x1E] = add_i_vx;
    chip8_instructions->opF[0x29] = ld_f_vx;
    chip8_instructions->opF[0x33] = ld_b_vx;
    chip8_instructions->opF[0x55] = ld_loc_i_vx;
    chip8_instructions->opF[0x65] = ld_vx_loc_i;
}

void chip8_exec(chip8_t *chip8) {
    if (chip8->cpu.PC > 4094) {
        chip8->power = OFF;
        return;
    }
    uint16_t opcode = (chip8->mem.map[chip8->cpu.PC] << 8) | chip8->mem.map[chip8->cpu.PC+1];
    chip8->instrs->primary[(opcode >> 12) & 0xF](chip8, opcode);
}

/*================================================ Main ==================================================== */
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program.ch8>\n", argv[0]);
        return 1;
    }

    srand((unsigned)time(NULL));

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
    clear_window(&chip8);

    while(chip8.power == ON) {
        handle_input(&chip8);
        chip8_exec(&chip8);
        update_window(&chip8);
    }

    destroy_window(&chip8);
    free(chip8_program);
    return 0;
}