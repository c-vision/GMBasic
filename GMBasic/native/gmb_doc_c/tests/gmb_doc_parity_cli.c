/* gmb_doc_parity_cli.c — CLI per la parity suite CONVERT (C vs IL).
 * Uso: gmb_doc_parity_cli <path> <fmt>
 * Stampa su stdout l'output convertito (per il confronto normalizzato col backend C#). */
#include "gmb_doc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <path> <fmt>\n", argv[0]); return 2; }
    char* r = gmb_doc_convert(argv[1], argv[2]);
    if (!r) { fprintf(stderr, "gmb_doc_convert NULL\n"); return 1; }
    fputs(r, stdout);
    free(r);
    return 0;
}
