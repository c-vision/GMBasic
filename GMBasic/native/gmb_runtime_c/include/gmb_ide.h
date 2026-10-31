/* ============================================================================
 * gmb_ide.h — Modello IDE (nuovo modello, TUTTO in C). Documento multi-tab +
 * analisi del linguaggio GMBasic: outline/simboli, code completion, go-to-definition,
 * find-references, error list (lint), refactoring (rename), snippet, make
 * (bas -> bytecode .a1 / libreria .a1). Nessuna dipendenza dal C#.
 * ============================================================================ */
#ifndef GMB_IDE_H
#define GMB_IDE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_IDE_MAX_TABS   32
#define GMB_IDE_MAX_LINES  4000
#define GMB_IDE_LINE_CAP   512
#define GMB_IDE_MAX_SYMS   1024
#define GMB_IDE_CHAR_CAP   256

typedef struct gmb_ide_doc {
    char name[128];
    char lines[GMB_IDE_MAX_LINES][GMB_IDE_LINE_CAP];
    int  line_count;
    bool dirty;
} gmb_ide_doc_t;

enum { GMB_IDE_SYM_SUB = 0, GMB_IDE_SYM_FUNCTION, GMB_IDE_SYM_GENERATOR, GMB_IDE_SYM_VAR, GMB_IDE_SYM_CONST, GMB_IDE_SYM_TYPE };
typedef struct gmb_ide_sym {
    char name[GMB_IDE_CHAR_CAP];
    int  kind;      /* GMB_IDE_SYM_* */
    int  line;      /* 0-based */
} gmb_ide_sym_t;

typedef struct gmb_ide {
    gmb_ide_doc_t docs[GMB_IDE_MAX_TABS];
    int  doc_count;
    int  active;
    /* cache di analisi del documento attivo */
    gmb_ide_sym_t syms[GMB_IDE_MAX_SYMS];
    int  sym_count;
    int  analyzed_doc;
    bool analyzed_dirty;
} gmb_ide_t;

/* ---- vita ---- */
void gmb_ide_init(gmb_ide_t* ide);
gmb_ide_t* gmb_ide_static(void);   /* istanza unica condivisa (per il binding C#) */
int  gmb_ide_active(const gmb_ide_t* ide);                       /* indice doc attivo, -1 */
/* Apre (o rimpiazza) un tab; ritorna l'indice. text puo' essere NULL. */
int  gmb_ide_tab_open(gmb_ide_t* ide, const char* name, const char* text);
int  gmb_ide_tab_close(gmb_ide_t* ide, int idx);
int  gmb_ide_tab_activate(gmb_ide_t* ide, int idx);
int  gmb_ide_tab_count(const gmb_ide_t* ide);
const char* gmb_ide_tab_name(const gmb_ide_t* ide, int idx);

/* ---- documento ---- */
void gmb_ide_set_text(gmb_ide_t* ide, int idx, const char* text);   /* split su \n, strip numero BASIC */
int  gmb_ide_doc_text(const gmb_ide_t* ide, int idx, char* out, int cap);   /* join su \n */
int  gmb_ide_doc_lines(const gmb_ide_t* ide, int idx);
const char* gmb_ide_doc_line(const gmb_ide_t* ide, int idx, int line);
void gmb_ide_doc_set_line(gmb_ide_t* ide, int idx, int line, const char* text);
void gmb_ide_doc_insert_line(gmb_ide_t* ide, int idx, int at, const char* text);
void gmb_ide_doc_delete_line(gmb_ide_t* ide, int idx, int at);
int  gmb_ide_doc_find(const gmb_ide_t* ide, int idx, const char* term, int from_line); /* -1 se niente */

/* ---- analisi ---- */
void gmb_ide_analyze(gmb_ide_t* ide);                                  /* aggiorna la cache */
int  gmb_ide_sym_count(gmb_ide_t* ide);
const char* gmb_ide_sym_name(gmb_ide_t* ide, int i);
int  gmb_ide_sym_kind(gmb_ide_t* ide, int i);
int  gmb_ide_sym_line(gmb_ide_t* ide, int i);
/* outline: "line|kind|name;line|kind|name;..." */
int  gmb_ide_outline(gmb_ide_t* ide, char* out, int cap);
/* completion: parole (keyword+snippet+simboli) che iniziano con prefix, "w;w;..." */
int  gmb_ide_complete(gmb_ide_t* ide, const char* prefix, char* out, int cap);
/* go-to-definition: riga (0-based) della definizione del simbolo, -1 se non trovato */
int  gmb_ide_goto_def(gmb_ide_t* ide, const char* name);
/* find-references: righe (0-based) dove compare l'identificatore, "l;l;..." */
int  gmb_ide_find_refs(gmb_ide_t* ide, const char* name, char* out, int cap);
/* lint: errori/warning "line|msg;line|msg;..." */
int  gmb_ide_lint(gmb_ide_t* ide, char* out, int cap);
/* rename: rinomina l'identificatore (solo se non e' keyword); ritorna n. sostituzioni */
int  gmb_ide_rename(gmb_ide_t* ide, const char* from, const char* to);
/* snippet per kind (SUB/FUNCTION/GENERATOR/FOR/IF/WHILE/SELECT/CLASS): testo multi-riga */
int  gmb_ide_snippet(const char* kind, char* out, int cap);

/* ---- make ---- */
/* Compila il documento attivo in bytecode (chunk .a1). Ritorna 0 = ok. */
int  gmb_ide_make_bas(gmb_ide_t* ide, const char* out_path);
/* Crea una libreria (non-main) dal documento attivo. Ritorna 0 = ok. */
int  gmb_ide_make_lib(gmb_ide_t* ide, const char* out_path);

/* keyword list (per completion/lint). */
int  gmb_ide_is_keyword(const char* word);

/* ---- wrapper FFI-friendly (firma banale per `DECLARE ... LIB "gmb_gfx"` da BASIC).
 * Operano sull'istanza statica gmb_ide_static() e su un buffer statico condiviso per i
 * ritorni stringa (valido fino alla chiamata successiva). ---- */
const char* gmb_idef_outline(void);
const char* gmb_idef_complete(const char* prefix);
long long   gmb_idef_gotodef(const char* name);
const char* gmb_idef_refs(const char* name);
const char* gmb_idef_lint(void);
int         gmb_idef_rename(const char* from, const char* to);
const char* gmb_idef_snippet(const char* kind);
int         gmb_idef_tabcount(void);
const char* gmb_idef_tabname(int i);
int         gmb_idef_tabopen(const char* name, const char* text);
int         gmb_idef_tabclose(int idx);
int         gmb_idef_tabactivate(int idx);
void        gmb_idef_settext(const char* text);
const char* gmb_idef_text(int idx);
int         gmb_idef_makebas(const char* path);
int         gmb_idef_makelib(const char* path);

#ifdef __cplusplus
}
#endif
#endif /* GMB_IDE_H */
