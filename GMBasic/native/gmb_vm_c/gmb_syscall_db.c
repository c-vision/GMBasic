/* gmb_syscall_db.c — handler SysCall DB commerciali estratti dallo switch monolitico
 * di gmb_vm_exec_db_syscall (task 6.5.2). Ogni case → gmb_sys_<NOME>() dichiara
 * localmente le variabili condivise che usa; la tabella gmb_sys_impl mappa
 * GMB_SYS_* → handler. Il dispatcher gmb_vm_exec_db_syscall fa lookup per codice.
 * Conteggio coerente: sizeof(gmb_sys_impl) / voci = copertura per dominio. */
#include "gmb_vm_core.h"
#include "gmb_db.h"
#include <unistd.h>
#include <unistd.h>

static int gmb_sys_GMB_SYS_ISAM_USE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t a;
int rc;
int i;
int n;
double d;
{
        gmb_stack_entry_t fc = gmb_task_pop(task); /* fieldCount */
        int fieldCount = fc.type == 1 ? (int)fc.num : 0;
        char fieldsCsv[1024]; char idxField[128];
        fieldsCsv[0] = 0; idxField[0] = 0;
        gmb_obj_release(fc.ref);
        {
            char ftName[64][64], ftType[64][64];
            int  ftLen[64], ftn = 0;
            for (i = 0; i < fieldCount && i < 64; i++) {
                gmb_stack_entry_t t1 = gmb_task_pop(task); /* typeCode */
                gmb_stack_entry_t t2 = gmb_task_pop(task); /* sid */
                gmb_stack_entry_t t3 = gmb_task_pop(task); /* len */
                gmb_stack_entry_t t4 = gmb_task_pop(task); /* name */
                if (t4.type == 2 && t4.ref) vm_entry_str(t4, ftName[ftn], 64);
                else snprintf(ftName[ftn], 64, "F%d", ftn);
                vm_entry_str(t1, ftType[ftn], 64);
                ftLen[ftn] = t3.type == 1 ? (int)t3.num : 0;
                if (t2.type == 1 && ftn < 64) vm->vm_isam_tmp_sids[ftn] = (int)t2.num;
                gmb_obj_release(t1.ref); gmb_obj_release(t2.ref); gmb_obj_release(t3.ref); gmb_obj_release(t4.ref);
                ftn++;
            }
            vm->vm_isam_tmp_count = ftn;
            /* Il pop LIFO inverte i campi: riallinea nomi/tipi/len/sid allo schema. */
            for (i = 0; i < ftn / 2; i++) {
                char tb[64]; int tt;
                memcpy(tb, ftName[i], 64); memcpy(ftName[i], ftName[ftn-1-i], 64); memcpy(ftName[ftn-1-i], tb, 64);
                memcpy(tb, ftType[i], 64); memcpy(ftType[i], ftType[ftn-1-i], 64); memcpy(ftType[ftn-1-i], tb, 64);
                tt = ftLen[i]; ftLen[i] = ftLen[ftn-1-i]; ftLen[ftn-1-i] = tt;
                tt = vm->vm_isam_tmp_sids[i]; vm->vm_isam_tmp_sids[i] = vm->vm_isam_tmp_sids[ftn-1-i]; vm->vm_isam_tmp_sids[ftn-1-i] = tt;
            }
            /* CSV campi (il prefisso TypeName|IndexedBy| è aggiunto dopo i pop). */
            for (i = 0; i < ftn; i++) {
                char code = 'C';
                if      (strncasecmp(ftType[i], "LONG", 4) == 0)     code = 'N';
                else if (strncasecmp(ftType[i], "INTEGER", 7) == 0)  code = 'N';
                else if (strncasecmp(ftType[i], "DOUBLE", 6) == 0)   code = 'F';
                else if (strncasecmp(ftType[i], "SINGLE", 6) == 0)   code = 'S';
                else if (strncasecmp(ftType[i], "BIT", 3) == 0)      code = 'L';
                else if (strncasecmp(ftType[i], "DATE", 4) == 0)     code = 'D';
                else if (strncasecmp(ftType[i], "JSON", 4) == 0)     code = 'J';
                /* CURRENCY (16 byte, .NET decimal): mancava -> default 'C' (StringType) e lo schema
                 * VM/C divergeva dal C# (RECSIZE 74 vs 87). 'Y' = CurrencyType (vedi isam_parse_schema). */
                else if (strncasecmp(ftType[i], "CURRENCY", 8) == 0) code = 'Y';
                char seg[96];
                snprintf(seg, sizeof(seg), "%s%s:%c:%d", i ? "," : "", ftName[i], code, ftLen[i] > 0 ? ftLen[i] : 1);
                strncat(fieldsCsv, seg, sizeof(fieldsCsv) - strlen(fieldsCsv) - 1);
            }
        }
        gmb_stack_entry_t al = gmb_task_pop(task); /* alias */
        char alb[128] = "";
        if (al.ref) vm_entry_str(al, alb, (int)sizeof(alb));
        gmb_obj_release(al.ref);
        gmb_stack_entry_t ic = gmb_task_pop(task); /* idxCount */
        int idxCount = ic.type == 1 ? (int)ic.num : 0;
        gmb_obj_release(ic.ref);
        /* INDEXED BY a, b, c: il chunk pusha per ogni chiave (keyExpr, IsUnsafe, IsDescending)
         * in ordine inverso + il conteggio. Nel pop (i%3: 0=IsDescending, 1=IsUnsafe,
         * 2=keyExpr) si raccolgono TUTTI i keyExpr. Prima si prendeva solo il PRIMO
         * (`i<3` → idxField="ID"): lo schema aveva un solo indice e `SET ORDER TO "Nome"`/
         * SEEK su indice secondario non trovavano mai (§3 vmc vs il). */
        for (i = 0; i < idxCount * 3 && i < 128; i++) {
            gmb_stack_entry_t t = gmb_task_pop(task);
            if (i % 3 == 2 && t.type == 2 && t.ref) {
                char kb[64]; kb[0] = 0;
                vm_entry_str(t, kb, (int)sizeof(kb));
                if (kb[0]) {
                    if (idxField[0]) strncat(idxField, ",", (int)sizeof(idxField) - strlen(idxField) - 1);
                    strncat(idxField, kb, (int)sizeof(idxField) - strlen(idxField) - 1);
                }
            }
            gmb_obj_release(t.ref);
        }
        gmb_stack_entry_t isU = gmb_task_pop(task); gmb_obj_release(isU.ref);
        gmb_stack_entry_t lock = gmb_task_pop(task); gmb_obj_release(lock.ref);
        gmb_stack_entry_t tn = gmb_task_pop(task);
        char tnb[256]; vm_entry_str(tn, tnb, (int)sizeof(tnb));
        gmb_obj_release(tn.ref);
        /* Schema "<TypeName>|<IndexedBy>|<fields>" (parità engine C#/C99Native). */
        vm->vm_isam_tmp_schema[0] = 0;
        if (tnb[0]) strncat(vm->vm_isam_tmp_schema, tnb, sizeof(vm->vm_isam_tmp_schema) - strlen(vm->vm_isam_tmp_schema) - 1);
        strncat(vm->vm_isam_tmp_schema, "|", sizeof(vm->vm_isam_tmp_schema) - strlen(vm->vm_isam_tmp_schema) - 1);
        if (idxField[0]) strncat(vm->vm_isam_tmp_schema, idxField, sizeof(vm->vm_isam_tmp_schema) - strlen(vm->vm_isam_tmp_schema) - 1);
        strncat(vm->vm_isam_tmp_schema, "|", sizeof(vm->vm_isam_tmp_schema) - strlen(vm->vm_isam_tmp_schema) - 1);
        strncat(vm->vm_isam_tmp_schema, fieldsCsv, sizeof(vm->vm_isam_tmp_schema) - strlen(vm->vm_isam_tmp_schema) - 1);
        gmb_stack_entry_t fn = gmb_task_pop(task); /* fileName */
        char fileb[1024];
        vm_entry_str(fn, fileb, (int)sizeof(fileb));
        gmb_obj_release(fn.ref);
        /* Parita' C#: ISAM OPEN crea il file se non esiste (schema dai campi del TYPE). */
        if (access(fileb, F_OK) != 0) (void)gmb_db_isam_create(fileb, vm->vm_isam_tmp_schema);
        char* rp_isam = gmb_path_resolve_db(fileb);
        int64_t hh = gmb_db_isam_open(rp_isam);
        free(rp_isam);
        gmb_db_set_current_isam(hh);
        {
            /* Registra alias -> handle nelle due sedi: VM-level (per JOIN WITH e altre
             * operazioni VM) e runtime condiviso (gmb_db_isam_select per parità C99). */
            if (alb[0] && hh > 0) {
                gmb_db_isam_set_alias(hh, alb);
                int found = 0;
                for (i = 0; i < vm->vm_isam_alias_count; i++)
                    if (strcasecmp(vm->vm_isam_alias[i], alb) == 0) { vm->vm_isam_alias_handle[i] = hh; found = 1; break; }
                if (!found && vm->vm_isam_alias_count < 32) {
                    snprintf(vm->vm_isam_alias[vm->vm_isam_alias_count], 64, "%s", alb);
                    vm->vm_isam_alias_handle[vm->vm_isam_alias_count] = hh;
                    (vm->vm_isam_alias_count)++;
                }
            }
        }
        {
            int slot = vm_isam_sid_slot(vm, hh);
            if (slot >= 0) {
                vm->vm_isam_sid_count[slot] = vm->vm_isam_tmp_count;
                for (i = 0; i < vm->vm_isam_tmp_count; i++) vm->vm_isam_sids[slot][i] = vm->vm_isam_tmp_sids[i];
            }
        }
        push_num_entry(task, (double)hh, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- ISAM: APPEND / PUT (dict -> CSV) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_APPEND(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
char csv[16384];
gmb_stack_entry_t e;
gmb_stack_entry_t a;
gmb_obj_t* src;
int rc;
        e = gmb_task_pop(task); src = e.ref;
        a = gmb_task_pop(task);
        {
            int fok = gmb_db_isam_fields(gmb_db_current_isam(), fields, (int)sizeof(fields));
            int dok = vm_dict_to_csv(vm, src, fields, csv, (int)sizeof(csv));
            if (fok == 0 && dok == 0) {
                rc = gmb_db_isam_insert(gmb_db_current_isam(), csv);
                push_num_entry(task, rc == 0 ? 12345.0 : 0.0, GMB_BT_NUMBER);
            } else {
                push_num_entry(task, 0.0, GMB_BT_NUMBER);
            }
        }
        gmb_obj_release(e.ref); gmb_obj_release(a.ref);
        return 0;

    /* ---- ISAM: APPENDMANY (list di dict -> N insert) ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_ISAM_PUT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
char csv[16384];
gmb_stack_entry_t e;
gmb_stack_entry_t a;
gmb_obj_t* src;
int rc;
        e = gmb_task_pop(task); src = e.ref;
        a = gmb_task_pop(task);
        {
            int fok = gmb_db_isam_fields(gmb_db_current_isam(), fields, (int)sizeof(fields));
            int dok = vm_dict_to_csv(vm, src, fields, csv, (int)sizeof(csv));
            if (fok == 0 && dok == 0) {
                rc = gmb_db_isam_put(gmb_db_current_isam(), csv);
                push_num_entry(task, rc == 0 ? 12345.0 : 0.0, GMB_BT_NUMBER);
            } else {
                push_num_entry(task, 0.0, GMB_BT_NUMBER);
            }
        }
        gmb_obj_release(e.ref); gmb_obj_release(a.ref);
        return 0;

    /* ---- ISAM: APPENDMANY (list di dict -> N insert) ---- */
    return -1; /* non gestito */
}

/* ---- ISAM: CLOSE [alias] (1 arg: alias stringa, oppure nil = area corrente) ---- */
static int gmb_sys_GMB_SYS_ISAM_CLOSE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char alias[128];
    int64_t h = 0;
    int i;
    (void)code;
    alias[0] = 0;
    for (i = 0; i < argc; i++) {
        gmb_stack_entry_t e = gmb_task_pop(task);
        if (i == 0 && e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) vm_entry_str(e, alias, (int)sizeof(alias));
        gmb_obj_release(e.ref);
    }
    if (alias[0]) {
        for (i = 0; i < vm->vm_isam_alias_count; i++)
            if (strcasecmp(vm->vm_isam_alias[i], alias) == 0) { h = vm->vm_isam_alias_handle[i]; break; }
        if (h <= 0) h = gmb_db_isam_handle_for(alias);
    } else {
        h = gmb_db_current_isam();
    }
    if (h > 0) {
        (void)gmb_db_isam_close(h);
        if (gmb_db_current_isam() == h) gmb_db_set_current_isam(0);
        /* togli l'alias chiuso dal registro VM (compattazione) */
        for (i = 0; i < vm->vm_isam_alias_count; ) {
            if (vm->vm_isam_alias_handle[i] == h) {
                int k;
                for (k = i; k + 1 < vm->vm_isam_alias_count; k++) {
                    memcpy(vm->vm_isam_alias[k], vm->vm_isam_alias[k + 1], sizeof(vm->vm_isam_alias[k]));
                    vm->vm_isam_alias_handle[k] = vm->vm_isam_alias_handle[k + 1];
                }
                vm->vm_isam_alias_count--;
            } else i++;
        }
    }
    return 0;
}

static int gmb_sys_GMB_SYS_ISAM_APPENDMANY(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
char csv[16384];
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int n;
{
        e = gmb_task_pop(task); /* batch (list) */
        a = gmb_task_pop(task); /* area */
        n = 0;
        if (e.ref && e.ref->kind == GMB_OBJ_LIST) {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)e.ref;
            if (sv->list) {
                if (gmb_db_isam_fields(gmb_db_current_isam(), fields, (int)sizeof(fields)) == 0) {
                    for (size_t k = 0; k < sv->list->count; k++) {
                        gmb_val_t item = sv->list->items[k];
                        if (item.kind == GMB_TYPE_OBJECT && item.v_ptr
                            && vm_dict_to_csv(vm, (gmb_obj_t*)item.v_ptr, fields, csv, (int)sizeof(csv)) == 0
                            && gmb_db_isam_insert(gmb_db_current_isam(), csv) == 0)
                            n++;
                    }
                }
            }
        }
        push_num_entry(task, (double)n, GMB_BT_NUMBER);
        gmb_obj_release(e.ref); gmb_obj_release(a.ref);
        return 0;
    }

    /* ---- ISAM: CREATE INDEX (area, name, expr, keylen) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_CREATE_INDEX(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char b1[512];
char b2[512];
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int rc;
{
        int keylen = 0;
        gmb_stack_entry_t k = gmb_task_pop(task);
        if (k.type == 1) keylen = (int)k.num;
        e = gmb_task_pop(task); vm_entry_str(e, b2, (int)sizeof(b2)); /* expr */
        a = gmb_task_pop(task); vm_entry_str(a, b1, (int)sizeof(b1)); /* name */
        gmb_stack_entry_t area = gmb_task_pop(task); (void)area;
        rc = gmb_db_isam_create_index(gmb_db_current_isam(), b1, b2, keylen);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        gmb_obj_release(k.ref); gmb_obj_release(e.ref); gmb_obj_release(a.ref); gmb_obj_release(area.ref);
        return 0;
    }

    /* ---- ISAM: SORT TO (area, dest, expr) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_SORT_TO(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char b1[512];
char b2[512];
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int rc;
{
        e = gmb_task_pop(task); vm_entry_str(e, b2, (int)sizeof(b2)); /* expr */
        a = gmb_task_pop(task); vm_entry_str(a, b1, (int)sizeof(b1)); /* dest */
        gmb_stack_entry_t area = gmb_task_pop(task); (void)area;
        rc = gmb_db_isam_sort_to(gmb_db_current_isam(), b1, b2, 0, NULL);
        push_num_entry(task, rc == 0 ? 1.0 : 0.0, GMB_BT_NUMBER);
        gmb_obj_release(e.ref); gmb_obj_release(a.ref); gmb_obj_release(area.ref);
        return 0;
    }

    /* ---- ISAM: INFO (ritorna JSON stringa) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_INFO(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
{
        e = gmb_task_pop(task); /* area */
        char jbuf[512];
        if (gmb_db_isam_info(gmb_db_current_isam(), jbuf, (int)sizeof(jbuf)) == 0) {
            gmb_obj_string_t* s = gmb_str_cstr(jbuf);
            push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        } else {
            gmb_obj_string_t* s = gmb_str_cstr("{}");
            push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        }
        gmb_obj_release(e.ref);
        return 0;
    }

    /* ---- DBF: CREATE TABLE (fileName, typeName, [name,len,isString,type]*, fieldCount) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_CREATE_TABLE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int i;
double d;
{
        gmb_stack_entry_t fc = gmb_task_pop(task);
        int fcnt = fc.type == 1 ? (int)fc.num : 0;
        gmb_obj_release(fc.ref);
        char dn[64][64], dt[64][8];
        int  dl[64], dfc = 0;
        for (i = 0; i < fcnt && i < 64; i++) {
            gmb_stack_entry_t t1 = gmb_task_pop(task); /* typeCode */
            gmb_stack_entry_t t2 = gmb_task_pop(task); /* isString */
            gmb_stack_entry_t t3 = gmb_task_pop(task); /* len */
            gmb_stack_entry_t t4 = gmb_task_pop(task); /* name */
            char tc[64], nb[64];
            vm_entry_str(t1, tc, 64);
            vm_entry_str(t4, nb, 64);
            char code = 'N';
            int isStr = t2.type == 1 ? (int)t2.num : 0;
            if      (strncasecmp(tc, "STRING", 6) == 0)  code = 'C';
            else if (strncasecmp(tc, "DOUBLE", 6) == 0)  code = 'F';
            else if (strncasecmp(tc, "SINGLE", 6) == 0)  code = 'S';
            else if (strncasecmp(tc, "BIT", 3) == 0)     code = 'L';
            else if (strncasecmp(tc, "DATE", 4) == 0)    code = 'D';
            /* CURRENCY/MONEY: 'Y' (16 byte, .NET decimal) come BuildSchemaDefs C99;
             * senza, cadeva su 'N' (10) e il valore perdeva i decimali (§11 Val 12 vs 12.34). */
            else if (strncasecmp(tc, "CURRENCY", 8) == 0 || strncasecmp(tc, "MONEY", 5) == 0) code = 'Y';
            snprintf(dt[dfc], 8, "%c", code);
            snprintf(dn[dfc], 64, "%s", nb);
            /* Lunghezza: per le stringhe quella dichiarata; per i numerici/data la VM
             * non la pusha (0) e `?: 1` creava campi da 1 byte incapaci di contenere il
             * valore. Default per tipo come il C99 (BuildSchemaDefs). */
            int dlv = t3.type == 1 ? (int)t3.num : 0;
            if (dlv <= 0) {
                if      (code == 'C') dlv = 20;
                else if (code == 'D') dlv = 8;
                else if (code == 'L') dlv = 1;
                else if (code == 'Y') dlv = 16;
                else                  dlv = 10;
            }
            dl[dfc] = dlv;
            dfc++;
            gmb_obj_release(t1.ref); gmb_obj_release(t2.ref); gmb_obj_release(t3.ref); gmb_obj_release(t4.ref);
        }
        /* Il pop LIFO inverte i campi: ribalta per allinearli all'ordine del TYPE. */
        for (i = 0; i < dfc / 2; i++) {
            char tb[64]; int tt;
            memcpy(tb, dn[i], 64); memcpy(dn[i], dn[dfc-1-i], 64); memcpy(dn[dfc-1-i], tb, 64);
            memcpy(tb, dt[i], 8);  memcpy(dt[i], dt[dfc-1-i], 8);  memcpy(dt[dfc-1-i], tb, 8);
            tt = dl[i]; dl[i] = dl[dfc-1-i]; dl[dfc-1-i] = tt;
        }
        gmb_stack_entry_t tname = gmb_task_pop(task); /* typeName */
        gmb_obj_release(tname.ref);
        gmb_stack_entry_t fname = gmb_task_pop(task); /* fileName */
        char fileb[1024];
        vm_entry_str(fname, fileb, (int)sizeof(fileb));
        gmb_obj_release(fname.ref);
        {
            char defs[2048];
            int pd = 0;
            defs[0] = 0;
            for (i = 0; i < dfc; i++) {
                int add = snprintf(defs + pd, (int)sizeof(defs) - pd, "%s%s:%s:%d",
                    i ? "," : "", dn[i], dt[i], dl[i] > 0 ? dl[i] : 1);
                if (add > 0) pd += add;
            }
            char* rp_dbf = gmb_path_resolve_db(fileb);
            /* gmb_db_dbf_create ora CREA e APRE, ritornando l'handle (>=0) o -1: prima
             * ritornava uno stato e qui si riapriva con gmb_db_dbf_open. */
            int64_t crc = gmb_db_dbf_create(rp_dbf, defs);
            int hh = crc >= 0 ? (int)crc : 0;
            free(rp_dbf);
            gmb_db_set_current_dbf(hh);
            push_num_entry(task, (double)hh, GMB_BT_NUMBER);
        }
        return 0;
    }

    /* ---- DBF: APPEND RECORD (dict -> CSV) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_APPEND_RECORD(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
char csv[16384];
gmb_stack_entry_t e;
gmb_obj_t* src;
        e = gmb_task_pop(task); src = e.ref;
        if (gmb_db_dbf_fields(gmb_db_current_dbf(), fields, (int)sizeof(fields)) == 0
            && vm_dict_to_csv(vm, src, fields, csv, (int)sizeof(csv)) == 0) {
            gmb_db_dbf_append(gmb_db_current_dbf(), csv);
        }
        gmb_obj_release(e.ref);
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;

    /* ---- DBF: REPLACE (count, [value, fname]...) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_REPLACE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
int i;
{
        /* Scope in CIMA (CompileScopeClause pusha SEMPRE 4 arg: hasScope, scopeType, arg1,
         * arg2), sotto il count, sotto le coppie (fname,value). Il handler popa il count per
         * primo → leggeva arg2 (NIL) → nrep=0 → DBF REPLACE no-op (§11 valori 0). */
        for (i = 0; i < 4; i++) { gmb_stack_entry_t sc = gmb_task_pop(task); gmb_obj_release(sc.ref); }
        gmb_stack_entry_t cnt = gmb_task_pop(task);
        int nrep = cnt.type == 1 ? (int)cnt.num : 0;
        if (gmb_db_dbf_fields(gmb_db_current_dbf(), fields, (int)sizeof(fields)) != 0) nrep = 0;
        for (i = 0; i < nrep; i++) {
            gmb_stack_entry_t v = gmb_task_pop(task);
            gmb_stack_entry_t fn = gmb_task_pop(task);
            char valstr[512], fname[128];
            vm_entry_str(fn, fname, (int)sizeof(fname));
            if (v.type == 1) snprintf(valstr, sizeof(valstr), "%g", v.num);
            else vm_entry_str(v, valstr, (int)sizeof(valstr));
            int idx = vm_dbf_field_index(fields, fname);
            if (idx >= 0) gmb_db_dbf_replace_field(gmb_db_current_dbf(), idx, valstr);
            gmb_obj_release(v.ref); gmb_obj_release(fn.ref);
        }
        gmb_obj_release(cnt.ref);
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- DBF: GET RECORD (legge il record corrente -> dict) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_GET_RECORD(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
int i;
{
        gmb_obj_shared_val_t* dict = gmb_shared_dict_new();
        if (gmb_db_dbf_fields(gmb_db_current_dbf(), fields, (int)sizeof(fields)) == 0) {
            const char* p = fields;
            while (*p) {
                char fname[128]; int fi = 0;
                while (*p && *p != ',' && fi < 127) fname[fi++] = *p++;
                fname[fi] = 0;
                if (*p == ',') p++;
                /* Valore TIPIZZATO (numero/stringa/…) dello stesso campo per NOME: prima si
                 * copiava il TESTO di ogni campo come stringa → `STR(rec.ID)` rendeva 0 e i
                 * confronti numerici su `rec.Campo` erano sbagliati. gmb_db_dbf_field_val_name
                 * è la stessa lettura late-binding usata dai campi nudi (parità IL/PCode). */
                {
                    gmb_val_t v = gmb_db_dbf_field_val_name(gmb_db_current_dbf(), fname);
                    gmb_dict_set(dict->dict, fname, v);
                }
            }
        }
        push_ref_new(task, (gmb_obj_t*)dict, GMB_BT_OBJECT);
        return 0;
        return 0;
    }

    /* ---- SPEED: USE (tableName, dbAlias, typeSymbolId, typeName, alias, isUnsafe,
     * indexedByCount [+ (keyExpr,isUnsafe)*], (fieldName,fieldSymbolId)*, fieldCount) ----
     * Cattura i (nome,symbolId) di campo per PUT/GET (vedi vm_speed_field_slot sopra) —
     * prima gestita dal path generico "known" (gmb_db_apply_syscall_known), che scartava
     * questa metadata multi-arg (vedi gmb_db_apply_syscall_known in gmb_db_common.c: SPEED_USE
     * rimosso da quella lista). Replica register_table_alias+speed_use del path generico
     * per non regredire l'attach tabella. */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_USE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int rc;
int i;
int n;
{
        /* NON si pop-ano gli argomenti uno a uno nell'ordine LIFO "naturale":
         * indexedByCount sta SOTTO le proprie coppie chiave nell'ordine di push, quindi al
         * momento di raggiungerlo in pop (LIFO) non si sa ancora quante coppie ci sono
         * sopra — un pop sequenziale ingenuo (come nella versione precedente di questo
         * case) legge i valori nelle posizioni sbagliate (diagnosticato via trace: "dba"
         * conteneva il valore di "typeName", tutto lo stack scalato). Stessa soluzione del
         * riferimento PCodeVM (case SPEED_USE in PCodeProcessor.Syscalls.cs): si pop-a
         * tutto lo stack in un array ricostruendo l'ordine di push originale (pop LIFO
         * reinserito in testa), poi si indicizza posizionalmente. */
        int n = argc;
        if (n > 256) n = 256;
        gmb_stack_entry_t raw[256];
        for (i = n - 1; i >= 0; i--) raw[i] = gmb_task_pop(task);
        for (i = 256; i < argc; i++) { gmb_stack_entry_t t = gmb_task_pop(task); gmb_obj_release(t.ref); }

        int typeSymbolId = (n > 2 && raw[2].type == 1) ? (int)raw[2].num : 0;
        char alb[128] = "";   if (n > 4) vm_entry_str(raw[4], alb, (int)sizeof(alb));
        int idxKeyCount = (n > 6 && raw[6].type == 1) ? (int)raw[6].num : 0;
        /* Nome del campo chiave primaria dichiarato via INDEXED BY (solo la prima chiave,
         * come il riferimento PCodeVM): l'espressione può includere "#SymbolId" per un
         * VariableExpr (VariableExpr.ToString()), da scartare per ottenere il nome pulito. */
        char pkField[64] = "";
        if (idxKeyCount > 0 && n > 7) {
            char keb[64] = ""; vm_entry_str(raw[7], keb, (int)sizeof(keb));
            char* hash = strchr(keb, '#');
            if (hash) *hash = 0;
            snprintf(pkField, sizeof(pkField), "%s", keb);
        }

        int fieldCount = (n > 0 && raw[n-1].type == 1) ? (int)raw[n-1].num : 0;
        int fieldsStart = n - 1 - fieldCount * 2;
        int spTmpSids[64]; char spTmpNames[64][64]; int spTmpN = 0;
        if (fieldCount > 0 && fieldsStart >= 0) {
            for (i = 0; i < fieldCount && i < 64; i++) {
                vm_entry_str(raw[fieldsStart + i * 2], spTmpNames[spTmpN], 64);
                gmb_stack_entry_t sidE = raw[fieldsStart + i * 2 + 1];
                spTmpSids[spTmpN] = sidE.type == 1 ? (int)sidE.num : -1;
                spTmpN++;
            }
        }
        for (i = 0; i < n; i++) gmb_obj_release(raw[i].ref);

        /* typeSymbolId NON è un tableId valido per il motore Database/Manifest nativo: è
         * un simbolo del compilatore C#, mai registrato nel Manifest della tabella. Senza
         * una entry nel Manifest, db_get_cursor() (usato da GoTop/GoBottom/Skip) fallisce
         * sempre (Manifest_TryGetTable ritorna 0), quindi ogni GET successivo falliva
         * (rc=-1, nessun record corrente) anche con PUT riuscito — bug distinto da quello
         * del pop-order sopra, scoperto verificando che Database_GoTop richiede una entry
         * di Manifest creata da Manifest_CreateTable. Bisogna quindi CREARE la tabella nel
         * motore (gmb_db_speed_create_table -> Database_CreateTableWithFields ->
         * Manifest_CreateTable, idempotente per nome) e usare l'ID REALE che ritorna come
         * tableId per tutte le operazioni successive — non il typeSymbolId. Si registra la
         * tabella con il nome ALIAS (non il typeName UDT "Cliente"/"Ordine"): SPEED
         * SELECT/JOIN risolvono i nomi in FROM/JOIN via Manifest_TryGetTableByName con lo
         * stesso testo usato in "FROM Clienti"/"ALIAS Clienti", non con il tipo UDT. */
        char fieldsCsv[512] = ""; size_t fcPos = 0;
        for (i = 0; i < spTmpN && fcPos < sizeof(fieldsCsv) - 32; i++) {
            int w = snprintf(fieldsCsv + fcPos, sizeof(fieldsCsv) - fcPos, "%s%s", i ? "," : "", spTmpNames[i]);
            if (w > 0) fcPos += (size_t)w;
        }
        int realTableId = gmb_db_speed_create_table(gmb_db_current_speed(), alb, fieldsCsv);
        if (realTableId <= 0) realTableId = typeSymbolId; /* fallback difensivo */

        /* Registra l'ALIAS REALE (non il nome tabella — possono differire, es. ALIAS Cl):
         * PUT/GET/SEEK risolvono per l'alias passato allo statement, non per nome tabella. */
        gmb_db_speed_register_table_alias(alb, realTableId);
        rc = gmb_db_speed_use(gmb_db_current_speed(), realTableId);

        /* il registro campi va tenuto per TABELLA (tableId REALE), non per handle di
         * processo — SPEED condivide UN SOLO handle tra tutte le tabelle di un database. */
        int slot = vm_speed_field_slot(vm, (int64_t)realTableId);
        if (slot >= 0) {
            vm->vm_speed_field_count[slot] = spTmpN;
            for (i = 0; i < spTmpN; i++) {
                vm->vm_speed_field_sids[slot][i] = spTmpSids[i];
                snprintf(vm->vm_speed_field_names[slot][i], 64, "%s", spTmpNames[i]);
            }
            snprintf(vm->vm_speed_field_pk[slot], 64, "%s", pkField);
        }
        push_num_entry(task, (double)rc, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- SPEED: GO (alias, locationKind) — locationKind == GMB_DB_TOK_TOP e' TOP,
     * altrimenti BOTTOM. Escluso dal
     * dispatcher generico "known" (vedi gmb_db_apply_syscall_known in gmb_db_common.c):
     * quel path raccoglie un solo sarg/iarg totali e riusava ERRONEAMENTE locationKind
     * come tableId al posto dell'alias risolto, posizionando il cursore sulla tabella
     * sbagliata — ogni GET successivo falliva (rc=-1, nessun record corrente valido). ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_GO(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int rc;
{
        gmb_stack_entry_t loc = gmb_task_pop(task);   /* locationKind */
        gmb_stack_entry_t al5 = gmb_task_pop(task);   /* alias */
        int locationKind = loc.type == 1 ? (int)loc.num : 0;
        char al5b[128] = ""; vm_entry_str(al5, al5b, (int)sizeof(al5b));
        int tid5 = al5b[0] ? gmb_db_speed_table_by_alias(al5b) : 0;
        if (tid5 < 0) tid5 = 0;
        gmb_obj_release(loc.ref); gmb_obj_release(al5.ref);
        /* locationKind = (int)TokenKind.Top, emesso da VisitSpeedGoStatement
         * (EmitConstant((double)stmt.Location.Kind), Compiler.Speed.cs): è la posizione del
         * membro nell'enum C# (sequenziale, nessun valore esplicito). L'ordinale NON si
         * scrive qui: `GMB_DB_TOK_TOP` viene da gmb_db_tokenkind_gen.h, GENERATO da
         * TokenKind.cs (tools/gen_db_tokenkind.bas, `--check` = gate antidrift). STORIA:
         * 498 → 479 → 480, sempre inseguendo l'enum a mano; a 480 reali 482, quindi
         * `GO T, TOP` prendeva il ramo BOTTOM (set_filter.bas dava Gamma invece di Beta,
         * `top:` letto come ultimo record) e `BOTTOM` cadeva in un no-op muto. */
        rc = locationKind == GMB_DB_TOK_TOP ? gmb_db_speed_go_top(gmb_db_current_speed(), tid5)
                                            : gmb_db_speed_go_bottom(gmb_db_current_speed(), tid5);
        push_num_entry(task, (double)rc, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- SPEED: SKIP (alias, count) — stesso motivo di esclusione dal path generico di GO
     * sopra: il count veniva riusato come tableId e il recordCount reale scartato
     * (hardcoded a 1). ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_SKIP(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t cnt = gmb_task_pop(task);   /* count */
        gmb_stack_entry_t al6 = gmb_task_pop(task);   /* alias */
        int count = cnt.type == 1 ? (int)cnt.num : 1;
        char al6b[128] = ""; vm_entry_str(al6, al6b, (int)sizeof(al6b));
        int tid6 = al6b[0] ? gmb_db_speed_table_by_alias(al6b) : 0;
        if (tid6 < 0) tid6 = 0;
        gmb_obj_release(cnt.ref); gmb_obj_release(al6.ref);
        rc = gmb_db_speed_skip(gmb_db_current_speed(), tid6, count);
        push_num_entry(task, (double)rc, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- SPEED: PUT (alias, dict -> chiave sortable/valore JSON, parità PCodeVM
     * SpeedRecordToBytes/SpeedSortableDoubleBytes) ---- */
    return -1; /* non gestito */
}

/* Inserisce UN record SPEED da un dict VM (istanza UDT con chiavi "__m<sid>"): normalizza
 * le chiavi nei nomi campo reali (registro SPEED_USE), calcola la chiave (PK) e chiama
 * gmb_db_speed_set. Ritorna 0 su successo. Condivisa da SPEED PUT (singolo) e SPEED SETMANY. */
static int vm_speed_set_record(gmb_vm_t* vm, int tableId, gmb_dict_t* d)
{
    uint8_t keybuf[64]; int klen = 0;
    const uint8_t* val = (const uint8_t*)""; int vlen = 0;
    gmb_string_t jsonVal = {0};
    int slot = vm_speed_field_slot(vm, (int64_t)tableId);
    int fcount = slot >= 0 ? vm->vm_speed_field_count[slot] : 0;
    gmb_dict_t named = gmb_dict_new();
    for (size_t di = 0; di < d->count; di++) {
        const char* key = d->items[di].key;
        if (!key) continue;
        const char* outKey = key;
        if (key[0] == '_' && key[1] == '_') {
            /* "__m<sid>" (campo di istanza UDT) -> nome reale via registro; ogni altra
             * chiave "__..." (__key__/__class__/...) è un marker interno, non un campo. */
            if (key[2] == 'm' && key[3] >= '0' && key[3] <= '9') {
                int sid = atoi(key + 3);
                int found = 0;
                for (int fi = 0; fi < fcount; fi++)
                    if (vm->vm_speed_field_sids[slot][fi] == sid) { outKey = vm->vm_speed_field_names[slot][fi]; found = 1; break; }
                if (!found) continue;
            } else continue;
        }
        gmb_dict_set(&named, outKey, d->items[di].value);
    }
    const char* pkName = "ID";
    if (slot >= 0 && vm->vm_speed_field_pk[slot][0]) pkName = vm->vm_speed_field_pk[slot];
    gmb_val_t kv = vm_dict_get_ci(d, "__key__");
    if (kv.kind == GMB_TYPE_NIL) kv = vm_dict_get_ci(&named, pkName);
    if (kv.kind == GMB_TYPE_INT || kv.kind == GMB_TYPE_DOUBLE) {
        gmb_speed_sortable_double_bytes(gmb_val_to_double(kv), keybuf);
        klen = 8;
    } else if (kv.kind == GMB_TYPE_STRING && kv.v_string.data) {
        klen = (int)(kv.v_string.length < sizeof(keybuf) ? kv.v_string.length : sizeof(keybuf));
        memcpy(keybuf, kv.v_string.data, (size_t)klen);
    } else {
        char keystr[64]; vm_val_csv(kv, keystr, (int)sizeof(keystr));
        klen = (int)strlen(keystr); memcpy(keybuf, keystr, (size_t)klen);
    }
    jsonVal = gmb_dict_to_json(&named);
    gmb_dict_free(&named);
    val = (const uint8_t*)jsonVal.data; vlen = (int)jsonVal.length;
    int rc = gmb_db_speed_set(gmb_db_current_speed(), tableId, keybuf, klen, val, vlen);
    if (jsonVal.data) gmb_string_free(&jsonVal);
    return rc;
}

static int gmb_sys_GMB_SYS_SPEED_PUT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
gmb_stack_entry_t a;
gmb_obj_t* src;
int rc;
double d;
{
        e = gmb_task_pop(task); src = e.ref;   /* dict */
        a = gmb_task_pop(task);                 /* alias */
        char aliasb[128] = ""; vm_entry_str(a, aliasb, (int)sizeof(aliasb));
        /* Le tabelle SPEED condividono UN SOLO handle di processo (gmb_db_current_speed());
         * il tableId (== il symbolId del TYPE, vedi SPEED_USE sopra) seleziona QUALE tabella
         * — senza risolverlo dall'ALIAS passato allo statement, PUT/GET/SEEK avrebbero
         * sempre operato sull'ultima tabella USE-ata nel processo (qui: "Ordini" invece di
         * "Clienti"), con field-registry e dati della tabella SBAGLIATA. */
        int tableId = aliasb[0] ? gmb_db_speed_table_by_alias(aliasb) : 0;
        if (tableId < 0) tableId = 0;
        if (src && src->kind == GMB_OBJ_DICT) {
            gmb_dict_t* d = ((gmb_obj_shared_val_t*)src)->dict;
            if (d) rc = vm_speed_set_record(vm, tableId, d);
        }
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        gmb_obj_release(e.ref); gmb_obj_release(a.ref);
        return 0;
    }

    /* ---- SPEED: GET (alias) -> record corrente come dict "__m<sid>" (per OP_LOAD_MEMBER),
     * parità PCodeVM SpeedGet/BytesToSpeedRecord (JSON per-nome -> wrapper leggibile per
     * offset). Bug ABI preesistente qui accanto (non nella diagnosi Obsidian): il vecchio
     * codice passava vlen=0 come capacità iniziale a gmb_db_speed_get_current, che copia
     * SOLO se *vlen (capacità) >= glen (lunghezza reale) — con vlen=0 il memcpy non
     * scattava MAI per un record non vuoto, e gbuf restava non inizializzato: null/ID=0
     * indipendentemente dal formato del valore memorizzato. ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_GET(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char csv[16384];
gmb_stack_entry_t a;
int rc;
int n;
double d;
{
        a = gmb_task_pop(task); /* alias */
        char aliasb[128] = ""; vm_entry_str(a, aliasb, (int)sizeof(aliasb));
        int tableId = aliasb[0] ? gmb_db_speed_table_by_alias(aliasb) : 0;
        if (tableId < 0) tableId = 0;
        int vlen = (int)sizeof(csv); /* capacità del buffer, non 0 (vedi nota sopra) */
        rc = gmb_db_speed_get_current(gmb_db_current_speed(), tableId, csv, &vlen);
        gmb_obj_shared_val_t* rec = gmb_shared_dict_new();
        if (rec && rc == 0 && vlen > 0) {
            char jsbuf[16384];
            int n = vlen < (int)sizeof(jsbuf) - 1 ? vlen : (int)sizeof(jsbuf) - 1;
            memcpy(jsbuf, csv, (size_t)n); jsbuf[n] = 0;
            gmb_dict_t named = gmb_dict_new();
            gmb_json_parse_to_dict(jsbuf, &named);
            int slot = vm_speed_field_slot(vm, (int64_t)tableId);
            int fcount = slot >= 0 ? vm->vm_speed_field_count[slot] : 0;
            for (int fi = 0; fi < fcount; fi++) {
                gmb_val_t v = gmb_dict_get(named, vm->vm_speed_field_names[slot][fi]);
                if (v.kind != GMB_TYPE_NIL) {
                    char mkey[32]; snprintf(mkey, sizeof(mkey), "__m%d", vm->vm_speed_field_sids[slot][fi]);
                    gmb_dict_set(rec->dict, mkey, v);
                }
            }
            gmb_dict_free(&named);
        }
        if (rec) push_ref_new(task, (gmb_obj_t*)rec, GMB_BT_OBJECT);
        else push_nil(task);
        gmb_obj_release(a.ref);
        return 0;
    }

    /* ---- SPEED: SEEK (alias, key, idxName) -> chiave sortable, parità con PUT (prima
     * un testo "%g"/stringa contro la chiave BINARIA sortable scritta dal PUT, quindi
     * SEEK non trovava mai nulla) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_SEEK(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
gmb_stack_entry_t a;
int rc;
{
        gmb_stack_entry_t idx = gmb_task_pop(task); /* idxName */
        e = gmb_task_pop(task);                      /* keyVal */
        a = gmb_task_pop(task);                      /* alias */
        char aliasb[128] = ""; vm_entry_str(a, aliasb, (int)sizeof(aliasb));
        int tableId = aliasb[0] ? gmb_db_speed_table_by_alias(aliasb) : 0;
        if (tableId < 0) tableId = 0;
        uint8_t keybuf[64]; int klen;
        if (e.type == 1) { gmb_speed_sortable_double_bytes(e.num, keybuf); klen = 8; }
        else {
            char keystr[64];
            vm_entry_str(e, keystr, (int)sizeof(keystr));
            klen = (int)strlen(keystr);
            memcpy(keybuf, keystr, (size_t)klen);
        }
        rc = gmb_db_speed_seek(gmb_db_current_speed(), tableId, keybuf, klen, NULL);
        push_num_entry(task, rc == 0 ? 1.0 : 0.0, GMB_BT_NUMBER);
        gmb_obj_release(idx.ref); gmb_obj_release(e.ref); gmb_obj_release(a.ref);
        return 0;
    }

    /* ---- LEDGER: ADD (h, date, desc, [account, amount]... flat) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_ADD(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
{
        int npost = (argc - 3) / 2;
        if (npost < 0) npost = 0;
        if (npost > 32) npost = 32;
        char dateb[32], descb[512];
        char accb[32][128], amtb[32][64];
        gmb_stack_entry_t perr;
        for (i = npost - 1; i >= 0; i--) {
            perr = gmb_task_pop(task); /* amount */
            /* L'importo arriva NUMERICO dal bytecode: leggerlo come stringa dava "" →
             * importi a 0 nel journal C (balance TotalAmount 0.00) e accounts/balance/register
             * vuoti sulla VM/C. */
            if (perr.type == 1) snprintf(amtb[i], sizeof(amtb[i]), "%g", perr.num);
            else vm_entry_str(perr, amtb[i], (int)sizeof(amtb[i]));

            gmb_obj_release(perr.ref);
            perr = gmb_task_pop(task); /* account */
            vm_entry_str(perr, accb[i], (int)sizeof(accb[i]));
            gmb_obj_release(perr.ref);
        }
        perr = gmb_task_pop(task); /* desc */
        vm_entry_str(perr, descb, (int)sizeof(descb));
        gmb_obj_release(perr.ref);
        perr = gmb_task_pop(task); /* date */
        /* La data arriva come SERIALE numerico (DATEVALUE): va convertita in ISO "YYYY-MM-DD"
         * (gmb_dt_dtoc_iso), altrimenti vm_entry_str produce "" e ledger_parse_ymd fallisce →
         * LEDGER ADD/EDIT ritorna -1 → INTO nRet = 0 (§ DbLedgerFullTests vmc). Parità c99/C99n. */
        if (perr.type == 1) {
            gmb_string_t _ds = gmb_dt_dtoc_iso(perr.num);
            snprintf(dateb, sizeof(dateb), "%s", _ds.data ? _ds.data : "");
        } else {
            vm_entry_str(perr, dateb, (int)sizeof(dateb));
        }
        gmb_obj_release(perr.ref);
        /* handle: quello ESPLICITO del comando se numerico/valido, altrimenti il ledger
         * corrente. Ignorarlo usava gmb_db_current_ledger() (stale dopo un CLOSE di un'area
         * diversa) → LEDGER ADD ... INTO nRet valorizzava 0 (§ DbLedgerFullTests vmc). */
        int64_t lhandle = gmb_db_current_ledger();
        perr = gmb_task_pop(task); /* handle */
        if (perr.type == 1 && (int64_t)perr.num > 0) lhandle = (int64_t)perr.num;
        gmb_obj_release(perr.ref);
        {
            char accs[2048], amts[2048];
            int pa = 0, pm = 0;
            accs[0] = amts[0] = 0;
            for (i = 0; i < npost; i++) {
                if (i) { if (pa < (int)sizeof(accs)-1) accs[pa++]=','; if (pm < (int)sizeof(amts)-1) amts[pm++]=','; }
                int la = (int)strlen(accb[i]); if (pa+la < (int)sizeof(accs)-1) { memcpy(accs+pa, accb[i], (size_t)la); pa+=la; }
                int lm = (int)strlen(amtb[i]); if (pm+lm < (int)sizeof(amts)-1) { memcpy(amts+pm, amtb[i], (size_t)lm); pm+=lm; }
            }
            accs[pa]=0; amts[pm]=0;
            int64_t id = gmb_db_ledger_add_csv(lhandle, dateb, descb, accs, amts);
            push_num_entry(task, id > 0 ? (double)id : 0.0, GMB_BT_NUMBER);
        }
        return 0;
    }

    /* ---- LEDGER: EDIT (h, txid, date, desc, [account, amount]... flat) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_EDIT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
int n;
{
        int npost = (argc - 4) / 2;
        if (npost < 0) npost = 0;
        if (npost > 32) npost = 32;
        char dateb[32], descb[512];
        char accb[32][128], amtb[32][64];
        int64_t txid = 0;
        gmb_stack_entry_t perr;
        for (i = npost - 1; i >= 0; i--) {
            perr = gmb_task_pop(task); /* amount */
            if (perr.type == 1) snprintf(amtb[i], sizeof(amtb[i]), "%g", perr.num);
            else vm_entry_str(perr, amtb[i], (int)sizeof(amtb[i]));
            gmb_obj_release(perr.ref);
            perr = gmb_task_pop(task); /* account */
            vm_entry_str(perr, accb[i], (int)sizeof(accb[i]));
            gmb_obj_release(perr.ref);
        }
        perr = gmb_task_pop(task); /* desc */
        vm_entry_str(perr, descb, (int)sizeof(descb));
        gmb_obj_release(perr.ref);
        perr = gmb_task_pop(task); /* date */
        /* data SERIALE numerica → ISO (come LEDGER ADD). */
        if (perr.type == 1) {
            gmb_string_t _ds = gmb_dt_dtoc_iso(perr.num);
            snprintf(dateb, sizeof(dateb), "%s", _ds.data ? _ds.data : "");
        } else {
            vm_entry_str(perr, dateb, (int)sizeof(dateb));
        }
        gmb_obj_release(perr.ref);
        perr = gmb_task_pop(task); /* txid */
        txid = perr.type == 1 ? (int64_t)perr.num : 0;
        gmb_obj_release(perr.ref);
        /* handle esplicito se numerico/valido, altrimenti il corrente (come LEDGER ADD). */
        int64_t ehandle = gmb_db_current_ledger();
        perr = gmb_task_pop(task); /* handle */
        if (perr.type == 1 && (int64_t)perr.num > 0) ehandle = (int64_t)perr.num;
        gmb_obj_release(perr.ref);
        {
            /* Costruisci gli argomenti DIRETTAMENTE da accb/amtb (il round-trip CSV
             * accs/amts → vm_csv_split disallineava importi e conti: EDIT con importi
             * sbagliati → balance 835 vs 1720). */
            const char* ap2[32]; double am2[32];
            n = npost;
            for (i = 0; i < n; i++) { ap2[i] = accb[i]; am2[i] = atof(amtb[i]); }
            int64_t id = gmb_db_ledger_edit(ehandle, txid, dateb, descb, ap2, am2, n);
            push_num_entry(task, id > 0 ? (double)id : 0.0, GMB_BT_NUMBER);
        }
        return 0;
    }

    /* ---- LEDGER: BUDGET (h, account, amount, period) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_BUDGET(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
double d;
{
        gmb_stack_entry_t per = gmb_task_pop(task);
        gmb_stack_entry_t amt = gmb_task_pop(task);
        gmb_stack_entry_t acc = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        (void)hnd;
        char accb[128], perb[32];
        d = amt.type == 1 ? amt.num : 0;
        vm_entry_str(acc, accb, (int)sizeof(accb));
        vm_entry_str(per, perb, (int)sizeof(perb));
        rc = gmb_db_ledger_budget(gmb_db_current_ledger(), accb, d, perb);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        gmb_obj_release(per.ref); gmb_obj_release(amt.ref); gmb_obj_release(acc.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: VARIANCE (h, account, period) -> double ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_VARIANCE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t per = gmb_task_pop(task);
        gmb_stack_entry_t acc = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        (void)hnd;
        char accb[128], perb[32];
        double var = 0;
        vm_entry_str(acc, accb, (int)sizeof(accb));
        vm_entry_str(per, perb, (int)sizeof(perb));
        if (gmb_db_ledger_variance(gmb_db_current_ledger(), accb, perb, &var) != 0) var = 0;
        push_num_entry(task, var, GMB_BT_NUMBER);
        gmb_obj_release(per.ref); gmb_obj_release(acc.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: ACCOUNTS / BALANCE / REGISTER (h, query) -> JSON stringa ---- */
    return -1; /* non gestito */
}

/* Il runtime C del ledger serializza l'array ANNIDATO una volta (misurato: [["A","B"]] per gli
 * account, [[{...}]] per balance/register): si appiattisce al momento del parse, così la VM/C
 * ritorna la stessa struttura di IL/PCode (lista di stringhe / di dict).
 * § MiniTests/ledger/ledger_ops.bas. */
static void ledger_push_json_list(gmb_task_t* task, const char* js)
{
    /* API PUBBLICA del runtime condiviso (gmb_val_as_list / gmb_list_count / gmb_list_get):
     * NON si casta l'oggetto del parser JSON a `gmb_obj_shared_val_t`. Il parser produce un
     * `gmb_obj_t` con `data` = `gmb_list_t*` (`gmb_json_parse_arr` → `gmb_make_val_list` →
     * `gmb_obj_new_typed(p, "gmb_list")`), mentre `gmb_obj_shared_val_t` è il tipo del VM e
     * tiene la lista in `->list`: leggere `->list` sull'oggetto del parser significa
     * interpretare il campo `data` come un altro campo → PUNTATORE SPAZZATURA.
     * Era QUESTA la causa del segfault intermittente (lldb: `ledger_push_json_list + 88`,
     * EXC_BAD_ACCESS su 0xe004000000000000), non un doppio free: il valore di `data` cambia
     * col layout dell'heap, quindi a volte capitava di leggere un indirizzo valido → crash
     * non deterministico. */
    gmb_val_t v = gmb_json_parse(js && *js ? js : "[]");
    gmb_list_t srcList = gmb_val_as_list(v);
    /* Il runtime C può annidare l'array una volta: si usa l'interno se è l'unico elemento. */
    if (gmb_list_count(srcList) == 1)
    {
        gmb_val_t inner = gmb_list_get(srcList, 0);
        if (inner.kind == GMB_TYPE_OBJECT)
        {
            gmb_string_t tn = gmb_val_typename(inner);
            if (tn.data && strstr(tn.data, "LIST"))
            {
                gmb_list_free(&srcList);
                srcList = gmb_val_as_list(inner);
            }
        }
    }
    /* Lista nel formato che la VM si aspetta (gmb_shared_list_new: tipo del VM, con ->list).
     * Elementi COPIATI: `gmb_list_add` non prende ownership, quindi condividere i `gmb_val_t`
     * significherebbe due proprietari della stessa stringa. */
    gmb_obj_shared_val_t* outL = gmb_shared_list_new();
    if (outL)
    {
        gmb_int_t i, n = gmb_list_count(srcList);
        for (i = 0; i < n; i++)
        {
            gmb_val_t it = gmb_list_get(srcList, i);
            if (it.kind == GMB_TYPE_STRING)
                it = gmb_make_val_string(it.v_string.data ? it.v_string.data : "");
            else if (it.kind == GMB_TYPE_OBJECT) {
                /* Ogni elemento del report ledger è un OGGETTO JSON: l'IL/PCode lo espongono
                 * come STRINGA JSON (JSON.Stringify la racchiude tra virgolette). Prima →
                 * stringa VUOTA: registro/bilancio risultavano ["",""] (§ DbLedgerFullTests vmc). */
                gmb_string_t _js = gmb_val_to_json(it);
                it = gmb_make_val_string(_js.data ? _js.data : "");
            }
            gmb_list_add(outL->list, it);
        }
        push_ref_new(task, (gmb_obj_t*)outL, GMB_BT_OBJECT);
        gmb_list_free(&srcList);
        return;
    }
    gmb_list_free(&srcList);
    { gmb_obj_string_t* s0 = gmb_str_cstr("[]"); push_ref_new(task, (gmb_obj_t*)s0, GMB_BT_STRING); }
}
static int gmb_sys_GMB_SYS_LEDGER_ACCOUNTS(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t q = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        char qb[512], jbuf[4096] = "";   /* inizializzato: il fallback `ok == 0 && jbuf[0]` leggeva spazzatura quando la funzione non riempiva il buffer → gmb_json_parse su memoria non inizializzata → SEGFAULT (EXC_BAD_ACCESS in ledger_push_json_list) */
        vm_entry_str(q, qb, (int)sizeof(qb));

        /* Scarta l'handle e usava gmb_db_current_ledger() (stale dopo un CLOSE di un'altra
         * area → report vuoto). Usa l'handle ESPLICITO: uno dei due operandi è numerico. */
        int64_t lh = gmb_db_current_ledger();
        if (hnd.type == 1 && (int64_t)hnd.num > 0) lh = (int64_t)hnd.num;
        { int od = qb[0] != 0; int qi = 0; for (; qb[qi]; qi++) { if (qb[qi] < '0' || qb[qi] > '9') { od = 0; break; } }
          if (od) { if (q.type == 1) lh = (int64_t)q.num; qb[0] = 0; } }
        int ok = gmb_db_ledger_accounts(lh, qb, jbuf, (int)sizeof(jbuf));
                /* Il ledger ritorna ARRAY: IL/PCode pushano LISTE, non il testo JSON (con il testo
         * LEN() contava i caratteri: balance 49, register 194). */
        ledger_push_json_list(task, ok == 0 && jbuf[0] ? jbuf : "[]");
        gmb_obj_release(q.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: PLOT (h, query, chartType) -> dict {Labels, Amounts} ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_LEDGER_BALANCE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t q = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        char qb[512], jbuf[4096] = "";   /* inizializzato: il fallback `ok == 0 && jbuf[0]` leggeva spazzatura quando la funzione non riempiva il buffer → gmb_json_parse su memoria non inizializzata → SEGFAULT (EXC_BAD_ACCESS in ledger_push_json_list) */
        vm_entry_str(q, qb, (int)sizeof(qb));

        /* Usa l'handle ESPLICITO (uno dei due operandi è numerico), non il corrente stale. */
        int64_t lh = gmb_db_current_ledger();
        if (hnd.type == 1 && (int64_t)hnd.num > 0) lh = (int64_t)hnd.num;
        { int od = qb[0] != 0; int qi = 0; for (; qb[qi]; qi++) { if (qb[qi] < '0' || qb[qi] > '9') { od = 0; break; } }
          if (od) { if (q.type == 1) lh = (int64_t)q.num; qb[0] = 0; } }
        int ok = gmb_db_ledger_balance(lh, qb, jbuf, (int)sizeof(jbuf));
                /* Il ledger ritorna ARRAY: IL/PCode pushano LISTE, non il testo JSON (con il testo
         * LEN() contava i caratteri: balance 49, register 194). */
        ledger_push_json_list(task, ok == 0 && jbuf[0] ? jbuf : "[]");
        gmb_obj_release(q.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: PLOT (h, query, chartType) -> dict {Labels, Amounts} ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_LEDGER_REGISTER(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t q = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        char qb[512], jbuf[4096] = "";   /* inizializzato: il fallback `ok == 0 && jbuf[0]` leggeva spazzatura quando la funzione non riempiva il buffer → gmb_json_parse su memoria non inizializzata → SEGFAULT (EXC_BAD_ACCESS in ledger_push_json_list) */
        vm_entry_str(q, qb, (int)sizeof(qb));

        /* Usa l'handle ESPLICITO (uno dei due operandi è numerico), non il corrente stale. */
        int64_t lh = gmb_db_current_ledger();
        if (hnd.type == 1 && (int64_t)hnd.num > 0) lh = (int64_t)hnd.num;
        { int od = qb[0] != 0; int qi = 0; for (; qb[qi]; qi++) { if (qb[qi] < '0' || qb[qi] > '9') { od = 0; break; } }
          if (od) { if (q.type == 1) lh = (int64_t)q.num; qb[0] = 0; } }
        int ok = gmb_db_ledger_register(lh, qb, jbuf, (int)sizeof(jbuf));
                /* Il ledger ritorna ARRAY: IL/PCode pushano LISTE, non il testo JSON (con il testo
         * LEN() contava i caratteri: balance 49, register 194). */
        ledger_push_json_list(task, ok == 0 && jbuf[0] ? jbuf : "[]");
        gmb_obj_release(q.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: PLOT (h, query, chartType) -> dict {Labels, Amounts} ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_PLOT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
{
        gmb_stack_entry_t ct = gmb_task_pop(task);
        gmb_stack_entry_t q = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        (void)hnd; (void)ct;
        char qb[512];
        char labels[64][GMB_DB_LEDGER_NAME];
        double amounts[64];
        int nout = 0;
        vm_entry_str(q, qb, (int)sizeof(qb));
        /* PLOT ritorna il NUMERO di punti (IntegerType in StdLib, come IL e PCode): prima
         * pushereva un dict {Labels, Amounts} e il confronto `> 0` dava sempre falso. */
        (void)gmb_db_ledger_plot(gmb_db_current_ledger(), qb, labels, amounts, 64, &nout);
        push_num_entry(task, (double)nout, GMB_BT_NUMBER);
        gmb_obj_release(ct.ref); gmb_obj_release(q.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- LEDGER: IMPORT (h, csvPath, mapping) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_LEDGER_IMPORT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int rc;
{
        gmb_stack_entry_t map = gmb_task_pop(task);
        gmb_stack_entry_t cp = gmb_task_pop(task);
        gmb_stack_entry_t hnd = gmb_task_pop(task);
        (void)hnd;
        char mapb[1024], cpb[1024];
        vm_entry_str(map, mapb, (int)sizeof(mapb));
        vm_entry_str(cp, cpb, (int)sizeof(cpb));
        rc = gmb_db_ledger_import(gmb_db_current_ledger(), cpb, mapb);
        push_num_entry(task, (double)rc, GMB_BT_NUMBER);
        gmb_obj_release(map.ref); gmb_obj_release(cp.ref); gmb_obj_release(hnd.ref);
        return 0;
    }

    /* ---- ISAM: SELECT (area) — engine singolo-area: pop e conferma ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_SELECT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    gmb_stack_entry_t se = gmb_task_pop(task);
    char alb[128] = "";
    vm_entry_str(se, alb, (int)sizeof(alb));
    gmb_obj_release(se.ref);
    int64_t h = gmb_db_isam_select(alb);
    push_num_entry(task, h > 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sys_GMB_SYS_ISAM_JOIN_WITH(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
int i;
{
        gmb_stack_entry_t cnd = gmb_task_pop(task);                 /* condizione */
        char condb[1024];
        vm_entry_str(cnd, condb, (int)sizeof(condb));
        gmb_obj_release(cnd.ref);
        gmb_stack_entry_t dfe = gmb_task_pop(task);                 /* destFile */
        char dpath[1024];
        vm_entry_str(dfe, dpath, (int)sizeof(dpath));
        gmb_obj_release(dfe.ref);
        gmb_stack_entry_t s2 = gmb_task_pop(task);                  /* secondaryAlias */
        char s2b[128];
        vm_entry_str(s2, s2b, (int)sizeof(s2b));
        gmb_obj_release(s2.ref);
        gmb_stack_entry_t a1 = gmb_task_pop(task);                  /* primaryArea */
        char a1b[128];
        vm_entry_str(a1, a1b, (int)sizeof(a1b));
        gmb_obj_release(a1.ref);
        /* Risolvi handle: primario = current (o per alias), secondario per alias. */
        int64_t h1 = gmb_db_current_isam();
        int64_t h2 = 0;
        for (i = 0; i < vm->vm_isam_alias_count && h2 == 0; i++) {
            if (a1b[0] && strcasecmp(vm->vm_isam_alias[i], a1b) == 0) h1 = vm->vm_isam_alias_handle[i];
            if (s2b[0] && strcasecmp(vm->vm_isam_alias[i], s2b) == 0) h2 = vm->vm_isam_alias_handle[i];
        }
        /* Estrai left/right field dalla condizione "Sx.FIELD = Dx.FIELD". */
        char leftF[128] = "", rightF[128] = "";
        {
            const char* p = condb;
            const char* eq = strchr(p, '=');
            if (eq) {
                char ls[256], rs[256];
                size_t ln = (size_t)(eq - p);
                size_t rn = strlen(eq + 1);
                if (ln >= sizeof(ls)) ln = sizeof(ls) - 1;
                memcpy(ls, p, ln); ls[ln] = 0;
                if (rn >= sizeof(rs)) rn = sizeof(rs) - 1;
                memcpy(rs, eq + 1, rn); rs[rn] = 0;
                /* ultimo segmento dopo '.' (campo nudo) */
                char* d1 = strrchr(ls, '.'); if (d1) memmove(ls, d1 + 1, strlen(d1));
                char* d2 = strrchr(rs, '.'); if (d2) memmove(rs, d2 + 1, strlen(d2));
                /* trim */
                char* t1 = ls; while (*t1 && (*t1 == ' ' || *t1 == '\t')) t1++;
                char* t1e = ls + strlen(ls); while (t1e > t1 && (t1e[-1] == ' ' || t1e[-1] == '\t')) *--t1e = 0;
                char* t2 = rs; while (*t2 && (*t2 == ' ' || *t2 == '\t')) t2++;
                char* t2e = rs + strlen(rs); while (t2e > t2 && (t2e[-1] == ' ' || t2e[-1] == '\t')) *--t2e = 0;
                snprintf(leftF, sizeof(leftF), "%s", t1);
                snprintf(rightF, sizeof(rightF), "%s", t2);
            }
        }
        rc = (h1 > 0 && h2 > 0) ? gmb_db_isam_join_with(h1, h2, dpath, leftF, rightF) : GMB_DB_ERR;
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- ISAM: SET RELATION (childAlias, keyField, parentArea) — come il C#:
     * il figlio viene riposizionato via SEEK sulla chiave primaria quando il padre
     * naviga (gmb_db_isam_sync_relations). ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_SET_RELATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
{
        gmb_stack_entry_t ca3 = gmb_task_pop(task); char ca3b[128]; vm_entry_str(ca3, ca3b, (int)sizeof(ca3b)); gmb_obj_release(ca3.ref);
        gmb_stack_entry_t kf3 = gmb_task_pop(task); char kf3b[256]; vm_entry_str(kf3, kf3b, (int)sizeof(kf3b)); gmb_obj_release(kf3.ref);
        gmb_stack_entry_t pa3 = gmb_task_pop(task); char pa3b[128]; vm_entry_str(pa3, pa3b, (int)sizeof(pa3b)); gmb_obj_release(pa3.ref);
        int64_t parentR = gmb_db_current_isam();
        int64_t childR = 0;
        for (i = 0; i < vm->vm_isam_alias_count && (childR == 0 || parentR == 0); i++) {
            if (pa3b[0] && strcasecmp(vm->vm_isam_alias[i], pa3b) == 0) parentR = vm->vm_isam_alias_handle[i];
            if (ca3b[0] && strcasecmp(vm->vm_isam_alias[i], ca3b) == 0) childR = vm->vm_isam_alias_handle[i];
        }
        if (childR > 0 && parentR > 0)
            gmb_db_isam_set_relation(childR, parentR, kf3b);
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }

    /* ---- DBF: SELECT (area) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_SELECT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
{
        gmb_stack_entry_t se = gmb_task_pop(task);
        char alb2[128] = "";
        if (se.type == 1) {
            /* DBF SELECT <numero>: è l'AREA 1-based, non un alias. Risolve area → handle DBF;
             * se non c'è (area RDD) seleziona l'area RDD. Prima il numero veniva letto come
             * stringa "2" e registrato come alias → `DBF SELECT 2` non cambiava area
             * (SELECTED() restava 1, divergenza IL/PCode/C99). */
            int sa = (int)se.num;
            int64_t sh = gmb_db_dbf_find_handle_by_area(sa);
            if (sh > 0) gmb_db_set_current_dbf(sh); else (void)gmb_rdd_select(sa);
            push_num_entry(task, 0.0, GMB_BT_NUMBER);
            return 0;
        }
        if (se.ref) vm_entry_str(se, alb2, (int)sizeof(alb2));
        gmb_obj_release(se.ref);
        int64_t cur = gmb_db_current_dbf();
        if (alb2[0]) {
            /* AREA-AWARE: l'alias può essere un'area RDD (RDD USE ... ALIAS X) e non solo un
             * handle DBF. `DBF SELECT X` deve commutare ANCHE l'area RDD, altrimenti dopo
             * `DBF SELECT <alias RDD>` l'area corrente resta l'ultima (bug CustomerOrdersReport:
             * il loop annidato CUST×ORD si fermava a C1). */
            (void)gmb_rdd_select_alias(alb2);
            int f2 = 0;
            for (i = 0; i < vm->vm_dbf_alias_count; i++) {
                if (strcasecmp(vm->vm_dbf_alias[i], alb2) == 0) {
                    /* Commuta l'area corrente sull'area registrata per l'alias (parità IL
                     * `_dbfHandles`). Prima si scriveva l'handle CORRENTE sotto l'alias: un
                     * vero SELECT non cambiava area e le scritture finivano nell'ultima area
                     * aperta (bug replicazione DBF: l'APPEND dopo SELECT andava a Secondary e
                     * non veniva loggato). */
                    if (vm->vm_dbf_alias_handle[i] > 0) gmb_db_set_current_dbf(vm->vm_dbf_alias_handle[i]);
                    f2 = 1;
                    break;
                }
            }
            if (!f2 && cur > 0 && vm->vm_dbf_alias_count < 32) {
                /* Alias non ancora registrato (es. SELECT senza un USE che lo registri):
                 * registra l'area corrente — comportamento storico, solo fallback. */
                snprintf(vm->vm_dbf_alias[vm->vm_dbf_alias_count], 64, "%s", alb2);
                vm->vm_dbf_alias_handle[vm->vm_dbf_alias_count] = cur;
                (vm->vm_dbf_alias_count)++;
            }
        }
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: SORT (fieldCount, [desc, field]..., dest) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_SORT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
int i;
{
        /* Push order (VisitDbfSortStatement): dest, count, (field, desc)*, hasScope, scopeRef.
         * count sta SOTTO le coppie → non si può poppare in sequenza: si popa tutto in raw[]
         * (reinserendo in testa, come gmb_sys_GMB_SYS_SPEED_USE) e si indicizza.
         * Layout: raw[0]=dest, raw[1]=count, raw[2..1+2f]=coppie, poi hasScope, scopeRef.
         * PRIMA si poppava il count per primo (shift) → campi vuoti e SORT su file/dest
         * sbagliati; con il file sorgente in EOF il SORT produceva 0 righe e il loop
         * `DO WHILE NOT EOF` restava infinito (ClipperDbfLateBinding §6). */
        gmb_stack_entry_t e;
        gmb_stack_entry_t raw[64];
        char sdest[512] = "";
        char fieldsCsv[512] = "", descCsv[128] = "";
        int n = argc, fcnt, fd = 0, dd = 0;
        if (n < 4) n = 4;
        if (n > 64) n = 64;
        for (i = n - 1; i >= 0; i--) raw[i] = gmb_task_pop(task);
        fcnt = raw[1].type == 1 ? (int)raw[1].num : 0;
        if (fcnt < 0) fcnt = 0;
        if (fcnt > 8) fcnt = 8;
        if (2 + 2 * fcnt > n) fcnt = (n - 2) / 2;
        vm_entry_str(raw[0], sdest, (int)sizeof(sdest));
        for (i = 0; i < fcnt; i++) {
            char fn[64] = "";
            int ds;
            vm_entry_str(raw[2 + i * 2], fn, (int)sizeof(fn));
            ds = (raw[3 + i * 2].type != 2) ? ((int)raw[3 + i * 2].num != 0 ? 1 : 0) : 0;
            if (fn[0]) {
                int a1 = snprintf(fieldsCsv + fd, (int)sizeof(fieldsCsv) - fd, "%s%s", i ? "," : "", fn);
                if (a1 > 0) fd += a1;
            }
            {
                int a2 = snprintf(descCsv + dd, (int)sizeof(descCsv) - dd, "%s%d", i ? "," : "", ds ? 1 : 0);
                if (a2 > 0) dd += a2;
            }
        }
        { char _sc[1024]; _sc[0] = 0;          if (n >= 2 && raw[n - 1].type == 2 && raw[n - 1].ref)
              vm_entry_str(raw[n - 1], _sc, (int)sizeof(_sc));
          for (i = 0; i < n; i++) gmb_obj_release(raw[i].ref);
          {
            char prevf[1024]; prevf[0] = 0; int hadf = 0;
            if (gmb_db_dbf_get_filter(gmb_db_current_dbf(), prevf, (int)sizeof(prevf)) == GMB_DB_OK && prevf[0]) hadf = 1;
            if (_sc[0]) gmb_db_dbf_set_filter(gmb_db_current_dbf(), _sc);
            rc = gmb_db_dbf_sort_to_multi(gmb_db_current_dbf(), sdest, fieldsCsv, descCsv);
            if (_sc[0]) gmb_db_dbf_set_filter(gmb_db_current_dbf(), hadf ? prevf : "");
          }
        }
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: TOTAL ON (keyField, destFile, sumFields_csv, has_scope, scope_ref) —
     * cablato a gmb_db_dbf_total_on (DbfEngine_TotalOn). Il scope viene scartato. ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_DBF_TOTAL_ON(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char csv[16384];
gmb_stack_entry_t e;
int rc;
{
        gmb_stack_entry_t sr = gmb_task_pop(task);                            /* scope cond */
        char scopeCond[1024]; scopeCond[0] = 0;
        if (sr.type == 2 && sr.ref) vm_entry_str(sr, scopeCond, (int)sizeof(scopeCond));
        gmb_obj_release(sr.ref);
        gmb_stack_entry_t hs = gmb_task_pop(task); gmb_obj_release(hs.ref);   /* has_scope */
        gmb_stack_entry_t sf = gmb_task_pop(task);                            /* sumFields csv */
        char sumCsv[1024];
        vm_entry_str(sf, sumCsv, (int)sizeof(sumCsv));
        gmb_obj_release(sf.ref);
        gmb_stack_entry_t df = gmb_task_pop(task);                            /* destFile */
        char dpath[1024];
        vm_entry_str(df, dpath, (int)sizeof(dpath));
        gmb_obj_release(df.ref);
        gmb_stack_entry_t kf = gmb_task_pop(task);                            /* keyField */
        char kfld[256];
        vm_entry_str(kf, kfld, (int)sizeof(kfld));
        gmb_obj_release(kf.ref);
        rc = gmb_db_dbf_total_on_cond(gmb_db_current_dbf(), dpath, kfld, sumCsv,
                                       scopeCond[0] ? scopeCond : NULL);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: CONVERT TO ISAM (sourceDbf, destIsdb, typeName, pkField, keyLength,
     * isUnsafe, [name,type per campo], fieldCount) — crea l'ISAM con lo schema dagli
     * override dei campi e importa il DBF via gmb_db_isam_import_dbf. ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_CONVERT_DBF_TO_ISAM(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t a;
gmb_obj_t* src;
int i;
double d;
{
        gmb_stack_entry_t fc = gmb_task_pop(task);
        int fcnt = fc.type == 1 ? (int)fc.num : 0;
        gmb_obj_release(fc.ref);
        char oNames[64][64], oTypes[64][8];
        int ofc = 0;
        for (i = 0; i < fcnt && i < 64; i++) {
            gmb_stack_entry_t t1 = gmb_task_pop(task); /* typeCode */
            gmb_stack_entry_t t2 = gmb_task_pop(task); /* name */
            char tb[64], nb[64];
            vm_entry_str(t1, tb, (int)sizeof(tb));
            vm_entry_str(t2, nb, (int)sizeof(nb));
            char code = 'N';
            if      (strncasecmp(tb, "STRING", 6) == 0)  code = 'C';
            else if (strncasecmp(tb, "LONG", 4) == 0)    code = 'N';
            else if (strncasecmp(tb, "INTEGER", 7) == 0) code = 'N';
            else if (strncasecmp(tb, "DOUBLE", 6) == 0)  code = 'F';
            else if (strncasecmp(tb, "SINGLE", 6) == 0)  code = 'S';
            else if (strncasecmp(tb, "BIT", 3) == 0)     code = 'L';
            else if (strncasecmp(tb, "DATE", 4) == 0)    code = 'D';
            else if (strncasecmp(tb, "JSON", 4) == 0)    code = 'J';
            else if (strncasecmp(tb, "CURRENCY", 8) == 0) code = 'Y';
            snprintf(oTypes[ofc], 8, "%c", code);
            snprintf(oNames[ofc], 64, "%s", nb);
            ofc++;
            gmb_obj_release(t1.ref); gmb_obj_release(t2.ref);
        }
        for (i = 0; i < ofc / 2; i++) {
            char tb[64]; memcpy(tb, oNames[i], 64); memcpy(oNames[i], oNames[ofc-1-i], 64); memcpy(oNames[ofc-1-i], tb, 64);
            memcpy(tb, oTypes[i], 8); memcpy(oTypes[i], oTypes[ofc-1-i], 8); memcpy(oTypes[ofc-1-i], tb, 8);
        }
        char pkb[64] = "", tnb[128] = "";
        gmb_stack_entry_t iu = gmb_task_pop(task); gmb_obj_release(iu.ref);  /* isUnsafe */
        gmb_stack_entry_t kl = gmb_task_pop(task); gmb_obj_release(kl.ref);  /* keyLength */
        gmb_stack_entry_t pk = gmb_task_pop(task); vm_entry_str(pk, pkb, (int)sizeof(pkb)); gmb_obj_release(pk.ref);  /* pkField */
        gmb_stack_entry_t tn = gmb_task_pop(task); vm_entry_str(tn, tnb, (int)sizeof(tnb)); gmb_obj_release(tn.ref);  /* typeName */
        gmb_stack_entry_t dst = gmb_task_pop(task); char dstb[1024]; vm_entry_str(dst, dstb, (int)sizeof(dstb)); gmb_obj_release(dst.ref);
        gmb_stack_entry_t src = gmb_task_pop(task); char srcb[1024]; vm_entry_str(src, srcb, (int)sizeof(srcb)); gmb_obj_release(src.ref);
        int crc = -1;
        char defs[2048] = "";
        if (ofc > 0) {
            int pd = 0; defs[0] = 0;
            for (i = 0; i < ofc; i++) {
                int add = snprintf(defs + pd, (int)sizeof(defs) - pd, "%s%s:%c:%d",
                    i ? "," : "", oNames[i], oTypes[i][0], oTypes[i][0] == 'C' ? 32 : (oTypes[i][0] == 'D' ? 8 : 10));
                if (add > 0) pd += add;
            }
            /* Schema "<TypeName>|<IndexedBy (PK)>|<fields>" come ISAM USE / C99Native: senza il
             * PK Engine_Append falliva (irc=-1) → 0 record convertiti (§11). */
            char schema[2304];
            snprintf(schema, sizeof(schema), "%s|%s|%s", tnb, pkb, defs);
            crc = gmb_db_isam_create(dstb, schema);
        }
        int64_t ih = crc == 0 ? gmb_db_isam_open(dstb) : 0;
        gmb_db_set_current_isam(ih);
        int irc = (ih > 0) ? gmb_db_isam_import_dbf(ih, srcb) : GMB_DB_ERR;
        push_num_entry(task, irc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: UPDATE / SET_DEFAULT / SET_PATH / SET_EXCLUSIVE /
     * SET_SCHEMA / SET_DURABILITY / JOIN_WITH / SET_RELATION:
     * consuma gli argomenti (scope/coderef), nessun engine dedicato -> gap documentato. ---- */
    /* ---- DBF: SET DEFAULT PATH / SET PATH (1 arg: percorso) — default dir per gli
     * open con percorso relativo (parita' C# DbfSetDefaultPath).
     * NOTA: l'enum C# SysCall è disallineato di 11 dalle macro GMB_SYS_* C (divergenza
     * pre-esistente a ISAM_USE): i chunk generati dal Compiler C# emettono 133/134, quelli
     * di gmb_compiler_c 144/145. Accettiamo entrambi finché l'enum non viene risincronizzato. ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_SET_DEFAULT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int i;
int n;
{
        gmb_stack_entry_t pe = gmb_task_pop(task);
        char pb[1024];
        vm_entry_str(pe, pb, (int)sizeof(pb));
        gmb_obj_release(pe.ref);
        gmb_db_dbf_set_default_path(pb);
        gmb_path_set_default(pb);
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: UPDATE (keyExpr, alias, count, [field, valueExpr]* ) — itera i record e
     * valuta le espressioni di valore contro i campi del record (DbfEngine_Update).
     * Il count è pushato PRIMA delle coppie (piu' profondo): si popa tutto e si
     * ricostruisce — arr[0..n] top-first = [valueN, fieldN, ..., value1, field1,
     * count, alias, keyExpr]. ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_DBF_SET_PATH(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int i;
int n;
{
        gmb_stack_entry_t pe = gmb_task_pop(task);
        char pb[1024];
        vm_entry_str(pe, pb, (int)sizeof(pb));
        gmb_obj_release(pe.ref);
        gmb_db_dbf_set_default_path(pb);
        gmb_path_set_search(pb);
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: UPDATE (keyExpr, alias, count, [field, valueExpr]* ) — itera i record e
     * valuta le espressioni di valore contro i campi del record (DbfEngine_Update).
     * Il count è pushato PRIMA delle coppie (piu' profondo): si popa tutto e si
     * ricostruisce — arr[0..n] top-first = [valueN, fieldN, ..., value1, field1,
     * count, alias, keyExpr]. ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_UPDATE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
int i;
{
        gmb_stack_entry_t buf[64];
        int nb = argc < 64 ? argc : 64;
        for (i = 0; i < nb; i++) buf[i] = gmb_task_pop(task);
        int nrep = 0;
        char ufld[32][64], uexpr[32][64];
        char ualias[128], ukey[256];
        int un = 0;
        ualias[0] = 0; ukey[0] = 0;
        if (nb >= 3) {
            gmb_stack_entry_t cnt = buf[nb - 3];
            nrep = cnt.type == 1 ? (int)cnt.num : 0;
            int npairs = nb - 3;
            if (nrep > npairs / 2) nrep = npairs / 2;
            /* Pila (compiler): key, alias, count, [field, value]… → buf[nb-1]=key,
             * buf[nb-2]=alias (LIFO). Servono a DBF UPDATE ON <key> FROM <alias>. */
            vm_entry_str(buf[nb - 2], ualias, (int)sizeof(ualias));
            vm_entry_str(buf[nb - 1], ukey, (int)sizeof(ukey));
            /* coppie in buf[0..npairs-1]: [valueN, fieldN, ...] */
            for (i = 0; i < nrep && un < 32; i++) {
                gmb_stack_entry_t ve = buf[i * 2];      /* value (top-first) */
                gmb_stack_entry_t fe = buf[i * 2 + 1];  /* field */
                vm_entry_str(fe, ufld[un], 64);
                vm_entry_str(ve, uexpr[un], 64);
                un++;
            }
        }
        for (i = 0; i < nb; i++) gmb_obj_release(buf[i].ref);
        {
            char fcsv[2048], ecsv[2048]; int fp = 0, ep = 0; fcsv[0] = 0; ecsv[0] = 0;
            for (i = 0; i < un; i++) {
                int a1 = snprintf(fcsv + fp, (int)sizeof(fcsv) - fp, "%s%s", i ? "," : "", ufld[i]);
                int a2 = snprintf(ecsv + ep, (int)sizeof(ecsv) - ep, "%s%s", i ? "," : "", uexpr[i]);
                if (a1 > 0) fp += a1; if (a2 > 0) ep += a2;
            }
            /* DBF UPDATE ON <key> FROM <alias> REPLACE …: se l'alias risolve a un'area DIVERSA
             * dalla corrente, aggiorna per chiave dal SORGENTE (nomi nudi = destinazione,
             * `<alias>.<campo>` = sorgente). Altrimenti resta l'update semplice. */
            if (ualias[0] && ukey[0]) {
                int64_t srch = 0;
                for (i = 0; i < vm->vm_dbf_alias_count; i++)
                    if (strcasecmp(vm->vm_dbf_alias[i], ualias) == 0) { srch = vm->vm_dbf_alias_handle[i]; break; }
                if (srch > 0 && srch != gmb_db_current_dbf())
                    rc = gmb_db_dbf_update_on(gmb_db_current_dbf(), srch, ualias, ukey, fcsv, ecsv);
                else
                    rc = gmb_db_dbf_update(gmb_db_current_dbf(), fcsv, ecsv);
            } else {
                rc = gmb_db_dbf_update(gmb_db_current_dbf(), fcsv, ecsv);
            }
        }
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_DBF_JOIN_WITH(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    int rc = -1;
    int i;
    {
        /* Pila (dal C# Compiler.Dbf.cs, in ordine push): alias, destFile, condRef,
         * nFields[, fieldRefs...]. Il JOIN C ignora i campi espliciti (solo nested-loop
         * + condizione), quindi si scartano da cima senza interpretarli. */
        if (argc >= 4)
        {
            gmb_stack_entry_t nf = gmb_task_pop(task); gmb_obj_release(nf.ref);
            int nfields = argc - 4;
            for (i = 0; i < nfields; i++) { gmb_stack_entry_t fr = gmb_task_pop(task); gmb_obj_release(fr.ref); }
            gmb_stack_entry_t cj = gmb_task_pop(task); char condj[1024]; vm_entry_str(cj, condj, (int)sizeof(condj)); gmb_obj_release(cj.ref);
            gmb_stack_entry_t dj = gmb_task_pop(task); char destj[1024]; vm_entry_str(dj, destj, (int)sizeof(destj)); gmb_obj_release(dj.ref);
            gmb_stack_entry_t aj = gmb_task_pop(task); char alj[128]; vm_entry_str(aj, alj, (int)sizeof(alj)); gmb_obj_release(aj.ref);
            int64_t srcj = gmb_db_current_dbf();
            int64_t secj = 0;
            for (i = 0; i < vm->vm_dbf_alias_count && secj == 0; i++)
                if (strcasecmp(vm->vm_dbf_alias[i], alj) == 0) secj = vm->vm_dbf_alias_handle[i];
            rc = (srcj > 0 && secj > 0) ? gmb_db_dbf_join_with(srcj, secj, destj, condj) : GMB_DB_ERR;
        }
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_DBF_SET_EXCLUSIVE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    /* DBF SET EXCLUSIVE ON|OFF: i vecchi path catch-all (che ritornavano 0) sono stati
     * rimossi in favore del path condiviso gmb_db_apply_syscall. Trattato come no-op
     * con successo per non lasciare un caso non routinato; il path condiviso fa il
     * lavoro vero. */
    push_num_entry(task, -1.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sys_GMB_SYS_DBF_SET_RELATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int i;
{
        gmb_stack_entry_t ca = gmb_task_pop(task); char calb[128]; vm_entry_str(ca, calb, (int)sizeof(calb)); gmb_obj_release(ca.ref);
        gmb_stack_entry_t ke = gmb_task_pop(task); char keb[256]; vm_entry_str(ke, keb, (int)sizeof(keb)); gmb_obj_release(ke.ref);
        int64_t parentR = gmb_db_current_dbf();
        int64_t childR = 0;
        for (i = 0; i < vm->vm_dbf_alias_count && childR == 0; i++)
            if (strcasecmp(vm->vm_dbf_alias[i], calb) == 0) childR = vm->vm_dbf_alias_handle[i];
        if (childR > 0 && parentR > 0)
            gmb_db_dbf_set_relation(childR, parentR, keb);
        else if (parentR > 0 && !keb[0])
            gmb_db_dbf_set_relation(0, parentR, "");   /* SET RELATION TO senza chiave: azzera */
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: LIST / CREATE VIEW / AGGREGATE / REPORT (Cat3) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_LIST(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t cn = gmb_task_pop(task); char condb[1024]; condb[0] = 0;
        vm_entry_str(cn, condb, (int)sizeof(condb)); gmb_obj_release(cn.ref);   /* FOR/WHILE */
        gmb_stack_entry_t tp = gmb_task_pop(task); int tpv = tp.type == 1 ? (int)tp.num : 0; gmb_obj_release(tp.ref);
        gmb_stack_entry_t ds = gmb_task_pop(task); char dsb[1024]; vm_entry_str(ds, dsb, (int)sizeof(dsb)); gmb_obj_release(ds.ref);
        gmb_stack_entry_t fs = gmb_task_pop(task); char fsb[2048]; vm_entry_str(fs, fsb, (int)sizeof(fsb)); gmb_obj_release(fs.ref);
        if (condb[0]) gmb_db_dbf_set_filter(gmb_db_current_dbf(), condb);
        rc = gmb_db_dbf_list(gmb_db_current_dbf(), fsb, dsb, tpv);
        if (condb[0]) gmb_db_dbf_set_filter(gmb_db_current_dbf(), "");
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_DBF_CREATE_VIEW(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t vn = gmb_task_pop(task); char vnb[128]; vm_entry_str(vn, vnb, (int)sizeof(vnb)); gmb_obj_release(vn.ref);
        rc = gmb_db_dbf_create_view(vnb);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_DBF_AGGREGATE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t ag = gmb_task_pop(task); char agb[1024]; vm_entry_str(ag, agb, (int)sizeof(agb)); gmb_obj_release(ag.ref);
        gmb_stack_entry_t dp = gmb_task_pop(task); char dpb[1024]; vm_entry_str(dp, dpb, (int)sizeof(dpb)); gmb_obj_release(dp.ref);
        gmb_stack_entry_t kf = gmb_task_pop(task); char kfb[256]; vm_entry_str(kf, kfb, (int)sizeof(kfb)); gmb_obj_release(kf.ref);
        rc = gmb_db_dbf_aggregate(gmb_db_current_dbf(), kfb, dpb, agb);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_DBF_REPORT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t tp2 = gmb_task_pop(task); int tpv2 = tp2.type == 1 ? (int)tp2.num : 0; gmb_obj_release(tp2.ref);
        gmb_stack_entry_t ds2 = gmb_task_pop(task); char ds2b[1024]; vm_entry_str(ds2, ds2b, (int)sizeof(ds2b)); gmb_obj_release(ds2.ref);
        gmb_stack_entry_t ff = gmb_task_pop(task); char ffb[1024]; vm_entry_str(ff, ffb, (int)sizeof(ffb)); gmb_obj_release(ff.ref);
        rc = gmb_db_dbf_report(gmb_db_current_dbf(), ffb, ds2b, tpv2);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: APPENDMANY (list of dict -> N append) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_APPENDMANY(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
char fields[1024];
char csv[16384];
{
        gmb_stack_entry_t be = gmb_task_pop(task); /* batch */
        gmb_stack_entry_t ae = gmb_task_pop(task); /* alias */
        int ins = 0;
        if (be.ref && be.ref->kind == GMB_OBJ_LIST) {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)be.ref;
            if (sv->list && gmb_db_dbf_fields(gmb_db_current_dbf(), fields, (int)sizeof(fields)) == 0) {
                for (size_t k = 0; k < sv->list->count; k++) {
                    gmb_val_t item = sv->list->items[k];
                    if (item.kind == GMB_TYPE_OBJECT && item.v_ptr
                        && vm_dict_to_csv(vm, (gmb_obj_t*)item.v_ptr, fields, csv, (int)sizeof(csv)) == 0
                        && gmb_db_dbf_append(gmb_db_current_dbf(), csv) == 0)
                        ins++;
                }
            }
        }
        push_num_entry(task, (double)ins, GMB_BT_NUMBER);
        gmb_obj_release(be.ref); gmb_obj_release(ae.ref);
        return 0;
    }

    /* ---- DBF: CLOSE DATABASES ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_CLOSE_DATABASES(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
        /* Parità IL: `DBF CLOSE` (statement) chiude TUTTE le aree aperte (DBF_CLOSE_DATABASES),
         * non solo la corrente. Chiudere solo la corrente lasciava aperte le altre aree e un
         * successivo SELECT commutava su un'area che il riferimento aveva già chiuso. */
        gmb_db_dbf_close_all();
        /* Anche le aree RDD (RDD USE): `DBF CLOSE` libera TUTTE le aree → il prossimo USE le
         * riusa (parità IL). Senza, le aree cell-store si accumulavano e GETFIELD(areaN,..) con
         * area fissa leggeva l'area sbagliata (TextDriversDemo: DELIM leggeva il CSV). */
        gmb_rdd_close_all();
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    /* ---- DBF: APPEND FROM (file[, hasScope, condRef]) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_APPEND_FROM(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        char sfb[1024] = "";
        char afCond[1024]; afCond[0] = 0;
        if (argc >= 3) {
            gmb_stack_entry_t a1 = gmb_task_pop(task);   /* condition (testo) */
            if (a1.type == 2 && a1.ref) vm_entry_str(a1, afCond, (int)sizeof(afCond));
            gmb_obj_release(a1.ref);
            gmb_stack_entry_t a2 = gmb_task_pop(task); gmb_obj_release(a2.ref);   /* hasScope */
        }
        gmb_stack_entry_t sf2 = gmb_task_pop(task);
        vm_entry_str(sf2, sfb, (int)sizeof(sfb));
        gmb_obj_release(sf2.ref);
        rc = gmb_db_dbf_append_from_cond(gmb_db_current_dbf(), sfb, afCond[0] ? afCond : NULL);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: COPY STRUCTURE (dest) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_COPY_STRUCTURE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t de = gmb_task_pop(task);
        char db2[1024];
        vm_entry_str(de, db2, (int)sizeof(db2));
        gmb_obj_release(de.ref);
        rc = gmb_db_dbf_copy_structure(gmb_db_current_dbf(), db2);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: INDEX ON (keyExpr, file, codeRef) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_INDEX_ON(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t fn2 = gmb_task_pop(task);
        gmb_stack_entry_t ke = gmb_task_pop(task);
        gmb_stack_entry_t cr = gmb_task_pop(task);
        char fnb[1024], keb[512];
        vm_entry_str(fn2, fnb, (int)sizeof(fnb));
        vm_entry_str(ke, keb, (int)sizeof(keb));
        /* Il compiler pusha il testo dell'espressione (es. "Region#-1" col suffisso
         * SymbolId): il C# p-code lo stripa alla '#'. Per un indice COMPOSTO
         * ("Citta#12 + Nome#13") troncare al PRIMO '#' lasciava "Citta": l'indice
         * composto non veniva creato e SET ORDER/SEEK fallivano (ClipperDbfLateBinding,
         * gamba VM/C: secondo elenco in ordine naturale invece che per Citta+Nome).
         * Si rimuovono TUTTI i suffissi #<SymbolId> mantenendo l'espressione. */
        { char* s = keb; char* w = keb;
          while (*s) {
              if (*s == '#') {
                  s++;
                  if (*s == '-') s++;
                  while (*s >= '0' && *s <= '9') s++;
              } else *w++ = *s++;
          }
          *w = 0; }
        gmb_obj_release(fn2.ref); gmb_obj_release(ke.ref); gmb_obj_release(cr.ref);
        rc = gmb_db_dbf_index_on(gmb_db_current_dbf(), keb, fnb, 0);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: REPLICATION (SET/APPLY/FOLLOW) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_SET_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        int rrc;
        rrc = gmb_db_dbf_set_replication(gmb_db_current_dbf(), r1b);
        /* SETREPLICATION ritorna il PATH del log (StringType in StdLib, come IlRuntime.
         * DbfSetReplication e il ramo PCode). Prima pushava 0/-1 (numero): il chiamante,
         * che si aspetta una stringa, stampava vuoto (§ MiniTests/dbf/dbf_replication.bas:
         * PCode "setrepl: /", VMC "setrepl:").
         * Il path ritornato deve essere quello RISOLTO dal motore (`"."`/`"DEFAULT"` →
         * `<percorso tabella>.repl`), non la stringa in ingresso: con
         * `DBF.SETREPLICATION(Primary, ".")` la VM/C stampava `Replication log: .` mentre IL
         * stampava il path completo (misurato su DbfReplication.bas). */
        (void)rrc;
        {
            gmb_string_t resolved = gmb_db_dbf_set_replication_str(gmb_db_current_dbf(), r1b);
            gmb_obj_string_t* rs = gmb_str_cstr((resolved.data && resolved.data[0]) ? resolved.data : "");
            push_ref_new(task, (gmb_obj_t*)rs, GMB_BT_STRING);
        }
        return 0;
    }
    /* ---- DBF: LUPDATE (data ultimo aggiornamento -> stringa) ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_DBF_APPLY_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        int rrc;
        rrc = gmb_db_dbf_apply_replication(gmb_db_current_dbf(), r1b);
        /* Il runtime ritorna il NUMERO di operazioni applicate (>= 0), 0/negativo su errore —
         * esattamente come il riferimento (`IlRuntime.DbfApplyReplication` → conteggio,
         * `DatabaseService.DbfApplyReplication` → 0 nel catch). Qui si trattava `rrc != 0`
         * come ERRORE, quindi con 5 operazioni applicate la VM/C stampava `-1` (misurato su
         * DbfReplication.bas e sul probe /tmp/probe_repl2.bas: IL `applied: 2`, VM/C `-1`). */
        push_num_entry(task, rrc > 0 ? (double)rrc : 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- DBF: LUPDATE (data ultimo aggiornamento -> stringa) ---- */
    return -1; /* non gestito */
}
static int gmb_sys_GMB_SYS_DBF_FOLLOW_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        int rrc;
        rrc = gmb_db_dbf_follow_replication(gmb_db_current_dbf(), r1b, r2b[0] ? "checkpoint" : NULL);
        /* conteggio, non bit (vedi APPLYREPLICATION) */
        push_num_entry(task, rrc > 0 ? (double)rrc : 0.0, GMB_BT_NUMBER);;
        return 0;
    }
    /* ---- DBF: LUPDATE (data ultimo aggiornamento -> stringa) ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_INTEGRITY(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        /* ISAM.INTEGRITY(alias) → JSON {"valid":...,"message":...} (StringType/JsonType).
         * Il path syscall generico veicola solo un numero (*result), quindi il risultato
         * arrivava 0 al chiamante (§ MiniTests/dbf/isam_replication.bas: VMC "integrity: 0").
         * Il codice esce dal path "known" (gmb_db_common.c) e passa da qui. */
        (void)vm; (void)code; (void)argc;
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        gmb_obj_release(r1.ref);
        gmb_string_t js = gmb_db_isam_integrity_str(gmb_db_current_isam());
        gmb_obj_string_t* s = gmb_str_cstr(js.data ? js.data : "{}");
        if (js.data) free(js.data);
        push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        return 0;
    }
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_SET_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        /* ISAM.SETREPLICATION(alias, logPath): il path è l'ULTIMO argomento (in cima allo
         * stack → primo pop). Ritorna il path come StringType, come il ramo DBF e IL/PCode. */
        (void)vm; (void)code;
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        int rrc = gmb_db_isam_set_replication(gmb_db_current_isam(), r1b);
        gmb_obj_string_t* rs = gmb_str_cstr(rrc == 0 ? r1b : "");
        push_ref_new(task, (gmb_obj_t*)rs, GMB_BT_STRING);
        return 0;
    }
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_APPLY_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        (void)vm; (void)code;
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        push_num_entry(task, (double)gmb_db_isam_apply_replication(gmb_db_current_isam(), r1b), GMB_BT_NUMBER);
        return 0;
    }
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_ISAM_FOLLOW_REPLICATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        (void)vm; (void)code;
        gmb_stack_entry_t r1 = gmb_task_pop(task);
        char r1b[1024]; vm_entry_str(r1, r1b, (int)sizeof(r1b));
        gmb_obj_release(r1.ref);
        int r2b[1] = {0};
        if (argc >= 2) { gmb_stack_entry_t r2 = gmb_task_pop(task); if (r2.type == 1) r2b[0] = (int)r2.num; gmb_obj_release(r2.ref); }
        push_num_entry(task, (double)gmb_db_isam_follow_replication(gmb_db_current_isam(), r1b, NULL), GMB_BT_NUMBER);
        return 0;
    }
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_DBF_LUPDATE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t le = gmb_task_pop(task);
        gmb_obj_release(le.ref);
        char lbuf[32];
        if (gmb_db_dbf_lupdate(gmb_db_current_dbf(), lbuf, (int)sizeof(lbuf)) == 0) {
            gmb_obj_string_t* s = gmb_str_cstr(lbuf);
            push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        } else {
            gmb_obj_string_t* s = gmb_str_cstr("");
            push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        }
        return 0;
    }

    /* ---- SPEED: CREATE/DROP INDEX, DELETE, SET ORDER ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_CREATE_INDEX(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t kl = gmb_task_pop(task);
        gmb_stack_entry_t ke2 = gmb_task_pop(task);
        gmb_stack_entry_t nm = gmb_task_pop(task);
        gmb_stack_entry_t tb = gmb_task_pop(task);
        char keb[512], nmb[128];
        int klen = kl.type == 1 ? (int)kl.num : 0;
        vm_entry_str(ke2, keb, (int)sizeof(keb));
        vm_entry_str(nm, nmb, (int)sizeof(nmb));
        int tid2 = tb.type == 1 ? (int)tb.num : 0;
        gmb_obj_release(kl.ref); gmb_obj_release(ke2.ref); gmb_obj_release(nm.ref); gmb_obj_release(tb.ref);
        rc = gmb_db_speed_create_index(gmb_db_current_speed(), tid2, nmb, keb, klen);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_DROP_INDEX(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t nm2 = gmb_task_pop(task);
        gmb_stack_entry_t tb2 = gmb_task_pop(task);
        char nmb2[128];
        vm_entry_str(nm2, nmb2, (int)sizeof(nmb2));
        int tid3 = tb2.type == 1 ? (int)tb2.num : 0;
        gmb_obj_release(nm2.ref); gmb_obj_release(tb2.ref);
        rc = gmb_db_speed_drop_index(gmb_db_current_speed(), tid3, nmb2);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_SET_ORDER(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
int rc;
{
        gmb_stack_entry_t nm3 = gmb_task_pop(task);
        gmb_stack_entry_t tb3 = gmb_task_pop(task);
        char nmb3[128];
        vm_entry_str(nm3, nmb3, (int)sizeof(nmb3));
        int tid4 = tb3.type == 1 ? (int)tb3.num : 0;
        gmb_obj_release(nm3.ref); gmb_obj_release(tb3.ref);
        rc = gmb_db_speed_set_order(gmb_db_current_speed(), tid4, nmb3);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_DELETE_RECORD(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
int rc;
double d;
{
        gmb_stack_entry_t kd = gmb_task_pop(task);
        gmb_stack_entry_t td = gmb_task_pop(task);
        /* td è l'ALIAS (stringa), non un tableId numerico (VisitSpeedDeleteStatement:
         * EmitFileNumber(stmt.Alias) prima del Source) — risolvilo come PUT/GET/SEEK. */
        char tdalias[128] = ""; vm_entry_str(td, tdalias, (int)sizeof(tdalias));
        int tdid = tdalias[0] ? gmb_db_speed_table_by_alias(tdalias) : 0;
        if (tdid < 0) tdid = 0;
        /* Chiave sortable binaria, parità con la codifica scritta dal PUT (vedi sopra) —
         * una chiave numerica testuale "%g" non avrebbe mai trovato il record. Il Source
         * (VisitExpression(stmt.Source) in VisitSpeedDeleteStatement) può essere sia uno
         * scalare (chiave grezza) sia un intero record/istanza UDT (stesso caso di PUT,
         * es. "SPEED DELETE Clienti, clienteRec") — in quel caso va normalizzato con lo
         * stesso registro campi e la stessa risoluzione __key__/pkField di PUT, non
         * trattato come se fosse già la chiave. */
        uint8_t kdb[64]; int kdlen = 0;
        if (kd.type == 2 && kd.ref && kd.ref->kind == GMB_OBJ_DICT) {
            gmb_dict_t* d = ((gmb_obj_shared_val_t*)kd.ref)->dict;
            gmb_dict_t named = gmb_dict_new();
            int slot = vm_speed_field_slot(vm, (int64_t)tdid);
            int fcount = slot >= 0 ? vm->vm_speed_field_count[slot] : 0;
            for (size_t di = 0; di < d->count; di++) {
                const char* key = d->items[di].key;
                if (!key) continue;
                const char* outKey = key;
                if (key[0] == '_' && key[1] == '_') {
                    if (key[2] == 'm' && key[3] >= '0' && key[3] <= '9') {
                        int sid = atoi(key + 3);
                        int found = 0;
                        for (int fi = 0; fi < fcount; fi++)
                            if (vm->vm_speed_field_sids[slot][fi] == sid) { outKey = vm->vm_speed_field_names[slot][fi]; found = 1; break; }
                        if (!found) continue;
                    } else continue;
                }
                gmb_dict_set(&named, outKey, d->items[di].value);
            }
            const char* pkName = "ID";
            if (slot >= 0 && vm->vm_speed_field_pk[slot][0]) pkName = vm->vm_speed_field_pk[slot];
            gmb_val_t kv = vm_dict_get_ci(d, "__key__");
            if (kv.kind == GMB_TYPE_NIL) kv = vm_dict_get_ci(&named, pkName);
            if (kv.kind == GMB_TYPE_INT || kv.kind == GMB_TYPE_DOUBLE) {
                gmb_speed_sortable_double_bytes(gmb_val_to_double(kv), kdb);
                kdlen = 8;
            } else if (kv.kind == GMB_TYPE_STRING && kv.v_string.data) {
                kdlen = (int)(kv.v_string.length < sizeof(kdb) ? kv.v_string.length : sizeof(kdb));
                memcpy(kdb, kv.v_string.data, (size_t)kdlen);
            }
            gmb_dict_free(&named);
        } else if (kd.type == 1) {
            gmb_speed_sortable_double_bytes(kd.num, kdb); kdlen = 8;
        } else {
            char kdstr[64];
            vm_entry_str(kd, kdstr, (int)sizeof(kdstr));
            kdlen = (int)strlen(kdstr);
            memcpy(kdb, kdstr, (size_t)kdlen);
        }
        gmb_obj_release(kd.ref); gmb_obj_release(td.ref);
        rc = gmb_db_speed_delete(gmb_db_current_speed(), tdid, kdb, kdlen);
        push_num_entry(task, rc == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- SPEED: SET RELATION (parentAlias, keyExpr, childAlias) — il figlio viene
     * riposizionato sulla chiave corrente del padre (tabella nello stesso Database).
     * ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_SET_RELATION(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
{
        gmb_stack_entry_t ca4 = gmb_task_pop(task); char ca4b[128]; vm_entry_str(ca4, ca4b, (int)sizeof(ca4b)); gmb_obj_release(ca4.ref);
        gmb_stack_entry_t ke4 = gmb_task_pop(task); gmb_obj_release(ke4.ref); /* keyExpr non usato (key = chiave tabella) */
        gmb_stack_entry_t pa4 = gmb_task_pop(task); char pa4b[128]; vm_entry_str(pa4, pa4b, (int)sizeof(pa4b)); gmb_obj_release(pa4.ref);
        int64_t cur = gmb_db_current_speed();
        if (cur > 0) {
            int childT = gmb_db_speed_table_by_alias(ca4b);
            int parentT = gmb_db_speed_table_by_alias(pa4b);
            if (childT >= 0 && parentT >= 0)
                gmb_db_speed_set_relation_table(cur, parentT, childT);
        }
        push_num_entry(task, 0.0, GMB_BT_NUMBER);
        return 0;
    }
    /* ---- SPEED: SET FILTER / CREATE VIEW / REFRESH VIEW / DROP VIEW / SETMANY —
     * consuma e gap documentato. ---- */
    /* ---- SPEED: CREATE VIEW (viewName, typeSymbolId, typeName, selectStmt, queryExpr)
     * — registra la vista sul table corrente (snapshot). ---- */
    return -1; /* non gestito */
}

static int gmb_sys_GMB_SYS_SPEED_CREATE_VIEW(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t qe = gmb_task_pop(task); gmb_obj_release(qe.ref);
        gmb_stack_entry_t ss = gmb_task_pop(task); gmb_obj_release(ss.ref);
        gmb_stack_entry_t tn = gmb_task_pop(task); gmb_obj_release(tn.ref);
        gmb_stack_entry_t ts = gmb_task_pop(task); gmb_obj_release(ts.ref);
        gmb_stack_entry_t vn = gmb_task_pop(task); char vnb[128]; vm_entry_str(vn, vnb, (int)sizeof(vnb)); gmb_obj_release(vn.ref);
        int64_t cur = gmb_db_current_speed();
        int rc5 = (cur > 0 && vnb[0]) ? gmb_db_speed_create_view(vnb, (int)gmb_db_speed_curtable(cur)) : GMB_DB_ERR;
        push_num_entry(task, rc5 == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_REFRESH_VIEW(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t vn2 = gmb_task_pop(task); char vn2b[128]; vm_entry_str(vn2, vn2b, (int)sizeof(vn2b)); gmb_obj_release(vn2.ref);
        push_num_entry(task, gmb_db_speed_refresh_view(vn2b) == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_DROP_VIEW(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t vn3 = gmb_task_pop(task); char vn3b[128]; vm_entry_str(vn3, vn3b, (int)sizeof(vn3b)); gmb_obj_release(vn3.ref);
        push_num_entry(task, gmb_db_speed_drop_view(vn3b) == 0 ? 0.0 : -1.0, GMB_BT_NUMBER);
        return 0;
    }
}

static int gmb_sys_GMB_SYS_SPEED_SET_FILTER(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    /* Stack: condition(top), hasCondition, alias(bottom) */
    gmb_stack_entry_t ce = gmb_task_pop(task);
    char condb[1024] = "";
    if (ce.type == 2 && ce.ref) vm_entry_str(ce, condb, (int)sizeof(condb));
    gmb_obj_release(ce.ref);
    /* hasCondition: flag booleano, scartato */
    gmb_stack_entry_t hc = gmb_task_pop(task); gmb_obj_release(hc.ref);
    /* alias */
    char aliasb[128] = "";
    if (argc >= 3) {
        gmb_stack_entry_t ae = gmb_task_pop(task);
        if (ae.type == 2 && ae.ref) vm_entry_str(ae, aliasb, (int)sizeof(aliasb));
        gmb_obj_release(ae.ref);
    }
    int64_t h = gmb_db_current_speed();
    int rc = GMB_DB_ERR;
    if (h > 0) {
        /* Se l'alias è specificato ≠ alias corrente, trova il tableId */
        if (aliasb[0]) {
            int tid = gmb_db_speed_table_by_alias(aliasb);
            if (tid > 0) {
                gmb_db_speed_set_filter(h, condb);
                /* riposiziona per applicare il filtro subito (stile DBF/ISAM) */
                gmb_db_speed_go_top(h, tid);
            }
        } else {
            gmb_db_speed_set_filter(h, condb);
        }
        rc = GMB_DB_OK;
    }
    push_num_entry(task, rc == GMB_DB_OK ? 0.0 : -1.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sys_GMB_SYS_SPEED_SETMANY(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)code; (void)argc;
    /* Stack (compilatore C#): [alias, records] → pop records (lista) poi alias.
     * La lista è un gmb_obj_shared_val_t del VM con ->list; ogni elemento è un'istanza
     * UDT (dict con chiavi "__m<sid>") → si riusa vm_speed_set_record (stessa logica del PUT).
     * Ritorna il numero di record inseriti. */
    gmb_stack_entry_t rec = gmb_task_pop(task);
    gmb_stack_entry_t al = gmb_task_pop(task);
    char aliasb[128] = "";
    vm_entry_str(al, aliasb, (int)sizeof(aliasb));
    int tableId = aliasb[0] ? gmb_db_speed_table_by_alias(aliasb) : 0;
    if (tableId < 0) tableId = 0;
    int n = 0;
    if (rec.ref) {
        gmb_list_t* lp = ((gmb_obj_shared_val_t*)rec.ref)->list;
        if (lp) {
            gmb_int_t cnt = gmb_list_count(*lp);
            for (gmb_int_t i = 0; i < cnt; i++) {
                gmb_val_t v = gmb_list_get(*lp, i);
                if (v.kind != GMB_TYPE_OBJECT) continue;
                /* gmb_val_as_dict gestisce ENTRAMBE le rappresentazioni di object
                 * (gmb_obj_shared_val_t del VM e gmb_obj_t del parser JSON) senza
                 * castare a mano un layout: un cast diretto dava SIGSEGV. */
                gmb_dict_t d = gmb_val_as_dict(v);
                if (vm_speed_set_record(vm, tableId, &d) == 0) n++;
            }
        }
    }
    gmb_obj_release(rec.ref); gmb_obj_release(al.ref);
    push_num_entry(task, (double)n, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sys_GMB_SYS_SPEED_CLOSE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    /* Stack (compilatore C#): [alias] — alias vuoto/"" = area corrente o tutte.
     * SPEED condivide UN handle di processo (gmb_db_current_speed): si chiude quello. */
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
    (void)gmb_db_speed_close(gmb_db_current_speed());
    push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sys_GMB_SYS_LEDGER_GET(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
{
        gmb_stack_entry_t tx = gmb_task_pop(task);
        gmb_stack_entry_t hd = gmb_task_pop(task);
        int64_t txid = tx.type == 1 ? (int64_t)tx.num : 0;
        int64_t gh = (hd.type == 1 && (int64_t)hd.num > 0) ? (int64_t)hd.num : gmb_db_current_ledger();
        gmb_obj_release(tx.ref); gmb_obj_release(hd.ref);
        /* Parità IL (object?[]): GET ritorna la LISTA dei movimenti della transazione,
         * non un singolo oggetto JSON (che dava LEN = lunghezza stringa). */
        gmb_val_t gv = gmb_db_ledger_get_val(gh, txid);
        gmb_list_t src = gmb_val_as_list(gv);
        gmb_obj_shared_val_t* l = gmb_shared_list_new();
        if (l) {
            /* Ogni elemento è una STRINGA JSON (il C# LedgerGet → string[] di LedgerEntryJson),
             * così JSON.Stringify la serializza con le virgolette escapate (\u0022). Prima era
             * un DICT → JSON.Stringify rendeva l'oggetto grezzo (no escape), divergenza vmc. */
            for (size_t i = 0; i < src.count; i++) {
                gmb_string_t _js = gmb_val_to_json(gmb_list_get(src, i));
                gmb_list_add(l->list, gmb_make_val_string_counted(_js.data ? _js.data : ""));
            }
            push_ref_new(task, (gmb_obj_t*)l, GMB_BT_OBJECT);
        } else {
            gmb_obj_string_t* s = gmb_str_cstr("{}");
            push_ref_new(task, (gmb_obj_t*)s, GMB_BT_STRING);
        }
        return 0;
    }

}


/* Escape di una stringa in un letterale JSON (per il piano SPEED). */
static void gmb_sq_json_esc(char* dst, size_t cap, const char* s)
{
    size_t n = 0;
    if (cap == 0) return;
    dst[n++] = '"';
    for (; s && *s && n + 3 < cap; s++) {
        if (*s == '"' || *s == '\\') { dst[n++] = '\\'; dst[n++] = *s; }
        else if (*s == '\n') { dst[n++] = '\\'; dst[n++] = 'n'; }
        else if (*s == '\r') { dst[n++] = '\\'; dst[n++] = 'r'; }
        else dst[n++] = *s;
    }
    dst[n++] = '"'; dst[n] = 0;
}

/* SPEED SELECT ... INTO <array UDT> — esegue la query sul motore condiviso
 * gmb_db_speed_select (native/gmb_db_c/src/speed/gmb_speed_query.c) e restituisce una lista
 * di istanze UDT (dict con chiavi "__m<sid>", come SPEED_GET) per il TYPE di destinazione
 * (`OF <Tipo>`): i symbolId arrivano dal compilatore (VisitSpeedSelectStatement), perché il
 * runtime C non ha la SymbolTable. Pop order LIFO, speculare al push del compilatore:
 * SelectCount, [alias,expr]*, OrderCount, [desc,expr]*, Having, GroupCount, [expr]*,
 * Where, JoinRight, JoinLeft, JoinAlias, FromAlias, DestCount, [sid,name]*. */
static int gmb_sys_GMB_SYS_SPEED_SELECT(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char selE[32][256], selA[32][128]; int selN = 0;
    char ordE[32][256]; int ordD[32]; int ordN = 0;
    char having[512] = "";
    char grpE[32][256]; int grpN = 0;
    char where[512] = "";
    char joinRight[128] = "", joinLeft[256] = "", joinAlias[128] = "", fromAlias[128] = "";
    char destName[32][64]; int destSid[32]; int destN = 0;
    int i;
    char plan[16384]; size_t pl = 0;
    gmb_string_t out;
    gmb_val_t arr;
    gmb_list_t rows;
    gmb_obj_shared_val_t* list;

    { gmb_stack_entry_t e = gmb_task_pop(task); selN = e.type == 1 ? (int)e.num : 0; gmb_obj_release(e.ref); }
    if (selN > 32) selN = 32;
    for (i = selN - 1; i >= 0; i--) {
        gmb_stack_entry_t a = gmb_task_pop(task); /* alias */
        gmb_stack_entry_t x = gmb_task_pop(task); /* expr */
        vm_entry_str(a, selA[i], (int)sizeof(selA[i]));
        vm_entry_str(x, selE[i], (int)sizeof(selE[i]));
        gmb_obj_release(a.ref); gmb_obj_release(x.ref);
    }
    { gmb_stack_entry_t e = gmb_task_pop(task); ordN = e.type == 1 ? (int)e.num : 0; gmb_obj_release(e.ref); }
    if (ordN > 32) ordN = 32;
    for (i = ordN - 1; i >= 0; i--) {
        gmb_stack_entry_t d = gmb_task_pop(task); /* desc */
        gmb_stack_entry_t x = gmb_task_pop(task); /* expr */
        ordD[i] = (d.type == 1 && d.num != 0) ? 1 : 0;
        vm_entry_str(x, ordE[i], (int)sizeof(ordE[i]));
        gmb_obj_release(d.ref); gmb_obj_release(x.ref);
    }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, having, (int)sizeof(having)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); grpN = e.type == 1 ? (int)e.num : 0; gmb_obj_release(e.ref); }
    if (grpN > 32) grpN = 32;
    for (i = grpN - 1; i >= 0; i--) {
        gmb_stack_entry_t x = gmb_task_pop(task);
        vm_entry_str(x, grpE[i], (int)sizeof(grpE[i]));
        gmb_obj_release(x.ref);
    }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, where, (int)sizeof(where)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, joinRight, (int)sizeof(joinRight)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, joinLeft, (int)sizeof(joinLeft)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, joinAlias, (int)sizeof(joinAlias)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); vm_entry_str(e, fromAlias, (int)sizeof(fromAlias)); gmb_obj_release(e.ref); }
    { gmb_stack_entry_t e = gmb_task_pop(task); destN = e.type == 1 ? (int)e.num : 0; gmb_obj_release(e.ref); }
    if (destN > 32) destN = 32;
    for (i = destN - 1; i >= 0; i--) {
        gmb_stack_entry_t s = gmb_task_pop(task); /* sid */
        gmb_stack_entry_t n = gmb_task_pop(task); /* name */
        destSid[i] = s.type == 1 ? (int)s.num : 0;
        vm_entry_str(n, destName[i], (int)sizeof(destName[i]));
        gmb_obj_release(s.ref); gmb_obj_release(n.ref);
    }
    if (!fromAlias[0]) { push_nil(task); return 0; }

    /* Piano JSON (stesso formato di gmb_db_speed_select). */
    pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, "{\"from\":");
    gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, fromAlias); pl = strlen(plan);
    if (joinAlias[0]) {
        /* Il compilatore scompone la condizione in (chiave sinistra, campo destro nudo):
         * il motore vuole la condizione completa "Sx = Dx" (sq_decompose_join la ri-scompone). */
        char jc[512];
        snprintf(jc, sizeof(jc), "%s = %s", joinLeft, joinRight);
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"join\":");
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, joinAlias); pl = strlen(plan);
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"joinCond\":");
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, jc); pl = strlen(plan);
    }
    if (where[0]) {
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"where\":");
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, where); pl = strlen(plan);
    }
    if (grpN > 0) {
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"groupBy\":[");
        for (i = 0; i < grpN; i++) {
            if (i) plan[pl++] = ',';
            gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, grpE[i]); pl = strlen(plan);
        }
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, "]");
    }
    if (having[0]) {
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"having\":");
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, having); pl = strlen(plan);
    }
    if (ordN > 0) {
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"orderBy\":[");
        for (i = 0; i < ordN; i++) {
            char otmp[260];
            if (i) plan[pl++] = ',';
            snprintf(otmp, sizeof(otmp), "%s%s", ordD[i] ? "~" : "", ordE[i]);
            gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, otmp); pl = strlen(plan);
        }
        pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, "]");
    }
    pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, ",\"star\":%s,\"selE\":[",
                           (selN == 1 && strcmp(selE[0], "*") == 0) ? "true" : "false");
    for (i = 0; i < selN; i++) {
        if (i) plan[pl++] = ',';
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, selE[i]); pl = strlen(plan);
    }
    pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, "],\"selA\":[");
    for (i = 0; i < selN; i++) {
        if (i) plan[pl++] = ',';
        gmb_sq_json_esc(plan + pl, sizeof(plan) - pl, selA[i]); pl = strlen(plan);
    }
    pl += (size_t)snprintf(plan + pl, sizeof(plan) - pl, "]}");

    out = gmb_string_alloc("");
    (void)gmb_db_speed_select(gmb_db_current_speed(), plan, &out);
    arr = gmb_json_parse(out.data ? out.data : "[]");
    rows = gmb_val_as_list(arr);
    list = gmb_shared_list_new();
    if (list) {
        gmb_int_t rn = gmb_list_count(rows);
        for (gmb_int_t ri = 0; ri < rn; ri++) {
            gmb_val_t rv = gmb_list_get(rows, ri);
            gmb_obj_shared_val_t* rec;
            gmb_dict_t rd;
            if (rv.kind != GMB_TYPE_OBJECT) continue;
            rec = gmb_shared_dict_new();
            if (!rec) continue;
            rd = gmb_val_as_dict(rv);
            for (i = 0; i < destN; i++) {
                gmb_val_t v = gmb_dict_get(rd, destName[i]);
                if (v.kind != GMB_TYPE_NIL) {
                    char mk[32];
                    snprintf(mk, sizeof(mk), "__m%d", destSid[i]);
                    gmb_dict_set(rec->dict, mk, v);
                }
            }
            gmb_list_add(list->list, gmb_make_val_obj((gmb_obj_t*)rec));
        }
    }
    gmb_string_free(&out);
    if (list) push_ref_entry(task, (gmb_obj_t*)list, GMB_BT_OBJECT);
    else push_nil(task);
    return 0;
}

/* Tabella GMB_IMPL: {label GMB_SYS_*, handler}. Il valore numerico è il #define
 * di gmb_db.h; il lookup per codice. Conteggio: sizeof(gmb_sys_impl)/sizeof. */
typedef struct { int code; int (*handler)(gmb_vm_t*, gmb_task_t*, int, int); } gmb_sys_impl_t;
/* Alias dell'ultima area DBF aperta nella VM (ALIAS()). La VM/C non ha un registro delle
 * aree per nome: il handler DBF USE lo popola. */
static char g_vm_dbf_current_alias[64] = "";
const char* gmb_vm_dbf_current_alias(void) { return g_vm_dbf_current_alias; }
void gmb_vm_dbf_set_alias_n(const char* s, int n) {
    int k = (n > 0 && n < (int)sizeof(g_vm_dbf_current_alias) - 1) ? n : 0;
    if (k > 0) { memcpy(g_vm_dbf_current_alias, s, (size_t)k); g_vm_dbf_current_alias[k] = 0; }
    else g_vm_dbf_current_alias[0] = 0;
}




/* ---- DBF: USE (path, locking, isUnsafe, indexPaths…, alias, typeId, typeName, campi…) ----
 * Semantica: apre il file; se manca e c'è un TYPE (`USE ... AS Tipo`), lo CREA dalla
 * struttura del TYPE — comportamento documentato (late-binding/Clipper), parità IL/C99
 * (`DbfCreateIfMissing` / `_dh = gmb_db_dbf_create(...)`). Prima il path generico
 * (gmb_db_apply_syscall_known) faceva solo open: con il file assente g_last_dbf=0 e TUTTI
 * i comandi area-correnti (LASTREC/RECNO/EOF/SEEK/GET/APPEND) diventavano no-op silenziosi
 * (osservato: ClipperDbfLateBinding, gamba VM/C con DBF.SELECTED()=0). Ora il codice passa
 * da qui e la VM riceve anche i metadati di campo (nome,len,isString,typeCode). */
static int gmb_sys_GMB_SYS_DBF_USE(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    gmb_stack_entry_t raw[512];
    int n = argc, i;
    char pathb[1024] = "", aliasb[128] = "";
    int64_t h = 0;
    (void)code;
    if (n <= 0) { push_num_entry(task, 0.0, GMB_BT_NUMBER); return 0; }
    if (n > 512) {
        /* fallback difensivo (non raggiunto: argc max = 8 + idx + 4*64): come il path
         * generico, tiene l'ULTIMA stringa popata (= path, il primo push). */
        char last[1024] = "";
        for (i = 0; i < n; i++) {
            gmb_stack_entry_t e = gmb_task_pop(task);
            if (e.type == 2 && e.ref) vm_entry_str(e, last, (int)sizeof(last));
            gmb_obj_release(e.ref);
        }
        { char* rp = gmb_path_resolve_db(last);
          h = gmb_db_dbf_open(rp);
          free(rp);
          gmb_db_set_current_dbf(h > 0 ? h : 0); }
        push_num_entry(task, (double)(h > 0 ? h : 0), GMB_BT_NUMBER);
        return 0;
    }
    /* ricostruisce l'ordine di push (pop LIFO re-inserito in testa) */
    for (i = n - 1; i >= 0; i--) raw[i] = gmb_task_pop(task);
    vm_entry_str(raw[0], pathb, (int)sizeof(pathb));

    /* campo count = ultimo push */
    int fc = raw[n - 1].type == 1 ? (int)raw[n - 1].num : 0;
    if (fc < 0) fc = 0;
    if (fc > 64) fc = 64;

    /* alias = raw[n-4-5*fc]; typeName = raw[n-2-5*fc] (presente = `AS Tipo`) */
    if (n - 4 - 5 * fc >= 0) {
        gmb_stack_entry_t* a = &raw[n - 4 - 5 * fc];
        if (a->type == 2) vm_entry_str(*a, aliasb, (int)sizeof(aliasb));
    }
    int hasType = (n - 2 - 5 * fc >= 0) && raw[n - 2 - 5 * fc].type == 2;

    {
        char* rp = gmb_path_resolve_db(pathb);
        h = gmb_db_dbf_open(rp);
        if (h <= 0 && hasType && fc > 0) {
            char defs[4096];
            int pd = 0;
            defs[0] = 0;
            for (i = 0; i < fc; i++) {
                int b = n - 1 - 5 * fc + i * 5;   /* (nome, symId, len, isString, typeCode) */
                char nb[64] = "", tc[16] = "STRING";
                char tcode = 'N';
                int isStr, dlv;
                vm_entry_str(raw[b], nb, (int)sizeof(nb));
                if (raw[b + 4].type == 2) vm_entry_str(raw[b + 4], tc, (int)sizeof(tc));
                isStr = raw[b + 3].type == 1 ? (int)raw[b + 3].num : 0;
                dlv = raw[b + 2].type == 1 ? (int)raw[b + 2].num : 0;
                (void)isStr;
                /* stessa mappatura di gmb_sys_GMB_SYS_DBF_CREATE_TABLE */
                if      (strncasecmp(tc, "STRING", 6) == 0)  tcode = 'C';
                else if (strncasecmp(tc, "DOUBLE", 6) == 0)  tcode = 'F';
                else if (strncasecmp(tc, "SINGLE", 6) == 0)  tcode = 'S';
                else if (strncasecmp(tc, "BIT", 3) == 0)     tcode = 'L';
                else if (strncasecmp(tc, "DATE", 4) == 0)    tcode = 'D';
                else if (strncasecmp(tc, "CURRENCY", 8) == 0 || strncasecmp(tc, "MONEY", 5) == 0) tcode = 'Y';
                if (dlv <= 0) {
                    if      (tcode == 'C') dlv = 20;
                    else if (tcode == 'D') dlv = 8;
                    else if (tcode == 'L') dlv = 1;
                    else if (tcode == 'Y') dlv = 16;
                    else                   dlv = 10;
                }
                if (nb[0]) {
                    int add = snprintf(defs + pd, (int)sizeof(defs) - pd, "%s%s:%c:%d",
                                       pd ? "," : "", nb, tcode, dlv);
                    if (add > 0) pd += add;
                }
            }
            /* crea E apre (setta l'area corrente); con defs valide */
            h = pd > 0 ? gmb_db_dbf_create(rp, defs) : -1;
        }
        free(rp);
    }
    gmb_db_set_current_dbf(h > 0 ? h : 0);

    if (h > 0 && aliasb[0]) {
        int found = 0, j;
        (void)gmb_db_dbf_set_alias(h, aliasb);
        for (j = 0; j < vm->vm_dbf_alias_count; j++)
            if (strcasecmp(vm->vm_dbf_alias[j], aliasb) == 0) {
                vm->vm_dbf_alias_handle[j] = h; found = 1; break;
            }
        if (!found && vm->vm_dbf_alias_count < 32) {
            snprintf(vm->vm_dbf_alias[vm->vm_dbf_alias_count], 64, "%s", aliasb);
            vm->vm_dbf_alias_handle[vm->vm_dbf_alias_count] = h;
            vm->vm_dbf_alias_count++;
        }
        gmb_vm_dbf_set_alias_n(aliasb, (int)strlen(aliasb));
    }

    for (i = 0; i < n; i++) gmb_obj_release(raw[i].ref);
    push_num_entry(task, (double)(h > 0 ? h : 0), GMB_BT_NUMBER);
    return 0;
}

static const gmb_sys_impl_t gmb_sys_impl[] = {
/* {label, handler} */
    { GMB_SYS_ISAM_USE, gmb_sys_GMB_SYS_ISAM_USE },
    { GMB_SYS_ISAM_APPEND, gmb_sys_GMB_SYS_ISAM_APPEND },
    { GMB_SYS_ISAM_PUT, gmb_sys_GMB_SYS_ISAM_PUT },
    { GMB_SYS_ISAM_APPENDMANY, gmb_sys_GMB_SYS_ISAM_APPENDMANY },
    { GMB_SYS_ISAM_CLOSE, gmb_sys_GMB_SYS_ISAM_CLOSE },
    { GMB_SYS_ISAM_CREATE_INDEX, gmb_sys_GMB_SYS_ISAM_CREATE_INDEX },
    { GMB_SYS_ISAM_SORT_TO, gmb_sys_GMB_SYS_ISAM_SORT_TO },
    { GMB_SYS_ISAM_INFO, gmb_sys_GMB_SYS_ISAM_INFO },
    { GMB_SYS_DBF_USE, gmb_sys_GMB_SYS_DBF_USE },
    { GMB_SYS_DBF_CREATE_TABLE, gmb_sys_GMB_SYS_DBF_CREATE_TABLE },
    { GMB_SYS_DBF_APPEND_RECORD, gmb_sys_GMB_SYS_DBF_APPEND_RECORD },
    { GMB_SYS_DBF_REPLACE, gmb_sys_GMB_SYS_DBF_REPLACE },
    { GMB_SYS_DBF_GET_RECORD, gmb_sys_GMB_SYS_DBF_GET_RECORD },
    { GMB_SYS_SPEED_USE, gmb_sys_GMB_SYS_SPEED_USE },
    { GMB_SYS_SPEED_GO, gmb_sys_GMB_SYS_SPEED_GO },
    { GMB_SYS_SPEED_SKIP, gmb_sys_GMB_SYS_SPEED_SKIP },
    { GMB_SYS_SPEED_PUT, gmb_sys_GMB_SYS_SPEED_PUT },
    { GMB_SYS_SPEED_GET, gmb_sys_GMB_SYS_SPEED_GET },
    { GMB_SYS_SPEED_CLOSE, gmb_sys_GMB_SYS_SPEED_CLOSE },
    { GMB_SYS_SPEED_SELECT, gmb_sys_GMB_SYS_SPEED_SELECT },
    { GMB_SYS_SPEED_SEEK, gmb_sys_GMB_SYS_SPEED_SEEK },
    { GMB_SYS_LEDGER_ADD, gmb_sys_GMB_SYS_LEDGER_ADD },
    { GMB_SYS_LEDGER_EDIT, gmb_sys_GMB_SYS_LEDGER_EDIT },
    { GMB_SYS_LEDGER_BUDGET, gmb_sys_GMB_SYS_LEDGER_BUDGET },
    { GMB_SYS_LEDGER_VARIANCE, gmb_sys_GMB_SYS_LEDGER_VARIANCE },
    { GMB_SYS_LEDGER_ACCOUNTS, gmb_sys_GMB_SYS_LEDGER_ACCOUNTS },
    { GMB_SYS_LEDGER_BALANCE, gmb_sys_GMB_SYS_LEDGER_BALANCE },
    { GMB_SYS_LEDGER_REGISTER, gmb_sys_GMB_SYS_LEDGER_REGISTER },
    { GMB_SYS_LEDGER_PLOT, gmb_sys_GMB_SYS_LEDGER_PLOT },
    { GMB_SYS_LEDGER_IMPORT, gmb_sys_GMB_SYS_LEDGER_IMPORT },
    { GMB_SYS_ISAM_SELECT, gmb_sys_GMB_SYS_ISAM_SELECT },
    { GMB_SYS_ISAM_JOIN_WITH, gmb_sys_GMB_SYS_ISAM_JOIN_WITH },
    { GMB_SYS_ISAM_SET_RELATION, gmb_sys_GMB_SYS_ISAM_SET_RELATION },
    { GMB_SYS_DBF_SELECT, gmb_sys_GMB_SYS_DBF_SELECT },
    { GMB_SYS_DBF_SORT, gmb_sys_GMB_SYS_DBF_SORT },
    { GMB_SYS_DBF_TOTAL_ON, gmb_sys_GMB_SYS_DBF_TOTAL_ON },
    { GMB_SYS_CONVERT_DBF_TO_ISAM, gmb_sys_GMB_SYS_CONVERT_DBF_TO_ISAM },
    { GMB_SYS_DBF_CONVERT_TO_ISAM, gmb_sys_GMB_SYS_CONVERT_DBF_TO_ISAM },
    { GMB_SYS_DBF_SET_DEFAULT, gmb_sys_GMB_SYS_DBF_SET_DEFAULT },
    { GMB_SYS_DBF_SET_PATH, gmb_sys_GMB_SYS_DBF_SET_PATH },
    { GMB_SYS_DBF_UPDATE, gmb_sys_GMB_SYS_DBF_UPDATE },
    { GMB_SYS_DBF_SET_EXCLUSIVE, gmb_sys_GMB_SYS_DBF_SET_EXCLUSIVE },
    /* SET_SCHEMA e SET_DURABILITY erano mappati al handler di SET_EXCLUSIVE (catch-all che
     * ritorna 0): swallowava comandi diversi con un valore sbagliato. Entrambi hanno un case
     * nel path condiviso gmb_db_apply_syscall (con la convenzione -1 = successo), quindi
     * senza voce ci arrivano. JOIN_WITH invece NON ha case condiviso: la sua voce è mantenuta
     * (comportamento invariato = gap documentato dal 2026-08-31, commit 034e6785f) per non
     * lasciare gli argomenti non consumati sulla pila — implementarlo è lavoro separato
     * (esiste la primitiva gmb_db_dbf_join_with). */
    { GMB_SYS_DBF_JOIN_WITH, gmb_sys_GMB_SYS_DBF_JOIN_WITH },
    { GMB_SYS_DBF_SET_RELATION, gmb_sys_GMB_SYS_DBF_SET_RELATION },
    { GMB_SYS_DBF_LIST, gmb_sys_GMB_SYS_DBF_LIST },
    { GMB_SYS_DBF_CREATE_VIEW, gmb_sys_GMB_SYS_DBF_CREATE_VIEW },
    { GMB_SYS_DBF_AGGREGATE, gmb_sys_GMB_SYS_DBF_AGGREGATE },
    { GMB_SYS_DBF_REPORT, gmb_sys_GMB_SYS_DBF_REPORT },
    { GMB_SYS_DBF_APPENDMANY, gmb_sys_GMB_SYS_DBF_APPENDMANY },
    { GMB_SYS_DBF_CLOSE_DATABASES, gmb_sys_GMB_SYS_DBF_CLOSE_DATABASES },
    { GMB_SYS_DBF_APPEND_FROM, gmb_sys_GMB_SYS_DBF_APPEND_FROM },
    { GMB_SYS_DBF_COPY_STRUCTURE, gmb_sys_GMB_SYS_DBF_COPY_STRUCTURE },
    { GMB_SYS_DBF_INDEX_ON, gmb_sys_GMB_SYS_DBF_INDEX_ON },
    { GMB_SYS_ISAM_INTEGRITY, gmb_sys_GMB_SYS_ISAM_INTEGRITY },
    { GMB_SYS_ISAM_SET_REPLICATION, gmb_sys_GMB_SYS_ISAM_SET_REPLICATION },
    { GMB_SYS_ISAM_APPLY_REPLICATION, gmb_sys_GMB_SYS_ISAM_APPLY_REPLICATION },
    { GMB_SYS_ISAM_FOLLOW_REPLICATION, gmb_sys_GMB_SYS_ISAM_FOLLOW_REPLICATION },
    { GMB_SYS_DBF_SET_REPLICATION, gmb_sys_GMB_SYS_DBF_SET_REPLICATION },
    { GMB_SYS_DBF_APPLY_REPLICATION, gmb_sys_GMB_SYS_DBF_APPLY_REPLICATION },
    { GMB_SYS_DBF_FOLLOW_REPLICATION, gmb_sys_GMB_SYS_DBF_FOLLOW_REPLICATION },
    { GMB_SYS_DBF_LUPDATE, gmb_sys_GMB_SYS_DBF_LUPDATE },
    { GMB_SYS_SPEED_CREATE_INDEX, gmb_sys_GMB_SYS_SPEED_CREATE_INDEX },
    { GMB_SYS_SPEED_DROP_INDEX, gmb_sys_GMB_SYS_SPEED_DROP_INDEX },
    { GMB_SYS_SPEED_SET_ORDER, gmb_sys_GMB_SYS_SPEED_SET_ORDER },
    { GMB_SYS_SPEED_DELETE_RECORD, gmb_sys_GMB_SYS_SPEED_DELETE_RECORD },
    { GMB_SYS_SPEED_SET_RELATION, gmb_sys_GMB_SYS_SPEED_SET_RELATION },
    { GMB_SYS_SPEED_CREATE_VIEW, gmb_sys_GMB_SYS_SPEED_CREATE_VIEW },
    { GMB_SYS_SPEED_REFRESH_VIEW, gmb_sys_GMB_SYS_SPEED_REFRESH_VIEW },
    { GMB_SYS_SPEED_DROP_VIEW, gmb_sys_GMB_SYS_SPEED_DROP_VIEW },
    { GMB_SYS_SPEED_SET_FILTER, gmb_sys_GMB_SYS_SPEED_SET_FILTER },
    { GMB_SYS_SPEED_SETMANY, gmb_sys_GMB_SYS_SPEED_SETMANY },
    { GMB_SYS_LEDGER_GET, gmb_sys_GMB_SYS_LEDGER_GET },

};

int gmb_vm_exec_db_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    for (size_t i = 0; i < sizeof(gmb_sys_impl) / sizeof(gmb_sys_impl[0]); i++)
        if (gmb_sys_impl[i].code == code)
            return gmb_sys_impl[i].handler(vm, task, code, argc);
    return -1;
}
