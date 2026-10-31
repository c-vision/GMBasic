/* gmb_rag_selfcheck.c — parity test per gmb_rag.c (RAG C paritario col C# AiVectorRuntime:
 * schema SqlVectorSchema, chunking TextChunker, hybrid FTS5+RRF, filtri, context grounded).
 * Eseguito: cmake --build && ./gmb_rag_selfcheck
 */
#include "gmb_runtime.h"
#include <stdio.h>

int main(void)
{
    int rc = gmb_rag_selfcheck();
    if (rc == 0) { printf("gmb_rag_selfcheck: ALL PASS\n"); }
    else { printf("gmb_rag_selfcheck: FAIL (rc=%d)\n", rc); return 1; }
    /* L7 Training: anche le metriche BLEU/ROUGE (stesso eseguibile, niente target CMake nuovi). */
    rc = gmb_rageval_selfcheck();
    if (rc == 0) { printf("gmb_rageval_selfcheck: ALL PASS\n"); return 0; }
    printf("gmb_rageval_selfcheck: FAIL (rc=%d)\n", rc);
    return 1;
}
