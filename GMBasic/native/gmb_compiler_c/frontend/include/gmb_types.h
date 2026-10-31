/* gmb_types.h — Type Registry + risoluzione tipi/MEBRI del compilatore C (pari al C#
 * `Linker.InferTypes` / `UserDefinedType.Fields`). Registra i tipi dichiarati (CLASS/UDT/TYPE/
 * ENUM) con i loro campi; risolve un nome-tipo e un accesso `Tipo.campo`. Fondamento di §6
 * (il linker C risolve tipi/membri/firme come il C#). COMPILATORE C A SE' STANTE: solo C.
 */
#ifndef GMB_TYPES_H
#define GMB_TYPES_H

#include "gmb_ast.h"

typedef enum {
    GMB_TY_BUILTIN, GMB_TY_UDT, GMB_TY_CLASS, GMB_TY_INTERFACE, GMB_TY_ENUM,
    GMB_TY_ARRAY, GMB_TY_FUNCTION, GMB_TY_UNKNOWN
} gmb_type_kind_t;

typedef struct {
    char name[64];
    char type[64];      /* tipo del campo */
    int  is_static;
} gmb_field_t;

typedef struct {
    char name[64];
    gmb_type_kind_t kind;
    gmb_field_t* fields; int n_fields, cap_fields;
    char base[64];      /* base (INHERITS), "" se nessuna */
    int  is_interface;
} gmb_type_t;

typedef struct {
    gmb_type_t* types; int n_types, cap_types;
} gmb_typereg_t;

void gmb_typereg_init(gmb_typereg_t* r);
void gmb_typereg_free(gmb_typereg_t* r);

/* Popola dai tipi dichiarati nell'AST (CLASS/INTERFACE/UDT/TYPE/ENUM). Ritorna i tipi registrati. */
int  gmb_typereg_build(gmb_typereg_t* r, gmb_stmt_t** stmts, int n);

/* Pubblica. */
const gmb_type_t*  gmb_typereg_find(const gmb_typereg_t* r, const char* name);
const gmb_field_t* gmb_typereg_field(const gmb_typereg_t* r, const char* type_name, const char* field);
int  gmb_type_is_builtin(const char* name);
const char* gmb_type_kind_name(gmb_type_kind_t k);

#endif /* GMB_TYPES_H */
