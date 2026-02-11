#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENC(op, rd, rs, rt, imm)                                               \
    ((uint32_t)((op << 27) | (rd << 22) | (rs << 17) | (rt << 12) |            \
                ((imm) & 0xFFF)))

static int failures = 0;

//helpers
void writeInstr(FILE *f, uint32_t i) { fwrite(&i, 4, 1, f); }

void writeHalt(FILE *f) { writeInstr(f, ENC(0x0F, 0, 0, 0, 0)); }

void run_and_capture(const char *bin) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./sim %s > out.txt 2>&1", bin);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
        exit(1);
    }
}

int compare_output(const char *expected) {
    FILE *f = fopen("out.txt", "r");
    if (!f)
        return 0;

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    //trailing
    while (n > 0 &&
           (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
        buf[n - 1] = '\0';
        n--;
    }

    char expected_copy[1024];
    strncpy(expected_copy, expected, sizeof(expected_copy) - 1);
    expected_copy[sizeof(expected_copy) - 1] = '\0';

    size_t e = strlen(expected_copy);
    while (e > 0 &&
           (expected_copy[e - 1] == '\n' || expected_copy[e - 1] == '\r' ||
            expected_copy[e - 1] == ' ')) {
        expected_copy[e - 1] = '\0';
        e--;
    }

    return strcmp(buf, expected_copy) == 0;
}

void report(const char *name, const char *expected) {
    if (compare_output(expected)) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);

        FILE *f = fopen("out.txt", "r");
        if (f) {
            char buf[1024];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            fclose(f);

            printf("  Expected:\n%s", expected);
            printf("  Got:\n%s\n", buf);
        }

        failures++;
    }
}

// tests

void test_logic() {
    FILE *f = fopen("test_logic.bin", "wb");

    writeInstr(f, ENC(0x12, 1, 0, 0, 10));
    writeInstr(f, ENC(0x12, 2, 0, 0, 12));
    writeInstr(f, ENC(0x00, 3, 1, 2, 0));
    writeInstr(f, ENC(0x12, 4, 0, 0, 1));
    writeInstr(f, ENC(0x0F, 4, 3, 0, 4));
    writeHalt(f);
    fclose(f);

    run_and_capture("test_logic.bin");
    report("Logic", "8\n");
}

void test_shift() {
    FILE *f = fopen("test_shift.bin", "wb");

    writeInstr(f, ENC(0x12, 1, 0, 0, 8));
    writeInstr(f, ENC(0x07, 1, 0, 0, 2));
    writeInstr(f, ENC(0x12, 4, 0, 0, 1));
    writeInstr(f, ENC(0x0F, 4, 1, 0, 4));
    writeHalt(f);
    fclose(f);

    run_and_capture("test_shift.bin");
    report("Shift", "32\n");
}

void test_arith() {
    FILE *f = fopen("test_arith.bin", "wb");

    writeInstr(f, ENC(0x12, 1, 0, 0, 20));
    writeInstr(f, ENC(0x12, 2, 0, 0, 5));
    writeInstr(f, ENC(0x18, 3, 1, 2, 0));
    writeInstr(f, ENC(0x1D, 5, 1, 2, 0));
    writeInstr(f, ENC(0x12, 4, 0, 0, 1));
    writeInstr(f, ENC(0x0F, 4, 3, 0, 4));
    writeInstr(f, ENC(0x0F, 4, 5, 0, 4));
    writeHalt(f);
    fclose(f);

    run_and_capture("test_arith.bin");
    report("Arithmetic", "25\n4\n");
}

void test_memory() {
    FILE *f = fopen("test_memory.bin", "wb");

    writeInstr(f, ENC(0x12, 1, 0, 0, 100));
    writeInstr(f, ENC(0x12, 2, 0, 0, 200));
    writeInstr(f, ENC(0x13, 2, 1, 0, 0));
    writeInstr(f, ENC(0x10, 3, 2, 0, 0));
    writeInstr(f, ENC(0x12, 4, 0, 0, 1));
    writeInstr(f, ENC(0x0F, 4, 3, 0, 4));
    writeHalt(f);
    fclose(f);

    run_and_capture("test_memory.bin");
    report("Memory", "100\n");
}

void test_branch() {
    FILE *f = fopen("test_branch.bin", "wb");

    // r1 = 5
    writeInstr(f, ENC(0x12, 1, 0, 0, 5));

    writeInstr(f, ENC(0x0A, 0, 0, 0, 8));

    writeInstr(f, ENC(0x12, 1, 0, 0, 999));
    writeInstr(f, ENC(0x12, 1, 0, 0, 999));

    writeInstr(f, ENC(0x12, 4, 0, 0, 1));
    writeInstr(f, ENC(0x0F, 4, 1, 0, 4));

    writeHalt(f);
    fclose(f);

    run_and_capture("test_branch.bin");
    report("Branch", "999");
}

void test_invalid() {
    FILE *f = fopen("test_invalid.bin", "wb");
    writeInstr(f, ENC(0x1F, 0, 0, 0, 0));
    fclose(f);

    int r = system("./sim test_invalid.bin > /dev/null 2>&1");
    if (r != 0) {
        printf("[PASS] Invalid opcode\n");
    } else {
        printf("[FAIL] Invalid opcode\n");
        failures++;
    }
}

// main

int main() {
    printf("Compiling simulator...\n");
    if (system("gcc -Wall -Wextra -std=c11 -O2 Simulator.c -o sim") != 0) {
        printf("Failed to compile Simulator.c\n");
        return 1;
    }

    printf("\nRunning tests...\n\n");

    test_logic();
    test_shift();
    test_arith();
    test_memory();
    test_branch();
    test_invalid();

    printf("\n-------------------------\n");

    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("%d TEST(S) FAILED\n", failures);
        return 1;
    }
}
