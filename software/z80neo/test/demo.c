/*
 * demo.c — z80neo bare-metal C example
 *
 * Built with the in-tree z88dk target:  zcc +z80neo demo.c -o demo -create-app
 * Link/load at 0x0000, console on ports 0x80 (data) / 0x81 (status).  See the
 * repository AGENTS.md ("C programs (zcc +z80neo)") for the full workflow.
 *
 * Output is Intel HEX (demo.ihx); copy it to the SD card root and point
 * Z80NEO.INI BANK_PROG[0] at it.
 */

#include <stdio.h>
#include <string.h>

/* Direct hardware access: the console status port (bit0 = RX ready,
 * bit1 = TX buffer has room).  Requires the Zilog Z80 target. */
__sfr __at 0x81 SERIAL_STATUS;

/* Classic stdio's getchar() already echoes the key via fputc_cons, so this
 * program only prints the key code and never echoes again. */

static unsigned int count_primes(unsigned int limit)
{
    static unsigned char composite[256];
    unsigned int count = 0;

    memset(composite, 0, sizeof(composite));
    for (unsigned int i = 2; i < limit && i < sizeof(composite); i++) {
        if (composite[i])
            continue;
        count++;
        for (unsigned int j = i * 2; j < sizeof(composite); j += i)
            composite[j] = 1;
    }
    return count;
}

int main(void)
{
    unsigned char status = SERIAL_STATUS;

    printf("\n");
    printf("z80neo C demo\n");
    printf("=============\n");
    printf("types: int=%u long=%u ptr=%u\n",
           (unsigned)sizeof(int), (unsigned)sizeof(long),
           (unsigned)sizeof(void *));
    printf("console status 0x81=0x%02X\n", (unsigned)status);
    printf("  tx-rdy=%u rx-rdy=%u\n",
           (unsigned)((status >> 1) & 1), (unsigned)(status & 1));

    printf("primes < 256: %u\n", count_primes(256));

    printf("\nType a key; it is echoed for you.\n");
    printf("Press 'q' (or ESC) to quit.\n\n");

    for (;;) {
        int c = getchar();              /* echoes the key itself */
        if (c == 'q' || c == 'Q' || c == 27)
            break;
        printf("key = 0x%02X\n", (unsigned)c);
    }

    printf("bye\n");
    return 0;
}
