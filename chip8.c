#include <stdint.h>

typedef struct memory_t {
    uint8_t map[4096];
} memory_t;

typedef struct stack_t {
    uint16_t data[16]; // used to store the address that the interpreter should return to
    uint8_t  sp;
} stack_t;

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
    cpu_t regs;
    stack_t     stack;
    display_t   display;
    keyboard_t  keyboard;
    uint8_t     delay_timer;
    uint8_t     sound_timer;
} chip8_t;

int main() {
     
}