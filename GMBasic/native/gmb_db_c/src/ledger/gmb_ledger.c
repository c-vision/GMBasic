/* FILE: gmb_ledger.c — ABI gmb_db_ledger_* delega a LedgerManager (port 1:1) */
#include "gmb_db_internal.h"
static int ledger_entry_to_json(const LedgerEntry* e, char* out, int cap);

static int ledger_parse_ymd(const char* date_ymd, int* y, int* m, int* d)
{
    if (!date_ymd || strlen(date_ymd) < 10 || date_ymd[4] != '-' || date_ymd[7] != '-')
        return GMB_DB_ERR;
    *y = atoi(date_ymd);
    *m = atoi(date_ymd + 5);
    *d = atoi(date_ymd + 8);
    if (*y < 1 || *m < 1 || *m > 12 || *d < 1 || *d > 31) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int64_t gmb_db_ledger_open(const char* journal_path)
{
    int64_t h;
    gmb_handle_t* hh;
    if (!journal_path) {
        gmb_db_set_err("null journal path");
        return -1;
    }
    h = gmb_db_alloc(GMB_H_LEDGER);
    if (h < 0) return -1;
    hh = &g_handles[h];
    hh->u.ledger.ctx = LedgerContext_New(journal_path);
    if (!hh->u.ledger.ctx) {
        hh->kind = GMB_H_FREE;
        gmb_db_set_err("cannot open ledger");
        return -1;
    }
    LedgerManager_ReadIndexFromFile(LedgerContext_IndexFilePath(hh->u.ledger.ctx), hh->u.ledger.ctx);
    /* Cursore NON posizionato all'apertura: nel backend di riferimento il cursore è assente finché
     * GoTop/GoBottom/Go/Skip non lo muovono (DatabaseService._ledgerCursorIndex → LedgerFound
     * false). Senza questo l'handle azzerato partiva da 0 e FOUND rispondeva TRUE. */
    hh->u.ledger.cursor = -1;
    gmb_db_set_current_ledger(h);   /* l'apertura diretta (C API) diventa il ledger corrente,
                                     * come per DBF/ISAM: senza questo le funzioni senza handle
                                     * (gmb_db_current_ledger) rispondevano su un ledger 0. */
    return h;
}

int gmb_db_ledger_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.ledger.ctx) {
        if (LedgerContext_IsDirty(hh->u.ledger.ctx))
            LedgerManager_SaveIndexToFile(LedgerContext_IndexFilePath(hh->u.ledger.ctx), hh->u.ledger.ctx);
        LedgerContext_Dispose(hh->u.ledger.ctx);
        hh->u.ledger.ctx = NULL;
    }
    hh->kind = GMB_H_FREE;
    return GMB_DB_OK;
}

int64_t gmb_db_ledger_add(int64_t h, const char* date_ymd, const char* desc,
                          const char* const* accounts, const double* amounts, int n)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry entries[16];
    int y, m, d, i;
    int64_t id;
    if (!hh || !hh->u.ledger.ctx || !date_ymd || !desc || !accounts || !amounts || n < 1 || n > 16)
        return -1;
    if (ledger_parse_ymd(date_ymd, &y, &m, &d) != GMB_DB_OK) return -1;
    memset(entries, 0, sizeof(entries));
    for (i = 0; i < n; i++) {
        entries[i].Description = desc;
        entries[i].Account = accounts[i];
        entries[i].Amount = GmbDecimal_FromDouble(amounts[i]);
        entries[i].AmountIsExplicit = 1;
        entries[i].ExchangeRate = 1.0;
    }
    id = LedgerManager_AddTransaction(hh->u.ledger.ctx, y, m, d, desc, entries, n);
    return id < 1 ? -1 : id;
}

int64_t gmb_db_ledger_add_csv(int64_t h, const char* date_ymd, const char* desc,
                              const char* accounts_csv, const char* amounts_csv)
{
    char acc[16][256];
    char ams[16][256];
    const char* ap[16];
    double av[16];
    int na, nm, i, n;
    na = gmb_db_split_csv(accounts_csv, acc, 16);
    nm = gmb_db_split_csv(amounts_csv, ams, 16);
    n = na < nm ? na : nm;
    for (i = 0; i < n; i++) {
        ap[i] = acc[i];
        av[i] = atof(ams[i]);
    }
    return gmb_db_ledger_add(h, date_ymd, desc, ap, av, n);
}

int gmb_db_ledger_count(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    return hh && hh->u.ledger.ctx ? LedgerManager_IndexCount(hh->u.ledger.ctx) : 0;
}

int gmb_db_ledger_get(int64_t h, int64_t tx_id, char* date_buf, int dcap, char* desc_buf, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry got[8];
    int n = 0;
    const char* d;
    if (!hh || !hh->u.ledger.ctx || tx_id < 1 || !date_buf || dcap < 11 || !desc_buf || cap <= 0)
        return GMB_DB_ERR;
    if (LedgerManager_GetTransaction(hh->u.ledger.ctx, tx_id, got, 8, &n) != GMB_DB_OK || n < 1)
        return GMB_DB_NOTFOUND;
    snprintf(date_buf, (size_t)dcap, "%04d-%02d-%02d", got[0].DateY, got[0].DateM, got[0].DateD);
    d = got[0].Description ? got[0].Description : "";
    strncpy(desc_buf, d, (size_t)cap - 1);
    desc_buf[cap - 1] = 0;
    return GMB_DB_OK;
}

/* ==================== Cursore LEDGER (pattern DBF/ISAM) ==================== */

static int ledger_sorted_ids(int64_t h, int64_t** out, int* nOut)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n, rc;
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    n = LedgerManager_IndexCount(hh->u.ledger.ctx);
    if (n == 0) { *out = NULL; *nOut = 0; return GMB_DB_OK; }
    ids = (int64_t*)malloc((size_t)n * sizeof(int64_t));
    if (!ids) return GMB_DB_ERR;
    rc = LedgerManager_GetSortedIndexIds(hh->u.ledger.ctx, ids, n, nOut);
    if (rc != GMB_DB_OK) { free(ids); return rc; }
    *out = ids;
    return GMB_DB_OK;
}

int gmb_db_ledger_go_top(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n;
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    if (ledger_sorted_ids(h, &ids, &n) != GMB_DB_OK) return GMB_DB_ERR;
    hh->u.ledger.cursor = n > 0 ? 0 : -1;
    free(ids);
    return GMB_DB_OK;
}

int gmb_db_ledger_go_bottom(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n;
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    if (ledger_sorted_ids(h, &ids, &n) != GMB_DB_OK) return GMB_DB_ERR;
    hh->u.ledger.cursor = n > 0 ? n - 1 : -1;
    free(ids);
    return GMB_DB_OK;
}

int gmb_db_ledger_go(int64_t h, int64_t tx_id)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n, i;
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    if (ledger_sorted_ids(h, &ids, &n) != GMB_DB_OK) return GMB_DB_ERR;
    hh->u.ledger.cursor = -1;
    for (i = 0; i < n; i++) {
        if (ids[i] == tx_id) { hh->u.ledger.cursor = i; break; }
    }
    free(ids);
    return GMB_DB_OK;
}

int gmb_db_ledger_skip(int64_t h, int n)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int count, cur;
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    count = LedgerManager_IndexCount(hh->u.ledger.ctx);
    cur = hh->u.ledger.cursor + n;
    if (cur < -1) cur = -1;
    if (cur > count) cur = count;
    hh->u.ledger.cursor = cur;
    return GMB_DB_OK;
}

int gmb_db_ledger_eof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int count;
    if (!hh || !hh->u.ledger.ctx) return 1;
    count = LedgerManager_IndexCount(hh->u.ledger.ctx);
    return (hh->u.ledger.cursor < 0 || hh->u.ledger.cursor >= count) ? 1 : 0;
}

int gmb_db_ledger_bof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx) return 1;
    return hh->u.ledger.cursor < 0 ? 1 : 0;
}

int64_t gmb_db_ledger_recno(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n;
    int64_t id = 0;
    if (!hh || !hh->u.ledger.ctx) return 0;
    if (ledger_sorted_ids(h, &ids, &n) != GMB_DB_OK) return 0;
    if (hh->u.ledger.cursor >= 0 && hh->u.ledger.cursor < n)
        id = ids[hh->u.ledger.cursor];
    free(ids);
    return id;
}

int gmb_db_ledger_found(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int count;
    if (!hh || !hh->u.ledger.ctx) return 0;
    count = LedgerManager_IndexCount(hh->u.ledger.ctx);
    return (hh->u.ledger.cursor >= 0 && hh->u.ledger.cursor < count) ? 1 : 0;
}

int gmb_db_ledger_get_current(int64_t h, char* date_buf, int dcap, char* desc_buf, int cap)
{
    int64_t id = gmb_db_ledger_recno(h);
    if (id < 1) return GMB_DB_NOTFOUND;
    return gmb_db_ledger_get(h, id, date_buf, dcap, desc_buf, cap);
}

/* ==================== Operazioni estese (port C# ILBackend) ==================== */

int64_t gmb_db_ledger_edit(int64_t h, int64_t tx_id, const char* date_ymd, const char* desc,
                           const char* const* accounts, const double* amounts, int n)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry entries[16];
    int y, m, d, i;
    if (!hh || !hh->u.ledger.ctx || tx_id < 1 || !date_ymd || !desc || !accounts || !amounts || n < 1 || n > 16)
        return -1;
    if (ledger_parse_ymd(date_ymd, &y, &m, &d) != GMB_DB_OK) return -1;
    memset(entries, 0, sizeof(entries));
    for (i = 0; i < n; i++) {
        entries[i].Description = desc;
        entries[i].Account = accounts[i];
        entries[i].Amount = GmbDecimal_FromDouble(amounts[i]);
        entries[i].AmountIsExplicit = 1;
        entries[i].ExchangeRate = 1.0;
    }
    return LedgerManager_UpdateTransaction(hh->u.ledger.ctx, tx_id, y, m, d, desc, entries, n);
}

int gmb_db_ledger_delete(int64_t h, int64_t tx_id)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx || tx_id < 1) return GMB_DB_ERR;
    return LedgerManager_DeleteTransaction(hh->u.ledger.ctx, tx_id) ? GMB_DB_OK : GMB_DB_NOTFOUND;
}

int gmb_db_ledger_save(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    if (LedgerManager_SaveIndexToFile(LedgerContext_IndexFilePath(hh->u.ledger.ctx), hh->u.ledger.ctx) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (LedgerContext_IsBudgetsDirty(hh->u.ledger.ctx)) {
        if (LedgerManager_SaveBudgets(LedgerContext_BudgetFilePath(hh->u.ledger.ctx), hh->u.ledger.ctx) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int gmb_db_ledger_pack(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    hh->u.ledger.cursor = -1;
    return LedgerManager_PackJournal(hh->u.ledger.ctx);
}


/* ── Varianti "valore" per l'emitter C99Native ───────────────────────────────────────────────
 * Gli array del ledger (ACCOUNTS/BALANCE/REGISTER) devono arrivare al codice generato come
 * LISTE, non come testo JSON: il testo farebbe contare i caratteri a LEN() (parità IL/PCode,
 * che ritornano array). Si parsa il JSON già prodotto dal runtime e si appiattisce l'annidamento
 * extra (i _str del C serializzano l'array una volta di troppo). */
static gmb_val_t ledger_json_to_list_val(const char* js)
{
    gmb_val_t v = gmb_json_parse(js && *js ? js : "[]");
    /* Solo API pubbliche del runtime (gmb_val_as_list/gmb_list_count/gmb_list_get): il tipo
     * interno degli oggetti è del VM e non è visibile qui. */
    gmb_list_t l = gmb_val_as_list(v);
    if (gmb_list_count(l) == 1)
    {
        gmb_val_t inner = gmb_list_get(l, 0);
        if (gmb_val_typename(inner).data && strstr(gmb_val_typename(inner).data, "LIST"))
            return inner;
    }
    return v;
}

gmb_val_t gmb_db_ledger_accounts_val(int64_t h, const char* root)
{
    gmb_string_t js = gmb_db_ledger_accounts_str(h, root);
    gmb_val_t v = ledger_json_to_list_val(js.data);
    if (js.data) free(js.data);
    return v;
}

gmb_val_t gmb_db_ledger_balance_val(int64_t h, const char* query)
{
    gmb_string_t js = gmb_db_ledger_balance_str(h, query);
    gmb_val_t v = ledger_json_to_list_val(js.data);
    if (js.data) free(js.data);
    (void)query;
    return v;
}

gmb_val_t gmb_db_ledger_register_val(int64_t h, const char* query)
{
    gmb_string_t js = gmb_db_ledger_register_str(h, query);
    gmb_val_t v = ledger_json_to_list_val(js.data);
    if (js.data) free(js.data);
    (void)query;
    return v;
}

/* LEDGER.GET come LISTA (parità IL object?[] e degli altri _val): gli elementi sono
 * i movimenti della transazione. */
gmb_val_t gmb_db_ledger_get_val(int64_t h, int64_t tx_id)
{
    char* buf = (char*)malloc(65536);
    if (!buf) return gmb_make_val_list(gmb_list_new());
    buf[0] = '[';
    buf[1] = ']';
    buf[2] = 0;
    gmb_db_ledger_get_json(h, tx_id, buf, 65536);
    gmb_val_t v = ledger_json_to_list_val(buf);
    free(buf);
    return v;
}

int gmb_db_ledger_validate(int64_t h)
{    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int64_t* ids;
    int n, i;
    LedgerEntry got[32];
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    if (ledger_sorted_ids(h, &ids, &n) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int cnt = 0;
        if (LedgerManager_GetTransaction(hh->u.ledger.ctx, ids[i], got, 32, &cnt) == GMB_DB_OK
            && cnt > 0 && !LedgerManager_IsTransactionBalanced(got, cnt)) {
            free(ids);
            return 0;
        }
    }
    free(ids);
    return 1;
}

int gmb_db_ledger_budget(int64_t h, const char* account, double amount, const char* period)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    return LedgerManager_SetBudget(hh->u.ledger.ctx, account, GmbDecimal_FromDouble(amount), period);
}

int gmb_db_ledger_get_json(int64_t h, int64_t tx_id, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry entries[32];
    int n = 0, i, pos = 0;
    if (!hh || !hh->u.ledger.ctx || tx_id < 1 || !out || outCap <= 0) return GMB_DB_ERR;
    if (LedgerManager_GetTransaction(hh->u.ledger.ctx, tx_id, entries, 32, &n) != GMB_DB_OK || n < 1)
        return GMB_DB_NOTFOUND;
    pos = snprintf(out, (size_t)outCap, "[");
    for (i = 0; i < n; i++) {
        char entryJson[1024];
        ledger_entry_to_json(&entries[i], entryJson, (int)sizeof(entryJson));
        if (pos < outCap - 1)
            pos += snprintf(out + pos, (size_t)(outCap - pos), "%s%s",
                i ? "," : "", entryJson);
    }
    if (pos < outCap - 1) pos += snprintf(out + pos, (size_t)(outCap - pos), "]");
    return GMB_DB_OK;
}

int gmb_db_ledger_variance(int64_t h, const char* account, const char* period, double* var_out)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerBudgetVariance v;
    if (!hh || !hh->u.ledger.ctx || !var_out) return GMB_DB_ERR;
    if (LedgerManager_GetVariance(hh->u.ledger.ctx, account, period, &v) != GMB_DB_OK) return GMB_DB_ERR;
    *var_out = GmbDecimal_ToDouble(v.Variance);
    return GMB_DB_OK;
}

int gmb_db_ledger_plot(int64_t h, const char* query, char labels[][GMB_DB_LEDGER_NAME], double* amounts,
                       int max, int* n_out)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    if (!hh || !hh->u.ledger.ctx) return GMB_DB_ERR;
    return LedgerManager_GetBalancePlotData(hh->u.ledger.ctx, query, labels, amounts, max, n_out);
}

int gmb_db_ledger_import(int64_t h, const char* csv_path, const char* mapping_rules)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    int imported = 0;
    int rc;
    /* Parità col servizio C#: LedgerImport ritorna il NUMERO di righe importate e 0 su errore
     * (DatabaseService.Commercial.LedgerImport: `try { ... } catch { return 0; }`). Un mapping
     * senza date/account/amount è un ArgumentException in C#, cioè 0 — non un codice d'errore.
     * La diagnostica resta disponibile via gmb_db_set_err (§0bis: mai un silenzio senza traccia). */
    if (!hh || !hh->u.ledger.ctx) { gmb_db_set_err("ledger: handle non valido"); return 0; }
    rc = LedgerManager_ImportCsv(hh->u.ledger.ctx, csv_path, mapping_rules, &imported);
    if (rc != GMB_DB_OK) { gmb_db_set_err("ledger import: mapping o file non valido (0 righe importate)"); return 0; }
    return imported;
}


/* --- FASE 3: LEDGER esteso (accounts / balance / register — output JSON) --- */
int gmb_db_ledger_accounts(int64_t h, const char* root, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    char names[512][GMB_LEDGER_NAME];
    int n = 0, i, pos = 0;
    if (!hh || !hh->u.ledger.ctx || !out || outCap <= 0) return GMB_DB_ERR;
    if (LedgerManager_GetAccounts(hh->u.ledger.ctx, root, names, 512, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    pos = snprintf(out, (size_t)outCap, "[");
    for (i = 0; i < n; i++) {
        if (pos < outCap - 1)
            pos += snprintf(out + pos, (size_t)(outCap - pos),
                "%s\"%s\"", i ? "," : "", names[i]);
    }
    if (pos < outCap - 1) pos += snprintf(out + pos, (size_t)(outCap - pos), "]");
    return GMB_DB_OK;
}

int gmb_db_ledger_balance(int64_t h, const char* query, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry entries[512];
    int n = 0, i, j, pos = 0;
    if (!hh || !hh->u.ledger.ctx || !out || outCap <= 0) return GMB_DB_ERR;
    if (LedgerManager_GetFilteredEntries(hh->u.ledger.ctx, query, entries, 512, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    pos = snprintf(out, (size_t)outCap, "[");
    {
        /* Raccogli (conto, somma) UNICI e ORDINA alfabeticamente (il C# fa
         * `.GroupBy(Account).OrderBy(b => b.Account)`): l'ordine per inserzione divergeva
         * (Banca,Salario,Cena,... vs Banca,Contanti,Salario,...). */
        const char* seen[512];
        double sums[512];
        int seenN = 0, k, first = 1;
        for (i = 0; i < n; i++) {
            int already = 0, s;
            if (!entries[i].Account || entries[i].Account[0] == 0) continue;
            for (s = 0; s < seenN; s++) if (strcmp(seen[s], entries[i].Account) == 0) { already = 1; break; }
            if (already) continue;
            { double sum = 0;
              for (j = 0; j < n; j++)
                  if (entries[j].Account && strcmp(entries[i].Account, entries[j].Account) == 0)
                      sum += GmbDecimal_ToDouble(entries[j].Amount);
              seen[seenN] = entries[i].Account; sums[seenN] = sum; seenN++; }
        }
        for (i = 1; i < seenN; i++) {   /* insertion sort per conto (strcmp) */
            const char* ak = seen[i]; double sk = sums[i];
            for (k = i - 1; k >= 0 && strcmp(seen[k], ak) > 0; k--) { seen[k + 1] = seen[k]; sums[k + 1] = sums[k]; }
            seen[k + 1] = ak; sums[k + 1] = sk;
        }
        for (i = 0; i < seenN; i++) {
            if (pos < outCap - 1)
                pos += snprintf(out + pos, (size_t)(outCap - pos), "%s{\"Account\":\"%s\",\"TotalAmount\":%g}",
                    first ? "" : ",", seen[i], sums[i]);
            first = 0;
        }
    }
    if (pos < outCap - 1) pos += snprintf(out + pos, (size_t)(outCap - pos), "]");
    return GMB_DB_OK;
}

/* Serializza UN posting ledger nel JSON completo (parità C# LedgerEntry):
 * {"TransactionId":N,"Date":{},"Status":" ","Description":"...","Account":"...",
 *  "Amount":X,"Comment":null,"AmountIsExplicit":true,"Currency":null,
 *  "ExchangeRate":1,"BaseAmount":Y} */
static int ledger_entry_to_json(const LedgerEntry* e, char* out, int cap)
{
    char amt[64], dateIso[24];
    double amount = GmbDecimal_ToDouble(e->Amount);
    snprintf(amt, sizeof(amt), "%g", amount);
    snprintf(dateIso, sizeof(dateIso), "%04d-%02d-%02d", e->DateY, e->DateM, e->DateD);
    const char* comment = (e->Comment && *e->Comment) ? e->Comment : NULL;
    /* Formato IDENTICO al C# (IL/PCode): Date come "YYYY-MM-DD" (prima {}), niente
     * Currency/ExchangeRate/BaseAmount (che il C# non emette). Parità 5-backend
     * (§ DbLedgerFullTests). */
    return snprintf(out, (size_t)cap,
        "{\"TransactionId\":%lld,\"Date\":\"%s\",\"Status\":\"%c\",\"Description\":\"%s\","
        "\"Account\":\"%s\",\"Amount\":%s,\"Comment\":%s,\"AmountIsExplicit\":%s}",
        (long long)e->TransactionId, dateIso, e->Status ? e->Status : ' ',
        e->Description ? e->Description : "", e->Account ? e->Account : "",
        amt, comment ? "\"\"" : "null", e->AmountIsExplicit ? "true" : "false");
}

int gmb_db_ledger_register(int64_t h, const char* query, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_LEDGER);
    LedgerEntry entries[512];
    int n = 0, i, pos = 0;
    if (!hh || !hh->u.ledger.ctx || !out || outCap <= 0) return GMB_DB_ERR;
    if (LedgerManager_GetFilteredEntries(hh->u.ledger.ctx, query, entries, 512, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    pos = snprintf(out, (size_t)outCap, "[");
    for (i = 0; i < n; i++) {
        char entryJson[1024];
        ledger_entry_to_json(&entries[i], entryJson, (int)sizeof(entryJson));
        if (pos < outCap - 1)
            pos += snprintf(out + pos, (size_t)(outCap - pos), "%s%s",
                i ? "," : "", entryJson);
    }
    if (pos < outCap - 1) pos += snprintf(out + pos, (size_t)(outCap - pos), "]");
    return GMB_DB_OK;
}

static gmb_string_t ledger_out_str(int rc, const char* buf, const char* fallback)
{
    return gmb_string_alloc((rc == 0 && buf && buf[0]) ? buf : fallback);
}

gmb_string_t gmb_db_ledger_register_str(int64_t h, const char* q)
{
    char buf[16384]; buf[0] = 0;
    int rc = gmb_db_ledger_register(h, q ? q : "", buf, (int)sizeof(buf));
    return ledger_out_str(rc, buf, "[]");
}
gmb_string_t gmb_db_ledger_balance_str(int64_t h, const char* q)
{
    char buf[16384]; buf[0] = 0;
    int rc = gmb_db_ledger_balance(h, q ? q : "", buf, (int)sizeof(buf));
    return ledger_out_str(rc, buf, "[]");
}
gmb_string_t gmb_db_ledger_accounts_str(int64_t h, const char* q)
{
    char buf[16384]; buf[0] = 0;
    int rc = gmb_db_ledger_accounts(h, q ? q : "", buf, (int)sizeof(buf));
    return ledger_out_str(rc, buf, "[]");
}
gmb_string_t gmb_db_ledger_plot_json(int64_t h, const char* q)
{
    char labels[64][GMB_DB_LEDGER_NAME];
    double amounts[64];
    int nout = 0;
    char buf[16384];
    int pos = 0;
    buf[pos++] = '[';
    if (gmb_db_ledger_plot(h, q ? q : "", labels, amounts, 64, &nout) == 0 && nout > 0)
    {
        for (int i = 0; i < nout && pos < (int)sizeof(buf) - 256; i++)
        {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s{\"label\":\"%s\",\"amount\":%.6g}",
                i ? "," : "", labels[i], amounts[i]);
        }
    }
    if (pos < (int)sizeof(buf)) buf[pos++] = ']';
    buf[pos] = 0;
    return gmb_string_alloc(buf);
}
