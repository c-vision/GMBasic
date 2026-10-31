/* native/gmb_compiler_c/include/gmb_mcp_schema.h
 *
 * InputSchema MCP dai parametri di una PROCEDURA — UNA sola implementazione
 * condivisa dai due codegen del compilatore C (P-Code: gmb_compiler.c; C99:
 * gmb_c99_emit.c). Vietato duplicare la logica nei due emitter (§0bis).
 *
 * Parità con `IlRuntime.BuildInputSchema`/`McpSchemaBuilder` (backend gestiti):
 * {"type":"object","properties":{<p>:{...}},"required":[...]} — "{}" se la
 * procedura non ha parametri, così il registro C (#) e il C# combaciano.
 */
#ifndef GMB_MCP_SCHEMA_H
#define GMB_MCP_SCHEMA_H

#include "gmb_ast.h"

/* Schema dai parametri espliciti (nomi/tipi/flag array). Ritorna heap, MAI NULL. */
char* gmb_mcp_schema_from_params(char** params, char** types,
                                 uint8_t* is_array, int n_params);

/* Dalla dichiarazione di procedura dell'AST (`stmt` di FUNCTION/SUB).
 * `fs` NULL → "{}". */
char* gmb_mcp_input_schema_json(const gmb_stmt_t* fs);

#endif /* GMB_MCP_SCHEMA_H */
