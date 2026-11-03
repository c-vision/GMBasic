/* FILE: native/gmb_db_c/src/SPEED/QueryPlan.c */
/* Port 1:1 di modules/Database/SPEED/QueryPlan.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"
#include "gmb_expr.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.QueryPlan.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Collections.Immutable;
//
//namespace Basic.Databases.Speed;
//
//// Nota di Progettazione: Queste classi 'record' rappresentano un piano di esecuzione
//// pulito e disaccoppiato dall'AST del parser. L'Executor sarà responsabile di
//// mappare i nodi dell'AST in queste strutture prima di passare il piano al motore Speed.
//// L'uso di 'string' per le espressioni garantisce l'indipendenza dall'Executor.
//
///// <summary>
///// Rappresenta un singolo campo o espressione da proiettare nel risultato di una query (clausola SELECT).
///// </summary>
///// <param name="ExpressionString">La stringa dell'espressione da valutare per questo campo (es. "UPPER(Nome)" o "Prezzo * Quantita").</param>
///// <param name="Alias">Il nome del campo nella tabella/array di destinazione (es. "NomeMaiuscolo" o "TotaleRiga").</param>
//public sealed record QueryProjection(string ExpressionString, string Alias);
//
///// <summary>
///// Rappresenta una sorgente dati per la query (una tabella, vista o sottoquery).
///// </summary>
///// <param name="TableName">Il nome della tabella originale.</param>
///// <param name="Alias">L'alias usato per la tabella all'interno della query (es. "FROM Clienti AS c").</param>
//public sealed record QuerySource(string TableName, string Alias);
//
///// <summary>
///// Rappresenta una clausola di join tra due sorgenti dati, strutturata per un'esecuzione efficiente
///// tramite ricerca indicizzata.
///// </summary>
///// <param name="RightSource">La tabella "destra" (interna) del join.</param>
///// <param name="LeftKeyExpressionString">L'espressione da valutare sulla tabella sinistra per ottenere il valore da cercare.</param>
///// <param name="RightIndexName">Il nome dell'indice sulla tabella destra da usare per la ricerca (SEEK).</param>
//public sealed record QueryJoin(QuerySource RightSource, string LeftKeyExpressionString, string RightIndexName);
//
///// <summary>
///// Rappresenta una colonna di ordinamento nella clausola ORDER BY.
///// </summary>
///// <param name="ExpressionString">La stringa dell'espressione da usare per l'ordinamento.</param>
///// <param name="Descending">True se l'ordinamento è discendente (DESC), altrimenti False (ASC).</param>
//public sealed record QueryOrderBy(string ExpressionString, bool Descending);
//
///// <summary>
///// Rappresenta il piano di esecuzione completo per una query `SPEED SELECT`.
///// Questo oggetto viene costruito dall'Executor e passato al motore `Database` per l'esecuzione.
///// Contiene solo tipi di dati semplici per garantire il disaccoppiamento.
///// </summary>
///// <param name="Projections">La lista dei campi da includere nel risultato (clausola SELECT).</param>
///// <param name="DestinationVariableName">Il nome della variabile array UDT di destinazione per i risultati (`INTO` clause).</param>
///// <param name="DestinationUdtName">Il nome del `TYPE` di destinazione per i risultati (dalla definizione dell'array).</param>
///// <param name="FromSource">La tabella di partenza (clausola FROM).</param>
///// <param name="JoinClause">La clausola di join opzionale.</param>
///// <param name="WhereClauseString">La stringa dell'espressione di filtro opzionale (clausola WHERE).</param>
///// <param name="GroupByClauseStrings">La lista di stringhe di espressioni di raggruppamento opzionali (clausola GROUP BY).</param>
///// <param name="HavingClauseString">La stringa dell'espressione di filtro post-aggregazione opzionale (clausola HAVING).</param>
///// <param name="OrderByClauses">La lista di colonne di ordinamento opzionali (clausola ORDER BY).</param>
//public sealed record QueryPlan(
//    ImmutableArray<QueryProjection> Projections,
//    string DestinationVariableName,
//    string DestinationUdtName,
//    QuerySource FromSource,
//    QueryJoin? JoinClause,
//    string? WhereClauseString,
//    ImmutableArray<string>? GroupByClauseStrings,
//    string? HavingClauseString,
//    ImmutableArray<QueryOrderBy>? OrderByClauses
//);

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

void QueryPlan_Init(QueryPlan* plan)
{
    if (plan) memset(plan, 0, sizeof(*plan));
}

void SpeedEvalValue_Clear(SpeedEvalValue* v)
{
    if (!v) return;
    free(v->bytes);
    memset(v, 0, sizeof(*v));
}

int QueryPlan_AddProjection(QueryPlan* plan, const char* expression, const char* alias)
{
    QueryProjection* p;
    if (!plan || !expression || !expression[0]) return GMB_DB_ARG;
    if (plan->ProjectionN >= QUERYPLAN_MAX_PROJ) return GMB_DB_ERR;
    p = &plan->Projections[plan->ProjectionN++];
    snprintf(p->ExpressionString, sizeof(p->ExpressionString), "%s", expression);
    snprintf(p->Alias, sizeof(p->Alias), "%s", (alias && alias[0]) ? alias : expression);
    return GMB_DB_OK;
}

void QueryResult_Dispose(QueryResult* result)
{
    int i, j;
    if (!result) return;
    for (i = 0; i < result->N; i++) {
        for (j = 0; j < result->Rows[i].CellN; j++)
            free(result->Rows[i].Cells[j].Bytes);
        free(result->Rows[i].PrimaryKey);
    }
    free(result->Rows);
    result->Rows = NULL;
    result->N = 0;
}

static void qp_strip_alias(const char* expr, const char* alias, char* out, int cap)
{
    const char* dot;
    if (!expr) { if (out && cap) out[0] = 0; return; }
    if (alias && alias[0] && (dot = strchr(expr, '.')) != NULL) {
        size_t alen = strlen(alias);
        char tmp[64];
        if ((size_t)(dot - expr) == alen && alen < sizeof(tmp)) {
            memcpy(tmp, expr, alen);
            tmp[alen] = 0;
            if (gmb_strcasecmp(tmp, alias) == 0) {
                snprintf(out, (size_t)cap, "%s", dot + 1);
                return;
            }
        }
    }
    snprintf(out, (size_t)cap, "%s", expr);
}

static int64_t qp_read_le(const uint8_t* p, int n)
{
    int64_t v = 0;
    int i;
    for (i = 0; i < n && i < 8; i++)
        v |= (int64_t)(unsigned)p[i] << (8 * i);
    return v;
}

static int qp_field_matches(const Manifest* man, int tableId, const uint8_t* udt, int udtLen,
                            const QueryOptimizerParsedEquality* eq)
{
    uint8_t* actual = NULL;
    int actualLen = 0, ok;
    char typeName[32], coerced[256];
    SpeedDataTypeKind kind;
    if (!SpeedMarshaller_ExtractField(man, tableId, udt, udtLen, eq->FieldName, &actual, &actualLen))
        return 0;
    Manifest_FindField(man, tableId, eq->FieldName, typeName, (int)sizeof(typeName), NULL);
    kind = SpeedDataType_Parse(typeName);
    QueryOptimizer_CoerceLiteral(eq->RawValue, coerced, (int)sizeof(coerced));
    if (SpeedDataType_IsNumeric(kind)) {
        int64_t want = (int64_t)strtoll(coerced, NULL, 10);
        ok = (qp_read_le(actual, actualLen) == want);
    } else {
        ok = (actualLen == (int)strlen(coerced) && memcmp(actual, coerced, (size_t)actualLen) == 0);
    }
    free(actual);
    return ok;
}

static int qp_row_matches(const Manifest* man, int tableId, const uint8_t* udt, int udtLen,
                          const QueryOptimizerParsedEquality* eqs, int n)
{
    int i;
    if (n <= 0) return 1;
    for (i = 0; i < n; i++) {
        if (!qp_field_matches(man, tableId, udt, udtLen, &eqs[i]))
            return 0;
    }
    return 1;
}

static int qp_push_row(QueryResult* out, const QueryResultRow* row)
{
    QueryResultRow* grown;
    grown = (QueryResultRow*)realloc(out->Rows, (size_t)(out->N + 1) * sizeof(*grown));
    if (!grown) return 0;
    out->Rows = grown;
    out->Rows[out->N++] = *row;
    return 1;
}

static int qp_cell_index(const QueryResultRow* row, const char* name)
{
    int i;
    for (i = 0; i < row->CellN; i++)
        if (gmb_strcasecmp(row->Cells[i].Alias, name) == 0) return i;
    return -1;
}

static int qp_cmp_bytes(const uint8_t* a, int al, const uint8_t* b, int bl)
{
    int n = al < bl ? al : bl;
    int c = n > 0 ? memcmp(a ? a : (const uint8_t*)"", b ? b : (const uint8_t*)"", (size_t)n) : 0;
    if (c != 0) return c;
    return al - bl;
}

static int qp_has_agg(const QueryPlan* plan)
{
    int i;
    for (i = 0; i < plan->ProjectionN; i++)
        if (strchr(plan->Projections[i].ExpressionString, '(')) return 1;
    return 0;
}

static int qp_dup_bytes(const uint8_t* src, int len, uint8_t** out, int* outLen)
{
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (len < 0) len = 0;
    if (len > 0 && !src) return 0;
    if (len == 0) return 1;
    *out = (uint8_t*)malloc((size_t)len);
    if (!*out) return 0;
    memcpy(*out, src, (size_t)len);
    if (outLen) *outLen = len;
    return 1;
}

static void qp_free_row(QueryResultRow* row)
{
    int j;
    if (!row) return;
    for (j = 0; j < row->CellN; j++) free(row->Cells[j].Bytes);
    free(row->PrimaryKey);
    memset(row, 0, sizeof(*row));
}

static int qp_copy_cell(QueryResultCell* dst, const QueryResultCell* src)
{
    memset(dst, 0, sizeof(*dst));
    snprintf(dst->Alias, sizeof(dst->Alias), "%s", src->Alias);
    return qp_dup_bytes(src->Bytes, src->Len, &dst->Bytes, &dst->Len);
}

static int qp_row_from_udt(const Manifest* man, int tableId, const uint8_t* udt, int udtLen,
                           const uint8_t* pk, int pkLen, const char* alias, QueryResultRow* row)
{
    int i, n;
    memset(row, 0, sizeof(*row));
    if (pk && pkLen > 0 && !qp_dup_bytes(pk, pkLen, &row->PrimaryKey, &row->PrimaryKeyLen))
        return 0;
    n = Manifest_FieldCount(man, tableId);
    for (i = 0; i < n && row->CellN < QUERYPLAN_MAX_PROJ; i++) {
        char name[64];
        QueryResultCell* c = &row->Cells[row->CellN];
        if (!Manifest_GetField(man, tableId, i, name, (int)sizeof(name), NULL, 0, NULL))
            continue;
        if (!SpeedMarshaller_ExtractField(man, tableId, udt, udtLen, name, &c->Bytes, &c->Len))
            continue;
        if (alias && alias[0])
            snprintf(c->Alias, sizeof(c->Alias), "%s.%s", alias, name);
        else
            snprintf(c->Alias, sizeof(c->Alias), "%s", name);
        row->CellN++;
    }
    return 1;
}

static int qp_find_cell(const QueryResultRow* row, const char* expr, const char* alias)
{
    char field[64];
    int i, idx;
    if (!row || !expr || !expr[0]) return -1;
    idx = qp_cell_index(row, expr);
    if (idx >= 0) return idx;
    qp_strip_alias(expr, alias, field, (int)sizeof(field));
    idx = qp_cell_index(row, field);
    if (idx >= 0) return idx;
    if (alias && alias[0]) {
        char q[64];
        snprintf(q, sizeof(q), "%s.%s", alias, field);
        idx = qp_cell_index(row, q);
        if (idx >= 0) return idx;
    }
    for (i = 0; i < row->CellN; i++) {
        const char* dot = strrchr(row->Cells[i].Alias, '.');
        if (dot && gmb_strcasecmp(dot + 1, field) == 0) return i;
    }
    return -1;
}

static int qp_cell_matches_eq(const QueryResultRow* row, const QueryOptimizerParsedEquality* eq,
                              const char* alias)
{
    uint8_t* actual;
    int actualLen, idx, ok;
    char coerced[256];
    const char* useAlias = (eq->TableAlias[0]) ? eq->TableAlias : alias;
    idx = qp_find_cell(row, eq->FieldName, useAlias);
    if (idx < 0) return 0;
    actual = row->Cells[idx].Bytes;
    actualLen = row->Cells[idx].Len;
    QueryOptimizer_CoerceLiteral(eq->RawValue, coerced, (int)sizeof(coerced));
    if (coerced[0] && (coerced[0] == '-' || (coerced[0] >= '0' && coerced[0] <= '9'))) {
        int64_t want = (int64_t)strtoll(coerced, NULL, 10);
        ok = (qp_read_le(actual, actualLen) == want);
    } else {
        ok = (actualLen == (int)strlen(coerced) && memcmp(actual, coerced, (size_t)actualLen) == 0);
    }
    return ok;
}

static int qp_row_matches_eqs(const QueryResultRow* row, const QueryOptimizerParsedEquality* eqs, int n,
                              const char* alias)
{
    int i;
    if (n <= 0) return 1;
    for (i = 0; i < n; i++) {
        if (!qp_cell_matches_eq(row, &eqs[i], alias)) return 0;
    }
    return 1;
}

static int qp_scan_source(Database* self, int mainId, const char* indexName, int* outId, int* isIndex)
{
    Manifest* man = Database_GetManifest(self);
    char mainName[128], idxTable[256];
    int idxId = 0;
    if (outId) *outId = mainId;
    if (isIndex) *isIndex = 0;
    if (!man || !indexName || !indexName[0]) return 0;
    if (!Manifest_TryGetTable(man, mainId, mainName, (int)sizeof(mainName))) return 0;
    if (gmb_strcasecmp(indexName, "Id") == 0) return 1;
    Manifest_IndexTableName(mainName, indexName, idxTable, (int)sizeof(idxTable));
    if (Manifest_TryGetTableByName(man, idxTable, &idxId) && idxId > 0) {
        if (outId) *outId = idxId;
        if (isIndex) *isIndex = 1;
        return 1;
    }
    return 1;
}

static int qp_join_lookup(Database* self, int rightId, const char* indexName,
                          const uint8_t* key, int keyLen, uint8_t** udt, int* udtLen,
                          uint8_t** pk, int* pkLen)
{
    int srcId = rightId, isIndex = 0, found;
    if (udt) *udt = NULL;
    if (udtLen) *udtLen = 0;
    if (pk) *pk = NULL;
    if (pkLen) *pkLen = 0;
    if (!key || keyLen <= 0) return 0;
    if (!qp_scan_source(self, rightId, indexName, &srcId, &isIndex)) return 0;
    if (!isIndex) {
        found = Database_Get(self, rightId, key, keyLen, udt, udtLen);
        if (!found) return 0;
        return qp_dup_bytes(key, keyLen, pk, pkLen);
    }
    found = Database_Get(self, srcId, key, keyLen, pk, pkLen);
    if (!found || !pk || !*pk) return 0;
    found = Database_Get(self, rightId, *pk, *pkLen, udt, udtLen);
    if (!found) {
        free(*pk);
        *pk = NULL;
        *pkLen = 0;
        return 0;
    }
    return 1;
}

static int qp_collect_table(Database* self, int tableId, const char* alias,
                            const QueryOptimizerParsedEquality* eqs, int nEq,
                            const char* whereAlias, QueryResult* out)
{
    DatabaseScan* scan = Database_Scan(self, tableId);
    Manifest* man = Database_GetManifest(self);
    uint8_t *k = NULL, *v = NULL;
    int kl = 0, vl = 0;
    if (!scan) return GMB_DB_ERR;
    while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
        QueryResultRow row;
        if (v && qp_row_from_udt(man, tableId, v, vl, k, kl, alias, &row)) {
            if (qp_row_matches_eqs(&row, eqs, nEq, whereAlias)) {
                if (!qp_push_row(out, &row)) {
                    qp_free_row(&row);
                    free(k); free(v);
                    Database_ScanDispose(scan);
                    return GMB_DB_ERR;
                }
            } else {
                qp_free_row(&row);
            }
        }
        free(k);
        free(v);
    }
    Database_ScanDispose(scan);
    return GMB_DB_OK;
}

static int qp_execute_join(Database* self, const QueryPlan* plan, QueryResult* left, QueryResult* out,
                           SpeedExpressionEvaluator ev, void* user)
{
    Manifest* man = Database_GetManifest(self);
    int rightId = 0, i;
    char idxName[64];
    const char* rightAlias = plan->JoinClause.RightSource.Alias;
    uint8_t i64key[8];
    if (!ev) ev = SpeedExpressionEvaluator_Dummy;
    if (!man || !plan->JoinClause.RightSource.TableName[0]) return GMB_DB_ARG;
    if (!Manifest_TryGetTableByName(man, plan->JoinClause.RightSource.TableName, &rightId) || rightId <= 0)
        return GMB_DB_ERR;
    idxName[0] = 0;
    if (!QueryOptimizer_ResolveJoinIndex(man, rightId,
                                         plan->JoinClause.RightIndexName[0] ? plan->JoinClause.RightIndexName : "Id",
                                         idxName, (int)sizeof(idxName)))
        return GMB_DB_ERR;
    for (i = 0; i < left->N; i++) {
        SpeedEvalValue kv;
        const uint8_t* key;
        int keyLen, k;
        uint8_t *rudt = NULL, *rpk = NULL;
        int rlen = 0, rpkLen = 0;
        QueryResultRow rightRow, joined;
        int c;
        memset(&kv, 0, sizeof(kv));
        if (ev(plan->JoinClause.LeftKeyExpressionString, &left->Rows[i], user, &kv) != GMB_DB_OK) {
            SpeedEvalValue_Clear(&kv);
            continue;
        }
        if (kv.bytes && kv.len > 0) {
            key = kv.bytes;
            keyLen = kv.len;
        } else {
            for (k = 0; k < 8; k++) i64key[k] = (uint8_t)((kv.i64 >> (8 * k)) & 0xff);
            key = i64key;
            keyLen = 8;
        }
        if (kv.isNull || !qp_join_lookup(self, rightId, idxName, key, keyLen,
                                         &rudt, &rlen, &rpk, &rpkLen)) {
            SpeedEvalValue_Clear(&kv);
            continue;
        }
        SpeedEvalValue_Clear(&kv);
        if (!qp_row_from_udt(man, rightId, rudt, rlen, rpk, rpkLen, rightAlias, &rightRow)) {
            free(rudt); free(rpk);
            return GMB_DB_ERR;
        }
        free(rudt);
        free(rpk);
        memset(&joined, 0, sizeof(joined));
        if (left->Rows[i].PrimaryKey &&
            !qp_dup_bytes(left->Rows[i].PrimaryKey, left->Rows[i].PrimaryKeyLen,
                          &joined.PrimaryKey, &joined.PrimaryKeyLen)) {
            qp_free_row(&rightRow);
            return GMB_DB_ERR;
        }
        for (c = 0; c < left->Rows[i].CellN && joined.CellN < QUERYPLAN_MAX_PROJ; c++) {
            if (!qp_copy_cell(&joined.Cells[joined.CellN], &left->Rows[i].Cells[c])) {
                qp_free_row(&joined); qp_free_row(&rightRow); return GMB_DB_ERR;
            }
            joined.CellN++;
        }
        for (c = 0; c < rightRow.CellN && joined.CellN < QUERYPLAN_MAX_PROJ; c++) {
            if (!qp_copy_cell(&joined.Cells[joined.CellN], &rightRow.Cells[c])) {
                qp_free_row(&joined); qp_free_row(&rightRow); return GMB_DB_ERR;
            }
            joined.CellN++;
        }
        qp_free_row(&rightRow);
        if (!qp_push_row(out, &joined)) {
            qp_free_row(&joined);
            return GMB_DB_ERR;
        }
    }
    return GMB_DB_OK;
}

static int qp_parse_agg(const char* expr, char* func, int fcap, char* inner, int icap)
{
    const char* p;
    const char* e;
    int n, i;
    if (func && fcap) func[0] = 0;
    if (inner && icap) inner[0] = 0;
    if (!expr) return 0;
    p = strchr(expr, '(');
    if (!p) return 0;
    n = (int)(p - expr);
    while (n > 0 && isspace((unsigned char)expr[n - 1])) n--;
    if (n <= 0 || n >= fcap) return 0;
    for (i = 0; i < n; i++) func[i] = (char)toupper((unsigned char)expr[i]);
    func[n] = 0;
    e = strrchr(p, ')');
    if (!e || e <= p + 1) {
        if (icap) inner[0] = 0;
        return 1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    n = (int)(e - p);
    while (n > 0 && isspace((unsigned char)p[n - 1])) n--;
    if (n >= icap) n = icap - 1;
    memcpy(inner, p, (size_t)n);
    inner[n] = 0;
    return 1;
}

static int qp_set_i64(QueryResultCell* c, const char* alias, int64_t v)
{
    int i;
    snprintf(c->Alias, sizeof(c->Alias), "%s", alias ? alias : "");
    free(c->Bytes);
    c->Bytes = (uint8_t*)malloc(8);
    if (!c->Bytes) { c->Len = 0; return 0; }
    c->Len = 8;
    for (i = 0; i < 8; i++) c->Bytes[i] = (uint8_t)((v >> (8 * i)) & 0xff);
    return 1;
}

static int qp_set_dbl(QueryResultCell* c, const char* alias, double v)
{
    snprintf(c->Alias, sizeof(c->Alias), "%s", alias ? alias : "");
    free(c->Bytes);
    c->Bytes = (uint8_t*)malloc(sizeof(double));
    if (!c->Bytes) { c->Len = 0; return 0; }
    c->Len = (int)sizeof(double);
    memcpy(c->Bytes, &v, sizeof(double));
    return 1;
}

static int dummy_get_field(void* ctx, const char* name, char* out, int cap)
{
    const QueryResultRow* row = (const QueryResultRow*)ctx;
    int idx;
    if (!out || cap <= 0) return GMB_DB_ERR;
    out[0] = 0;
    idx = qp_find_cell(row, name, NULL);
    if (idx < 0) return GMB_DB_ERR;
    {
        const QueryResultCell* c = &row->Cells[idx];
        if (c->Len == 4 || c->Len == 8) {
            snprintf(out, (size_t)cap, "%lld", (long long)qp_read_le(c->Bytes, c->Len));
        } else {
            int n = c->Len < cap - 1 ? c->Len : cap - 1;
            if (n < 0) n = 0;
            if (c->Bytes && n > 0) memcpy(out, c->Bytes, (size_t)n);
            out[n] = 0;
        }
    }
    return GMB_DB_OK;
}

static int dummy_text_to_val(const char* s, gmb_val_t* out)
{
    char* end = NULL;
    double d;
    *out = gmb_val_nil();
    if (!s || !*s) return 0;
    if ((s[0] == 'T' || s[0] == 't' || s[0] == 'Y' || s[0] == 'y') && s[1] == 0) {
        *out = gmb_make_val_bool(1);
        return 0;
    }
    if ((s[0] == 'F' || s[0] == 'f' || s[0] == 'N' || s[0] == 'n') && s[1] == 0) {
        *out = gmb_make_val_bool(0);
        return 0;
    }
    d = strtod(s, &end);
    if (end && end != s && *end == 0) {
        gmb_int_t i = (gmb_int_t)d;
        if ((double)i == d) *out = gmb_make_val_int(i);
        else *out = gmb_make_val_double(d);
        return 0;
    }
    *out = gmb_make_val_string(s);
    return 0;
}

static int dummy_expr_get(void* env, const char* name, gmb_val_t* out)
{
    char buf[512];
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!name || dummy_get_field(env, name, buf, (int)sizeof(buf)) != GMB_DB_OK)
        return 0;
    return dummy_text_to_val(buf, out);
}

static int dummy_val_to_speed(gmb_val_t v, SpeedEvalValue* out)
{
    switch (v.kind) {
        case GMB_TYPE_NIL:
            out->isNull = 1;
            return GMB_DB_OK;
        case GMB_TYPE_BOOL:
            out->isBool = 1;
            out->truth = v.v_bool ? 1 : 0;
            out->i64 = out->truth;
            return GMB_DB_OK;
        case GMB_TYPE_INT:
            out->i64 = (int64_t)v.v_int;
            return GMB_DB_OK;
        case GMB_TYPE_DOUBLE:
            out->isFloat = 1;
            out->f64 = v.v_double;
            return GMB_DB_OK;
        case GMB_TYPE_STRING: {
            const char* s = v.v_string.data ? v.v_string.data : "";
            int n = (int)v.v_string.length;
            if (n <= 0) n = (int)strlen(s);
            if (!qp_dup_bytes((const uint8_t*)s, n, &out->bytes, &out->len))
                return GMB_DB_ERR;
            return GMB_DB_OK;
        }
        default:
            out->isNull = 1;
            return GMB_DB_OK;
    }
}

int SpeedExpressionEvaluator_Dummy(const char* expressionString, const QueryResultRow* rowContext,
                                   void* user, SpeedEvalValue* out)
{
    char func[16], inner[64];
    const char* src = expressionString;
    gmb_val_t v = gmb_val_nil();
    (void)user;
    /* ponytail: Dummy = hook SPEED; body = gmb_expr_eval_filter. COUNT resta 1-per-riga
     * (l'aggregato è qp_aggregate). Ceiling: niente IF/funzioni utente. */
    if (out) memset(out, 0, sizeof(*out));
    if (!expressionString || !out) return GMB_DB_ARG;
    if (!rowContext) { out->isNull = 1; return GMB_DB_OK; }
    if (strchr(expressionString, '(') &&
        qp_parse_agg(expressionString, func, (int)sizeof(func), inner, (int)sizeof(inner))) {
        if (strcmp(func, "COUNT") == 0) {
            out->i64 = 1;
            return GMB_DB_OK;
        }
        src = inner[0] ? inner : expressionString;
    }
    int erw = gmb_eval_filter_env(src, dummy_expr_get, NULL, (void*)rowContext, &v);
    if (erw != 0) erw = gmb_expr_eval_filter(src, (void*)rowContext, dummy_expr_get, NULL, &v);
    if (erw != GMB_EXPR_OK) {
        out->isNull = 1;
        return GMB_DB_OK;
    }
    return dummy_val_to_speed(v, out);
}

static int qp_is_true(const SpeedEvalValue* v)
{
    if (!v || v->isNull) return 0;
    if (v->isBool) return v->truth != 0;
    if (v->isFloat) return v->f64 != 0.0;
    if (v->bytes && v->len > 0) return 1;
    return v->i64 != 0;
}

static int qp_eval(SpeedExpressionEvaluator ev, void* user, const char* expr,
                   const QueryResultRow* row, SpeedEvalValue* out)
{
    if (!ev) ev = SpeedExpressionEvaluator_Dummy;
    if (out) memset(out, 0, sizeof(*out));
    return ev(expr, row, user, out);
}

static int qp_clone_row(const QueryResultRow* src, QueryResultRow* dst)
{
    int c;
    memset(dst, 0, sizeof(*dst));
    if (src->PrimaryKey &&
        !qp_dup_bytes(src->PrimaryKey, src->PrimaryKeyLen, &dst->PrimaryKey, &dst->PrimaryKeyLen))
        return 0;
    for (c = 0; c < src->CellN && dst->CellN < QUERYPLAN_MAX_PROJ; c++) {
        if (!qp_copy_cell(&dst->Cells[dst->CellN], &src->Cells[c])) return 0;
        dst->CellN++;
    }
    return 1;
}

static int qp_value_to_cell(const SpeedEvalValue* v, const char* alias, QueryResultCell* c)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->Alias, sizeof(c->Alias), "%s", alias ? alias : "");
    if (!v || v->isNull) return 1;
    if (v->isBool) return qp_set_i64(c, alias, v->truth ? 1 : 0);
    if (v->isFloat) return qp_set_dbl(c, alias, v->f64);
    if (v->bytes && v->len > 0) return qp_dup_bytes(v->bytes, v->len, &c->Bytes, &c->Len);
    return qp_set_i64(c, alias, v->i64);
}

/* C#: Database.Aggregate */
static int qp_aggregate(const SpeedEvalValue* values, int n, const char* expression, SpeedEvalValue* out)
{
    char func[16], inner[64];
    int i, have = 0;
    int64_t sum = 0, mn = 0, mx = 0;
    memset(out, 0, sizeof(*out));
    if (!qp_parse_agg(expression, func, (int)sizeof(func), inner, (int)sizeof(inner))) {
        out->isNull = 1;
        return GMB_DB_OK;
    }
    if (strcmp(func, "COUNT") == 0) {
        out->i64 = n;
        return GMB_DB_OK;
    }
    for (i = 0; i < n; i++) {
        int64_t v = values[i].isFloat ? (int64_t)values[i].f64 : values[i].i64;
        if (values[i].isNull) continue;
        if (!have) { mn = mx = v; have = 1; }
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    if (strcmp(func, "SUM") == 0) { out->i64 = sum; return GMB_DB_OK; }
    if (strcmp(func, "AVG") == 0) {
        out->isFloat = 1;
        out->f64 = n > 0 ? ((double)sum / (double)n) : 0.0;
        return GMB_DB_OK;
    }
    if (strcmp(func, "MIN") == 0) { out->i64 = have ? mn : 0; return GMB_DB_OK; }
    if (strcmp(func, "MAX") == 0) { out->i64 = have ? mx : 0; return GMB_DB_OK; }
    out->isNull = 1;
    return GMB_DB_OK;
}

static int qp_aggregate_group(const QueryResultRow* rows, int n, const QueryPlan* plan, QueryResultRow* out,
                              SpeedExpressionEvaluator ev, void* user)
{
    int p, i;
    memset(out, 0, sizeof(*out));
    if (n > 0 && rows[0].PrimaryKey &&
        !qp_dup_bytes(rows[0].PrimaryKey, rows[0].PrimaryKeyLen, &out->PrimaryKey, &out->PrimaryKeyLen))
        return 0;
    for (p = 0; p < plan->ProjectionN && out->CellN < QUERYPLAN_MAX_PROJ; p++) {
        const char* alias = plan->Projections[p].Alias[0] ? plan->Projections[p].Alias
                                                          : plan->Projections[p].ExpressionString;
        const char* expr = plan->Projections[p].ExpressionString;
        QueryResultCell* cell = &out->Cells[out->CellN];
        SpeedEvalValue result;
        memset(cell, 0, sizeof(*cell));
        memset(&result, 0, sizeof(result));
        if (strchr(expr, '(')) {
            SpeedEvalValue vals[256];
            int vn = n > 256 ? 256 : n;
            for (i = 0; i < vn; i++) {
                memset(&vals[i], 0, sizeof(vals[i]));
                if (qp_eval(ev, user, expr, &rows[i], &vals[i]) != GMB_DB_OK) {
                    int j;
                    for (j = 0; j <= i; j++) SpeedEvalValue_Clear(&vals[j]);
                    qp_free_row(out);
                    return 0;
                }
            }
            if (qp_aggregate(vals, vn, expr, &result) != GMB_DB_OK) {
                for (i = 0; i < vn; i++) SpeedEvalValue_Clear(&vals[i]);
                qp_free_row(out);
                return 0;
            }
            for (i = 0; i < vn; i++) SpeedEvalValue_Clear(&vals[i]);
        } else {
            if (n <= 0 || qp_eval(ev, user, expr, &rows[0], &result) != GMB_DB_OK) {
                qp_free_row(out);
                return 0;
            }
        }
        if (!qp_value_to_cell(&result, alias, cell)) {
            SpeedEvalValue_Clear(&result);
            qp_free_row(out);
            return 0;
        }
        SpeedEvalValue_Clear(&result);
        out->CellN++;
    }
    return 1;
}

static int qp_keys_equal(const QueryResultRow* a, const QueryResultRow* b, const QueryPlan* plan)
{
    int g;
    for (g = 0; g < plan->GroupByN; g++) {
        int ia = qp_find_cell(a, plan->GroupByClauseStrings[g], plan->FromSource.Alias);
        int ib = qp_find_cell(b, plan->GroupByClauseStrings[g], plan->FromSource.Alias);
        int c = qp_cmp_bytes(ia >= 0 ? a->Cells[ia].Bytes : NULL, ia >= 0 ? a->Cells[ia].Len : 0,
                             ib >= 0 ? b->Cells[ib].Bytes : NULL, ib >= 0 ? b->Cells[ib].Len : 0);
        if (c != 0) return 0;
    }
    return 1;
}

static int qp_eval_having(const QueryResultRow* row, const char* clause,
                          SpeedExpressionEvaluator ev, void* user)
{
    SpeedEvalValue v;
    int ok;
    if (!clause || !clause[0]) return 1;
    memset(&v, 0, sizeof(v));
    if (qp_eval(ev, user, clause, row, &v) != GMB_DB_OK) return 0;
    ok = qp_is_true(&v);
    SpeedEvalValue_Clear(&v);
    return ok;
}

static int qp_group_having(const QueryResult* src, const QueryPlan* plan, QueryResult* out,
                           SpeedExpressionEvaluator ev, void* user)
{
    int i, used[256];
    if (src->N > 256) return GMB_DB_ERR;
    memset(used, 0, sizeof(used));
    if (plan->GroupByN <= 0) {
        QueryResultRow agg;
        if (!qp_has_agg(plan)) {
            for (i = 0; i < src->N; i++) {
                QueryResultRow copy;
                int c;
                memset(&copy, 0, sizeof(copy));
                if (src->Rows[i].PrimaryKey &&
                    !qp_dup_bytes(src->Rows[i].PrimaryKey, src->Rows[i].PrimaryKeyLen,
                                  &copy.PrimaryKey, &copy.PrimaryKeyLen))
                    return GMB_DB_ERR;
                for (c = 0; c < src->Rows[i].CellN; c++) {
                    if (!qp_copy_cell(&copy.Cells[copy.CellN], &src->Rows[i].Cells[c])) {
                        qp_free_row(&copy); return GMB_DB_ERR;
                    }
                    copy.CellN++;
                }
                if (!qp_push_row(out, &copy)) { qp_free_row(&copy); return GMB_DB_ERR; }
            }
            return GMB_DB_OK;
        }
        if (!qp_aggregate_group(src->Rows, src->N, plan, &agg, ev, user)) return GMB_DB_ERR;
        if (!qp_eval_having(&agg, plan->HavingClauseString, ev, user)) {
            qp_free_row(&agg);
            return GMB_DB_OK;
        }
        if (!qp_push_row(out, &agg)) { qp_free_row(&agg); return GMB_DB_ERR; }
        return GMB_DB_OK;
    }
    for (i = 0; i < src->N; i++) {
        QueryResultRow members[256];
        int n = 0, j;
        QueryResultRow agg;
        if (used[i]) continue;
        members[n++] = src->Rows[i];
        used[i] = 1;
        for (j = i + 1; j < src->N; j++) {
            if (used[j]) continue;
            if (qp_keys_equal(&src->Rows[i], &src->Rows[j], plan)) {
                members[n++] = src->Rows[j];
                used[j] = 1;
            }
        }
        if (!qp_aggregate_group(members, n, plan, &agg, ev, user)) return GMB_DB_ERR;
        if (!qp_eval_having(&agg, plan->HavingClauseString, ev, user)) {
            qp_free_row(&agg);
            continue;
        }
        if (!qp_push_row(out, &agg)) { qp_free_row(&agg); return GMB_DB_ERR; }
    }
    return GMB_DB_OK;
}

static int qp_project_eval(const QueryPlan* plan, const QueryResultRow* src,
                           SpeedExpressionEvaluator ev, void* user, QueryResultRow* dst)
{
    int i;
    memset(dst, 0, sizeof(*dst));
    if (src->PrimaryKey &&
        !qp_dup_bytes(src->PrimaryKey, src->PrimaryKeyLen, &dst->PrimaryKey, &dst->PrimaryKeyLen))
        return 0;
    if (plan->ProjectionN <= 0) {
        int c;
        for (c = 0; c < src->CellN && dst->CellN < QUERYPLAN_MAX_PROJ; c++) {
            if (!qp_copy_cell(&dst->Cells[dst->CellN], &src->Cells[c])) return 0;
            dst->CellN++;
        }
        return 1;
    }
    for (i = 0; i < plan->ProjectionN && dst->CellN < QUERYPLAN_MAX_PROJ; i++) {
        SpeedEvalValue v;
        const char* alias = plan->Projections[i].Alias[0] ? plan->Projections[i].Alias
                                                          : plan->Projections[i].ExpressionString;
        memset(&v, 0, sizeof(v));
        if (qp_eval(ev, user, plan->Projections[i].ExpressionString, src, &v) != GMB_DB_OK) {
            SpeedEvalValue_Clear(&v);
            return 0;
        }
        if (!qp_value_to_cell(&v, alias, &dst->Cells[dst->CellN])) {
            SpeedEvalValue_Clear(&v);
            return 0;
        }
        SpeedEvalValue_Clear(&v);
        dst->CellN++;
    }
    return 1;
}

static int qp_where_phase(const QueryResult* src, const QueryPlan* plan,
                          SpeedExpressionEvaluator ev, void* user, QueryResult* out)
{
    int i;
    if (!plan->WhereClauseString[0]) {
        for (i = 0; i < src->N; i++) {
            QueryResultRow copy;
            if (!qp_clone_row(&src->Rows[i], &copy)) return GMB_DB_ERR;
            if (!qp_push_row(out, &copy)) { qp_free_row(&copy); return GMB_DB_ERR; }
        }
        return GMB_DB_OK;
    }
    for (i = 0; i < src->N; i++) {
        SpeedEvalValue v;
        int keep;
        memset(&v, 0, sizeof(v));
        if (qp_eval(ev, user, plan->WhereClauseString, &src->Rows[i], &v) != GMB_DB_OK) {
            SpeedEvalValue_Clear(&v);
            return GMB_DB_ERR;
        }
        keep = qp_is_true(&v);
        SpeedEvalValue_Clear(&v);
        if (!keep) continue;
        {
            QueryResultRow copy;
            if (!qp_clone_row(&src->Rows[i], &copy)) return GMB_DB_ERR;
            if (!qp_push_row(out, &copy)) { qp_free_row(&copy); return GMB_DB_ERR; }
        }
    }
    return GMB_DB_OK;
}

static void qp_order_rows_eval(QueryResult* out, const QueryPlan* plan,
                               SpeedExpressionEvaluator ev, void* user)
{
    int i, j, k;
    if (!out || plan->OrderByN <= 0) return;
    for (i = 1; i < out->N; i++) {
        QueryResultRow key = out->Rows[i];
        j = i - 1;
        while (j >= 0) {
            int less = 0, equal = 1;
            for (k = 0; k < plan->OrderByN; k++) {
                SpeedEvalValue va, vb;
                int c;
                memset(&va, 0, sizeof(va));
                memset(&vb, 0, sizeof(vb));
                if (qp_eval(ev, user, plan->OrderByClauses[k].ExpressionString, &out->Rows[j], &va) != GMB_DB_OK ||
                    qp_eval(ev, user, plan->OrderByClauses[k].ExpressionString, &key, &vb) != GMB_DB_OK) {
                    SpeedEvalValue_Clear(&va);
                    SpeedEvalValue_Clear(&vb);
                    equal = 0;
                    less = 0;
                    break;
                }
                if (va.bytes || vb.bytes)
                    c = qp_cmp_bytes(va.bytes, va.len, vb.bytes, vb.len);
                else if (va.isFloat || vb.isFloat)
                    c = (va.f64 < vb.f64) ? -1 : (va.f64 > vb.f64 ? 1 : 0);
                else
                    c = (va.i64 < vb.i64) ? -1 : (va.i64 > vb.i64 ? 1 : 0);
                SpeedEvalValue_Clear(&va);
                SpeedEvalValue_Clear(&vb);
                if (c == 0) continue;
                equal = 0;
                less = plan->OrderByClauses[k].Descending ? (c < 0) : (c > 0);
                break;
            }
            if (equal || !less) break;
            out->Rows[j + 1] = out->Rows[j];
            j--;
        }
        out->Rows[j + 1] = key;
    }
}

static int qp_try_indexed_source(Database* self, const QueryPlan* plan, int tableId,
                                 const char* qualifyAlias, QueryResult* out, int* applied)
{
    Manifest* man = Database_GetManifest(self);
    QueryOptimizerParsedEquality eqs[QueryOptimizer_MaxEqualities];
    int nEq = 0, rc;
    uint8_t* val = NULL;
    int vlen = 0;
    *applied = 0;
    if (plan->JoinClause.Present || !plan->WhereClauseString[0]) return 0;
    if (!QueryOptimizer_TryParseCompoundAndEqualities(plan->WhereClauseString,
                                                      plan->FromSource.Alias[0] ? plan->FromSource.Alias : NULL,
                                                      eqs, QueryOptimizer_MaxEqualities, &nEq) || nEq <= 0)
        return 0;
    rc = Database_TrySeekIndexedEquality(self, tableId, plan->WhereClauseString, &val, &vlen);
    if (rc == 0) {
        free(val);
        return GMB_DB_OK;
    }
    *applied = 1;
    if (rc == 1 && val && qp_row_matches(man, tableId, val, vlen, eqs, nEq)) {
        QueryResultRow row;
        if (!qp_row_from_udt(man, tableId, val, vlen, NULL, 0, qualifyAlias, &row)) {
            free(val);
            return GMB_DB_ERR;
        }
        if (!qp_push_row(out, &row)) {
            qp_free_row(&row);
            free(val);
            return GMB_DB_ERR;
        }
    }
    free(val);
    return GMB_DB_OK;
}

static int qp_execute_complex(Database* self, const QueryPlan* plan,
                              SpeedExpressionEvaluator ev, void* user, QueryResult* out)
{
    Manifest* man = Database_GetManifest(self);
    int leftId = 0, rc, whereByIndex = 0;
    QueryResult src, filtered, grouped;
    const char* leftAlias;
    memset(&src, 0, sizeof(src));
    memset(&filtered, 0, sizeof(filtered));
    memset(&grouped, 0, sizeof(grouped));
    if (!ev) ev = SpeedExpressionEvaluator_Dummy;
    if (!man || !plan->FromSource.TableName[0]) return GMB_DB_ARG;
    if (!Manifest_TryGetTableByName(man, plan->FromSource.TableName, &leftId) || leftId <= 0)
        return GMB_DB_ERR;
    leftAlias = plan->JoinClause.Present
        ? (plan->FromSource.Alias[0] ? plan->FromSource.Alias : plan->FromSource.TableName)
        : (plan->FromSource.Alias[0] ? plan->FromSource.Alias : NULL);

    rc = qp_try_indexed_source(self, plan, leftId, leftAlias, &src, &whereByIndex);
    if (rc != GMB_DB_OK) { QueryResult_Dispose(&src); return rc; }
    if (!whereByIndex) {
        rc = qp_collect_table(self, leftId, leftAlias, NULL, 0,
                              plan->FromSource.Alias[0] ? plan->FromSource.Alias : NULL, &src);
        if (rc != GMB_DB_OK) { QueryResult_Dispose(&src); return rc; }
    }

    if (plan->JoinClause.Present) {
        QueryResult joined;
        memset(&joined, 0, sizeof(joined));
        rc = qp_execute_join(self, plan, &src, &joined, ev, user);
        QueryResult_Dispose(&src);
        if (rc != GMB_DB_OK) { QueryResult_Dispose(&joined); return rc; }
        src = joined;
        whereByIndex = 0;
    }

    if (whereByIndex) {
        filtered = src;
        memset(&src, 0, sizeof(src));
    } else {
        rc = qp_where_phase(&src, plan, ev, user, &filtered);
        QueryResult_Dispose(&src);
        if (rc != GMB_DB_OK) { QueryResult_Dispose(&filtered); return rc; }
    }

    if (plan->GroupByN > 0 || qp_has_agg(plan)) {
        rc = qp_group_having(&filtered, plan, &grouped, ev, user);
        QueryResult_Dispose(&filtered);
        if (rc != GMB_DB_OK) { QueryResult_Dispose(&grouped); return rc; }
        *out = grouped;
        qp_order_rows_eval(out, plan, ev, user);
        return GMB_DB_OK;
    }

    qp_order_rows_eval(&filtered, plan, ev, user);
    {
        int i;
        for (i = 0; i < filtered.N; i++) {
            QueryResultRow proj;
            if (!qp_project_eval(plan, &filtered.Rows[i], ev, user, &proj)) {
                qp_free_row(&proj); QueryResult_Dispose(&filtered); QueryResult_Dispose(out); return GMB_DB_ERR;
            }
            if (!qp_push_row(out, &proj)) {
                qp_free_row(&proj); QueryResult_Dispose(&filtered); QueryResult_Dispose(out); return GMB_DB_ERR;
            }
        }
    }
    QueryResult_Dispose(&filtered);
    return GMB_DB_OK;
}

int Database_ExecuteQueryEval(Database* self, const QueryPlan* plan,
                              SpeedExpressionEvaluator evaluator, void* evalUser,
                              QueryResult* out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!self || !plan || !out) return GMB_DB_ARG;
    return qp_execute_complex(self, plan, evaluator, evalUser, out);
}

int Database_ExecuteQuery(Database* self, const QueryPlan* plan, QueryResult* out)
{
    return Database_ExecuteQueryEval(self, plan, SpeedExpressionEvaluator_Dummy, NULL, out);
}

int Database_RefreshViewEval(Database* self, const char* viewName, const QueryPlan* plan,
                             SpeedExpressionEvaluator evaluator, void* evalUser)
{
    Manifest* man;
    QueryResult result;
    char names[QUERYPLAN_MAX_PROJ][64];
    char types[QUERYPLAN_MAX_PROJ][32];
    int lens[QUERYPLAN_MAX_PROJ];
    int fieldN = 0, i, oldId = 0, newId, rc;
    if (!self || !viewName || !plan) return GMB_DB_ARG;
    if (!evaluator) evaluator = SpeedExpressionEvaluator_Dummy;
    man = Database_GetManifest(self);
    if (!man || !Manifest_TryGetTableByName(man, viewName, &oldId) || oldId <= 0)
        return GMB_DB_ERR;
    fieldN = Manifest_FieldCount(man, oldId);
    for (i = 0; i < fieldN && i < QUERYPLAN_MAX_PROJ; i++) {
        if (!Manifest_GetField(man, oldId, i, names[i], 64, types[i], 32, &lens[i]))
            return GMB_DB_ERR;
    }
    if (Database_DropTableInternal(self, viewName) != GMB_DB_OK) return GMB_DB_ERR;
    newId = Database_CreateTable(self, viewName);
    if (newId <= 0) return GMB_DB_ERR;
    for (i = 0; i < fieldN; i++) {
        if (Manifest_AddField(man, newId, names[i], types[i], lens[i]) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    rc = Database_ExecuteQueryEval(self, plan, evaluator, evalUser, &result);
    if (rc != GMB_DB_OK) {
        QueryResult_Dispose(&result);
        return rc;
    }
    Database_BeginTransaction(self);
    for (i = 0; i < result.N; i++) {
        const uint8_t* fvals[QUERYPLAN_MAX_PROJ];
        int flens[QUERYPLAN_MAX_PROJ];
        uint8_t* udt = NULL;
        int udtLen = 0, f;
        const uint8_t* pk;
        int pkLen;
        for (f = 0; f < fieldN; f++) {
            int c = qp_cell_index(&result.Rows[i], names[f]);
            fvals[f] = (c >= 0) ? result.Rows[i].Cells[c].Bytes : NULL;
            flens[f] = (c >= 0) ? result.Rows[i].Cells[c].Len : 0;
        }
        if (!SpeedMarshaller_SerializeUdt(man, newId, fvals, flens, &udt, &udtLen)) {
            Database_Rollback(self);
            QueryResult_Dispose(&result);
            return GMB_DB_ERR;
        }
        pk = result.Rows[i].PrimaryKey;
        pkLen = result.Rows[i].PrimaryKeyLen;
        if (!pk || pkLen <= 0) {
            pk = fvals[0];
            pkLen = flens[0];
        }
        if (!pk || pkLen <= 0 ||
            Database_Set(self, newId, pk, pkLen, udt, udtLen) != GMB_DB_OK) {
            free(udt);
            Database_Rollback(self);
            QueryResult_Dispose(&result);
            return GMB_DB_ERR;
        }
        free(udt);
    }
    QueryResult_Dispose(&result);
    return Database_Commit(self);
}

int Database_RefreshView(Database* self, const char* viewName, const QueryPlan* plan)
{
    return Database_RefreshViewEval(self, viewName, plan, SpeedExpressionEvaluator_Dummy, NULL);
}

/* Nested Query* types C# — in C sono struct in gmb_db_port.h, non API void. */

static int qp_put_emp(Database* db, int tableId, const char* pk, const char* name, int age, const char* dept)
{
    Manifest* man = Database_GetManifest(db);
    const uint8_t* fields[3];
    int flens[3];
    uint8_t ageb[4], *udt = NULL;
    int n, rc;
    uint32_t a = (uint32_t)age;
    ageb[0] = (uint8_t)(a & 0xff);
    ageb[1] = (uint8_t)((a >> 8) & 0xff);
    ageb[2] = (uint8_t)((a >> 16) & 0xff);
    ageb[3] = (uint8_t)((a >> 24) & 0xff);
    fields[0] = (const uint8_t*)name;
    flens[0] = (int)strlen(name);
    fields[1] = ageb;
    flens[1] = 4;
    fields[2] = (const uint8_t*)dept;
    flens[2] = (int)strlen(dept);
    if (!SpeedMarshaller_SerializeUdt(man, tableId, fields, flens, &udt, &n)) return 0;
    rc = Database_Set(db, tableId, (const uint8_t*)pk, (int)strlen(pk), udt, n);
    free(udt);
    return rc == GMB_DB_OK;
}

static int qp_put_dept(Database* db, int tableId, const char* name, int floor)
{
    Manifest* man = Database_GetManifest(db);
    const uint8_t* fields[2];
    int flens[2];
    uint8_t flb[4], *udt = NULL;
    int n, rc;
    uint32_t a = (uint32_t)floor;
    flb[0] = (uint8_t)(a & 0xff);
    flb[1] = (uint8_t)((a >> 8) & 0xff);
    flb[2] = (uint8_t)((a >> 16) & 0xff);
    flb[3] = (uint8_t)((a >> 24) & 0xff);
    fields[0] = (const uint8_t*)name;
    flens[0] = (int)strlen(name);
    fields[1] = flb;
    flens[1] = 4;
    if (!SpeedMarshaller_SerializeUdt(man, tableId, fields, flens, &udt, &n)) return 0;
    rc = Database_Set(db, tableId, (const uint8_t*)name, (int)strlen(name), udt, n);
    free(udt);
    return rc == GMB_DB_OK;
}

static int qp_put_person(Database* db, int tableId, const char* pk, const char* name, int age)
{
    Manifest* man = Database_GetManifest(db);
    const uint8_t* fields[2];
    int flens[2];
    uint8_t ageb[4], *udt = NULL;
    int n, rc;
    uint32_t a = (uint32_t)age;
    ageb[0] = (uint8_t)(a & 0xff);
    ageb[1] = (uint8_t)((a >> 8) & 0xff);
    ageb[2] = (uint8_t)((a >> 16) & 0xff);
    ageb[3] = (uint8_t)((a >> 24) & 0xff);
    fields[0] = (const uint8_t*)name;
    flens[0] = (int)strlen(name);
    fields[1] = ageb;
    flens[1] = 4;
    if (!SpeedMarshaller_SerializeUdt(man, tableId, fields, flens, &udt, &n)) return 0;
    rc = Database_Set(db, tableId, (const uint8_t*)pk, (int)strlen(pk), udt, n);
    free(udt);
    return rc == GMB_DB_OK;
}

int QueryPlan_SelfCheck(void)
{
    char dir[] = "/tmp/gmb_speed_qp_XXXXXX";
    Database* db;
    Manifest* man;
    int tableId, viewId, n, found, vlen, empId, deptId;
    QueryPlan plan;
    QueryResult result;
    uint8_t* val = NULL;

    if (!mkdtemp(dir)) return -140;
    db = Database_Open(dir);
    if (!db) return -141;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -142; }
    man = Database_GetManifest(db);
    if (Manifest_AddField(man, tableId, "Name", "StringType", 0) != GMB_DB_OK ||
        Manifest_AddField(man, tableId, "Age", "Long", 0) != GMB_DB_OK) {
        Database_Dispose(db); return -143;
    }
    if (!qp_put_person(db, tableId, "k001", "alice", 42) ||
        !qp_put_person(db, tableId, "k002", "bob", 7)) {
        Database_Dispose(db); return -144;
    }
    if (Database_CreateSecondaryIndex(db, tableId, "ByName", "Name", 0) <= 0) {
        Database_Dispose(db); return -145;
    }

    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Person");
    snprintf(plan.WhereClauseString, sizeof(plan.WhereClauseString), "Name = 'alice'");
    if (QueryPlan_AddProjection(&plan, "Name", "Name") != GMB_DB_OK) {
        Database_Dispose(db); return -146;
    }
    if (Database_ExecuteQuery(db, &plan, &result) != GMB_DB_OK) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -147;
    }
    if (result.N != 1 || result.Rows[0].CellN < 1 ||
        result.Rows[0].Cells[0].Len != 5 ||
        memcmp(result.Rows[0].Cells[0].Bytes, "alice", 5) != 0) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -148;
    }
    QueryResult_Dispose(&result);

    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Person");
    if (QueryPlan_AddProjection(&plan, "Name", "Name") != GMB_DB_OK) {
        Database_Dispose(db); return -149;
    }
    plan.OrderByN = 1;
    snprintf(plan.OrderByClauses[0].ExpressionString, sizeof(plan.OrderByClauses[0].ExpressionString), "Name");
    plan.OrderByClauses[0].Descending = 1;
    if (Database_ExecuteQuery(db, &plan, &result) != GMB_DB_OK) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -150;
    }
    if (result.N != 2 || result.Rows[0].Cells[0].Len != 3 ||
        memcmp(result.Rows[0].Cells[0].Bytes, "bob", 3) != 0 ||
        result.Rows[1].Cells[0].Len != 5 ||
        memcmp(result.Rows[1].Cells[0].Bytes, "alice", 5) != 0) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -151;
    }
    QueryResult_Dispose(&result);

    QueryResult_Dispose(&result);

    viewId = Database_CreateTable(db, "VPerson");
    if (viewId <= 0) { Database_Dispose(db); return -153; }
    if (Manifest_AddField(man, viewId, "Name", "StringType", 0) != GMB_DB_OK) {
        Database_Dispose(db); return -154;
    }
    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Person");
    if (QueryPlan_AddProjection(&plan, "Name", "Name") != GMB_DB_OK) {
        Database_Dispose(db); return -155;
    }
    if (Database_RefreshView(db, "VPerson", &plan) != GMB_DB_OK) {
        Database_Dispose(db); return -156;
    }
    if (!Manifest_TryGetTableByName(Database_GetManifest(db), "VPerson", &viewId) || viewId <= 0) {
        Database_Dispose(db); return -157;
    }
    n = 0;
    {
        DatabaseScan* scan = Database_Scan(db, viewId);
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            n++;
            free(k); free(v);
        }
        Database_ScanDispose(scan);
    }
    if (n != 2) { Database_Dispose(db); return -158; }
    found = Database_Get(db, viewId, (const uint8_t*)"k001", 4, &val, &vlen);
    if (!found || !val) { free(val); Database_Dispose(db); return -159; }
    free(val);

    empId = Database_CreateTable(db, "Emp");
    deptId = Database_CreateTable(db, "Dept");
    if (empId <= 0 || deptId <= 0) { Database_Dispose(db); return -160; }
    if (Manifest_AddField(man, empId, "Name", "StringType", 0) != GMB_DB_OK ||
        Manifest_AddField(man, empId, "Age", "Long", 0) != GMB_DB_OK ||
        Manifest_AddField(man, empId, "Dept", "StringType", 0) != GMB_DB_OK ||
        Manifest_AddField(man, deptId, "Name", "StringType", 0) != GMB_DB_OK ||
        Manifest_AddField(man, deptId, "Floor", "Long", 0) != GMB_DB_OK) {
        Database_Dispose(db); return -161;
    }
    if (!qp_put_emp(db, empId, "e1", "alice", 42, "eng") ||
        !qp_put_emp(db, empId, "e2", "bob", 7, "eng") ||
        !qp_put_emp(db, empId, "e3", "carol", 9, "hr") ||
        !qp_put_dept(db, deptId, "eng", 1) ||
        !qp_put_dept(db, deptId, "hr", 2)) {
        Database_Dispose(db); return -162;
    }
    if (Database_CreateSecondaryIndex(db, deptId, "ByName", "Name", 0) <= 0) {
        Database_Dispose(db); return -163;
    }

    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Emp");
    snprintf(plan.FromSource.Alias, sizeof(plan.FromSource.Alias), "e");
    plan.JoinClause.Present = 1;
    snprintf(plan.JoinClause.RightSource.TableName, sizeof(plan.JoinClause.RightSource.TableName), "Dept");
    snprintf(plan.JoinClause.RightSource.Alias, sizeof(plan.JoinClause.RightSource.Alias), "d");
    snprintf(plan.JoinClause.LeftKeyExpressionString, sizeof(plan.JoinClause.LeftKeyExpressionString), "Dept");
    snprintf(plan.JoinClause.RightIndexName, sizeof(plan.JoinClause.RightIndexName), "Name");
    if (QueryPlan_AddProjection(&plan, "e.Name", "Name") != GMB_DB_OK ||
        QueryPlan_AddProjection(&plan, "d.Floor", "Floor") != GMB_DB_OK) {
        Database_Dispose(db); return -164;
    }
    if (Database_ExecuteQuery(db, &plan, &result) != GMB_DB_OK) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -165;
    }
    if (result.N != 3) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -166;
    }
    {
        int i, saw = 0;
        for (i = 0; i < result.N; i++) {
            if (result.Rows[i].CellN >= 2 && result.Rows[i].Cells[0].Len == 5 &&
                memcmp(result.Rows[i].Cells[0].Bytes, "alice", 5) == 0 &&
                qp_read_le(result.Rows[i].Cells[1].Bytes, result.Rows[i].Cells[1].Len) == 1)
                saw = 1;
        }
        if (!saw) {
            QueryResult_Dispose(&result); Database_Dispose(db); return -167;
        }
    }
    QueryResult_Dispose(&result);

    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Emp");
    snprintf(plan.GroupByClauseStrings[0], sizeof(plan.GroupByClauseStrings[0]), "Dept");
    plan.GroupByN = 1;
    snprintf(plan.HavingClauseString, sizeof(plan.HavingClauseString), "Cnt > 1");
    if (QueryPlan_AddProjection(&plan, "Dept", "Dept") != GMB_DB_OK ||
        QueryPlan_AddProjection(&plan, "COUNT(*)", "Cnt") != GMB_DB_OK ||
        QueryPlan_AddProjection(&plan, "SUM(Age)", "Ages") != GMB_DB_OK) {
        Database_Dispose(db); return -168;
    }
    if (Database_ExecuteQuery(db, &plan, &result) != GMB_DB_OK) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -169;
    }
    if (result.N != 1 || result.Rows[0].CellN < 3 ||
        result.Rows[0].Cells[0].Len != 3 || memcmp(result.Rows[0].Cells[0].Bytes, "eng", 3) != 0 ||
        qp_read_le(result.Rows[0].Cells[1].Bytes, result.Rows[0].Cells[1].Len) != 2 ||
        qp_read_le(result.Rows[0].Cells[2].Bytes, result.Rows[0].Cells[2].Len) != 49) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -170;
    }
    QueryResult_Dispose(&result);

    QueryPlan_Init(&plan);
    snprintf(plan.FromSource.TableName, sizeof(plan.FromSource.TableName), "Emp");
    snprintf(plan.WhereClauseString, sizeof(plan.WhereClauseString), "Dept = 'hr'");
    if (QueryPlan_AddProjection(&plan, "COUNT(*)", "N") != GMB_DB_OK) {
        Database_Dispose(db); return -171;
    }
    if (Database_ExecuteQuery(db, &plan, &result) != GMB_DB_OK) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -172;
    }
    if (result.N != 1 || qp_read_le(result.Rows[0].Cells[0].Bytes, result.Rows[0].Cells[0].Len) != 1) {
        QueryResult_Dispose(&result); Database_Dispose(db); return -173;
    }
    QueryResult_Dispose(&result);

    Database_Dispose(db);
    return GMB_DB_OK;
}
