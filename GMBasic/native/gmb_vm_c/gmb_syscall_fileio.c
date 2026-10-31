/* gmb_syscall_fileio.c — handler SysCall File I/O (range 15-30) estratti dallo
 * switch in gmb_vm_exec_syscall (task 6.5.2). Ogni case → gmb_fo_<code>().
 * La tabella gmb_fo_impl mappa code → handler; dispatcher per lookup. */
#include "gmb_vm_core.h"
#include "gmb_strbuf.h"
#include "gmb_db.h"   /* GET_FILE (21): ISAM GET → oggetto-record (gmb_db_isam_*) */
#include <unistd.h>
#include <sys/stat.h>

/* MOVE src TO dst (SysCall 29): sposta file (parity File.Move C#) via runtime
 * condiviso gmb_file_move (rename + fallback copia+cancella). Prima: stub che
 * scartava gli argomenti (MiniTest-Oracolo, gruppo IO). */
static int gmb_fo_29(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    {
        gmb_stack_entry_t de = gmb_task_pop(task);
        gmb_stack_entry_t e = gmb_task_pop(task);
        const char* srcp = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
        const char* dstp = (de.type == 2 && de.ref && de.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)de.ref)->data : "";
        if (srcp[0] && dstp[0])
        {
            gmb_string_t a = { (char*)srcp, strlen(srcp), 1 }, b = { (char*)dstp, strlen(dstp), 1 };
            gmb_file_move(a, b);
        }
        if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
        if (de.type == 2 && de.ref) gmb_obj_release(de.ref);
        return 0;
    }
}

/* XCOPY src TO dst [, rec [, ovr]] (SysCall 30): copia directory via runtime
 * condiviso gmb_fs_copy_directory. Layout stack del compilatore C# (push
 * src, dst, rec, ovr; NIL se flag assenti → 0). Prima: stub (stesso handler di 29). */
static int gmb_fo_30(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    {
        gmb_stack_entry_t oe = gmb_task_pop(task);
        gmb_stack_entry_t re = gmb_task_pop(task);
        gmb_stack_entry_t de = gmb_task_pop(task);
        gmb_stack_entry_t e = gmb_task_pop(task);
        const char* srcp = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
        const char* dstp = (de.type == 2 && de.ref && de.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)de.ref)->data : "";
        int rec = (re.type == 1 && re.num != 0) ? 1 : 0;
        int ovr = (oe.type == 1 && oe.num != 0) ? 1 : 0;
        if (srcp[0] && dstp[0])
        {
            gmb_string_t a = { (char*)srcp, strlen(srcp), 1 }, b = { (char*)dstp, strlen(dstp), 1 };
            gmb_fs_copy_directory(a, b, rec, ovr);
        }
        if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
        if (de.type == 2 && de.ref) gmb_obj_release(de.ref);
        if (re.type == 2 && re.ref) gmb_obj_release(re.ref);
        if (oe.type == 2 && oe.ref) gmb_obj_release(oe.ref);
        return 0;
    }
}

static int gmb_fo_15(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int n;
        {
            double n = gmb_task_pop_num(task);
            int idx = (int)n;
            if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
            {
                gmb_file_close(g_vm_file_handles[idx]);
                g_vm_file_handles[idx] = 0;
                g_vm_file_content_types[idx] = 0;
            }
            return 0;
        }
}

static int gmb_fo_16(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int i;
int n;
        {
            gmb_stack_entry_t pv[16];
            for (int i = 0; i < argc && i < 16 && task->sp > 0; i++)
                pv[i] = gmb_task_pop(task);
            /* push order: FileNumber PRIMA, poi gli argomenti → l'ultimo pop è FileNumber */
            double n = argc >= 1 ? (pv[argc - 1].type == 1 ? pv[argc - 1].num : 0.0) : 0.0;
            int idx = (int)n;
            if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
            {
                gmb_strbuf_t b = gmb_strbuf_new();
                /* pv[0] = ULTIMO argomento (i pop sono in ordine inverso): si scrive in
                 * ordine SORGENTE da pv[argc-2] a pv[0]. Prima l'ordine era invertito
                 * ("Mario101" invece di "101Mario") e WRITE# non metteva né virgole né
                 * virgolette → il file non era rileggibile da INPUT # (divergenza VM/C). */
                int is_write = (code == 19);
                for (int i = argc - 2; i >= 0; i--)
                {
                    e = pv[i];
                    if (is_write && i != argc - 2) gmb_strbuf_append(&b, ",", 1);
                    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING)
                    {
                        gmb_obj_string_t* os = (gmb_obj_string_t*)e.ref;
                        if (is_write)
                        {
                            gmb_string_t q = gmb_csv_quote((gmb_string_t){ os->data, os->length, 0 });
                            if (q.data) { gmb_strbuf_append(&b, q.data, strlen(q.data)); free(q.data); }
                        }
                        else gmb_strbuf_append(&b, os->data, os->length);
                    }
                    else if (e.type == 1)
                    {
                        if (is_write)
                        {
                            gmb_string_t q = gmb_csv_double(e.num);
                            if (q.data) { gmb_strbuf_append(&b, q.data, strlen(q.data)); free(q.data); }
                        }
                        else
                        {
                            char num[64]; int nl = snprintf(num, sizeof(num), "%g", e.num);
                            gmb_strbuf_append(&b, num, (size_t)nl);
                        }
                    }
                    else if (e.ref && (e.ref->kind == GMB_OBJ_DICT || e.ref->kind == GMB_OBJ_LIST))
                    {
                        /* Handle JSON (content type 1): serializza l'oggetto/lista come JSON
                         * (parità PCode PRINT_FILE con FileContentTypes == 1). Prima l'argomento
                         * oggetto veniva SALTATO → il file conteneva solo la newline e
                         * JSON.LOAD dava `{}`. */
                        if (idx >= 0 && idx < 64 && g_vm_file_content_types[idx] == 1)
                        {
                            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)e.ref;
                            gmb_string_t js = (e.ref->kind == GMB_OBJ_DICT)
                                ? gmb_dict_to_json(sv->dict)
                                : gmb_val_to_json(gmb_make_val_obj(gmb_obj_new_typed(sv->list, "gmb_list")));
                            if (js.data) gmb_strbuf_append(&b, js.data, strlen(js.data));
                        }
                    }
                }
                gmb_string_t s = gmb_strbuf_to_string(&b);
                gmb_file_write_string(g_vm_file_handles[idx], s);
                if (is_write || code == 16) gmb_file_write_newline(g_vm_file_handles[idx]);
                if (s.data) free(s.data);
                gmb_strbuf_free(&b);
            }
            for (int i = 0; i < argc && i < 16; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
            return 0;
        }
}

/* Descrittore RANDOM "nome:tok;nome:tok" (vedi TypeLayout.TryRandomRecordDescriptor) → nomi + token
 * concatenati ("ls20ct"). Ritorna il numero di campi. */
static int vm_rec_parse(const char* desc, char names[][64], char* tokens, size_t cap)
{
    int nf = 0;
    size_t tl = 0;
    const char* p = desc ? desc : "";
    tokens[0] = 0;
    while (*p && nf < 64)
    {
        const char* semi = strchr(p, ';');
        size_t plen = semi ? (size_t)(semi - p) : strlen(p);
        const char* colon = NULL;
        for (size_t i = plen; i > 0; i--) if (p[i - 1] == ':') { colon = p + i - 1; break; }
        if (colon && colon > p)
        {
            size_t nl = (size_t)(colon - p);
            if (nl > 63) nl = 63;
            memcpy(names[nf], p, nl); names[nf][nl] = 0;
            size_t kl = plen - (size_t)(colon + 1 - p);
            if (tl + kl + 1 < cap) { memcpy(tokens + tl, colon + 1, kl); tl += kl; tokens[tl] = 0; }
            nf++;
        }
        p += plen;
        if (*p == ';') p++;
    }
    return nf;
}

/* PUT_FILE (SysCall 21) — PUT #n[,rec], source: scrittura RAW su file, col numero di
 * record IGNORATO sul file classico e byte[]/stringa scritti come byte grezzi
 * (parità handler PUT_FILE di PCodeProcessor.Syscalls.cs). ASSENTE dalla tabella
 * gmb_fo_impl: il PUT # cadeva senza handler → NO-OP silenzioso, HttpMultiPart su
 * VM/C produceva un file da 0 byte (invisibile a parity4, che confronta solo STDOUT). */
static int gmb_fo_21(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    gmb_stack_entry_t pv[8];
    int npop = 0;
    for (int i = 0; i < argc && i < 8 && task->sp > 0; i++, npop++)
        pv[i] = gmb_task_pop(task);
    /* push order (VisitPutFileStatement): FileNumber, Record(o NIL), Source [, descrittore UDT] →
     * senza descrittore pv[0]=Source ... pv[npop-1]=FileNumber; con descrittore (argc==4)
     * pv[0]=descrittore, pv[1]=Source (dict UDT), pv[2]=Record, pv[3]=FileNumber. */
    int has_desc = (argc >= 4 && npop >= 4);
    if (npop >= 2) {
        int idx = (pv[npop - 1].type == 1) ? (int)pv[npop - 1].num : -1;
        if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0) {
            gmb_stack_entry_t src = has_desc ? pv[1] : pv[0];
            if (has_desc && pv[0].type == 2 && pv[0].ref && pv[0].ref->kind == GMB_OBJ_STRING
                && ((gmb_obj_string_t*)pv[0].ref)->length > 0 && ((gmb_obj_string_t*)pv[0].ref)->data[0] == '@'
                && src.type == 2 && src.ref && src.ref->kind == GMB_OBJ_LIST) {
                /* PUT #n, pos, <array di BYTE> (descriptor "@y"): un byte grezzo per elemento. */
                gmb_obj_shared_val_t* sl = (gmb_obj_shared_val_t*)src.ref;
                int cnt = (int)sl->list->count;
                gmb_double_t* tmp = (gmb_double_t*)malloc(sizeof(gmb_double_t) * (size_t)(cnt ? cnt : 1));
                if (tmp) {
                    for (int i = 0; i < cnt; i++) tmp[i] = gmb_val_to_double(sl->list->items[i]);
                    gmb_file_put_bytes_d(g_vm_file_handles[idx], (pv[2].type == 1) ? (gmb_int_t)pv[2].num : 0, tmp, cnt);
                    free(tmp);
                }
                for (int i = 0; i < npop; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
                return 0;
            }
            if (has_desc && gmb_file_is_random(g_vm_file_handles[idx])
                && src.type == 2 && src.ref && src.ref->kind == GMB_OBJ_DICT) {
                /* UDT su file RANDOM: record a layout canonico (gmb_file_put_record, gmb_file.c). */
                char names[64][64], tokens[512], desc[2048];
                vm_entry_str(pv[0], desc, (int)sizeof desc);
                int nf = vm_rec_parse(desc, names, tokens, sizeof tokens);
                gmb_val_t vals[64];
                gmb_obj_shared_val_t* sd = (gmb_obj_shared_val_t*)src.ref;
                for (int i = 0; i < nf; i++) vals[i] = gmb_dict_get(*sd->dict, names[i]);
                gmb_int_t rec = (pv[2].type == 1) ? (gmb_int_t)pv[2].num : 0;
                gmb_int_t rc = gmb_file_put_record(g_vm_file_handles[idx], rec, tokens, vals, nf);
                for (int i = 0; i < npop; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
                if (rc == -2) gmb_raise_basic_error(task, 59, "Bad record length");   /* catturabile (TRY/ON ERROR) */
                return 0;
            }
            if (src.type == 1) {

                char num[64]; int nl = snprintf(num, sizeof(num), "%g", src.num);
                gmb_file_write_bytes(g_vm_file_handles[idx], num, (size_t)nl);
            } else if (src.ref && src.ref->kind == GMB_OBJ_STRING) {
                gmb_obj_string_t* os = (gmb_obj_string_t*)src.ref;
                if (os->data && os->length > 0)
                    gmb_file_write_bytes(g_vm_file_handles[idx], os->data, (size_t)os->length);
            } else if (src.ref && (src.ref->kind == GMB_OBJ_DICT || src.ref->kind == GMB_OBJ_LIST)) {
                /* oggetto → testo (parità PCode `Write(source.ToString())` nel PUT_FILE). */
                gmb_obj_string_t* js = gmb_vm_json_stringify(src.ref);
                if (js && js->data) {
                    gmb_file_write_bytes(g_vm_file_handles[idx], js->data, (size_t)js->length);
                    gmb_obj_release((gmb_obj_t*)js);
                }
            }
        }
    }
    for (int i = 0; i < npop; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
    return 0;
}

/* INPUT_FILE (SysCall 17) e LINE_INPUT_FILE (18) — parità PCodeVM.
 * Il compiler emette: push FileNumber, OP_SYSCALL INPUT_FILE n, poi AssignVariable
 * per ogni target. Il syscall DEVE pushare il valore letto (una riga di testo, o il
 * dict JSON se l'handle è stato aperto con content-type JSON), e le AssignVariable
 * successive lo assegnano. Prima era un placeholder che leggeva la riga ma non
 * assegnava mai → INPUT #ch, cfg non popolava la variabile (JSON OPEN ... INPUT). */
static int gmb_fo_17(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    /* Stack (compilatore): fileNumber, poi N hint (1=numerico, 0=stringa). */
    int targets = argc > 0 ? argc - 1 : 0;
    int hints[32];
    for (int i = 0; i < targets && i < 32; i++) hints[i] = 0;
    for (int i = targets - 1; i >= 0; i--)
    {
        gmb_stack_entry_t e = task->sp > 0 ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        if (i < 32) hints[i] = (e.type == 1) ? (int)e.num : 0;
        if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
    }
    double fileNumD = gmb_task_pop_num(task);
    int idx = (int)fileNumD;
    if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
    {
        int ct = g_vm_file_content_types[idx];
        if (ct == 1)
        {
            /* JSON: leggi tutto il file e parsalo in un dict (un valore per target). */
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_string_t line = { 0 };
                gmb_strbuf_t sb = gmb_strbuf_new();
                while (gmb_file_read_line(g_vm_file_handles[idx], &line) == 0 && line.data)
                {
                    gmb_strbuf_append(&sb, line.data, line.length);
                    gmb_strbuf_append(&sb, "\n", 1);
                    free(line.data);
                    line.data = NULL;
                }
                free(line.data);
                if (sb.length > 0)
                    gmb_json_parse_to_dict(sb.data, out->dict);
                gmb_strbuf_free(&sb);
                for (int i = 0; i < targets; i++)
                    push_ref_new(task, (gmb_obj_t*)out, GMB_BT_OBJECT);
            }
            else for (int i = 0; i < targets; i++) push_num_entry(task, 0.0, GMB_BT_NUMBER);
        }
        else
        {
            /* INPUT # canonico: UNA riga → campi CSV (quote-aware, TRUE/FALSE), campo
             * i-esimo convertito per tipo (parità IL/PCode/C99*). */
            gmb_int_t nf = 0;
            gmb_string_t* fields = gmb_file_read_csv_line(g_vm_file_handles[idx], &nf);
            for (int i = 0; i < targets; i++)
            {
                if (i < 32 && hints[i] == 1)
                    push_num_entry(task, fields && i < (int)nf ? gmb_string_to_double(fields[i]) : 0.0, GMB_BT_NUMBER);
                else
                {
                    const char* fd = (fields && i < (int)nf && fields[i].data) ? fields[i].data : "";
                    gmb_obj_string_t* so = gmb_str_cstr(fd);
                    push_ref_new(task, (gmb_obj_t*)so, GMB_BT_STRING);
                }
            }
            if (fields)
            {
                for (int i = 0; i < (int)nf; i++) free(fields[i].data);
                free(fields);
            }
        }
    }
    else for (int i = 0; i < targets; i++)
    {
        if (i < 32 && hints[i] == 1) push_num_entry(task, 0.0, GMB_BT_NUMBER);
        else { gmb_obj_string_t* so = gmb_str_cstr(""); push_ref_new(task, (gmb_obj_t*)so, GMB_BT_STRING); }
    }
    return 0;
}

/* LINE_INPUT_FILE (SysCall 18): legge una RIGA INTERA (nessuno split CSV) e la pusha.
 * Prima era registrato su gmb_fo_17 (INPUT_FILE, con hint/CSV): `LINE INPUT #F, s` non
 * consumava l'hint e lasciava valori spurii → s = 1/garbage su VM/C. */
static int gmb_fo_18(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    double fileNumD = gmb_task_pop_num(task);
    int idx = (int)fileNumD;
    if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
    {
        gmb_string_t line = { 0 };
        if (gmb_file_read_line(g_vm_file_handles[idx], &line) == 0 && line.data)
        {
            gmb_obj_string_t* so = gmb_str_cstr(line.data);
            free(line.data);
            push_ref_new(task, (gmb_obj_t*)so, GMB_BT_STRING);
        }
        else push_num_entry(task, 0.0, GMB_BT_NUMBER);
    }
    else push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_fo_23(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
        {
            e = gmb_task_pop(task);
            const char* p = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            if (p[0]) remove(p);
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            return 0;
        }
}

static int gmb_fo_24(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
gmb_stack_entry_t a;
        {
            gmb_stack_entry_t ne = gmb_task_pop(task);
            e = gmb_task_pop(task);
            const char* oldp = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            const char* newp = (ne.type == 2 && ne.ref && ne.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)ne.ref)->data : "";
            if (oldp[0] && newp[0])
            {
                gmb_string_t a = { (char*)oldp, strlen(oldp), 1 }, b = { (char*)newp, strlen(newp), 1 };
                gmb_file_rename(a, b);
            }
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            if (ne.type == 2 && ne.ref) gmb_obj_release(ne.ref);
            return 0;
        }
}

static int gmb_fo_25(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
        {
            e = gmb_task_pop(task);
            const char* p = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            if (p[0]) { gmb_string_t s = { (char*)p, strlen(p), 1 }; gmb_file_mkdir(s); }
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            return 0;
        }
}

static int gmb_fo_26(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int recursive;
        {
            /* Il compilatore pusha SEMPRE 2 argomenti: (path, flag). Pop LIFO: prima il flag. */
            e = gmb_task_pop(task);
            recursive = (e.type == 1 && e.num != 0) ? 1 : 0;
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            e = gmb_task_pop(task);
            const char* p = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            if (p[0]) { gmb_string_t s = { (char*)p, strlen(p), 1 };
                if (recursive) gmb_file_rmdir_recursive(s); else gmb_file_rmdir(s); }
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            return 0;
        }
}

static int gmb_fo_27(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
        {
            e = gmb_task_pop(task);
            const char* p = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            if (p[0]) chdir(p);
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            return 0;
        }
}

static int gmb_fo_28(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
        {
            gmb_stack_entry_t de = gmb_task_pop(task);
            e = gmb_task_pop(task);
            const char* srcp = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)e.ref)->data : "";
            const char* dstp = (de.type == 2 && de.ref && de.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)de.ref)->data : "";
            if (srcp[0] && dstp[0])
            {
                FILE* fin = fopen(srcp, "rb");
                if (fin)
                {
                    FILE* fout = fopen(dstp, "wb");
                    if (fout)
                    {
                        char buf[8192]; size_t r;
                        while ((r = fread(buf, 1, sizeof(buf), fin)) > 0) fwrite(buf, 1, r, fout);
                        fclose(fout);
                    }
                    fclose(fin);
                }
            }
            if (e.type == 2 && e.ref) gmb_obj_release(e.ref);
            if (de.type == 2 && de.ref) gmb_obj_release(de.ref);
            return 0;
        }
}


typedef struct { int code; int (*handler)(gmb_vm_t*, gmb_task_t*, int, int); } gmb_fo_impl_t;
/* SEEK n, pos — COMANDO legacy, posizione 1-BASED (`SEEK 1, 1` = inizio). Il C# compila
 * FileNumber e POI position (Compiler.Files.cs: VisitSeekStatement), quindi la position si
 * poppa per prima e il numero di file per ultimo. Il numero indicizza g_vm_file_handles[]
 * (la tabella dei file legacy, come gmb_fo_15/16), NON un handle di FOpen.
 * Prima il codice 22 non era in gmb_fo_impl: il comando veniva IGNORATO in silenzio
 * (§ MiniTests/io/io_misc.bas: dopo `SEEK 1, 3` IL/PCode/C99 leggevano 2, la VM/C 0). */
static int gmb_fo_22(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    double pos = gmb_task_pop_num(task);
    double fileNum = gmb_task_pop_num(task);
    int idx = (int)fileNum;
    if (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
        gmb_file_seek(g_vm_file_handles[idx], (long)(pos > 1 ? pos - 1 : 0), 0);
    return 0;
}

/* GET_FILE (SysCall 21): `ISAM GET <area>, <udt>`.
 * Il chunk emette (disassemblaggio del probe minimo):
 *     OP_PUSH_CONST 'E' · OP_PUSH_NIL · OP_SYSCALL GET_FILE (2 args) · OP_STORE_LOCAL
 * quindi la syscall deve **pushare l'oggetto-record** che l'OP_STORE_LOCAL salva nella variabile
 * UDT. Prima il codice 21 NON era nella tabella e la variabile restava vuota (bug
 * `Bugs/(open) - VMC-ISAM-GET-Non-Popola-UDT-e-Syscall-40-Area.md`: sample RDD_Buffer_Demo
 * stampava `Originale:  $` invece di `Originale: Mario Rossi $ 45000`).
 * La forma dell'oggetto è quella che il PCode costruisce in `PCodeProcessor.Syscalls.cs` (GET_FILE):
 * un dict con i valori per NOME più `__class__.FieldNames` = mappa offset→nome, che è ciò che
 * `OP_LOAD_MEMBER` usa (`vm_field_name_from_offset`). Gli offset sono i **SymbolId** dei campi,
 * raccolti dall'handler `ISAM USE` in `vm->vm_isam_tmp_sids[]`. */
static int gmb_fo_20(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)code;
    if (argc >= 4)
    {
        /* GET #n, rec, <UDT>: push FileNumber, Record(o NIL), descrittore, istanza UDT corrente →
         * pop: pv[0]=istanza, pv[1]=descrittore, pv[2]=Record, pv[3]=FileNumber. Il record viene
         * decodificato (gmb_file_get_record) e scritto NELL'ISTANZA per nome campo; l'istanza è
         * rimessa sullo stack per la AssignVariable successiva. */
        gmb_stack_entry_t pv[4];
        for (int i = 0; i < 4; i++) pv[i] = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        int idx = (pv[3].type == 1) ? (int)pv[3].num : -1;
        int h = (idx >= 0 && idx < 64) ? (int)g_vm_file_handles[idx] : 0;
        if (h > 0 && pv[1].type == 2 && pv[1].ref && pv[1].ref->kind == GMB_OBJ_STRING
            && ((gmb_obj_string_t*)pv[1].ref)->length > 0 && ((gmb_obj_string_t*)pv[1].ref)->data[0] == '@'
            && pv[0].type == 2 && pv[0].ref && pv[0].ref->kind == GMB_OBJ_LIST)
        {
            /* GET #n, pos, <array di BYTE> (descriptor "@y"): legge count byte (EOF → 0) in place. */
            gmb_obj_shared_val_t* dl = (gmb_obj_shared_val_t*)pv[0].ref;
            int cnt = (int)dl->list->count;
            gmb_double_t* tmp = (gmb_double_t*)calloc((size_t)(cnt ? cnt : 1), sizeof(gmb_double_t));
            if (tmp) {
                gmb_file_get_bytes_d(h, (pv[2].type == 1) ? (gmb_int_t)pv[2].num : 0, tmp, cnt);
                for (int i = 0; i < cnt; i++) dl->list->items[i] = gmb_make_val_double(tmp[i]);
                free(tmp);
            }
            push_ref_entry(task, pv[0].ref, GMB_BT_OBJECT);
            for (int i = 0; i < 4; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
            return 0;
        }
        if (h > 0 && gmb_file_is_random(h))
        {
            char names[64][64], tokens[512], desc[2048];
            vm_entry_str(pv[1], desc, (int)sizeof desc);
            int nf = vm_rec_parse(desc, names, tokens, sizeof tokens);
            gmb_val_t vals[64];
            gmb_int_t rec = (pv[2].type == 1) ? (gmb_int_t)pv[2].num : 0;
            gmb_int_t rc = gmb_file_get_record(h, rec, tokens, vals, nf);
            if (rc == -2)
            {
                /* la AssignVariable successiva (RESUME NEXT) deve trovare un valore: rimette l'istanza invariata */
                if (pv[0].type == 2 && pv[0].ref) push_ref_entry(task, pv[0].ref, GMB_BT_OBJECT); else push_nil(task);
                gmb_raise_basic_error(task, 59, "Bad record length");
            }
            else
            {
                gmb_obj_shared_val_t* inst = NULL;
                int fresh = 0;
                if (pv[0].type == 2 && pv[0].ref && pv[0].ref->kind == GMB_OBJ_DICT) inst = (gmb_obj_shared_val_t*)pv[0].ref;
                else { inst = gmb_shared_dict_new(); fresh = 1; }
                const char* tp = tokens;
                for (int i = 0; i < nf && inst; i++)
                {
                    /* STRING * N: valore riempito a N con spazi (semantica del campo a lunghezza fissa). */
                    if (*tp == 's')
                    {
                        int n = 0; tp++;
                        while (*tp >= '0' && *tp <= '9') n = n * 10 + (*tp++ - '0');
                        gmb_string_t sv = gmb_val_to_string(vals[i]);
                        char* pad = (char*)malloc((size_t)n + 1);
                        if (pad)
                        {
                            size_t cp = sv.data ? sv.length : 0;
                            if (cp > (size_t)n) cp = (size_t)n;
                            if (cp) memcpy(pad, sv.data, cp);
                            for (size_t k = cp; k < (size_t)n; k++) pad[k] = ' ';
                            pad[n] = 0;
                            gmb_dict_set(inst->dict, names[i], gmb_make_val_string_counted_n(pad, (size_t)n));
                            free(pad);
                        }
                        gmb_string_free(&sv);
                    }
                    else
                    {
                        tp += (*tp == 'c' || *tp == 'd' || *tp == 'l' || *tp == 'f' || *tp == 't' || *tp == 'y' || *tp == 'Y' || *tp == 'i' || *tp == 'I') ? 1 : 0;
                        gmb_dict_set(inst->dict, names[i], gmb_make_val_double(gmb_val_to_double(vals[i])));
                    }
                }
                if (inst)
                {
                    if (fresh) push_ref_new(task, (gmb_obj_t*)inst, GMB_BT_OBJECT);
                    else push_ref_entry(task, (gmb_obj_t*)inst, GMB_BT_OBJECT);
                }
                else push_nil(task);
            }
        }
        else
        {
            if (pv[0].type == 2 && pv[0].ref) push_ref_entry(task, pv[0].ref, GMB_BT_OBJECT); else push_nil(task);
            gmb_raise_basic_error(task, 5, "GET: destinazione UDT supportata solo su file RANDOM");
        }
        for (int i = 0; i < 4; i++) if (pv[i].type == 2 && pv[i].ref) gmb_obj_release(pv[i].ref);
        return 0;
    }
    gmb_stack_entry_t recNum = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    gmb_stack_entry_t area = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    if (recNum.ref) gmb_obj_release(recNum.ref);

    {
        /* FIX (bug VM/C "ISAM GET multi-area dà vuoto"): risolvi l'AREA dall'argomento, non
         * solo la corrente. L'argomento `area` e' l'alias (stringa) o -1 (area corrente).
         * Prima usava SEMPRE gmb_db_current_isam(): con area esplicita != corrente leggeva
         * l'area sbagliata → record vuoto → EOF mai vero → loop infinito (DbIsamFullTests). */
        int64_t h = 0;
        if (area.type == 2 && area.ref) {
            char alb[128]; vm_entry_str(area, alb, (int)sizeof alb);
            int j;
            for (j = 0; j < vm->vm_isam_alias_count; j++)
                if (strcasecmp(vm->vm_isam_alias[j], alb) == 0) { h = vm->vm_isam_alias_handle[j]; break; }
            if (h <= 0) h = gmb_db_isam_select(alb);
        }
        if (h <= 0) h = gmb_db_current_isam();
        if (h > 0)
        {
            /* I SymbolId dei campi vano presi PER-HANDLE (vm_isam_sids[slot]): vm_isam_tmp_sids
             * contiene solo quelli dell'ULTIMO ISAM USE (l'ultima area aperta). Con più aree
             * aperte (Clienti poi Ordini) un GET su Clienti costruiva FieldNames con i SymbolId
             * di Ordini → OP_LOAD_MEMBER su `clienteRec.ID/Nome` non risolveva mai → record vuoto
             * (bug VMC-ISAM-GET-Non-Popola-UDT). */
            int sid_slot = vm_isam_sid_slot(vm, h);
            char names[1024] = "";
            char val[512];
            gmb_obj_shared_val_t* inst = gmb_shared_dict_new();
            gmb_obj_shared_val_t* cls = gmb_shared_dict_new();
            gmb_obj_shared_val_t* fnames = gmb_shared_dict_new();
            if (inst && cls && fnames &&
                gmb_db_isam_fields(h, names, (int)sizeof(names)) == GMB_DB_OK)
            {
                const char* p = names;
                int i = 0;
                while (*p && i < 64)
                {
                    char fn[64]; int n = 0;
                    while (*p && *p != ',' && n < 63) fn[n++] = *p++;
                    fn[n] = 0;
                    if (*p == ',') p++;
                    if (!fn[0]) break;
                    val[0] = 0;
                    gmb_db_isam_read(h, i, val, (int)sizeof(val));
                    /* Bug (open) VMC-ISAM-GET-UDT-DOUBLE-Senza-Padding: il campo arrivava
                     * SEMPRE come stringa, anche per schema numerico (N/F/I/B → "LongType"),
                     * perché gmb_db_isam_read restituisce testo per costruzione — a valle,
                     * PRINT su un campo DOUBLE non applicava il padding Clipper (che scatta
                     * solo per un vero NUMBER, non per una stringa che contiene cifre).
                     * Stesso identico criterio già usato per i campi DBF
                     * (vm_expr_dbf_record_get, gmb_vm_core.c: `t == 'N' || t == 'F'`). */
                    {
                        int ft = gmb_db_isam_field_type(h, i);
                        /* CURRENCY ('Y') come NUMERO (non stringa) → padding Clipper come l'IL.
                         * BIT ('L'): il valore letto è "-1"/"0" (non '1'/'T') → l'ex test
                         * val[0]=='1' lo classificava SEMPRE False (Attivo sempre False). */
                        if (ft == 'N' || ft == 'Y') gmb_dict_set(inst->dict, fn, gmb_make_val_double(strtod(val, NULL)));
                        else if (ft == 'L') gmb_dict_set(inst->dict, fn, gmb_make_val_bool(strtod(val, NULL) != 0.0 || val[0] == 'T' || val[0] == 't'));
                        else gmb_dict_set(inst->dict, fn, gmb_make_val_string_counted(val));
                    }
                    if (sid_slot >= 0 && i < vm->vm_isam_sid_count[sid_slot])
                    {
                        char key[24];
                        snprintf(key, sizeof(key), "%d", vm->vm_isam_sids[sid_slot][i]);
                        gmb_dict_set(fnames->dict, key, gmb_make_val_string_counted(fn));
                    }
                    i++;
                }
                gmb_dict_set(cls->dict, "FieldNames", gmb_make_val_obj((gmb_obj_t*)fnames));
                gmb_dict_set(inst->dict, "__class__", gmb_make_val_obj((gmb_obj_t*)cls));
                if (area.ref) gmb_obj_release(area.ref);
                push_ref_entry(task, (gmb_obj_t*)inst, GMB_BT_OBJECT);
                return 0;
            }
            if (inst) gmb_obj_release((gmb_obj_t*)inst);
            if (cls) gmb_obj_release((gmb_obj_t*)cls);
            if (fnames) gmb_obj_release((gmb_obj_t*)fnames);
        }
    }
    /* Non-ISAM (GET # su file binario): non implementato → nessun valore, mai un dato finto. */
    if (area.ref) gmb_obj_release(area.ref);
    push_nil(task);
    return 0;
}

static const gmb_fo_impl_t gmb_fo_impl[] = {
    { 20, gmb_fo_20 },   /* GET_FILE nel BYTECODE = 20 (misurato: l'enum C# dice 21) */
    { 21, gmb_fo_21 },   /* PUT_FILE nel BYTECODE = 21 (GET=20, SEEK=22 — tabella legacy) */
    { 22, gmb_fo_22 },
/* {label, handler} */
    { 15, gmb_fo_15 },
    { 16, gmb_fo_16 },
    { 19, gmb_fo_16 },
    { 17, gmb_fo_17 },
    { 18, gmb_fo_18 },
    { 23, gmb_fo_23 },
    { 24, gmb_fo_24 },
    { 25, gmb_fo_25 },
    { 26, gmb_fo_26 },
    { 27, gmb_fo_27 },
    { 28, gmb_fo_28 },
    { 29, gmb_fo_29 },
    { 30, gmb_fo_30 },

};

int gmb_syscall_fileio(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    for (size_t i = 0; i < sizeof(gmb_fo_impl) / sizeof(gmb_fo_impl[0]); i++)
        if (gmb_fo_impl[i].code == code)
            return gmb_fo_impl[i].handler(vm, task, code, argc);
    return -1;
}
