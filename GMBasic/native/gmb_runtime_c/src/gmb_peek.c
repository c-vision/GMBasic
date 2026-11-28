/* gmb_peek.c — PEEK/POKE: memoria virtuale del programma (vedi include/gmb_peek.h).
 *
 * Una sola sede (§0bis): le quattro gambe chiamano queste funzioni, nessun backend tiene una
 * propria memoria. Lo stato è di processo e volatile, come la memoria del riferimento.
 */
#include "gmb_peek.h"

#include <string.h>

static unsigned char g_mem[GMB_PEEK_SIZE];

int gmb_peek_size(void) { return GMB_PEEK_SIZE; }

int gmb_peek(unsigned int addr)
{
    if (addr >= (unsigned int)GMB_PEEK_SIZE) return -1;
    return (int)g_mem[addr];
}

int gmb_poke(unsigned int addr, int value)
{
    if (addr >= (unsigned int)GMB_PEEK_SIZE) return -1;
    g_mem[addr] = (unsigned char)(value & 0xFF);
    return 0;
}

int gmb_peek_fill(int value)
{
    memset(g_mem, (unsigned char)(value & 0xFF), sizeof(g_mem));
    return 0;
}
