#include <stdio.h>
#include <stdint.h>

// Force the bits into specific slots to avoid any shift logic errors
uint32_t pack(uint32_t op, uint32_t rd, uint32_t rs, uint32_t rt, uint32_t L) {
    uint32_t res = 0;
    res |= (op & 0x1F);           // 5 bits for Op (0-4) - check if your manual says 4 or 5
    res |= (rd & 0x1F) << 5;      // Start Rd at bit 5
    res |= (rs & 0x1F) << 10;     // Start Rs at bit 10
    res |= (rt & 0x1F) << 15;     // Start Rt at bit 15
    res |= (L  & 0x1FFFF) << 20;  // Immediate in the remaining space
    return res;
}

void write_instr(FILE *f, uint32_t instr) {
    fwrite(&instr, 4, 1, f);
}

int main() {
    FILE *f = fopen("test.bin", "wb"); // Make sure this matches the name you load
    if (!f) return 1;

    // 1. Move 0x1008 into R2. 
    // Since R2 is 0, we add 0x1008 to it.
    write_instr(f, pack(0x19, 2, 0, 0, 0x1008)); 

    // 2. CALL R2. 
    // This should now show "jumping to r2 which contains 0x1008"
    write_instr(f, pack(0x0C, 2, 0, 0, 0));

    // 3. The code at 0x1008 (Subroutine)
    // We add a NOP (OR r0, r0, r0) so we can see it hit the subroutine
    write_instr(f, pack(0x01, 0, 0, 0, 0)); 

    // 4. RETURN
    write_instr(f, pack(0x0D, 0, 0, 0, 0));

    // 5. HALT
    write_instr(f, pack(0x0F, 0, 0, 0, 0));

    fclose(f);
    printf("Fixed test.bin generated.\n");
    return 0;
}