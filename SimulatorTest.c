#include <stdio.h>
#include <stdint.h>

uint32_t encode_instruction(uint8_t opcode, uint8_t rd, uint8_t rs, uint8_t rt, uint16_t immediate) {
    uint32_t instr = 0;
    instr |= (uint32_t)(opcode & 0x1F) << 27;
    instr |= (uint32_t)(rd & 0x1F) << 22;
    instr |= (uint32_t)(rs & 0x1F) << 17;
    instr |= (uint32_t)(rt & 0x1F) << 12;
    instr |= (uint32_t)(immediate & 0xFFF);
    return instr;
}

void write_tko_file(const char* filename, uint32_t* buffer, size_t count) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to open file");
        return;
    }

    fwrite(buffer, sizeof(uint32_t), count, f);
    fclose(f);
    printf("Created %s\n", filename);
}

void create_test_halt() {
    uint32_t code[] = {
        encode_instruction(0xf, 0, 0, 0, 0) // halt
    };
    write_tko_file("test_halt.tko", code, 1);
}

void create_test_addi() {
    uint32_t code[] = {
        encode_instruction(0x2, 0, 0, 0, 0),    // xor r0, r0, r0
        encode_instruction(0x2, 1, 1, 1, 0),    // xor r1, r1, r1
        encode_instruction(0x19, 1, 0, 0, 1),   // addi r1, 1
        encode_instruction(0xf, 1, 0, 0, 4),    // out r1, r0
        encode_instruction(0xf, 0, 0, 0, 0)     // halt
    };
    write_tko_file("test_addi.tko", code, 5);
}

void create_test_add() {
    uint32_t code[] = {
        encode_instruction(0x2, 0, 0, 0, 0),      // xor r0, r0, r0
        encode_instruction(0x19, 0, 0, 0, 752),   // addi r0, 752
        encode_instruction(0x2, 1, 1, 1, 0),      // xor r1, r1, r1
        encode_instruction(0x19, 1, 0, 0, 153),   // addi r1, 153
        encode_instruction(0x18, 5, 0, 1, 0),     // add r5, r0, r1
        encode_instruction(0x2, 6, 6, 6, 0),      // xor r6, r6, r6
        encode_instruction(0x19, 6, 0, 0, 1),     // addi r6, 1
        encode_instruction(0xf, 6, 5, 0, 4),      // out r6, r5
        encode_instruction(0xf, 0, 0, 0, 0)       // halt
    };
    write_tko_file("test_add.tko", code, 9);
}

int main() {
    printf("Generating test .tko files using C assembler encoding...\n");
    
    create_test_halt();
    create_test_addi();
    create_test_add();

    printf("\nDone! Run these tests with:\n");
    printf("  ./sim_debug test_halt.tko\n");
    printf("  ./sim_debug test_addi.tko\n");
    printf("  ./sim_debug test_add.tko\n");

    return 0;
}