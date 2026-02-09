#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START 0x1000
#define MEM_SIZE (1 << 20)
#define REG 32
#define INC 4

// global machine state
static uint64_t pc;
static int running;

static uint64_t regs[REG];
static uint8_t mem[MEM_SIZE];

// instruction data

typedef enum { R, I, BR, MOV, NO_OP, PRIV, OTHER } InstrType;

typedef struct {
    const char *name;
    uint8_t opcode;
    InstrType type;
    int argc;
} InstrInfo;

static InstrInfo instrs[] = {
    {"and", 0x00, R, 3},        {"or", 0x01, R, 3},
    {"xor", 0x02, R, 3},        {"not", 0x03, OTHER, 2},
    {"shftr", 0x04, R, 3},      {"shftri", 0x05, I, 2},
    {"shftl", 0x06, R, 3},      {"shftli", 0x07, I, 2},
    {"br", 0x08, BR, 1},        {"brr", 0x09, BR, 1},
    {"brnz", 0x0b, OTHER, 2},   {"call", 0x0c, BR, 1},
    {"return", 0x0d, NO_OP, 0}, {"brgt", 0x0e, R, 3},
    {"priv", 0x0f, PRIV, 4},    {"mov", 0x10, MOV, 2},
    {"addf", 0x14, R, 3},       {"subf", 0x15, R, 3},
    {"mulf", 0x16, R, 3},       {"divf", 0x17, R, 3},
    {"add", 0x18, R, 3},        {"addi", 0x19, I, 2},
    {"sub", 0x1a, R, 3},        {"subi", 0x1b, I, 2},
    {"mul", 0x1c, R, 3},        {"div", 0x1d, R, 3}};

// handler

typedef void (*handler)(uint32_t);
static handler hs[64];

// state initialization

void initMachine(void) {
    memset(mem, 0, MEM_SIZE);
    memset(regs, 0, sizeof(regs));

    running = 1;
    pc = START;
}

// instruction field helpers

static uint32_t getOpcode(uint32_t instr) { return (instr >> 26) & 0x3F; }

static uint32_t getrd(uint32_t instr) { return (instr >> 21) & 0x1F; }

static uint32_t getrs(uint32_t instr) { return (instr >> 16) & 0x1F; }

static uint32_t getrt(uint32_t instr) { return (instr >> 11) & 0x1F; }

static int32_t getImm(uint32_t instr) { return (int16_t)(instr & 0xFFFF); }

static int32_t getImm(uint32_t instr) {
    // lower 16 bits, sign-extended
    return (int16_t)(instr & 0xFFFF);
}

static uint32_t getL(uint32_t instr) {
    // unsigned literal (used for shifts, mov upper, priv)
    return instr & 0xFFFF;
}

static uint64_t load64(uint64_t addr) {
    if (addr + 7 >= MEM_SIZE) {
        fprintf(stderr, "Memory load out of bounds\n");
        exit(1);
    }

    uint64_t val = 0;
    for (int i = 0; i < 8; i++)
        val = (val << 8) | mem[addr + i];
    return val;
}

static void store64(uint64_t addr, uint64_t val) {
    if (addr + 7 >= MEM_SIZE) {
        fprintf(stderr, "Memory store out of bounds\n");
        exit(1);
    }

    for (int i = 7; i >= 0; i--) {
        mem[addr + i] = val & 0xFF;
        val >>= 8;
    }
}

// fetch

uint32_t fetch(void) {
    if (pc + 3 >= MEM_SIZE) {
        fprintf(stderr, "Error: PC out of bounds\n");
        exit(1);
    }

    uint32_t instr = 0;
    instr |= mem[pc] << 24;
    instr |= mem[pc + 1] << 16;
    instr |= mem[pc + 2] << 8;
    instr |= mem[pc + 3];

    return instr;
}

// handlers

void execInvalid(uint32_t instr) {
    (void)instr;
    fprintf(stderr, "Error: invalid opcode at PC=0x%lx\n", pc);
    exit(1);
}

// arithmetic

void execAdd(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] + regs[getrt(instr)];
    pc += INC;
}

void execAddi(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] + getImm(instr);
    pc += INC;
}

void execHalt(uint32_t instr) {
    (void)instr;
    running = 0;
}

void execSub(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] - regs[getrt(instr)];
    pc += INC;
}

void execSubi(uint32_t instr) {
    regs[getrd(instr)] -= getImm(instr);
    pc += INC;
}

void execMul(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] * regs[getrt(instr)];
    pc += INC;
}

void execDiv(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] / regs[getrt(instr)];
    pc += INC;
}

// logic operators

void execAnd(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] & regs[getrt(instr)];
    pc += INC;
}

void execOr(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] | regs[getrt(instr)];
    pc += INC;
}

void execXor(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] ^ regs[getrt(instr)];
    pc += INC;
}

void execNot(uint32_t instr) {
    regs[getrd(instr)] = ~regs[getrs(instr)];
    pc += INC;
}

// shifts

void execShftr(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] >> regs[getrt(instr)];
    pc += INC;
}

void execShftri(uint32_t instr) {
    regs[getrd(instr)] >>= getL(instr);
    pc += INC;
}

void execShftl(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] << regs[getrt(instr)];
    pc += INC;
}

void execShftli(uint32_t instr) {
    regs[getrd(instr)] <<= getL(instr);
    pc += INC;
}

// control flow

void execBr(uint32_t instr) { pc = regs[getrd(instr)]; }

void execBrr(uint32_t instr) { pc += regs[getrd(instr)]; }

void execBrrL(uint32_t instr) { pc += getImm(instr); }

void execBrnz(uint32_t instr) {
    if (regs[getrs(instr)] != 0)
        pc = regs[getrd(instr)];
    else
        pc += INC;
}

void execCall(uint32_t instr) {
    // push return address
    store64(regs[31] - 8, pc + INC);
    regs[31] -= 8;
    pc = regs[getrd(instr)];
}

void execReturn(uint32_t instr) {
    pc = load64(regs[31]);
    regs[31] += 8;
}

void execBrgt(uint32_t instr) {
    if ((int64_t)regs[getrs(instr)] > (int64_t)regs[getrt(instr)])
        pc = regs[getrd(instr)];
    else
        pc += INC;
}

// privileged

void execPriv(uint32_t instr) {
    uint32_t L = getL(instr);

    switch (L) {
    case 0x0: // halt
        running = 0;
        break;

    case 0x3: // input
        regs[getrd(instr)] = getchar();
        pc += INC;
        break;

    case 0x4: // output
        putchar((char)regs[getrs(instr)]);
        pc += INC;
        break;

    default:
        fprintf(stderr, "Illegal priv instruction\n");
        exit(1);
    }
}

// data movement

void execMovLoad(uint32_t instr) {
    uint64_t addr = regs[getrs(instr)] + getImm(instr);
    regs[getrd(instr)] = load64(addr);
    pc += INC;
}

void execMovReg(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)];
    pc += INC;
}

void execMovUpper(uint32_t instr) {
    uint64_t mask = 0x000FFFFFFFFFFFFFULL;
    regs[getrd(instr)] =
        (regs[getrd(instr)] & mask) | ((uint64_t)getL(instr) << 52);
    pc += INC;
}

void execMovStore(uint32_t instr) {
    uint64_t addr = regs[getrd(instr)] + getImm(instr);
    store64(addr, regs[getrs(instr)]);
    pc += INC;
}

// floating point

void execAddf(uint32_t instr) {
    double *d = (double *)regs;
    d[getrd(instr)] = d[getrs(instr)] + d[getrt(instr)];
    pc += INC;
}

void execSubf(uint32_t instr) {
    double *d = (double *)regs;
    d[getrd(instr)] = d[getrs(instr)] - d[getrt(instr)];
    pc += INC;
}

void execMulf(uint32_t instr) {
    double *d = (double *)regs;
    d[getrd(instr)] = d[getrs(instr)] * d[getrt(instr)];
    pc += INC;
}

void execDivf(uint32_t instr) {
    double *d = (double *)regs;
    d[getrd(instr)] = d[getrs(instr)] / d[getrt(instr)];
    pc += INC;
}

// handler table initialization

void initHandlers(void) {
    for (int i = 0; i < 64; i++)
        hs[i] = execInvalid;

    hs[0x00] = execAnd;
    hs[0x01] = execOr;
    hs[0x02] = execXor;
    hs[0x03] = execNot;
    hs[0x04] = execShftr;
    hs[0x05] = execShftri;
    hs[0x06] = execShftl;
    hs[0x07] = execShftli;

    hs[0x08] = execBr;
    hs[0x09] = execBrr;
    hs[0x0A] = execBrrL;
    hs[0x0B] = execBrnz;
    hs[0x0C] = execCall;
    hs[0x0D] = execReturn;
    hs[0x0E] = execBrgt;

    hs[0x0F] = execPriv;

    hs[0x10] = execMovLoad;
    hs[0x11] = execMovReg;
    hs[0x12] = execMovUpper;
    hs[0x13] = execMovStore;

    hs[0x14] = execAddf;
    hs[0x15] = execSubf;
    hs[0x16] = execMulf;
    hs[0x17] = execDivf;

    hs[0x18] = execAdd;
    hs[0x19] = execAddi;
    hs[0x1A] = execSub;
    hs[0x1B] = execSubi;
    hs[0x1C] = execMul;
    hs[0x1D] = execDiv;

    hs[0x3F] = execHalt;
}

// execution file

void run() {
    initHandlers();

    while (running) {
        uint32_t instr = fetch();
        uint32_t opcode = getOpcode(instr);
        hs[opcode](instr);
    }
}

// load object file

int processFile(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) {
        fprintf(stderr, "Error: input file could not be opened\n");
        return 1;
    }

    uint32_t addr = START;
    int byte;

    while ((byte = fgetc(f)) != EOF) {
        if (addr >= MEM_SIZE) {
            fprintf(stderr, "Error: program too large\n");
            fclose(f);
            return 1;
        }
        mem[addr++] = (uint8_t)byte;
    }

    fclose(f);
    return 0;
}

// main

int testmain(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error: must have exactly one input file\n");
        return 1;
    }

    initMachine();

    if (processFile(argv[1]) != 0)
        return 1;

    run();
    return 0;
}

int main(int argc, char *argv[]) { return testmain(argc, argv); }
