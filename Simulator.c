#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START 0x1000
#define MEM_SIZE (1 << 19)
#define REG 32
#define INC 4

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

static int32_t getL(uint32_t i) {
    int32_t imm = i & 0xFFF;
    if (imm & 0x800)
        imm |= ~0xFFF;
    return imm;
}

// memory helpers
uint64_t load64(uint64_t addr) {
    if (addr + 7 >= MEM_SIZE) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }

    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= ((uint64_t)mem[addr + i]) << (8 * i);
    return v;
}

void store64(uint64_t addr, uint64_t val) {
    if (addr + 7 >= MEM_SIZE) {
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
void execBrgt(uint32_t i) {
    if ((int64_t)regs[getrd(i)] > (int64_t)regs[getrs(i)])
        pc += getL(i);
    else
        NEXT;
}

// priv
void execPriv(uint32_t i) {
    int L = getL(i);

    if (L == 0) {
        running = 0;
        return;
    }

    if (L == 3) {
        scanf("%lu", &regs[getrd(i)]);
        NEXT;
        return;
    }
    if (L == 4) {
        if (regs[getrd(i)] == 1)
            printf("%lu\n", regs[getrs(i)]);
        NEXT;
        return;
    }

    fprintf(stderr, "Simulation error\n");
    exit(1);
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
        fprintf(stderr, "Simulation error");
        exit(1);
    }
    c = a / b;
    memcpy(&regs[getrd(i)], &c, 8);
    pc += INC;
}

// main loop
void runSim(void) {
    int cnt = 0;
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

        default:
            fprintf(stderr,
                    "Simulation error: unknown opcode 0x%02x at PC=0x%lx\n", op,
                    pc);
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
    procFile(argv[1]);
    runSim();
    return 0;
}
