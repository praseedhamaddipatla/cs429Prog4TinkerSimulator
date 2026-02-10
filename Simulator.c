#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START 0x1000
#define MEM_SIZE (1 << 19)
#define REG 32
#define INC 4

// global machine state
static uint64_t pc;
static int running;

static uint64_t regs[REG];
static uint8_t mem[MEM_SIZE];

void initMachine(void) {
    for (int i = 0; i < MEM_SIZE; i++)
        mem[i] = 0;
    for (int i = 0; i < REG; i++)
        regs[i] = 0;
    regs[31] = MEM_SIZE; // stack pointer
    running = 1;
    pc = START;
}

static uint32_t getOpcode(uint32_t instr) { return (instr >> 27) & 0x1F; }
static uint32_t getrd(uint32_t instr) { return (instr >> 21) & 0x1F; }
static uint32_t getrs(uint32_t instr) { return (instr >> 16) & 0x1F; }
static uint32_t getrt(uint32_t instr) { return (instr >> 11) & 0x1F; }

// 12-bit signed immediate
static int32_t getL(uint32_t instr) {
    int32_t imm = instr & 0xFFF;
    if (imm & 0x800)
        imm |= 0xFFFFF000;
    return imm;
}

// 16-bit signed immediate
static int32_t getImm(uint32_t instr) {
    int32_t imm = instr & 0xFFFF;
    if (imm & 0x8000)
        imm |= 0xFFFF0000;
    return imm;
}

static uint64_t load64(uint64_t addr) {
    if (addr > MEM_SIZE - 8) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    uint64_t val = 0;
    for (int i = 0; i < 8; i++)
        val |= ((uint64_t)mem[addr + i]) << (i * 8);
    return val;
}

static void store64(uint64_t addr, uint64_t val) {
    if (addr > MEM_SIZE - 8) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    for (int i = 0; i < 8; i++)
        mem[addr + i] = (val >> (i * 8)) & 0xFF;
}

uint32_t fetchInstr(void) {
    if (pc > MEM_SIZE - 4) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    return mem[pc] | (mem[pc + 1] << 8) | (mem[pc + 2] << 16) |
           (mem[pc + 3] << 24);
}

void execInvalid() {
    fprintf(stderr, "Simulation error\n");
    exit(1);
}

void execAdd(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] + regs[getrt(instr)];
    pc += INC;
}
void execAddi(uint32_t instr) {
    regs[getrd(instr)] += getL(instr);
    pc += INC;
}
void execSub(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] - regs[getrt(instr)];
    pc += INC;
}
void execSubi(uint32_t instr) {
    regs[getrd(instr)] -= getL(instr);
    pc += INC;
}
void execMul(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)] * regs[getrt(instr)];
    pc += INC;
}
void execDiv(uint32_t instr) {
    uint64_t rt = regs[getrt(instr)];
    if (rt == 0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    regs[getrd(instr)] = (uint64_t)((int64_t)regs[getrs(instr)] / (int64_t)rt);
    pc += INC;
}
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

void execShftr(uint32_t instr) {
    regs[getrd(instr)] = (int64_t)regs[getrs(instr)] >> regs[getrt(instr)];
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

void execBr(uint32_t instr) { pc = regs[getrd(instr)]; }
void execBrr(uint32_t instr) { pc += regs[getrd(instr)]; }
void execBrrL(uint32_t instr) { pc += getImm(instr) * 4; }
void execBrnz(uint32_t instr) {
    pc = (regs[getrs(instr)] != 0) ? regs[getrd(instr)] : pc + INC;
}
void execBrgt(uint32_t instr) {
    pc = ((int64_t)regs[getrs(instr)] > (int64_t)regs[getrt(instr)])
             ? regs[getrd(instr)]
             : pc + INC;
}

void execCall(uint32_t instr) {
    store64(regs[31] - 8, pc + INC);
    pc = regs[getrd(instr)];
}
void execReturn() { pc = load64(regs[31] - 8); }

void execPriv(uint32_t instr) {
    uint32_t l = getL(instr);
    if (l == 0x0)
        running = 0;
    else if (l == 0x3) {
        uint32_t rd = getrd(instr);
        if (scanf("%lu", &regs[rd]) != 1) {
            fprintf(stderr, "Simulation error\n");
            exit(1);
        }
    } else if (l == 0x4) {
        uint32_t rd = getrd(instr);
        uint32_t port = regs[rd];
        if (port == 1)
            printf("%lu\n", regs[getrs(instr)]);
    } else {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    pc += INC;
}

void execMovLoad(uint32_t instr) {
    regs[getrd(instr)] = load64(regs[getrs(instr)] + getImm(instr) * 8);
    pc += INC;
}
void execMovStore(uint32_t instr) {
    store64(regs[getrd(instr)] + getImm(instr) * 8, regs[getrs(instr)]);
    pc += INC;
}
void execMovReg(uint32_t instr) {
    regs[getrd(instr)] = regs[getrs(instr)];
    pc += INC;
}
void execMovUpper(uint32_t instr) {
    regs[getrd(instr)] = (regs[getrd(instr)] & 0x000FFFFFFFFFFFFFULL) |
                         ((uint64_t)getL(instr) << 52);
    pc += INC;
}

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
    if (d[getrt(instr)] == 0.0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    d[getrd(instr)] = d[getrs(instr)] / d[getrt(instr)];
    pc += INC;
}

void execHalt() { running = 0; }

void runSim() {
    int count = 0;
    while (running) {
        uint32_t instr = fetchInstr();
        uint32_t op = getOpcode(instr);

        switch (op) {
        case 0x00:
            execAnd(instr);
            break;
        case 0x01:
            execOr(instr);
            break;
        case 0x02:
            execXor(instr);
            break;
        case 0x03:
            execNot(instr);
            break;
        case 0x04:
            execShftr(instr);
            break;
        case 0x05:
            execShftri(instr);
            break;
        case 0x06:
            execShftl(instr);
            break;
        case 0x07:
            execShftli(instr);
            break;
        case 0x08:
            execBr(instr);
            break;
        case 0x09:
            execBrr(instr);
            break;
        case 0x0A:
            execBrrL(instr);
            break;
        case 0x0B:
            execBrnz(instr);
            break;
        case 0x0C:
            execCall(instr);
            break;
        case 0x0D:
            execReturn();
            break;
        case 0x0E:
            execBrgt(instr);
            break;
        case 0x0F:
            execPriv(instr);
            break;
        case 0x10:
            execMovLoad(instr);
            break;
        case 0x11:
            execMovReg(instr);
            break;
        case 0x12:
            execMovUpper(instr);
            break;
        case 0x13:
            execMovStore(instr);
            break;
        case 0x14:
            execAddf(instr);
            break;
        case 0x15:
            execSubf(instr);
            break;
        case 0x16:
            execMulf(instr);
            break;
        case 0x17:
            execDivf(instr);
            break;
        case 0x18:
            execAdd(instr);
            break;
        case 0x19:
            execAddi(instr);
            break;
        case 0x1A:
            execSub(instr);
            break;
        case 0x1B:
            execSubi(instr);
            break;
        case 0x1C:
            execMul(instr);
            break;
        case 0x1D:
            execDiv(instr);
            break;
        case 0x3F:
            execHalt();
            break;
        default:
            execInvalid();
            break;
        }

        regs[0] = 0; // r0 is always 0
        if (++count > 1000000)
            break; // safety
    }
}

int procFile(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f)
        return 1;
    uint32_t addr = START;
    int b;
    while ((b = fgetc(f)) != EOF) {
        if (addr >= MEM_SIZE) {
            fclose(f);
            return 1;
        }
        mem[addr++] = (uint8_t)b;
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid tinker filepath\n");
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
