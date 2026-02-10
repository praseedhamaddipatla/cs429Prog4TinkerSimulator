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

// machine initialization
void initMachine(void) {
    for (int i = 0; i < MEM_SIZE; i++)
        mem[i] = 0;
    for (int i = 0; i < REG; i++)
        regs[i] = 0;
    regs[31] = MEM_SIZE; // stack pointer
    running = 1;
    pc = START;
}

// instruction field helpers
static uint32_t getOpcode(uint32_t instr) { return (instr >> 27) & 0x1F; }
static uint32_t getrd(uint32_t instr) { return (instr >> 22) & 0x1F; }
static uint32_t getrs(uint32_t instr) { return (instr >> 17) & 0x1F; }
static uint32_t getrt(uint32_t instr) { return (instr >> 12) & 0x1F; }

// 12-bit signed immediate
static int32_t getImm(uint32_t instr) {
    int32_t imm = instr & 0xFFF;
    if (imm & 0x800)
        imm |= 0xFFFFF000;
    return imm;
}

// 12-bit signed L field
static int32_t getL(uint32_t instr) {
    int32_t l = instr & 0xFFF;
    if (l & 0x800)
        l |= 0xFFFFF000;
    return l;
}

// memory operations
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

// fetch instruction
uint32_t fetchInstr(void) {
    if (pc > MEM_SIZE - 4) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    uint32_t instr = 0;
    instr |= (uint32_t)mem[pc];
    instr |= (uint32_t)mem[pc + 1] << 8;
    instr |= (uint32_t)mem[pc + 2] << 16;
    instr |= (uint32_t)mem[pc + 3] << 24;
    return instr;
}

// invalid opcode handler
void execInvalid() {
    fprintf(stderr, "Simulation error\n");
    exit(1);
}

// arithmetic
void execAdd(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] + regs[rt];
    pc += INC;
}
void execAddi(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t l = getL(instr);
    regs[rd] += l;
    pc += INC;
}
void execSub(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] - regs[rt];
    pc += INC;
}
void execSubi(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t l = getL(instr);
    regs[rd] -= l;
    pc += INC;
}
void execMul(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] * regs[rt];
    pc += INC;
}
void execDiv(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    if (regs[rt] == 0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    regs[rd] = (uint64_t)((int64_t)regs[rs] / (int64_t)regs[rt]);
    pc += INC;
}

// logic
void execAnd(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] & regs[rt];
    pc += INC;
}
void execOr(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] | regs[rt];
    pc += INC;
}
void execXor(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] ^ regs[rt];
    pc += INC;
}
void execNot(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr);
    regs[rd] = ~regs[rs];
    pc += INC;
}

// shifts
void execShftr(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = (uint64_t)((int64_t)regs[rs] >> regs[rt]);
    pc += INC;
}
void execShftri(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t l = getL(instr);
    regs[rd] >>= l;
    pc += INC;
}
void execShftl(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    regs[rd] = regs[rs] << regs[rt];
    pc += INC;
}
void execShftli(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t l = getL(instr);
    regs[rd] <<= l;
    pc += INC;
}

// control flow
void execBr(uint32_t instr) {
    uint32_t rd = getrd(instr);
    pc = regs[rd];
}
void execBrr(uint32_t instr) {
    uint32_t rd = getrd(instr);
    pc += regs[rd] * 4;
}
void execBrrL(uint32_t instr) {
    int32_t l = getL(instr);
    pc += l * 4;
}
void execBrnz(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr);
    if (regs[rs] != 0)
        pc = regs[rd];
    else
        pc += INC;
}
void execCall(uint32_t instr) {
    uint64_t ret = pc + INC;
    regs[31] -= 8;
    store64(regs[31], ret);
    pc = regs[getrd(instr)];
}
void execReturn() {
    pc = load64(regs[31]);
    regs[31] += 8;
}
void execBrgt(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr), rt = getrt(instr);
    if ((int64_t)regs[rs] > (int64_t)regs[rt])
        pc = regs[rd];
    else
        pc += INC;
}

// privileged
void execPriv(uint32_t instr) {
    uint32_t l = getL(instr);
    if (l == 0x0) {
        running = 0;
    } else if (l == 0x3) {
        uint32_t rd = getrd(instr), rs = getrs(instr);
        if (regs[rs] == 0) {
            if (scanf("%lu", &regs[rd]) != 1) {
                fprintf(stderr, "Simulation error\n");
                exit(1);
            }
        }
        pc += INC;
    } else if (l == 0x4) {
        uint32_t rd = getrd(instr), rs = getrs(instr);
        if (regs[rd] == 1)
            printf("%lu\n", regs[rs]);
        pc += INC;
    } else {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
}

// data movement
void execMovLoad(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr);
    int32_t l = getL(instr);
    uint64_t addr = regs[rs] + l * 8;
    regs[rd] = load64(addr);
    pc += INC;
}

void execMovStore(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr);
    int32_t l = getL(instr);
    uint64_t addr = regs[rd] + l * 8;
    store64(addr, regs[rs]);
    pc += INC;
}

void execMovReg(uint32_t instr) {
    uint32_t rd = getrd(instr), rs = getrs(instr);
    regs[rd] = regs[rs];
    pc += INC;
}
void execMovUpper(uint32_t instr) {
    uint32_t rd = getrd(instr);
    int32_t l = getL(instr);
    uint64_t mask = 0x000FFFFFFFFFFFFFULL;
    regs[rd] = (regs[rd] & mask) | ((uint64_t)l << 52);
    pc += INC;
}

// floating point helpers
static double getDouble(uint32_t r) {
    double v;
    memcpy(&v, &regs[r], 8);
    return v;
}
static void setDouble(uint32_t r, double v) { memcpy(&regs[r], &v, 8); }

void execAddf(uint32_t instr) {
    setDouble(getrd(instr), getDouble(getrs(instr)) + getDouble(getrt(instr)));
    pc += INC;
}
void execSubf(uint32_t instr) {
    setDouble(getrd(instr), getDouble(getrs(instr)) - getDouble(getrt(instr)));
    pc += INC;
}
void execMulf(uint32_t instr) {
    setDouble(getrd(instr), getDouble(getrs(instr)) * getDouble(getrt(instr)));
    pc += INC;
}
void execDivf(uint32_t instr) {
    if (getDouble(getrt(instr)) == 0.0) {
        fprintf(stderr, "Simulation error\n");
        exit(1);
    }
    setDouble(getrd(instr), getDouble(getrs(instr)) / getDouble(getrt(instr)));
    pc += INC;
}

// halt
void execHalt() { running = 0; }

// execution loop
void runSim() {
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
        regs[0] = 0; // zero register
    }
}

// load binary file
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

// main
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
