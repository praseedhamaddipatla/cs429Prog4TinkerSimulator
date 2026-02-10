#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START 0x1000
#define MEM_SIZE (1 << 19)
#define REG 32
#define INC 8

// global machine state
static uint64_t pc;
static int running;

static uint64_t regs[REG];
static uint8_t mem[MEM_SIZE];

// state init

void initMachine(void) {
    int i;
    for (i = 0; i < MEM_SIZE; i++) {
        mem[i] = 0;
    }
    for (i = 0; i < REG; i++) {
        regs[i] = 0;
    }
    regs[31] = MEM_SIZE;
    running = 1;
    pc = START;
}

// instruction field helpers

static uint32_t getOpcode(uint32_t instr) { 
    uint32_t op = (instr >> 26);
    op = op & 0x3F;
    return op;
}

static uint32_t getrd(uint32_t instr) { 
    uint32_t rd = (instr >> 21);
    rd = rd & 0x1F;
    return rd;
}

static uint32_t getrs(uint32_t instr) { 
    uint32_t rs = (instr >> 16);
    rs = rs & 0x1F;
    return rs;
}

static uint32_t getrt(uint32_t instr) { 
    uint32_t rt = (instr >> 11);
    rt = rt & 0x1F;
    return rt;
}

static int32_t getImm(uint32_t instr) {
    // lower 16 bits, sign-extended
    int16_t imm = (int16_t)(instr & 0xFFFF);
    return (int32_t)imm;
}

static uint32_t getL(uint32_t instr) {
    // unsigned literal (used for shifts, mov upper, priv)
    uint32_t l = instr & 0xFFFF;
    return l;
}

static uint64_t load64(uint64_t addr) {
    if (addr > MEM_SIZE - 8) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    uint64_t val = 0;
    int i;
    for (i = 0; i < 8; i++) {
        val = val << 8;
        val = val | mem[addr + i];
    }
    return val;
}

static void store64(uint64_t addr, uint64_t val) {
    if (addr > MEM_SIZE - 8) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    int i;
    for (i = 7; i >= 0; i--) {
        mem[addr + i] = val & 0xFF;
        val = val >> 8;
    }
}

// fetch

uint32_t fetchInstr(void) {
    if (pc > MEM_SIZE - 4) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    uint32_t instr = 0;
    instr = instr | (mem[pc] << 24);
    instr = instr | (mem[pc + 1] << 16);
    instr = instr | (mem[pc + 2] << 8);
    instr = instr | mem[pc + 3];

    return instr;
}

// handlers

void execInvalid() {
    fprintf(stderr, "Simulation error\n");
    exit(1);
}

// arithmetic

void execAdd(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] + regs[rt];
    pc = pc + INC;
}

void execAddi(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t imm = getImm(instr);
    regs[rd] = regs[rd] + imm;
    pc += INC;
}

void execHalt() {
    running = 0;
}

void execSub(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] - regs[rt];
    pc = pc + INC;
}

void execSubi(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t imm = getImm(instr);
    regs[rd] = regs[rd] - imm;
    pc += INC;
}

void execMul(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] * regs[rt];
    pc = pc + INC;
}

void execDiv(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    if (regs[rt] == 0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    int64_t srs = (int64_t)regs[rs];
    int64_t srt = (int64_t)regs[rt];
    regs[rd] = (uint64_t)(srs / srt);
    pc = pc + INC;
}

// logic operators

void execAnd(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] & regs[rt];
    pc = pc + INC;
}

void execOr(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] | regs[rt];
    pc = pc + INC;
}

void execXor(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] ^ regs[rt];
    pc = pc + INC;
}

void execNot(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    regs[rd] = ~regs[rs];
    pc = pc + INC;
}

// shifts

void execShftr(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = (uint64_t)((int64_t)regs[rs] >> regs[rt]);
    pc += INC;
}

void execShftri(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t l = getL(instr);
    regs[rd] = regs[rd] >> l;
    pc = pc + INC;
}

void execShftl(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    regs[rd] = regs[rs] << regs[rt];
    pc = pc + INC;
}

void execShftli(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t l = getL(instr);
    regs[rd] = regs[rd] << l;
    pc = pc + INC;
}

// control flow

void execBr(uint32_t instr) { 
    uint32_t rd = getrd(instr);
    pc = regs[rd];
}

void execBrr(uint32_t instr) { 
    uint32_t rd = getrd(instr);
    pc = pc + regs[rd];
}

void execBrrL(uint32_t instr) { 
    int32_t imm = getImm(instr);
    pc = pc + imm;
}

void execBrnz(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    if (regs[rs] != 0) {
        pc = regs[rd];
    } else {
        pc = pc + INC;
    }
}

void execCall(uint32_t instr) {
    uint64_t ret = pc + INC;
    regs[31] -= 8;
    store64(regs[31], ret);
    uint32_t rd = getrd(instr);
    pc = regs[rd];
}

void execReturn() {
    pc = load64(regs[31]);
    regs[31] += 8;
}

void execBrgt(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    int64_t v1 = (int64_t)regs[rs];
    int64_t v2 = (int64_t)regs[rt];
    if (v1 > v2) {
        pc = regs[rd];
    } else {
        pc = pc + INC;
    }
}

// privileged

void execPriv(uint32_t instr) {
    uint32_t l = getL(instr);

    if (l == 0x0) {
        // halt
        running = 0;
    } else if (l == 0x3) {
        // input
        uint32_t rd = getrd(instr);
        uint32_t rs = getrs(instr);
        uint64_t p = regs[rs];
        if (p == 0) {
            int res = scanf("%lu", &regs[rd]);
            if (res != 1) {
                fprintf(stderr, "Simulation error\n");
                exit(1);
            }
        }
        pc = pc + INC;
    } else if (l == 0x4) {
        // output
        uint32_t rd = getrd(instr);
        uint32_t rs = getrs(instr);
        uint64_t p = regs[rd];
        if (p == 1) {
            printf("%lu\n", regs[rs]);
        }
        pc = pc + INC;
    } else {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
}

// data movement

void execMovLoad(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    int32_t imm = getImm(instr);
    uint64_t addr = regs[rs] + imm;
    uint64_t val = load64(addr);
    regs[rd] = val;
    pc = pc + INC;
}

void execMovReg(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    regs[rd] = regs[rs];
    pc = pc + INC;
}

void execMovUpper(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t l = getL(instr);
    uint64_t mask = 0x000FFFFFFFFFFFFFULL;
    uint64_t upper = (uint64_t)l;
    upper = upper << 52;
    uint64_t lower = regs[rd] & mask;
    regs[rd] = lower | upper;
    pc = pc + INC;
}

void execMovStore(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    int32_t imm = getImm(instr);
    uint64_t addr = regs[rd] + imm;
    uint64_t val = regs[rs];
    store64(addr, val);
    pc = pc + INC;
}

// floating point

void execAddf(uint32_t instr) {
    double *d = (double *)regs;
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    d[rd] = d[rs] + d[rt];
    pc = pc + INC;
}

void execSubf(uint32_t instr) {
    double *d = (double *)regs;
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    d[rd] = d[rs] - d[rt];
    pc = pc + INC;
}

void execMulf(uint32_t instr) {
    double *d = (double *)regs;
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    d[rd] = d[rs] * d[rt];
    pc = pc + INC;
}

void execDivf(uint32_t instr) {
    double *d = (double *)regs;
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    if (d[rt] == 0.0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    d[rd] = d[rs] / d[rt];
    pc = pc + INC;
}

// execution

void runSim() {
    while (running) {
        uint32_t instr = fetchInstr();
        uint32_t op = getOpcode(instr);
        
        switch (op) {
            case 0x00: execAnd(instr); break;
            case 0x01: execOr(instr); break;
            case 0x02: execXor(instr); break;
            case 0x03: execNot(instr); break;
            case 0x04: execShftr(instr); break;
            case 0x05: execShftri(instr); break;
            case 0x06: execShftl(instr); break;
            case 0x07: execShftli(instr); break;
            case 0x08: execBr(instr); break;
            case 0x09: execBrr(instr); break;
            case 0x0A: execBrrL(instr); break;
            case 0x0B: execBrnz(instr); break;
            case 0x0C: execCall(instr); break;
            case 0x0D: execReturn(); break;
            case 0x0E: execBrgt(instr); break;
            case 0x0F: execPriv(instr); break;
            case 0x10: execMovLoad(instr); break;
            case 0x11: execMovReg(instr); break;
            case 0x12: execMovUpper(instr); break;
            case 0x13: execMovStore(instr); break;
            case 0x14: execAddf(instr); break;
            case 0x15: execSubf(instr); break;
            case 0x16: execMulf(instr); break;
            case 0x17: execDivf(instr); break;
            case 0x18: execAdd(instr); break;
            case 0x19: execAddi(instr); break;
            case 0x1A: execSub(instr); break;
            case 0x1B: execSubi(instr); break;
            case 0x1C: execMul(instr); break;
            case 0x1D: execDiv(instr); break;
            case 0x3F: execHalt(); break;
            default: execInvalid(); break;
        }

        regs[0] = 0;
    }
}

// load obj file

int procFile(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) {
        return 1;
    }

    uint32_t addr = START;
    int b;

    while ((b = fgetc(f)) != EOF) {
        if (addr >= MEM_SIZE) {
            fclose(f);
            return 1;
        }
        mem[addr] = (uint8_t)b;
        addr = addr + 1;
    }

    fclose(f);
    return 0;
}

// main

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid tinker filepath\n");
        return 1;
    }

    initMachine();

    int res = procFile(argv[1]);
    if (res != 0) {
        fprintf(stderr, "Invalid tinker filepath\n");
        return 1;
    }

    runSim();
    return 0;
}