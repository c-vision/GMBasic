// FILE: IlProgramCompiler.Db.cs
// PARTIAL — dominio DATABASE (LEDGER/DBF/ISAM/SPEED: helper e alias).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio DATABASE (ledger/dbf/isam/speed helper).</summary>
public sealed partial class IlProgramCompiler
{

    private static void RequireLedgerArgCount(BuiltinFunctionCallExpr expr, int expected)
    {
        if (expr.Args.Length != expected)
            throw new NotSupportedException($"Backend IL: LEDGER.{expr.Name.Lexeme} richiede {expected} argomento(s).");
    }

    private static void RequireLedgerArgCountIn(BuiltinFunctionCallExpr expr, int min, int max)
    {
        if (expr.Args.Length < min || expr.Args.Length > max)
            throw new NotSupportedException($"Backend IL: LEDGER.{expr.Name.Lexeme} richiede da {min} a {max} argomenti.");
    }

    private static string DbfCreateTypeName(DataType dt) => NormalizeInferredType(dt) switch
    {
        StringType => "STRING",
        IntegerType => "INTEGER",
        DoubleType => "DOUBLE",
        LongType => "LONG",
        BitType => "BIT",
        DateType => "DATE",
        CurrencyType => "CURRENCY",
        _ => "STRING"
    };

    private static int DbfCreateFieldLen(DataType dt) => NormalizeInferredType(dt) switch
    {
        StringType st when st.FixedLength is > 0 => st.FixedLength.Value,
        StringType => 20,
        IntegerType => 4,
        DoubleType => 8,
        LongType => 10,
        BitType => 1,
        DateType => 8,
        // DbfCreateFile mappa "CURRENCY" a DbfFieldType.Currency, il formato binario nativo
        // dBASE7 a lunghezza FISSA di 8 byte (DbfFieldWriter.WriteCurrency/DbfFieldParser.
        // ParseCurrency azzerano il campo se destination.Length != 8) — 10 produceva sempre
        // zero indipendentemente dal valore scritto (scoperto isolando DBF REPLACE ... Valore
        // WITH 12.34 su un campo CURRENCY: il valore arrivava corretto a runtime ma il file
        // restava a "0000000000").
        CurrencyType => 8,
        _ => 10
    };

    /// <summary>Builtin DBF numerici (Recno/Lastrec/Selected): il runtime ritorna long ma la
    /// convenzione BASIC è double — conv.r8 dopo la call.</summary>
    private static string SerializeFilterCondition(Expr expr) => expr switch
    {
        VariableExpr v => v.NameCleaned.Lexeme,
        NumberExpr n => n.Value.ToString(System.Globalization.CultureInfo.InvariantCulture),
        StringExpr s => "\"" + s.Value.Replace("\"", "\"\"") + "\"",
        BooleanExpr b => b.Value ? "-1" : "0",
        UnaryExpr u when u.Op.Kind == TokenKind.Not => $"NOT {SerializeFilterCondition(u.Right)}",
        BinaryExpr b => $"{SerializeFilterCondition(b.Left)} {FilterOpText(b.Op.Kind)} {SerializeFilterCondition(b.Right)}",
        // Campo del RECORD attivo (`_dbRecord.Campo`, late-binding di un campo nudo) →
        // SOLO il nome del campo (il motore di filtro lo risolve per record).
        FieldAccessExpr fa when fa.Structure is VariableExpr fv
            && fv.NameCleaned.Lexeme.StartsWith("_dbRecord", StringComparison.Ordinal)
            => fa.Field.Lexeme,
        // Campo qualificato da alias (`Clienti.ID`, JOIN a due tabelle) → "Alias.Campo".
        FieldAccessExpr fa => $"{SerializeFilterCondition(fa.Structure)}.{fa.Field.Lexeme}",
        // `DBF ... FOR FN(rec) => <expr>`: il lambda è solo un placeholder per il
        // compilatore (il parametro non è usato, i campi sono late-binding) → serializza
        // il CORPO come filtro testuale ri-parsato dal motore DBF per record.
        LambdaExpr l when l.ExpressionBody is not null => SerializeFilterCondition(l.ExpressionBody),
        _ => throw new NotSupportedException(
            $"Backend IL: condizione scope '{expr.GetType().Name}' non serializzabile come filtro.")
    };

    private static string FilterOpText(TokenKind kind) => kind switch
    {
        TokenKind.GreaterThan => ">",
        TokenKind.GreaterEqual => ">=",
        TokenKind.LessThan => "<",
        TokenKind.LessEqual => "<=",
        TokenKind.Equal => "=",
        TokenKind.ExactlyEqual => "==",
        TokenKind.NotEqual => "<>",
        TokenKind.And => " AND ",
        TokenKind.Or => " OR ",
        _ => kind.ToString().ToUpperInvariant(),
    };

    private string BuildIsamSchemaJson(string typeName, IsamIndexedByClause? indexedBy)
    {
        var fields = new List<(string Name, string Type, int Len)>();
        var udt = FindUdtByName(typeName);
        if (udt != null)
        {
            foreach (var f in udt.Fields)
            {
                var len = (f.DataType as StringType)?.FixedLength ?? 0;
                fields.Add((f.Name.Lexeme, IsamTypeCode(f.DataType), len));
            }
        }
        var pk = "";
        if (indexedBy is { } idxBy && idxBy.KeyFields.Length > 0)
        {
            var kf = idxBy.KeyFields[0].KeyExpression;
            if (kf is VariableExpr kv) pk = kv.NameCleaned.Lexeme;
        }
        var indexes = indexedBy?.KeyFields
            .Select(kf => kf.KeyExpression is VariableExpr ikv ? ikv.NameCleaned.Lexeme : "")
            .Where(s => s.Length > 0)
            .ToList() ?? [];
        var sb = new System.Text.StringBuilder();
        sb.Append("{\"TypeName\":").Append(System.Text.Json.JsonSerializer.Serialize(typeName));
        sb.Append(",\"Fields\":[");
        sb.Append(string.Join(",", fields.Select(f => $"{{\"Name\":{System.Text.Json.JsonSerializer.Serialize(f.Name)},\"Type\":{System.Text.Json.JsonSerializer.Serialize(f.Type)},\"Length\":{f.Len}}}")));
        sb.Append("],\"PrimaryKey\":").Append(System.Text.Json.JsonSerializer.Serialize(pk));
        sb.Append(",\"Indexes\":[");
        sb.Append(string.Join(",", indexes.Select(ix => System.Text.Json.JsonSerializer.Serialize(ix))));
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>
    /// Testo BASIC pulito e ri-parsabile per un'espressione di query SPEED (SELECT/WHERE/JOIN
    /// ON/GROUP BY/HAVING/ORDER BY) — a differenza di ExprPlain.From (pensato per SET FILTER/
    /// SET RELATION, dove esiste UNA sola tabella e l'alias qualificante viene deliberatamente
    /// scartato, vedi ExprPlain.RelationKeyName/From: QualifiedNameExpr → solo l'ultima parte),
    /// qui l'alias DEVE restare (una query può avere JOIN fra due tabelle: "Clienti.ID" e
    /// "Ordini.ID_Cliente" sono campi diversi, non lo stesso campo "ID"/"ID_Cliente" isolato).
    /// Expr.ToString() per QualifiedNameExpr/BinaryExpr/letterali è già pulito (nessun
    /// suffisso #SymbolId, quei nodi non ne hanno); la sola VariableExpr lo aggiunge — rimosso
    /// con una pulizia regex difensiva, nel caso un identificatore bare risolva a un vero
    /// simbolo invece che a un QualifiedNameExpr non risolto (comportamento del parser non
    /// garantito qui, la difesa costa poco).
    /// </summary>
    private static string CleanQueryExprText(Expr? expr)
    {
        if (expr is null) return "";
        var text = expr.ToString() ?? "";
        text = System.Text.RegularExpressions.Regex.Replace(text, @"#-?\d+", "");
        // Il parser riscrive una bare call come `SUM(...)` in `MATH.SUM(...)` (risoluzione
        // automatica del namespace builtin) — ma Database.Aggregate (motore SPEED) e
        // EvaluateSpeedQueryExpr riconoscono COUNT/SUM/AVG/MIN/MAX solo come nome nudo
        // (identico al comportamento SQL standard, non un vero identificatore MATH.* che
        // riceverebbe un array). Rimossa qui, all'unico punto di serializzazione del testo di
        // query, invece che in entrambi i consumer (motore + evaluator).
        text = System.Text.RegularExpressions.Regex.Replace(
            text, @"\b\w+\.(SUM|COUNT|AVG|MIN|MAX)\s*\(", "$1(", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        return text;
    }

    /// <summary>
    /// Costruisce il piano di query JSON per `SPEED SELECT`/`CREATE VIEW ... AS SELECT`,
    /// consumato da IlRuntime.SpeedSelect (che lo mappa su Basic.Databases.Speed.QueryPlan ed
    /// esegue Database.ExecuteQuery — motore già completo: JOIN indicizzato, WHERE con
    /// ottimizzatore, GROUP BY/aggregati, HAVING, ORDER BY). `SELECT *` viene espanso qui, a
    /// compile-time, in una proiezione per campo del TYPE di destinazione (OF TypeName) —
    /// IlRuntime non ha accesso alla SymbolTable/AST, solo il compilatore può risolvere i campi
    /// di un UDT. L'ordine dei campi del TYPE di destinazione viaggia anch'esso nel JSON
    /// (DestFields): serve a IlRuntime per riordinare ogni riga risultato (un Dictionary
    /// keyed per alias SELECT) nell'object[] posizionale che il compilatore poi copia nello
    /// struct UDT di destinazione (stesso schema di EmitCopyObjectArrayToTarget).
    /// </summary>
    private string BuildSpeedQueryPlanJson(SpeedSelectStmt stmt)
    {
        if (stmt.From is null)
            throw new NotSupportedException("Backend IL: SPEED SELECT richiede la clausola FROM.");
        var fromAlias = stmt.From.TableAlias.Lexeme;
        var destTypeName = stmt.DestinationUdtToken?.Lexeme ?? "";
        var destUdt = FindUdtByName(destTypeName);
        var destFields = destUdt?.Fields.Select(f => f.Name.Lexeme).ToList() ?? [];

        var fields = new List<(string Expr, string Alias)>();
        if (stmt.SelectFields is { } sf)
        {
            foreach (var f in sf)
            {
                if (f.Expression is StringExpr se && se.Value == "*")
                {
                    foreach (var fname in destFields)
                        fields.Add(($"{fromAlias}.{fname}", fname));
                }
                else
                {
                    var exprText = CleanQueryExprText(f.Expression);
                    fields.Add((exprText, f.Alias?.Lexeme ?? exprText));
                }
            }
        }

        var sb = new System.Text.StringBuilder();
        sb.Append("{\"Fields\":[");
        sb.Append(string.Join(",", fields.Select(f =>
            $"{{\"Expr\":{System.Text.Json.JsonSerializer.Serialize(f.Expr)},\"Alias\":{System.Text.Json.JsonSerializer.Serialize(f.Alias)}}}")));
        sb.Append("],\"FromAlias\":").Append(System.Text.Json.JsonSerializer.Serialize(fromAlias));
        if (stmt.Join is { } j)
        {
            sb.Append(",\"JoinAlias\":").Append(System.Text.Json.JsonSerializer.Serialize(j.TableAlias.Lexeme));
            sb.Append(",\"JoinCondition\":").Append(System.Text.Json.JsonSerializer.Serialize(CleanQueryExprText(j.Condition)));
        }
        if (stmt.Where is not null)
            sb.Append(",\"Where\":").Append(System.Text.Json.JsonSerializer.Serialize(CleanQueryExprText(stmt.Where)));
        if (stmt.GroupBy is { } gb && gb.Length > 0)
            sb.Append(",\"GroupBy\":[")
                .Append(string.Join(",", gb.Select(g => System.Text.Json.JsonSerializer.Serialize(CleanQueryExprText(g)))))
                .Append(']');
        if (stmt.Having is not null)
            sb.Append(",\"Having\":").Append(System.Text.Json.JsonSerializer.Serialize(CleanQueryExprText(stmt.Having)));
        if (stmt.OrderBy is { } ob && ob.Length > 0)
        {
            var obItems = new List<string>();
            foreach (var o in ob)
                obItems.Add($"{{\"Expr\":{System.Text.Json.JsonSerializer.Serialize(CleanQueryExprText(o.Expression))},\"Desc\":{(o.Descending ? "true" : "false")}}}");
            sb.Append(",\"OrderBy\":[")
                .Append(string.Join(",", obItems))
                .Append(']');
        }
        sb.Append(",\"DestFields\":[").Append(string.Join(",", destFields.Select(df => System.Text.Json.JsonSerializer.Serialize(df)))).Append(']');
        sb.Append('}');
        return sb.ToString();
    }

    private static string IsamTypeCode(DataType t) => t switch
    {
        StringType => "STRING",
        LongType => "LONG",
        IntegerType => "INTEGER",
        DoubleType => "DOUBLE",
        CurrencyType => "CURRENCY",
        BitType => "BIT",
        ByteType => "BYTE",
        SByteType => "SBYTE",
        SingleType => "SINGLE",
        ULongType => "ULONG",
        UIntegerType => "UINTEGER",
        QuadType => "QUAD",
        UQuadType => "UQUAD",
        DateType => "DATE",
        TimeType => "TIME",
        DateTimeType => "DATETIME",
        TimeSpanType => "TIMESPAN",
        JsonType => "JSON",
        _ => "STRING",
    };

    /// <summary>Spinge l'alias di un ISAM come stringa (FileNumber può essere un alias
    /// testuale o un handle numerico → convertito a stringa).</summary>
    private static string LedgerAmountValue(Expr amount) => amount switch
    {
        NumberExpr na => na.Value.ToString(System.Globalization.CultureInfo.InvariantCulture),
        ForeignAmountExpr fae when fae.Amount is NumberExpr faNum && fae.ExchangeRate is NumberExpr fxNum
            => (faNum.Value * fxNum.Value).ToString(System.Globalization.CultureInfo.InvariantCulture),
        _ => "0"
    };

    private void EmitIsamAliasArg(Expr fileNumber)
    {
        if (fileNumber is VariableExpr ivVe)
            _il.Emit(OpCodes.Ldstr, ivVe.NameCleaned.Lexeme);
        else if (fileNumber is NumberExpr ivNum)
            _il.Emit(OpCodes.Ldstr, ivNum.Value.ToString(System.Globalization.CultureInfo.InvariantCulture));
        else
        {
            EmitStringArg(fileNumber);
        }
    }

    private void EmitSpeedAliasArg(Expr alias)
    {
        if (alias is VariableExpr svVe)
            _il.Emit(OpCodes.Ldstr, svVe.NameCleaned.Lexeme);
        else EmitStringArg(alias);
    }

    
    /// <summary>Importo BASE di un posting Ledger: NumberExpr → valore; ForeignAmountExpr
    /// ("100.00 USD @ 0.95") → importo × tasso (il motore bilancia in valuta base).</summary>
    private void EmitLedgerAliasArg(Expr handle)
    {
        if (handle is VariableExpr lvVe)
            _il.Emit(OpCodes.Ldstr, lvVe.NameCleaned.Lexeme);
        else EmitStringArg(handle);
    }

}
