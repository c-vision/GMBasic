/* gmb_disasm.h — disassemblatore del bytecode A1/GMBC (UNICA implementazione).
 *
 * Vive nel modulo bytecode perché serve sia al compilatore C (`gmb dump disasm`) sia
 * alla REPL: prima la tabella opcode era duplicata dentro native/repl/chunk_utils.c.
 */
#ifndef GMB_DISASM_H
#define GMB_DISASM_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/* Disassembla il chunk contenuto nel file; stampa su `out` (NULL = stdout). 0 = ok. */
int gmb_disasm_chunk_file(const char* path);
/* Disassembla un buffer A1/GMBC in memoria. 0 = ok. */
int gmb_disasm_chunk_buffer(const uint8_t* data, size_t len, FILE* out);

#endif
