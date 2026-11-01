/* gmb_disasm.c — disassemblatore A1/GMBC (una sola implementazione).
 * Carica il chunk con gmb_chunk_load (native/gmb_vm_c/gmb_chunk.c) e stampa
 * pc, opcode, eventuale argomento immediato e riga sorgente.
 */
#include "gmb_disasm.h"
#include "gmb_chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tabella nomi opcode (indice = codice opcode) */
static const char* const gmb_opcode_names[] = {"OP_PUSH_CONST","OP_PUSH_NIL","OP_PUSH_TRUE","OP_PUSH_FALSE","OP_POP","OP_DUP","OP_ADD","OP_SUBTRACT","OP_MULTIPLY","OP_DIVIDE","OP_INT_DIVIDE","OP_POWER","OP_NEGATE","OP_NOT","OP_MOD","OP_?","OP_?","OP_?","OP_CMP_EQ","OP_CMP_NE","OP_CMP_GT","OP_CMP_GTE","OP_CMP_LT","OP_CMP_LTE","OP_JMP","OP_JMP_IF_FALSE","OP_JMP_IF_TRUE","OP_CALL","OP_CALL_VAR","OP_RETURN","OP_MAKE_LIST","OP_MAKE_DICT","OP_INDEX","OP_INDEX_ASSIGN","OP_STORE_LOCAL","OP_?","OP_LOAD_LOCAL","OP_STORE_GLOBAL","OP_LOAD_GLOBAL","OP_STORE_MEMBER","OP_LOAD_MEMBER","OP_SWAP","OP_DEFINE_FUNC","OP_CLOSE_UPVALUE","OP_MAKE_UPVALUE","OP_GET_UPVALUE","OP_SET_UPVALUE","OP_YIELD","OP_?","OP_?","OP_DUP_2","OP_?","OP_MAKE_UDT","OP_BOX","OP_UNBOX","OP_TYPEOF","OP_DATA","OP_SYSCALL","OP_BUILTIN","OP_TASK","OP_AWAIT","OP_PARK","OP_RESUME","OP_?","OP_?","OP_LINE","OP_LABEL","OP_JUMP_TABLE","OP_JUMP_IF_TRUE_OR_POP","OP_JUMP_IF_FALSE_OR_POP","OP_BEGIN_TRY","OP_END_TRY","OP_CATCH","OP_THROW","OP_RETHROW","OP_MAKE_ITERATOR","OP_ITER_NEXT","OP_ITER_DONE","OP_ITER_RESET","OP_FOREACH_PUSH","OP_FOREACH_POP","OP_BOX_PTR","OP_UNBOX_PTR","OP_REF_LOCAL","OP_REF_GLOBAL","OP_REF_MEMBER","OP_?","OP_?","OP_DEFINE_GENERATOR","OP_RESUME_GENERATOR","OP_FINALIZE","OP_ENTER","OP_EXIT","OP_LOAD_PROPERTY","OP_STORE_PROPERTY","OP_CALL_METHOD","OP_IMPORT_NS","OP_IMPORT_MEMBER","OP_MAKE_VARIANT","OP_CAST","OP_NEW_INSTANCE","OP_?","OP_GET_FIELD","OP_SET_FIELD","OP_STATIC_GET","OP_STATIC_SET","OP_MAKE_CLOSURE","OP_CALL_TAIL","OP_ARRAY_LEN","OP_STRING_ADD","OP_NUM_TO_STR","OP_STR_TO_NUM","OP_MAKE_RANGE","OP_SPREAD","OP_TASK_SPAWN","OP_TASK_JOIN","OP_MAKE_TUPLE","OP_TUPLE_INDEX","OP_DICT_KEYS","OP_DICT_VALUES","OP_DICT_HAS","OP_LIST_APPEND","OP_LIST_POP","OP_MAP","OP_FILTER","OP_REDUCE","OP_MAKE_CHANNEL","OP_CHAN_SEND","OP_CHAN_RECV","OP_SELECT","OP_MAKE_TIMER","OP_TIMER_WAIT","OP_ANNOTATION","OP_DEBUG_BREAK","OP_PROFILE_START","OP_PROFILE_END","OP_AI_CALL","OP_AI_EMBED","OP_ML_TENSOR","OP_ML_CALL","OP_DB_OPEN","OP_DB_QUERY","OP_DB_EXEC","OP_NET_HTTP","OP_NET_WS","OP_CRYPTO","OP_COMPRESS","OP_JSON_PARSE","OP_JSON_STRINGIFY","OP_RUN_MODULE","OP_REQUIRE_MODULE","OP_SET_LOCAL_BY_INDEX","OP_GET_LOCAL_BY_INDEX","OP_CALL_INDIRECT","OP_CALL_VIRTUAL","OP_MAKE_STRUCT","OP_STRUCT_GET","OP_STRUCT_SET","OP_PUSH_ENV","OP_POP_ENV","OP_SCOPE_ENTER","OP_SCOPE_EXIT","OP_LINE_MARKER","OP_GOSUB","OP_GOTO_LABEL","OP_RETURN_FROM_GOSUB","OP_ON_ERROR","OP_RESUME_NEXT","OP_RESUME_LABEL"};

static const char* opcode_name(uint8_t op)
{
    if (op < sizeof(gmb_opcode_names) / sizeof(gmb_opcode_names[0])) return gmb_opcode_names[op];
    return "OP_?";
}

int gmb_disasm_chunk_buffer(const uint8_t* data, size_t len, FILE* out)
{
    if (!data || !len) return 1;
    if (!out) out = stdout;
    gmb_chunk_t* c = gmb_chunk_load(data, len, true);
    if (!c) { fprintf(stderr, "?chunk A1 non valido (magic/version)\n"); return 1; }
    fprintf(out, "; Disassembly (%u istruzioni, %u costanti)\n", c->code_len, c->consts_len);
    for (uint32_t pc = 0; pc < c->code_len; pc++) {
        uint8_t op = c->code[pc];
        uint32_t line = c->lines ? c->lines[pc] : 0;
        char args[64] = "";
        switch (op) {
            case 0:  /* PUSH_CONST */
                if (pc + 1 < c->code_len) { snprintf(args, sizeof(args), "const #%u", c->code[pc + 1]); pc++; }
                break;
            default: {
                uint8_t b1 = pc + 1 < c->code_len ? c->code[pc + 1] : 0;
                if (pc + 1 < c->code_len && b1 < c->consts_len) snprintf(args, sizeof(args), "#%u", b1);
                break;
            }
        }
        fprintf(out, "%4u  %-24s %-16s ; L%u\n", pc, opcode_name(op), args, line);
    }
    gmb_chunk_free(c);
    return 0;
}

int gmb_disasm_chunk_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "?file non trovato: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 256L * 1024 * 1024) { fclose(f); fprintf(stderr, "?chunk di dimensione non valida\n"); return 1; }
    uint8_t* data = (uint8_t*)malloc((size_t)n);
    if (!data) { fclose(f); return 1; }
    if (fread(data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(data); return 1; }
    fclose(f);
    int rc = gmb_disasm_chunk_buffer(data, (size_t)n, stdout);
    free(data);
    return rc;
}
