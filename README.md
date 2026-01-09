# CHIP-8 Emulator 

A simple, from scratch implementation of the **CHIP-8 virtual machine**, written in **C** with **SDL2** for graphics and input.

## What is CHIP-8?

CHIP-8 is a **virtual machine**, created in the mid-1970s by **Joseph Weisbecker** to make it easier to write games for early microcomputers such as the COSMAC VIP.

CHIP-8 programs were small, simple, and portable. Many classic games like *Pong*, *Breakout*, and *Space Invaders* were written for CHIP-8.

## CHIP-8 Architecture Overview

- **Memory:** 4096 bytes (4 KB)
- **Program start address:** `0x200`
- **Registers:**
  - 16 general-purpose 8-bit registers (`V0`–`VF`)
  - `VF` is also used as a flag register
- **Index register:** `I` (16-bit)
- **Program counter:** `PC`
- **Stack:** used for subroutine calls
- **Display:** 64 × 32 monochrome pixels
- **Timers:**
  - Delay timer
  - Sound timer


## Keyboard Layout

```
Original CHIP-8 keypad:        Mapped keyboard keys:

1 2 3 C                        1 2 3 4
4 5 6 D        --->            Q W E R
7 8 9 E                        A S D F
A 0 B F                        Z X C V
```


## Dependencies

This project depends on:

- **SDL2** — for window creation, rendering, and keyboard input

### Installing SDL2 (macOS with Homebrew)

```sh
brew install sdl2
```

Verify installation:

```sh
which sdl2-config
```


## Building the Emulator

From the project root:

```sh
make
```

This will produce an executable named:

```sh
chip8
```


## Running the Emulator

Run the emulator from the terminal and pass a CHIP-8 ROM:

```sh
./chip8 path/to/rom.ch8
```

Example:

```sh
./chip8 roms/PONG.ch8
```


## Project Goals

- Implement the CHIP-8 instruction set faithfully
- Keep the codebase simple and readable
- Make all machine state explicit

This project is intentionally written in **plain C**, without object systems or heavy abstractions, to keep the machine model visible.


## TODO

- [ ] Implement **audio output** (beep when sound timer > 0)
- [ ] Improve timing accuracy


## References

- Cowgod’s CHIP-8 Technical Reference