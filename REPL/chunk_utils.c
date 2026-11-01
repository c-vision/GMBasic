/* native/repl/chunk_utils.c — PRGINFO / DASM / LOADPRG sul formato chunk A1 (GMBC).
 * Usa gmb_chunk_load (native/gmb_vm_c/gmb_chunk.c) per deserializzare il chunk e
 * mostra header/info (PRGINFO), disassembla gli opcode (DASM) o carica per RUN (LOADPRG).
 */
#include "repl.h"
#include "gmb_chunk.h"
#include "gmb_disasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gmb_chunk_t* load_chunk(const char* file, bool lines, char* err, int errcap)
{
    FILE* f = fopen(file, "rb");
    if (!f) { snprintf(err, errcap, "?file non trovato: %s", file); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 256 * 1024 * 1024) { fclose(f); snprintf(err, errcap, "?chunk di dimensione non valida"); return NULL; }
    uint8_t* data = (uint8_t*)malloc((size_t)n);
    if (!data) { fclose(f); snprintf(err, errcap, "?allocazione"); return NULL; }
    fread(data, 1, (size_t)n, f);
    fclose(f);
    gmb_chunk_t* c = gmb_chunk_load(data, (size_t)n, lines);
    free(data);
    if (!c) snprintf(err, errcap, "?chunk A1 non valido (magic/version)");
    return c;
}

int gmb_prginfo(const char* file)
{
    char err[256];
    gmb_chunk_t* c = load_chunk(file, false, err, sizeof(err));
    if (!c) { fprintf(stderr, "%s\n", err); return 1; }
    printf("File    : %s\n", file);
    printf("Formato : A1/GMBC (magic \"GMBC\", version %d)\n", GMB_CHUNK_VERSION);
    printf("Code    : %u byte, %u istruzioni\n", c->code_len, c->code_len);
    printf("Costanti: %u\n", c->consts_len);
    printf("Data    : %u\n", c->data_len);
    printf("Righe   : %s\n", c->lines ? "presenti" : "non richieste");
    gmb_chunk_free(c);
    return 0;
}

int gmb_dasm(const char* file)
{
    /* Disassemblatore UNICO: native/gmb_bytecode/src/gmb_disasm.c (condiviso col CLI
     * `gmb dump disasm`). Qui non si duplica più la tabella degli opcode. */
    return gmb_disasm_chunk_file(file);
}

int gmb_loadprg(const char* file)
{
    char err[256];
    gmb_chunk_t* c = load_chunk(file, true, err, sizeof(err));
    if (!c) { fprintf(stderr, "%s\n", err); return 1; }
    printf("[prg] caricato: %s (%u istruzioni, %u costanti) — RUN per eseguire (vmc)\n",
        file, c->code_len, c->consts_len);
    gmb_chunk_free(c);
    return 0;
}
