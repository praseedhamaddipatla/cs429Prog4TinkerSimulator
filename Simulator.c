#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START 0x1000
#define MEM_SIZE (1 << 19)
#define REG 32
#define INC 4
#define DEBUG 1

static uint64_t pc;
static int running;
static uint64_t regs[REG];
static uint8_t mem[MEM_SIZE];

// initialization
void initMachine(void) {
    memset(mem, 0, sizeof(mem));
    memset(regs, 0, sizeof(regs));
    regs[31] = MEM_SIZE;
    pc = START;
    running = 1;
}

// helpers
static uint32_t getOpcode(uint32_t i) { return (i >> 27) & 0x1F; }
static uint32_t getrd(uint32_t i) { return (i >> 22) & 0x1F; }
static uint32_t getrs(uint32_t i) { return (i >> 17) & 0x1F; }
static uint32_t getrt(uint32_t i) { return (i >> 12) & 0x1F; }
static inline uint64_t getImm(uint32_t instr) { return instr & 0xFFF; }

static int32_t getL(uint32_t i) {
    int32_t imm = i & 0xFFF;
    if (imm & 0x800)
        imm |= ~0xFFF;
    return imm;
}

// memory helpers
uint64_t load64(uint64_t addr) {
    if (addr > MEM_SIZE - 8) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= ((uint64_t)mem[addr + i]) << (8 * i);
    return v;
}

void store64(uint64_t addr, uint64_t val) {
    if (addr % 8 != 0 || addr + 7 >= MEM_SIZE) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    for (int i = 0; i < 8; i++)
        mem[addr + i] = (val >> (8 * i)) & 0xFF;
}

// fetch
uint32_t fetchInstr(void) {
    if (pc + 3 >= MEM_SIZE) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    uint32_t instr = mem[pc] | (mem[pc + 1] << 8) | (mem[pc + 2] << 16) |
                     (mem[pc + 3] << 24);
    return instr;
}

// execution helpers
#define NEXT pc += INC

// logic
void execAnd(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] & regs[getrt(i)];
    NEXT;
}
void execOr(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] | regs[getrt(i)];
    NEXT;
}
void execXor(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] ^ regs[getrt(i)];
    NEXT;
}
void execNot(uint32_t i) {
    regs[getrd(i)] = ~regs[getrs(i)];
    NEXT;
}

// shifts
void execShftr(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] >> regs[getrt(i)];
    NEXT;
}
void execShftri(uint32_t i) {
    regs[getrd(i)] >>= getL(i);
    NEXT;
}
void execShftl(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] << regs[getrt(i)];
    NEXT;
}
void execShftli(uint32_t i) {
    regs[getrd(i)] <<= getL(i);
    NEXT;
}

// arithmetic
void execAdd(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] + regs[getrt(i)];
    NEXT;
}
void execAddi(uint32_t i) {
    regs[getrd(i)] += getL(i);
    NEXT;
}
void execSub(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] - regs[getrt(i)];
    NEXT;
}
void execSubi(uint32_t i) {
    regs[getrd(i)] -= getL(i);
    NEXT;
}
void execMul(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)] * regs[getrt(i)];
    NEXT;
}
void execDiv(uint32_t i) {
    if (!regs[getrt(i)]) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    regs[getrd(i)] = (int64_t)regs[getrs(i)] / (int64_t)regs[getrt(i)];
    NEXT;
}

// mov
void execMovLoad(uint32_t i) {
    uint64_t addr = regs[getrs(i)] + getL(i);
    regs[getrd(i)] = load64(addr);
    NEXT;
}

void execMovStore(uint32_t i) {
    uint64_t addr = regs[getrd(i)] + getL(i);
    store64(addr, regs[getrs(i)]);
    NEXT;
}

void execMovReg(uint32_t i) {
    regs[getrd(i)] = regs[getrs(i)];
    NEXT;
}

void execMovImm(uint32_t i) {
    regs[getrd(i)] = getL(i);
    NEXT;
}

// control
void execBrgt(uint32_t instr) {
    uint32_t rd = getrd(instr);
    uint32_t rs = getrs(instr);
    uint32_t rt = getrt(instr);
    int64_t v1 = (int64_t)regs[rs]; // Changed from rd
    int64_t v2 = (int64_t)regs[rt]; // Changed from rs
    if (v1 > v2) {
        pc = regs[rd];
    } else {
        pc = pc + INC;
    }
}

// priv
void execPriv(uint32_t i) {
    uint32_t L = getImm(i);

    switch (L) {
    case 0x0: // HALT
        exit(0);

    case 0x3: { // INPUT
        uint32_t rd = getrd(i);
        uint32_t rs = getrs(i);
        uint64_t p = regs[rs];
        if (p == 0) {
            unsigned long long val;
            if (scanf("%llu", &val) != 1) {
                fprintf(stderr, "Simulation error\n");
                exit(1);
            }
            regs[rd] = val;
        }
        pc = pc + INC;
        return;
    }

    case 0x4: { // OUTPUT
        uint32_t rd = getrd(i);
        uint32_t rs = getrs(i);
        uint64_t p = regs[rd];
        if (p == 1) { // Only output to port 1
            printf("%lu\n", (long unsigned int)regs[rs]);
        }
        pc = pc + INC;
        return;
    }

    default:
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
}

// floating point
void execAddf(uint32_t i) {
    double a, b, c;
    memcpy(&a, &regs[getrs(i)], 8);
    memcpy(&b, &regs[getrt(i)], 8);
    c = a + b;
    memcpy(&regs[getrd(i)], &c, 8);
    pc += INC;
}

void execSubf(uint32_t i) {
    double a, b, c;
    memcpy(&a, &regs[getrs(i)], 8);
    memcpy(&b, &regs[getrt(i)], 8);
    c = a - b;
    memcpy(&regs[getrd(i)], &c, 8);
    pc += INC;
}

void execMulf(uint32_t i) {
    double a, b, c;
    memcpy(&a, &regs[getrs(i)], 8);
    memcpy(&b, &regs[getrt(i)], 8);
    c = a * b;
    memcpy(&regs[getrd(i)], &c, 8);
    pc += INC;
}

void execDivf(uint32_t i) {
    double a, b, c;
    memcpy(&a, &regs[getrs(i)], 8);
    memcpy(&b, &regs[getrt(i)], 8);
    if (b == 0.0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    c = a / b;
    memcpy(&regs[getrd(i)], &c, 8);
    pc += INC;
}

void execBr(uint32_t i) { pc = regs[getrd(i)]; }
void execBrrReg(uint32_t i) { pc += regs[getrd(i)]; }
void execBrrImm(uint32_t i) { pc += getL(i); }
void execBrnz(uint32_t i) {
    if (regs[getrs(i)] == 0)
        NEXT;
    else
        pc = regs[getrd(i)];
}
void execCall(uint32_t i) {
    uint64_t retAddr = pc + INC;
    regs[31] -= 8;
    store64(regs[31], retAddr);
    pc = regs[getrd(i)];
}
void execReturn() {
    uint64_t retAddr = load64(regs[31]);
    regs[31] += 8;
    pc = retAddr;
}

// main loop
void runSim(void) {
    while (running) {
        uint32_t i = fetchInstr();
        uint32_t op = getOpcode(i);

        switch (op) {
        case 0x00:
            execAnd(i);
            break;
        case 0x01:
            execOr(i);
            break;
        case 0x02:
            execXor(i);
            break;
        case 0x03:
            execNot(i);
            break;
        case 0x04:
            execShftr(i);
            break;
        case 0x05:
            execShftri(i);
            break;
        case 0x06:
            execShftl(i);
            break;
        case 0x07:
            execShftli(i);
            break;

        case 0x18:
            execAdd(i);
            break;
        case 0x19:
            execAddi(i);
            break;
        case 0x1A:
            execSub(i);
            break;
        case 0x1B:
            execSubi(i);
            break;
        case 0x1C:
            execMul(i);
            break;
        case 0x1D:
            execDiv(i);
            break;

        case 0x10:
            execMovLoad(i);
            break;
        case 0x11:
            execMovReg(i);
            break;
        case 0x12:
            execMovImm(i);
            break;
        case 0x13:
            execMovStore(i);
            break;

        case 0x0E:
            execBrgt(i);
            break;
        case 0x0F:
            execPriv(i);
            break;

        case 0x14:
            execAddf(i);
            break;
        case 0x15:
            execSubf(i);
            break;
        case 0x16:
            execMulf(i);
            break;
        case 0x17:
            execDivf(i);
            break;

        case 0x08:
            execBr(i);
            break;
        case 0x09:
            execBrrReg(i);
            break;
        case 0x0A:
            execBrrImm(i);
            break;
        case 0x0B:
            execBrnz(i);
            break;
        case 0x0C:
            execCall(i);
            break;
        case 0x0D:
            execReturn();
            break;

        default:
            fprintf(stderr, "Simulation error\n");
            exit(1);
        }

        regs[0] = 0;
    }
}

// load file
int procFile(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f)
        return 1;

    uint64_t a = START;
    int c;
    while ((c = fgetc(f)) != EOF)
        mem[a++] = c;

    fclose(f);
    return 0;
}

// main
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    initMachine();

    if (procFile(argv[1]) != 0) {
        fprintf(stderr, "Invalid tinker filepath\n");
        return 1;
    }

    runSim();
    return 0;
}