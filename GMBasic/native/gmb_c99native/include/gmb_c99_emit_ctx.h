/* gmb_c99_emit_ctx.h — DEFINIZIONE UNICA di ctx_t (emitter C99 nativo).
 *
 * Storicamente ctx_t era duplicato in gmb_c99_emit.c e in gmb_c99_emit_internal.h
 * e i due erano DIVERGENTI (bug 'JSON.ToObject(UDT) non popola'): i partial leggevano
 * campi a offset sbagliato. Ora la definizione è UNICA qui; entrambi i file lo includono.
 *
 * PRECONDIZIONE: l'includente deve aver già definito sb_t, var_t, vars_t, fn_info_t
 * (e incluso gmb_ast.h per gmb_expr_t).
 */
#ifndef GMB_C99_EMIT_CTX_H
#define GMB_C99_EMIT_CTX_H

typedef struct {
    sb_t out;
    vars_t globals;
    /* DELEGATE FUNCTION <N>(sig) AS <ret> registrati dal modulo (2026-09-06) */
    char dlg_name[16][40]; int dlg_arity[16]; int dlg_str[16]; int dlg_ret[16]; int n_dlg;
    fn_info_t* fns; int nfn, fn_cap;
    fn_info_t* cur_fn;         /* funzione in emissione (NULL = top level) */
    int indent;
    int uses_eval;             /* programma usa &/EVAL → emetti env host_expr */
    int uses_net;              /* programma usa statement rete → gmb_net_init() in main */
    int uses_gpm;              /* FASE 2.6(b): programma concorrente → main via gmb_gpm_start */
    int uses_mcp;              /* registra tool MCP / serve MCP → emette l'executor generato */
    int uses_plugincall;       /* AI.PLUGIN.CALL → emette il dispatcher verso le procedure del programma */
    int uses_onerror;          /* FASE 2.6(c): ON ERROR/RESUME attivo → guard setjmp sui LET */
    int err_uid, res_uid;      /* contatori jmp_buf handler ON ERROR / guard RESUME NEXT */
    int err_decl_ids[64]; int n_err_decl;   /* ID jmp_buf ON ERROR da dichiarare nel prologo
                                             * della funzione in emissione (err_decl_record/
                                             * err_decl_insert): lo scope deve coprire tutta
                                             * la procedura, non il blocco dell'ON ERROR. */
    int try_depth;             /* FASE 2.6(d): dentro body TRY → ERROR solleva (gmb_raise_error) */
    /* Riga BASIC dello statement in emissione (aggiornata da emit_stmt): serve alle operazioni
     * che portano una riga a runtime (divisione/MOD -> gmb_safe_*), così l'errore riporta la
     * riga della ISTRUZIONE e non quella dell'operatore (che per uno statement spezzato su più
     * righe è diversa) — parità con ERR.LineNumber di IL/PCode/VM-C. */
    int cur_stmt_line;
    char catch_names[16][64]; int n_catch_names;   /* variabili CATCH attive (mapping .Number/...) */
    char catch_cnames[16][80];   /* nomi C paralleli a catch_names (<nome>__try<id>) */
    char future_names[16][64]; int n_futures;   /* target GO FUNCTION INTO → await implicito in lettura */
    char json_chans[16][64]; int n_json_chans;   /* canali aperti con JSON OPEN ... AS #h (nome della variabile handle): INPUT # legge l'intero documento */
    int future_is_str[16];                      /* 2.6(m): future con risultato stringa (await estrae v_string) */
    char mod_labels[16][64]; int mod_label_idx[16]; int n_mod_labels;
    int mod_lines[64]; int n_mod_lines;      /* bas_line delle righe di modulo (ibrido) */
    int nummod_tv[16]; int n_nummod;         /* GOSUB numerici da funzione → routine modulo */
    int uses_gosub;            /* 2.6(s): almeno un GOSUB → stack condiviso a file scope */
    int top_has_gosub;         /* 2.6(s): GOSUB nel top level (RETURN modulo con dispatch) */
    struct { int id; char back[80]; } gosub_sites[32]; int n_gosub_sites;  /* siti della funzione in emissione */
    /* ON KEY <key> GOTO/GOSUB <label>: handler verso un LABEL in-file (il target SUB usa invece
     * gmb_events_onkey_register). Il poll avviene nel SLEEP (parità emitter C#: legge il tasto
     * e fa il GOSUB al label). */
    struct { gmb_expr_t* key; char label[64]; } onkey_labels[32]; int n_onkey_labels;
    /* 2.7: classi dichiarate (struct gmb_obj_<nome>, campi, costruttore) */
    struct cls_info { char name[40]; char base[40]; char type_params[128]; int nfields; int has_ctor;
                      int has_destructor;              /* 2026-09-03: SUB DESTRUCTOR presente → gmb_user_<Cls>_DESTRUCTOR */
                      int is_interface;                     /* 2.7(d)p2b: INTERFACE (no struct/ctor) */
                      int is_udt;                           /* 2.7(d)p2b: TYPE/UDT (assegnamento = copia valore) */
                      int is_packed;                        /* PACKED TYPE: layout = somma dei campi (canone SIZEOF) */
                      char impl[8][40]; int nimpl;          /* 2.7(d)p2b: interfacce IMPLEMENTS */
                      char fname[128][40]; char ftype[128][40];
                      int nstatics; char sname[8][40]; char stype[8][40];
                      gmb_expr_t* sinit[8]; } classes[256]; int n_classes;
    /* label con NOME a livello modulo (es. handler ON ERROR dopo END SUB) → subroutine
     * lfn_<nome> (parity oracolo _moduleNamedLabels: salti cross-funzione da procedure) */
    sb_t go_wraps;             /* FASE 2.6(b): wrapper goroutine GO SUB (parity oracolo C#) */
    /* FASE 2.6(p): HTTP server — wrapper handler route→SUB, helper FETCH, route pending */
    int uses_fetch;
    sb_t http_wraps;
    /* 2026-09-21 (gruppo IO): wrapper callback WATCH path→SUB (firma void(void*)
     * richiesta da gmb_iowatch_add_fn; la SUB utente è gmb_user_<Cb>()) */
    sb_t iow_wraps;
    sb_t pol_cb_wraps;   /* Livello B: shim LLM POLICY CALLBACK (file scope) */
    sb_t msg_wraps;      /* MESSAGE: shim handler ON MESSAGE (file scope) */
    char msg_done[16][64]; int n_msg_done;
    char iow_done[16][64]; int n_iow_done;
    struct { gmb_expr_t* method; gmb_expr_t* path; char wrap[96]; } http_routes[64]; int n_http_routes;
    /* HTTP USE pending: la registrazione deve precedere qualunque HTTP ROUTE (la catena
     * middleware corre prima delle route nel runtime condiviso §0bis). */
    struct { gmb_expr_t* path; char wrap[96]; } http_mws[16]; int n_http_mws;
    int http_srv_started;
    int emitted_ws_cb_args;      /* 2.6(q): typedef gmb_ws_cb_args emessa una volta */
    gmb_stmt_t** top_defers; int n_top_defers, top_defer_cap;   /* DEFER top-level */
    gmb_stmt_t** top_stmts; int n_top_stmts;   /* statement top-level (scope label ON ERROR) */
    int num_targets[64]; int n_num_targets;     /* 2.8: righe numeriche target di GOTO/GOSUB → lbl_N */
    /* 2.8: DATA/READ/RESTORE — tabella dati globale raccolta in ordine di programma */
    struct data_item { int is_str; char* sval; double dval; int bas_line; } *data_items;
    int n_data_items, data_cap;
    struct { char name[64]; int line; } data_labels[64]; int n_data_labels;
    int handles[64]; int n_handles;   /* handle letterali #n usati (OPEN/CLOSE/...) */
    /* LIBRARY "nome": produce una libreria condivisa (.gmblib) — niente main; i PUBLIC
     * FUNCTION/SUB top-level diventano simboli esportati <lib>_<nome>. */
    int lib_mode;
    char lib_name[96];
    /* FASE 2.6(j): remap identificatori per variabili catturate dalle closure:
     * nome BASIC → espressione C (es. "fattore" → "__clo_Moltiplica.fattore"). */
    const char* imap_name[32]; const char* imap_expr[32]; int n_imap;
    /* DBF GET <var>: nomi dei RECORD VAR (vista del record corrente). `<var>.<campo>` legge il
     * campo dell'area corrente via late binding (parità Clipper DBF GET). */
    const char* recvar_name[16]; int n_recvar;
    /* FASE 2.6(k): RETURN/GOTO che attraversano FINALLY (parity oracolo     * __defer_retval/ret_cont): stack dei TRY-with-FINALLY attivi e continuazioni
     * registrate dai siti RETURN/GOTO (eseguite DOPO il body FINALLY). */
    struct { int id; gmb_stmt_t* s; } try_fin_stack[16]; int n_try_fin;
    struct { int try_id; int cont_id; int is_return; char label[64]; } ret_conts[64]; int n_ret_conts;
    int ret_cont_uid;
    /* FASE 2.6(w): AI.BLACKBOARDWAIT/AI.AGENTSWARM async (parity FETCH espressione, FASE 2.6p):
     * programma usa la wait bloccante in un'espressione → wrapper thread+future dedicato per
     * evitare di occupare il worker pthread della coroutine GPM per tutta l'attesa. */
    int uses_bb_wait;
    int uses_agent_swarm;
    int uses_doc;              /* CONVERT/DOCUMENT: emetti #include "gmb_doc.h" */
    int uses_agent_loop;       /* AI.AGENTRUN: emetti #include "gmb_agent_loop.h" */
    int uses_md;               /* CONVERT Markdown AST: emetti #include "gmb_md.h" */
    int uses_bits;             /* BITS: emetti #include "gmb_bits.h" (runtime condiviso) */
    int uses_tray;             /* TRAY: emetti #include "gmb_tray.h" (runtime condiviso) */
    int uses_events;           /* ON KEY: emetti #include "gmb_events.h" (runtime condiviso) */
    void* preds;               /* sb_t*: definizioni code block VALID/WHEN (in coda) */
    int pred_n;
    int force_dyn;             /* emissione forzata come DYNAMIC (code block VALID/WHEN) */
    int uses_tui;              /* TUI widget: emetti #include "gmb_tui_widgets.h" + "gmb_tui_app.h" */
    int uses_ui;               /* grafica/controls/TUI: boot UI necessario (boot condizionale, Eseguibili-C-Minimali) */
    int dbf_clipper;           /* IMPORT DBF (Clipper mode): un identificatore nudo non dichiarato
                                * è un CAMPO dell'area corrente → gmb_db_dbf_field_val_name. */
    /* Mappa nome-variabile → nome-tipo UDT (TYPE ... END TYPE), popolata al DIM (dove
     * cls_find risolve il tipo). Serve ai consumer che risolvono la var in un altro scope
     * (dove vclass può essere vuota per la doppia registrazione): es. JSON.TOJSON(udt). */
    char udt_var_names[64][2][40];
    int n_udt_var_names;
    /* Canone SIZEOF: testo del tipo BASIC dichiarato da ogni DIM (nome → "INTEGER",
     * "STRING * 10", "DOUBLE", ...). L'emitter non conserva il tipo BASIC (ogni numerico è un
     * gmb_double_t), e SIZEOF risponde della dimensione LOGICA del tipo dichiarato. Accessibile
     * SOLO da questo TU (la copia ctx_t degli altri partial è divergente dopo questo punto):
     * gli altri TU usano gmb_var_declared_type. */
    char var_type_text[64][2][32];
    int n_var_type_text;
    /* Alias SQL (`SQL OPEN path ALIAS TB` lega TB → "TB", parità emitter C# EmitSqlAliasData):
     * gli usi successivi di TB come argomento emettono il letterale, non la variabile. */
    char sql_aliases[16][40];
    int n_sql_aliases;
    /* Callback FFI (2026-09-29): pool di trampolini nativi emesso UNA SOLA VOLTA per
     * programma — vedi emit_ffi_callback_pool. */
    int ffi_callback_pool_emitted;
    char error[512];
    /* Canone EXIT/CONTINUE con etichetta 2026-09-30: etichetta del loop in emissione
     * (impostata per adiacenza in emit_tracked, letta all'ingresso dei loop). */
    char cur_loop_label[64];
    /* Nomi dichiarati (DIM/GLOBAL): distingue il contatore FOR con DIM esterna (→ ombra
     * block-scoped, l'esterno riemerge a NEXT) da quello creato dal loop (→ persiste). */
    char dim_names[512][48];
    int n_dim_names;
    /* READ unificato (C): campi @GET pendenti (var + is_str) per il loop inline al READ. */
    char read_vars[64][48]; int read_is_str[64]; int n_read_vars;
    /* JSON.ToObject: nome del TARGET dell'assegnamento in corso (NULL fuori) — serve a
     * js_to_object per scegliere la FORMA del ramo non-UDT: target LISTA -> gmb_val_as_list;
     * target BOXED (gmb_val_t, es. DIM x AS JSON) -> gmb_json_parse. Il parser C appiattisce
     * "LIST OF <T>" al nome dell'elemento, quindi il tipo-argomento non basta a distinguere. */
    const char* cur_assign_target;
    /* SCOPE DINAMICO (DYNAMIC, spec Variables-Scope §2.5): nomi dichiarati (per NOME, non
     * per simbolo) + tipo C inferito (1 = STRING). La lettura/scrittura passa dal runtime
     * gmb_dscope (parità emitter C# C99CodeEmitter.CollectDynamicScope). Popolato da
     * gmb_c99_collect_dynamic() PRIMA della registrazione (così un nome DYNAMIC non diventa
     * una globale implicita). */
    char dyn_names[128][48];
    int dyn_isstr[128];
    int n_dyn_names;
} ctx_t;

#endif /* GMB_C99_EMIT_CTX_H */
