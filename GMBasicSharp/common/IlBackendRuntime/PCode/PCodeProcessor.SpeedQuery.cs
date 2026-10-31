// FILE: Basic.IlBackend.PCode.PCodeProcessor.SpeedQuery.cs
//
// Implementazione self-contained delle SPEED syscall rimaste no-op (CREATE INDEX, SET FILTER,
// SET RELATION, SELECT, CREATE VIEW, REFRESH VIEW): un piccolo motore di query/evaluator che
// opera DIRETTAMENTE sul formato JSON già usato da SPEED.PUT/SPEED.GET (BytesToSpeedRecord),
// SENZA passare da Database.ExecuteQuery/CreateSecondaryIndex/SpeedMarshaller. Quei metodi si
// aspettano un formato binario a schema fisso, ma SPEED_USE crea le tabelle con uno schema
// fittizio a un solo campo ("__key__") e SPEED_PUT/GET scrivono/leggono JSON — instradare
// CREATE INDEX/SELECT attraverso le API schema-based avrebbe fatto fallire silenziosamente la
// (de)serializzazione di ogni record inserito via PUT e, peggio, avrebbe fatto sì che
// Database.Set() lanciasse un'eccezione (silenziata dal try/catch del chiamante, ma comunque
// uno spreco/rischio non necessario) ad ogni singolo PUT successivo alla creazione di un indice
// reale su quella tabella. Questo motore resta quindi intenzionalmente isolato da quello
// "reale": nessuna delle strutture qui sotto tocca Manifest/Schema/SpeedMarshaller.

using System.Globalization;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    // area/tableId → lista di (nome indice, nome campo). Solo metadata: il JOIN usa un
    // nested-loop scan diretto sui campi, non serve una vera struttura indicizzata alla scala
    // dei dataset di test di questo motore.
    private readonly Dictionary<int, List<(string Name, string FieldName)>> _speedIndexes = new();

    // tableId → predicato di filtro (Expr grezzo dal compilatore, o già "compilato" in un
    // RuntimeExpressionEvaluator se la sintassi qualificata passava una stringa macro).
    private readonly Dictionary<int, Func<Dictionary<string, object?>, bool>> _speedFilters = new();

    // tableId padre → (nome campo chiave, tableId figlio). Solo storage: nessuna sincronizzazione
    // automatica del cursore figlio durante la navigazione (non testata da alcun sample).
    private readonly Dictionary<int, (string KeyField, int ChildTableId)> _speedRelations = new();

    // nome vista → (statement SELECT statico, tableId della tabella FROM). Permette a SPEED
    // REFRESH VIEW di ritrovare la query originale.
    private readonly Dictionary<string, (SpeedSelectStmt Stmt, int FromTableId)> _speedViews =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Trova il SpeedDriver registrato per un dato tableId (per risalire dal figlio di
    /// una relazione, di cui _speedRelations conosce solo il tableId, al driver/alias reale).</summary>
    private Basic.Databases.Rdd.SpeedDriver? SpeedFindDriverByTableId(int tableId)
    {
        for (int a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
        {
            if (_dataSession.GetWorkArea(a)?.Driver is Basic.Databases.Rdd.SpeedDriver sd && sd._tableId == tableId)
                return sd;
        }
        return null;
    }

    /// <summary>Applica il filtro SPEED SET FILTER attivo (se presente) alla posizione corrente
    /// del cursore dopo un GO/SKIP: avanza (o retrocede) finché il record corrente non soddisfa
    /// il predicato o si raggiunge la fine della tabella. Va chiamato SUBITO dopo ogni
    /// riposizionamento del cursore (GoTop/GoBottom/Skip) — senza, SET FILTER restava storage
    /// puro senza alcun effetto sulla navigazione.</summary>
    private void SpeedApplyFilter(Basic.Databases.Rdd.SpeedDriver sd, int direction)
    {
        if (!_speedFilters.TryGetValue(sd._tableId, out var filter)) return;
        // Limite di sicurezza: evita un loop indefinito se lo scan non converge mai a EOF/BOF per
        // qualche motivo imprevisto (difesa, non atteso in condizioni normali).
        for (int guard = 0; guard < 1_000_000; guard++)
        {
            var bytes = sd._db!.GetCurrent(sd._tableId);
            if (bytes == null) return; // EOF/BOF: nessun record (più) da valutare
            if (filter(BytesToSpeedRecord(bytes))) return; // record corrente valido: fermati qui
            sd._db.Skip(sd._tableId, direction);
        }
    }

    /// <summary>Riposiziona il cursore della tabella figlia di una relazione SPEED SET RELATION
    /// (se presente) sul record la cui chiave corrisponde al campo chiave del record CORRENTE
    /// della tabella padre — usando l'ordine attivo del figlio per la SEEK, stesso schema di
    /// SPEED_SEEK. Va chiamato SUBITO dopo ogni riposizionamento del cursore padre.</summary>
    private void SpeedSyncChildRelation(Basic.Databases.Rdd.SpeedDriver parentSd)
    {
        if (!_speedRelations.TryGetValue(parentSd._tableId, out var rel)) return;
        var childSd = SpeedFindDriverByTableId(rel.ChildTableId);
        if (childSd?._db == null) return;
        var bytes = parentSd._db!.GetCurrent(parentSd._tableId);
        if (bytes == null) return;
        var row = BytesToSpeedRecord(bytes);
        if (!row.TryGetValue(rel.KeyField, out var keyVal) || keyVal == null) return;
        try { childSd._db.Seek(childSd._tableId, SpeedValueToBytes(keyVal), null); } catch { }
    }

    /// <summary>Nome del campo chiave primaria dichiarato via INDEXED BY per una tabella SPEED
    /// (registrato da SPEED USE in wa.Meta["PkField"]), o null se non dichiarato — in tal caso
    /// SPEED_PUT/SPEED_DELETE_RECORD ricadono sul fallback "ID".</summary>
    private string? SpeedResolvePkField(Basic.Databases.Rdd.SpeedDriver sd)
    {
        for (int a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
        {
            var wa = _dataSession.GetWorkArea(a);
            if (wa?.Driver == sd) return wa.Meta.GetValueOrDefault("PkField") as string;
        }
        return null;
    }

    /// <summary>(Nome campo, SymbolId compilatore) dell'UDT associato a una tabella SPEED
    /// (registrato da SPEED USE in wa.Meta["FieldSymbols"]) — servono a SPEED_GET per
    /// costruire il wrapper "__class__"/FieldNames che OP_LOAD_MEMBER richiede.</summary>
    private List<(string Name, int SymbolId)>? SpeedResolveFieldSymbols(Basic.Databases.Rdd.SpeedDriver sd)
    {
        for (int a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
        {
            var wa = _dataSession.GetWorkArea(a);
            if (wa?.Driver == sd) return wa.Meta.GetValueOrDefault("FieldSymbols") as List<(string Name, int SymbolId)>;
        }
        return null;
    }

    /// <summary>
    /// Applica a ogni riga risultato di una SELECT lo stesso wrapper "__class__"/FieldNames
    /// costruito da SPEED_GET (altrimenti "ris(i).Campo" dopo una SELECT risolve sempre a NIL,
    /// perché OP_LOAD_MEMBER richiede quella struttura) — usando i SymbolId di campo registrati
    /// per l'alias FROM da SPEED USE. Copre il caso comune (proiezione * o comunque nomi di
    /// colonna coincidenti con i campi della tabella FROM); per una SELECT con JOIN/alias di
    /// colonna calcolati che non coincidono con nessun campo dell'UDT sorgente, quei nomi
    /// restano semplicemente fuori dal wrapper (nessun SymbolId noto per loro) — stesso limite
    /// preesistente di SPEED_GET, non specifico di questa funzione.
    /// </summary>
    private List<Dictionary<string, object?>> SpeedWrapRowsForUdt(
        List<Dictionary<string, object?>> rows, Basic.Databases.Rdd.SpeedDriver fromSd)
    {
        var fieldSymbols = SpeedResolveFieldSymbols(fromSd);
        if (fieldSymbols == null || fieldSymbols.Count == 0) return rows;

        var fieldNames = new Dictionary<int, string>();
        foreach (var (name, symId) in fieldSymbols) fieldNames[symId] = name;
        var classInfo = new Dictionary<string, object?> { ["FieldNames"] = fieldNames };

        var wrappedRows = new List<Dictionary<string, object?>>(rows.Count);
        foreach (var row in rows)
        {
            var wrapped = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase) { ["__class__"] = classInfo };
            foreach (var (name, _) in fieldSymbols)
                wrapped[name] = row.TryGetValue(name, out var v) ? v : null;
            wrappedRows.Add(wrapped);
        }
        return wrappedRows;
    }

    // ═══════════════════════════════ Evaluator di espressioni ═══════════════════════════════

    /// <summary>
    /// Valuta un nodo AST direttamente (senza passare da P-Code) contro il dizionario di una
    /// riga. I riferimenti a campo qualificati (Alias.Campo, da un JOIN) vengono cercati per
    /// chiave "Alias.Campo"; quelli non qualificati per chiave nuda, con fallback fra i due —
    /// così la stessa espressione funziona sia su una riga a tabella singola sia su una riga
    /// unita da un JOIN.
    /// </summary>
    private static object? SpeedEvaluateExpr(Expr? expr, Dictionary<string, object?> row)
    {
        switch (expr)
        {
            case null: return null;
            case NumberExpr n: return n.Value;
            case StringExpr s: return s.Value;
            case CheckedStringExpr cs: return cs.Value;
            case BooleanExpr b: return b.Value;
            case NilExpr: return null;
            case GroupingExpr g: return SpeedEvaluateExpr(g.Expression, row);
            case VariableExpr v:
                return row.TryGetValue(v.NameCleaned.Lexeme, out var direct) ? direct : null;
            case FieldAccessExpr f:
            {
                var fieldName = f.Field.Lexeme;
                if (f.Structure is VariableExpr alias)
                {
                    var qualified = alias.NameCleaned.Lexeme + "." + fieldName;
                    if (row.TryGetValue(qualified, out var qv)) return qv;
                }
                return row.TryGetValue(fieldName, out var uv) ? uv : null;
            }
            case UnaryExpr u:
            {
                var val = SpeedEvaluateExpr(u.Right, row);
                return u.Op.Kind switch
                {
                    TokenKind.Not => !SpeedIsTrue(val),
                    TokenKind.Minus => -SpeedToDouble(val),
                    _ => val
                };
            }
            case BinaryExpr bin:
            {
                if (bin.Op.Kind == TokenKind.And)
                    return SpeedIsTrue(SpeedEvaluateExpr(bin.Left, row)) && SpeedIsTrue(SpeedEvaluateExpr(bin.Right, row));
                if (bin.Op.Kind == TokenKind.Or)
                    return SpeedIsTrue(SpeedEvaluateExpr(bin.Left, row)) || SpeedIsTrue(SpeedEvaluateExpr(bin.Right, row));

                var l = SpeedEvaluateExpr(bin.Left, row);
                var r = SpeedEvaluateExpr(bin.Right, row);
                return bin.Op.Kind switch
                {
                    TokenKind.Plus => (l is string || r is string)
                        ? Convert.ToString(l, CultureInfo.InvariantCulture) + Convert.ToString(r, CultureInfo.InvariantCulture)
                        : SpeedToDouble(l) + SpeedToDouble(r),
                    TokenKind.Minus => SpeedToDouble(l) - SpeedToDouble(r),
                    TokenKind.Multiply => SpeedToDouble(l) * SpeedToDouble(r),
                    TokenKind.Divide => SpeedToDouble(l) / SpeedToDouble(r),
                    TokenKind.Equal or TokenKind.ExactlyEqual => SpeedCompare(l, r) == 0,
                    TokenKind.NotEqual => SpeedCompare(l, r) != 0,
                    TokenKind.LessThan => SpeedCompare(l, r) < 0,
                    TokenKind.LessEqual => SpeedCompare(l, r) <= 0,
                    TokenKind.GreaterThan => SpeedCompare(l, r) > 0,
                    TokenKind.GreaterEqual => SpeedCompare(l, r) >= 0,
                    _ => null
                };
            }
            default:
                return null;
        }
    }

    private static bool SpeedIsTrue(object? v) => v switch
    {
        null => false,
        bool b => b,
        double d => d != 0.0,
        string s => !string.IsNullOrEmpty(s),
        _ => true
    };

    private static double SpeedToDouble(object? v)
    {
        if (v == null) return 0.0;
        try { return Convert.ToDouble(v, CultureInfo.InvariantCulture); }
        catch { return 0.0; }
    }

    private static int SpeedCompare(object? a, object? b)
    {
        if (a == null && b == null) return 0;
        if (a == null) return -1;
        if (b == null) return 1;
        if (a is string sa && b is string sb) return string.Compare(sa, sb, StringComparison.OrdinalIgnoreCase);
        if (a is bool ba && b is bool bb) return ba.CompareTo(bb);
        try
        {
            return Convert.ToDouble(a, CultureInfo.InvariantCulture)
                .CompareTo(Convert.ToDouble(b, CultureInfo.InvariantCulture));
        }
        catch
        {
            return string.Compare(a.ToString(), b.ToString(), StringComparison.OrdinalIgnoreCase);
        }
    }

    /// <summary>Riconosce COUNT/SUM/AVG/MIN/MAX(inner) come proiezione aggregata.</summary>
    private static bool SpeedTryGetAggregate(Expr expr, out string funcName, out Expr? inner)
    {
        funcName = "";
        inner = null;
        if (expr is UnresolvedCallExpr call)
        {
            var name = call.Name.Lexeme.ToUpperInvariant();
            if (name is "SUM" or "COUNT" or "AVG" or "MIN" or "MAX")
            {
                funcName = name;
                inner = call.Arguments.Length > 0 ? call.Arguments[0] : null;
                return true;
            }
        }
        return false;
    }

    private static object? SpeedAggregate(string funcName, List<object?> values)
    {
        var numeric = values.Where(v => v != null).Select(SpeedToDouble).ToList();
        return funcName switch
        {
            "SUM" => numeric.Sum(),
            "AVG" => numeric.Count > 0 ? numeric.Average() : 0.0,
            "MIN" => numeric.Count > 0 ? numeric.Min() : null,
            "MAX" => numeric.Count > 0 ? numeric.Max() : null,
            _ => null
        };
    }

    private static string SpeedFieldDisplayName(Expr expr) => expr switch
    {
        FieldAccessExpr f => f.Field.Lexeme,
        VariableExpr v => v.NameCleaned.Lexeme,
        UnresolvedCallExpr c => c.Name.Lexeme,
        _ => "expr"
    };

    /// <summary>Estrae il nome campo nudo da un riferimento semplice (VariableExpr o FieldAccessExpr),
    /// usato da CREATE INDEX/SET RELATION per la loro forma comune "solo nome campo".</summary>
    private static string? SpeedPlainFieldName(object? exprOrString) => exprOrString switch
    {
        string s => s,
        StringExpr se => se.Value,
        FieldAccessExpr f => f.Field.Lexeme,
        VariableExpr v => v.NameCleaned.Lexeme,
        _ => null
    };

    /// <summary>Equivalente a runtime di Compiler.DecomposeJoinCondition, usato da SPEED REFRESH
    /// VIEW per ri-derivare chiave sinistra/campo destro dalla condizione di JOIN memorizzata
    /// nello statement SELECT originale della vista (il compilatore fa la stessa scomposizione a
    /// tempo di compilazione per SPEED SELECT/CREATE VIEW, ma qui serve rifarla perché la vista
    /// viene ri-eseguita in un momento diverso, con solo l'AST grezzo a disposizione).</summary>
    private static (Expr LeftKeyExpr, string RightFieldName) DecomposeJoinConditionRuntime(
        Expr condition, string leftAlias, string rightAlias)
    {
        if (condition is BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.ExactlyEqual } bin)
        {
            var leftSideAlias = bin.Left is FieldAccessExpr { Structure: VariableExpr lv } ? lv.NameCleaned.Lexeme : null;
            var rightSideAlias = bin.Right is FieldAccessExpr { Structure: VariableExpr rv } ? rv.NameCleaned.Lexeme : null;
            if (string.Equals(leftSideAlias, leftAlias, StringComparison.OrdinalIgnoreCase) &&
                string.Equals(rightSideAlias, rightAlias, StringComparison.OrdinalIgnoreCase))
                return (bin.Left, SpeedFieldDisplayName(bin.Right));
            if (string.Equals(rightSideAlias, leftAlias, StringComparison.OrdinalIgnoreCase) &&
                string.Equals(leftSideAlias, rightAlias, StringComparison.OrdinalIgnoreCase))
                return (bin.Right, SpeedFieldDisplayName(bin.Left));
        }
        return (condition, "");
    }

    // ═══════════════════════════════ Scansione / JOIN / proiezione ═══════════════════════════════

    private IEnumerable<Dictionary<string, object?>> SpeedScanRows(Basic.Databases.Speed.Database db, int tableId)
    {
        foreach (var kvp in db.Scan(tableId))
        {
            if (kvp.Value == null) continue;
            yield return BytesToSpeedRecord(kvp.Value);
        }
    }

    private static Dictionary<string, object?> SpeedQualifyRow(Dictionary<string, object?> row, string alias)
    {
        var result = new Dictionary<string, object?>(row, StringComparer.OrdinalIgnoreCase);
        foreach (var (k, v) in row) result[$"{alias}.{k}"] = v;
        return result;
    }

    private static Dictionary<string, object?> SpeedStripQualification(
        Dictionary<string, object?> row, string alias1, string? alias2)
    {
        var result = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        var prefix1 = alias1 + ".";
        var prefix2 = alias2 != null ? alias2 + "." : null;
        foreach (var (k, v) in row)
        {
            if (k.StartsWith(prefix1, StringComparison.OrdinalIgnoreCase)) result[k[prefix1.Length..]] = v;
            else if (prefix2 != null && k.StartsWith(prefix2, StringComparison.OrdinalIgnoreCase)) result[k[prefix2.Length..]] = v;
            else if (!k.Contains('.')) result[k] = v;
        }
        return result;
    }

    /// <summary>
    /// Esegue la SELECT interna di una vista SPEED (usata sia da SPEED CREATE VIEW che da
    /// SPEED REFRESH VIEW per calcolare/ricalcolare i risultati) contro la tabella FROM già
    /// risolta come SpeedDriver. Centralizza la scomposizione del JOIN e la chiamata a
    /// SpeedExecuteSelect, usata identicamente da entrambe le syscall.
    /// </summary>
    private List<Dictionary<string, object?>> SpeedRunViewQuery(SpeedSelectStmt stmt, Basic.Databases.Rdd.SpeedDriver fromSd)
    {
        if (stmt.From == null || fromSd._db == null) return new List<Dictionary<string, object?>>();
        try
        {
            var selectFields = stmt.SelectFields?.Select(f => (f.Expression, f.Alias?.Lexeme)).ToList()
                                ?? new List<(Expr, string?)>();
            string? joinAlias = stmt.Join?.TableAlias.Lexeme;
            Expr? joinLeftKeyExpr = null; string? joinRightFieldName = null;
            Basic.Databases.Rdd.SpeedDriver? joinSd = null;
            if (stmt.Join != null)
            {
                joinSd = _dataSession.FindAlias(joinAlias!) as Basic.Databases.Rdd.SpeedDriver;
                var (lk, rf) = DecomposeJoinConditionRuntime(stmt.Join.Condition, stmt.From.TableAlias.Lexeme, joinAlias!);
                joinLeftKeyExpr = lk; joinRightFieldName = rf;
            }
            var resultRows = SpeedExecuteSelect(
                fromSd._db, stmt.From.TableAlias.Lexeme, fromSd._tableId,
                joinSd != null ? joinAlias : null, joinSd?._tableId ?? 0, joinLeftKeyExpr, joinRightFieldName,
                stmt.Where,
                stmt.GroupBy?.ToList() ?? new List<Expr>(),
                stmt.Having,
                stmt.OrderBy?.Select(o => (o.Expression, o.Descending)).ToList() ?? new List<(Expr, bool)>(),
                selectFields);
            return SpeedWrapRowsForUdt(resultRows, fromSd);
        }
        catch { return new List<Dictionary<string, object?>>(); }
    }

    /// <summary>
    /// Esegue una query SPEED SELECT statica (JOIN/WHERE/GROUP BY/HAVING/ORDER BY/proiezione) e
    /// restituisce le righe risultato come dizionari (chiave = alias di colonna). Nessun accesso
    /// a Database.ExecuteQuery — vedi commento di testata del file.
    /// </summary>
    private List<Dictionary<string, object?>> SpeedExecuteSelect(
        Basic.Databases.Speed.Database db,
        string fromAlias, int fromTableId,
        string? joinAlias, int joinTableId, Expr? joinLeftKeyExpr, string? joinRightFieldName,
        Expr? whereExpr,
        List<Expr> groupByExprs,
        Expr? havingExpr,
        List<(Expr Expr, bool Descending)> orderBy,
        List<(Expr Expr, string? Alias)> selectFields)
    {
        IEnumerable<Dictionary<string, object?>> rows;
        if (joinAlias != null)
        {
            var rightRows = SpeedScanRows(db, joinTableId).Select(r => SpeedQualifyRow(r, joinAlias)).ToList();
            var joined = new List<Dictionary<string, object?>>();
            foreach (var leftRaw in SpeedScanRows(db, fromTableId))
            {
                var left = SpeedQualifyRow(leftRaw, fromAlias);
                var leftKeyVal = SpeedEvaluateExpr(joinLeftKeyExpr, left);
                foreach (var right in rightRows)
                {
                    object? rightVal = null;
                    if (!string.IsNullOrEmpty(joinRightFieldName)) right.TryGetValue(joinRightFieldName, out rightVal);
                    if (SpeedCompare(leftKeyVal, rightVal) != 0) continue;
                    var merged = new Dictionary<string, object?>(left, StringComparer.OrdinalIgnoreCase);
                    foreach (var (k, v) in right) merged[k] = v;
                    joined.Add(merged);
                }
            }
            rows = joined;
        }
        else
        {
            rows = SpeedScanRows(db, fromTableId).Select(r => SpeedQualifyRow(r, fromAlias));
        }

        if (whereExpr != null)
            rows = rows.Where(r => SpeedIsTrue(SpeedEvaluateExpr(whereExpr, r)));

        var rowList = rows.ToList();
        var isStar = selectFields.Count == 1 && selectFields[0].Expr is StringExpr { Value: "*" };
        var hasAggregates = !isStar && selectFields.Any(f => SpeedTryGetAggregate(f.Expr, out _, out _));

        List<Dictionary<string, object?>> resultRows;
        if (groupByExprs.Count > 0 || hasAggregates)
        {
            var groups = groupByExprs.Count > 0
                ? rowList.GroupBy(r => string.Join(
                    "",
                    groupByExprs.Select(g => Convert.ToString(SpeedEvaluateExpr(g, r), CultureInfo.InvariantCulture) ?? "")))
                : rowList.GroupBy(_ => "");

            resultRows = new List<Dictionary<string, object?>>();
            foreach (var group in groups)
            {
                var groupRows = group.ToList();
                if (groupRows.Count == 0) continue;
                var outRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                foreach (var (fieldExpr, alias) in selectFields)
                {
                    var outName = alias ?? SpeedFieldDisplayName(fieldExpr);
                    if (SpeedTryGetAggregate(fieldExpr, out var funcName, out var inner))
                    {
                        outRow[outName] = funcName == "COUNT" && inner == null
                            ? (double)groupRows.Count
                            : SpeedAggregate(funcName, groupRows.Select(r => SpeedEvaluateExpr(inner, r)).ToList());
                    }
                    else
                    {
                        outRow[outName] = SpeedEvaluateExpr(fieldExpr, groupRows[0]);
                    }
                }

                if (havingExpr == null || SpeedIsTrue(SpeedEvaluateExpr(havingExpr, outRow)))
                    resultRows.Add(outRow);
            }
        }
        else if (isStar)
        {
            resultRows = rowList.Select(r => SpeedStripQualification(r, fromAlias, joinAlias)).ToList();
        }
        else
        {
            resultRows = rowList.Select(r =>
            {
                var outRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                foreach (var (fieldExpr, alias) in selectFields)
                    outRow[alias ?? SpeedFieldDisplayName(fieldExpr)] = SpeedEvaluateExpr(fieldExpr, r);
                return outRow;
            }).ToList();
        }

        if (orderBy.Count > 0)
        {
            var cmp = Comparer<object?>.Create(SpeedCompare);
            IOrderedEnumerable<Dictionary<string, object?>>? ordered = null;
            foreach (var (oExpr, desc) in orderBy)
            {
                object? KeySel(Dictionary<string, object?> r) => SpeedEvaluateExpr(oExpr, r);
                ordered = ordered == null
                    ? (desc ? resultRows.OrderByDescending(KeySel, cmp) : resultRows.OrderBy(KeySel, cmp))
                    : (desc ? ordered.ThenByDescending(KeySel, cmp) : ordered.ThenBy(KeySel, cmp));
            }
            resultRows = ordered!.ToList();
        }

        return resultRows;
    }
}
