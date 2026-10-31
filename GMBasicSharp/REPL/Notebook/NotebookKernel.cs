// FILE: REPL/Notebook/NotebookKernel.cs
// Notebook interattivo GMBASIC (Fase 0 del piano Todo/(partial) - Interactive-Notebook-Plan.md).
//
// Kernel per-cell: ogni cella è JIT-compilata a P-Code (Parser.ParseSingleStatement →
// Compiler.CompileProcedureBody, NO linker, NO IL — il percorso veloce delle macro Clipper,
// production-tested in IlRuntime.MacroEvalPCode) ed eseguita da una PCodeVm effimera.
// Lo scope condiviso tra celle è la tabella dei GLOBALI del runtime (LoadGlobal/StoreGlobal),
// con write-back tipato (CoerceGlobalWriteBack replicato da IlRuntime: senza, scrivere in una
// globale non-Double da cella rimpiazza il box e la lettura IL successiva InvalidCast-a).
//
// DIFFERENZE deliberate da MacroEvalPCode (le ragioni sono nel piano §2.3/§6bis):
// - Host dedicato per cella (HeadlessRuntimeHost su StringWriter) invece di IlRuntime.Current:
//   il PRINT della PCodeVM va su _runtime.Console.* (PCodeProcessor.FlushPrintBuffer), quindi
//   così l'output di ogni cella è catturato per-cell senza toccare Console.SetOut.
// - Cache per cella: cell_id → (chunk compilato + slotMap) finché il source non cambia —
//   la re-esecuzione salta parse+compile (gate %time del piano §2.4).
// - Nomi assegnati in una cella senza GLOBAL esplicito diventano globali del kernel alla
//   prima assegnazione (registro lazy): il notebook è REPL-like, non "programma pre-linkato".

using System.Collections.Immutable;
using System.Data;
using System.Diagnostics;
using System.Globalization;
using System.Text;
using Basic.IlBackend;
using Compiler;
using Basic.IlBackend.PCode;
using Basic.Runtime;
using Microsoft.Data.Sqlite;

namespace GMBasic.Notebook;

/// <summary>Risultato dell'esecuzione di una cella.</summary>
public sealed class CellResult
{
    public bool Ok { get; init; }
    public string? Error { get; init; }
    /// <summary>Output PRINT/WARN catturato dall'host dedicato della cella.</summary>
    public string Output { get; init; } = "";
    /// <summary>Valore dell'ultima cella-espressione (`_`), null per celle statement.</summary>
    public object? Value { get; init; }
    public TimeSpan CompileMs { get; init; }
    public TimeSpan ExecuteMs { get; init; }
    /// <summary>True se la cella è stata servita dalla cache di compilazione.</summary>
    public bool FromCache { get; init; }
    /// <summary>True se l'esecuzione ha scritto l'autosave .gmnb (quando AutosavePath è attivo).</summary>
    public bool Autosaved { get; init; }
}

/// <summary>
/// Kernel notebook single-session. NON thread-safe per costruzione multipla (gli statici di
/// IlRuntime — macro context, host corrente, bag globali — sono per-processo: piano §5,
/// multi-kernel = multi-processo). Una istanza = un notebook.
/// </summary>
public sealed class NotebookKernel : IDisposable
{
    private sealed record CompiledCell(
        Compiler.CompiledChunk Chunk,
        Dictionary<int, int> SlotToSymbolId,
        string Source,
        bool IsExpression,
        List<string> DeclaredGlobals,
        HashSet<string> Reads,
        HashSet<string> Writes);

    /// <summary>Vista pubblica di una cella per %order/run-all/frontend: source e dipendenze
    /// (reads/writes di variabili GLOBALI del kernel) estratte dal AST al momento del compile.</summary>
    public sealed record CellInfo(string CellId, string Source, IReadOnlySet<string> Reads, IReadOnlySet<string> Writes);

    private readonly Basic.BasicProgram _program;
    private readonly Dictionary<string, CompiledCell> _cache = new(StringComparer.Ordinal);
    private readonly List<string> _cellOrder = new(); // ordine di DEFINIZIONE (per Cells/RunAll)
    private readonly Dictionary<string, string> _sources = new(StringComparer.Ordinal); // source di OGNI cella eseguita (code E magic)
    private readonly Dictionary<string, string> _lastOutput = new(StringComparer.Ordinal); // ultimo output per cella (per export html)
    private readonly Dictionary<string, bool> _lastOk = new(StringComparer.Ordinal);
    private readonly Dictionary<string, int> _kernelGlobals = new(StringComparer.OrdinalIgnoreCase);
    private bool _disposed;
    private string? _lastCompileError;

    /// <summary>Callable per iniettare un host custom (test); default: HeadlessRuntimeHost.</summary>
    internal Func<StringWriter, IRuntimeHost>? HostFactoryOverride { get; set; }

    /// <summary>Autosave (Fase 3): se impostato, ogni Execute riscrive il notebook su questo
    /// path .gmnb (con versioning .bak). Il chiamante (REPL/IDE/server) lo attiva quando vuole
    /// la persistenza automatica; null = nessun autosave (default).</summary>
    public string? AutosavePath { get; set; }

    public NotebookKernel()
    {
        // Stesso contesto di MinimalMacroProgram (IlRuntime): DefaultNamespaces standard, niente
        // simboli pre-popolati. ProgramRootDirectory "" = working dir del processo (per celle
        // che aprono file relativi: la working directory è gestita dal chiamante/REPL).
        _program = new Basic.BasicProgram(new CommonLib.EffectiveSettings
        {
            DefaultNamespaces = ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS"]
        }, null, "");
        // Contesto di compilazione condiviso: TryResolveGlobalSymbolId (via _macroProgram) vede
        // i simboli globali del notebook sia dal probe di compilazione che dal write-back.
        Basic.IlBackend.IlRuntime.SetMacroContext(_program);
    }

    /// <summary>I nomi globali del kernel (per %vars): nome → SymbolId.</summary>
    public IReadOnlyDictionary<string, int> KernelGlobals => _kernelGlobals;

    // ─────────────────────────────────────────────────────────────────────────────
    // Esecuzione cella
    // ─────────────────────────────────────────────────────────────────────────────

    public CellResult Execute(string cellId, string source)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        // Traccia la cella SEMPRE (code E magic): l'ordine di definizione e il source servono
        // a Save/ExportBasic/Order — senza, le celle magic sparivano da .gmnb ed export.
        _sources[cellId] = source;
        if (!_cellOrder.Contains(cellId)) _cellOrder.Add(cellId);

        // Celle speciali (Fase 2): %sql / %ai / %chart — dispatch a handler dedicato invece
        // del percorso BASIC. La magia è la PRIMA riga della cella (es. "%sql SELECT 1+1").
        var magic = TryParseMagic(source);
        if (magic != null)
        {
            var sw = Stopwatch.StartNew();
            try
            {
                var r = ExecuteMagic(magic.Value.Kind, magic.Value.Arg, cellId);
                sw.Stop();
                _lastOutput[cellId] = r;
                _lastOk[cellId] = true;
                return new CellResult { Ok = true, Output = r, CompileMs = TimeSpan.Zero, ExecuteMs = sw.Elapsed, Autosaved = TryAutosave() };
            }
            catch (Exception ex)
            {
                sw.Stop();
                _lastOutput[cellId] = "✗ " + ex.Message;
                _lastOk[cellId] = false;
                return new CellResult { Ok = false, Error = ex.Message, CompileMs = TimeSpan.Zero, ExecuteMs = sw.Elapsed, Autosaved = TryAutosave() };
            }
        }

        var swCompile = Stopwatch.StartNew();

        var cached = _cache.GetValueOrDefault(cellId);
        var fromCache = cached != null && cached.Source == source;
        var cell = fromCache ? cached! : Compile(cellId, source);

        swCompile.Stop();
        if (cell == null)
            return new CellResult { Ok = false, Error = _lastCompileError ?? "compile error", CompileMs = TimeSpan.Zero };

        // Espressione: risultato per `_`; statement: solo output.
        var swExec = Stopwatch.StartNew();
        var (output, error, value) = Run(cell);
        swExec.Stop();

        var isOk = error == null;
        _lastOutput[cellId] = isOk ? output : "✗ " + error;
        _lastOk[cellId] = isOk;

        return new CellResult
        {
            Ok = isOk,
            Error = error,
            Output = output,
            Value = value,
            CompileMs = swCompile.Elapsed,
            ExecuteMs = swExec.Elapsed,
            FromCache = fromCache,
            Autosaved = TryAutosave(),
        };
    }

    /// <summary>Scrive l'autosave se <see cref="AutosavePath"/> è impostato. Best-effort:
    /// un errore di I/O non deve mai rompere l'esecuzione di una cella.</summary>
    private bool TryAutosave()
    {
        if (AutosavePath == null) return false;
        try { Save(AutosavePath); return true; }
        catch { return false; }
    }

    private CompiledCell? Compile(string cellId, string source)
    {
        try
        {
            // Cella multi-riga = UNO STATEMENT PER RIGA (semantica REPL classica; blocchi
            // strutturali multi-riga come FOR/IF su più righe NON sono in scope Fase 0 —
            // vanno scritti con ':' sulla stessa riga, che ParseSingleStatement accetta).
            // Il fallback a espressione resta sul source COMPLETO (una cella-espressione
            // è per definizione una riga sola).
            var stmts = new List<Basic.Stmt>();
            bool isExpression = false;
            Basic.Expr? expr = null;
            try
            {
                foreach (var rawLine in source.Split('\n'))
                {
                    var line = rawLine.TrimEnd('\r');
                    if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('\'')) continue;
                    var lineSrc = new Basic.SourceLine(-1, "", 0, line);
                    stmts.Add(Basic.Parser.ParseSingleStatement(line, _program, lineSrc, 1, Basic.StdLib.Instance, false));
                }
                if (stmts.Count == 0) return null; // cella vuota/solo commenti: niente da eseguire

                // Cella-espressione pura: UNA sola riga il cui statement è un ExpressionStmt
                // nudo (es. "x * 2", "1 + 1") — il notebook mostra il VALORE, come `_` di
                // Jupyter. L'espressione viene compilata con CompileExpressionBody perché il
                // risultato resti sullo stack (OP_RETURN), non scartato come statement.
                if (stmts.Count == 1 && stmts[0] is Basic.ExpressionStmt { Expression: var bare }
                    && bare is not Basic.AssignmentExpr)
                {
                    expr = bare;
                    isExpression = true;
                    stmts = [];
                }
            }
            catch (Basic.BasicException)
            {
                // Non tutti i token sono statement validi: riprova il source COMPLETO come
                // espressione (cella-espressione). Se anche questo fallisce, l'eccezione
                // sale al catch esterno → cella rossa.
                expr = Basic.Parser.ParseSingleExpression(source, _program,
                    new Basic.SourceLine(-1, "", 0, source), 1, Basic.StdLib.Instance);
                isExpression = true;
                stmts = [];
            }

            var compiler = new Compiler.Compiler
            {
                // Probe: nome noto come globale del kernel (assegnato in una cella precedente)
                // o già simbolo IsGlobal del programma. MacroEvalPCode usa TryResolveGlobalSymbolId;
                // qui il kernel È il programma ospite quindi risolve direttamente (stessa semantica
                // di IlRuntime.cs:13193 — FindSymbolsByName + IsCapturableGlobal).
                ExternalGlobalProbe = IsKnownKernelGlobal
            };

            Compiler.CompiledChunk chunk;
            if (isExpression)
            {
                chunk = compiler.CompileExpressionBody(
                    System.Collections.Immutable.ImmutableArray<Basic.Parameter>.Empty, expr!);
            }
            else
            {
                chunk = compiler.CompileProcedureBody(
                    System.Collections.Immutable.ImmutableArray<Basic.Parameter>.Empty, stmts.ToImmutableArray());
            }
            var slotToSymbolId = BuildSlotMap(compiler);
            chunk.GlobalCount = compiler.GlobalSlots.Count;

            // %order: estrazione reads/writes dai nodi AST (per dipendenza tra celle).
            var reads = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var writes = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (isExpression)
                CollectExprAccess(expr!, reads, writes);
            else
                foreach (var st in stmts) CollectStmtAccess(st, reads, writes);

            var cell = new CompiledCell(chunk, slotToSymbolId, source, isExpression, [], reads, writes);
            _cache[cellId] = cell;
            if (!_cellOrder.Contains(cellId)) _cellOrder.Add(cellId);
            return cell;
        }
        catch (Exception ex)
        {
            _lastCompileError = ex.Message; // cella rossa con messaggio, kernel vivo
            return null;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // %order: analisi reads/writes per cella (visitor sui nodi AST che il percorso per-cell
    // produce; NON è un linker — conta solo l'accesso a variabili, per le dipendenze globali).
    // ─────────────────────────────────────────────────────────────────────────────

    private static void CollectStmtAccess(Basic.Stmt st, HashSet<string> reads, HashSet<string> writes)
    {
        switch (st)
        {
            case Basic.LetStmt let:
                foreach (var a in let.Assignments)
                {
                    CollectTargetWrites(a.Target, reads, writes);
                    CollectExprAccess(a.Value, reads, writes);
                }
                break;
            case Basic.ExpressionStmt es:
                CollectExprAccess(es.Expression, reads, writes);
                break;
            case Basic.PrintStmt print:
                foreach (var a in print.Arguments)
                    if (a.Expression != null) CollectExprAccess(a.Expression, reads, writes);
                break;
            case Basic.RemStmt:
                break;
            default:
                CollectGeneric(st, reads, writes); // statement composti: best-effort via riflessione
                break;
        }
    }

    private static void CollectExprAccess(Basic.Expr e, HashSet<string> reads, HashSet<string> writes)
    {
        switch (e)
        {
            case Basic.VariableExpr v:
                reads.Add(v.NameCleaned.Lexeme);
                break;
            case Basic.ArrayAccessExpr a:
                reads.Add(a.NameCleaned.Lexeme);
                foreach (var i in a.Indices) CollectExprAccess(i, reads, writes);
                break;
            case Basic.AssignmentExpr assign:
                CollectTargetWrites(assign.Target, reads, writes);
                CollectExprAccess(assign.Value, reads, writes);
                break;
            default:
                CollectGeneric(e, reads, writes);
                break;
        }
    }

    private static void CollectTargetWrites(Basic.Target t, HashSet<string> reads, HashSet<string> writes)
    {
        switch (t)
        {
            case Basic.VariableTarget v:
                writes.Add(v.NameCleaned.Lexeme);
                break;
            case Basic.ArrayElementTarget a:
                writes.Add(a.NameCleaned.Lexeme);
                foreach (var i in a.Indices) CollectExprAccess(i, reads, writes);
                break;
            case Basic.FieldTarget f:
                if (f.Structure != null) CollectTargetWrites(f.Structure, reads, writes);
                break;
            case Basic.IndexedTarget ix:
                CollectExprAccess(ix.Base, reads, writes);
                foreach (var i in ix.Indices) CollectExprAccess(i, reads, writes);
                break;
            case Basic.MacroTarget m:
                CollectExprAccess(m.Expression, reads, writes);
                break;
        }
    }

    /// <summary>Visitor generico best-effort (riflessione sulle proprietà del nodo): copre i
    /// costrutti composti (FOR/IF/WHILE/CALL) che i case espliciti sopra non gestiscono. Le
    /// celle semplici sono coperte dai case; qui si aggiungono al più LETTURE in più — mai
    /// una dipendenza persa, quindi l'ordinamento resta conservativo e corretto per difetto.</summary>
    private static void CollectGeneric(Basic.AstNode node, HashSet<string> reads, HashSet<string> writes)
    {
        if (node == null) return;
        var props = node.GetType().GetProperties(
            System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
        foreach (var p in props)
        {
            if (p.GetIndexParameters().Length > 0) continue;
            var val = p.GetValue(node);
            switch (val)
            {
                case Basic.Stmt s: CollectStmtAccess(s, reads, writes); break;
                case Basic.Target t: CollectTargetWrites(t, reads, writes); break;
                case Basic.Expr e: CollectExprAccess(e, reads, writes); break;
                case IEnumerable<Basic.Expr> exprs:
                    foreach (var e in exprs) if (e != null) CollectExprAccess(e, reads, writes);
                    break;
                case IEnumerable<Basic.Stmt> stmts:
                    foreach (var s in stmts) if (s != null) CollectStmtAccess(s, reads, writes);
                    break;
            }
        }
    }

    private Dictionary<int, int> BuildSlotMap(Compiler.Compiler compiler)
    {
        // Replica di IlRuntime.ResolveGlobalCaptureSlots (private): slot P-Code → SymbolId del
        // programma ospite. Qui "ospite" = _program del kernel; un nome senza simbolo (prima
        // assegnazione) viene dichiarato ora come globale nuovo con SymbolId fresco.
        var map = new Dictionary<int, int>();
        foreach (var (name, slot) in compiler.GlobalSlots)
        {
            var existing = _program.Symbols.FindSymbolsByName(name).FirstOrDefault(IsCapturableGlobal);
            if (existing != null)
            {
                map[slot] = existing.Id;
                _kernelGlobals[name] = existing.Id;
            }
            else
            {
                var id = DeclareKernelGlobal(name);
                map[slot] = id;
            }
        }
        return map;
    }

    private int DeclareKernelGlobal(string name)
    {
        // Registra il simbolo IsGlobal nel SymbolTable del notebook — stesso identico pattern
        // di SourcePipeline.RegisterBspGlobal (Token sintetico + RegisterSymbol con i flag):
        // DataType = InferredType come fa il linker per i globali pre-link.
        var tok = new Basic.Token(Basic.TokenKind.Identifier, name, null, Basic.SourcePosition.Unknown);
        var id = _program.Symbols.RegisterSymbol(tok, Basic.SymbolFlags.IsGlobal | Basic.SymbolFlags.IsMutable,
                                                 new Basic.InferredType(), tok);
        _kernelGlobals[name] = id;
        return id;
    }

    private bool IsKnownKernelGlobal(string name)
    {
        // REPL-semantics del kernel (a differenza delle macro guest): un nome MAI visto,
        // quando il compilatore lo chiede al probe, viene REGISTRATO come nuovo globale del
        // kernel (NIL finché la cella non lo assegna) — è il comportamento Basic classico
        // delle variabili top-level, e rende "x = 42" in una cella una vera assegnazione a
        // globale condivisa (le macro non lo fanno perché sono ospiti di un programma già
        // linkato; il kernel È il programma).
        if (_kernelGlobals.TryGetValue(name, out var id)) return true;
        if (_program.Symbols.FindSymbolsByName(name).Any(IsCapturableGlobal)) return true;
        DeclareKernelGlobal(name);
        return true;
    }

    private static bool IsCapturableGlobal(Basic.SymbolTable.Symbol s) =>
        s.Flags.HasFlag(Basic.SymbolFlags.IsGlobal) && !s.Flags.HasFlag(Basic.SymbolFlags.IsConst);

    private (string output, string? error, object? value) Run(CompiledCell cell)
    {
        var sw = new StringWriter();
        var host = HostFactoryOverride != null ? HostFactoryOverride(sw) : new HeadlessRuntimeHost(sw, null);
        using var vm = new PCodeVm(cell.Chunk, host);

        var originals = new Dictionary<int, object?>(cell.SlotToSymbolId.Count);
        foreach (var (slot, symbolId) in cell.SlotToSymbolId)
        {
            var original = Basic.IlBackend.IlRuntime.LoadGlobal(symbolId);
            originals[slot] = original;
            vm.SetGlobal(slot, original);
        }

        // RunSync (non Run): esegue sul thread CHIAMANTE senza spawnare GOMAXPROCS worker OS
        // (un new PCodeVm.Run() per cella = thread spawn/sleep-watcher a ogni cella = i 3-22ms
        // misurati prima del fix; RunSync è il percorso già usato da DbExprPCodeEvaluator per
        // lo stesso motivo). Celle Fase 0 non usano GO/canali/SLEEP → mai yield/block.
        vm.RunSync();

        if (vm.ExecutionException != null)
            return (sw.ToString(), vm.ExecutionException.Message, null);

        foreach (var (slot, symbolId) in cell.SlotToSymbolId)
            Basic.IlBackend.IlRuntime.StoreGlobal(symbolId, CoerceGlobalWriteBack(vm.GetGlobal(slot), originals[slot]));

        object? value = null;
        if (cell.IsExpression)
            value = PeekVmResult(vm);

        return (sw.ToString(), null, value);
    }

    /// <summary>Legge il valore lasciato sullo stack dal OP_RETURN del CompileExpressionBody —
    /// stessa via di MacroEvalExprPCode (vm.PeekResult()).</summary>
    private static object? PeekVmResult(PCodeVm vm) => vm.PeekResult();

    /// <summary>Replica di IlRuntime.CoerceGlobalWriteBack (private): riconverte il double
    /// uniforme dello stack P-Code al tipo CLR reale della globale (es. GLOBAL X AS INTEGER)
    /// usando il valore ORIGINALE pre-seed come riferimento di tipo. Senza: InvalidCastException
    /// alla prima lettura IL successiva (bug documentato in IlRuntime.cs:13298).</summary>
    private static object? CoerceGlobalWriteBack(object? raw, object? original)
    {
        if (raw is null || original is null) return raw;
        var targetType = original.GetType();
        if (raw.GetType() == targetType) return raw;
        if (raw is not (double or float or int or long or short or byte or decimal)) return raw;
        try { return Convert.ChangeType(raw, targetType, CultureInfo.InvariantCulture); }
        catch { return raw; }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Magics
    // ─────────────────────────────────────────────────────────────────────────────

    /// <summary>%vars: elenca nome = valore (formattato) per ogni globale del kernel.</summary>
    public IEnumerable<(string Name, object? Value)> Vars()
    {
        foreach (var (name, id) in _kernelGlobals)
            yield return (name, Basic.IlBackend.IlRuntime.LoadGlobal(id));
    }

    /// <summary>%reset: svuota bag globali + cache + simboli dichiarati dal kernel.</summary>
    public void Reset()
    {
        foreach (var (_, id) in _kernelGlobals)
            Basic.IlBackend.IlRuntime.StoreGlobal(id, null);
        _kernelGlobals.Clear();
        _cache.Clear();
        _cellOrder.Clear();
        _sources.Clear();
        _lastOutput.Clear();
        _lastOk.Clear();
        _sql = null;
        VdbClear();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Celle speciali (Fase 2): %sql / %ai / %chart — il dispatch a magic NON passa dal
    // compilatore BASIC: la riga "%sql ..." è un'istruzione di CELL TYPE, come da piano §3
    // (celle `sql`, `ai`, `chart`). Il resto del source dopo la magia è l'input dell'handler.
    // ─────────────────────────────────────────────────────────────────────────────

    private enum MagicKind { Sql, Ai, Chart, Md, Vdb }

    private readonly record struct Magic(MagicKind Kind, string Arg);

    private static Magic? TryParseMagic(string source)
    {
        var firstLine = source.Split('\n')[0];
        var trimmed = firstLine.TrimStart();
        if (trimmed.Length < 2 || trimmed[0] != '%') return null;
        // la magia è la PRIMA PAROLA (es. "%sql", "%ai", "%chart"); il resto della cella è l'argomento
        var space = trimmed.IndexOfAny([' ', '\t', '\r']);
        var magicWord = space < 0 ? trimmed : trimmed[..space];
        var lower = magicWord.ToLowerInvariant();
        MagicKind kind = lower switch
        {
            "%sql" or "%sql:" => MagicKind.Sql,
            "%ai" or "%ai:" => MagicKind.Ai,
            "%chart" or "%chart:" => MagicKind.Chart,
            "%md" or "%md:" or "%markdown" => MagicKind.Md,
            "%vdb" or "%vdb:" => MagicKind.Vdb,
            _ => (MagicKind)(-1),
        };
        if ((int)kind < 0) return null;
        // argomento = ciò che segue la prima riga di magia (tutte le righe dopo, preservate)
        var afterMagic = space < 0 ? "" : firstLine[(space + 1)..];
        var rest = afterMagic + (source.Contains('\n') ? source[source.IndexOf('\n', StringComparison.Ordinal)..] : "");
        return new Magic(kind, rest.Trim());
    }

    private string ExecuteMagic(MagicKind kind, string arg, string cellId)
    {
        switch (kind)
        {
            case MagicKind.Sql: return ExecuteSqlMagic(arg);
            case MagicKind.Ai: return ExecuteAiMagic(arg);
            case MagicKind.Chart: return ExecuteChartMagic(arg, cellId);
            case MagicKind.Md: return arg; // %md: cella di documentazione — passthrough del testo
            case MagicKind.Vdb: return ExecuteVdbMagic(arg);
            default: return "(magia sconosciuta)";
        }
    }

    // --- %sql: esegue la query SQLite in una sessione in-memory PERSISTENTE per kernel
    // (le tabelle create in una cella restano per le successive — scope condiviso, come
    // le variabili BASIC). Stampa le righe come tabella monospaziata (output per-cell).
    private SqliteConnection? _sql;

    private string ExecuteSqlMagic(string sql)
    {
        if (string.IsNullOrWhiteSpace(sql)) return "(query vuota — %sql <query> | %sql CREATE/TABLE ecc.)";
        var conn = _sql ??= OpenSqlite();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        using var reader = cmd.ExecuteReader();

        var sw = new StringBuilder();
        var header = new List<string>();
        for (var i = 0; i < reader.FieldCount; i++) header.Add(reader.GetName(i));
        sw.AppendLine(string.Join(" | ", header));
        sw.AppendLine(new string('-', Math.Max(10, sw.Length == 0 ? 0 : header.Sum(h => h.Length) + 3 * Math.Max(0, header.Count - 1))));

        var rows = 0;
        while (reader.Read())
        {
            var vals = new string[reader.FieldCount];
            for (var i = 0; i < reader.FieldCount; i++)
                vals[i] = reader.IsDBNull(i) ? "NULL" : Convert.ToString(reader.GetValue(i), CultureInfo.InvariantCulture) ?? "";
            sw.AppendLine(string.Join(" | ", vals));
            rows++;
            if (rows >= 500) { sw.AppendLine("… (righe troncate a 500)"); break; }
        }
        if (rows == 0) sw.AppendLine("(0 righe)");
        sw.AppendLine($"[{rows} righe]");
        return sw.ToString().TrimEnd();
    }

    private static SqliteConnection OpenSqlite()
    {
        var conn = new SqliteConnection("Data Source=:memory:");
        conn.Open();
        return conn;
    }

    // --- %ai: inoltra il prompt al completer LLM attivo (LlmCompleterResolver.Default, lo
    // stesso usato dagli agenti/ASK). Nessun provider attivo → errore esplicito (mai stub).
    private static string ExecuteAiMagic(string prompt)
    {
        var completer = Basic.AI.Rag.LlmCompleterResolver.Default;
        if (completer == null)
            throw new InvalidOperationException(
                "%ai richiede un provider LLM attivo (AI REGISTER … / provider configurato). Nessun completer impostato.");
        if (string.IsNullOrWhiteSpace(prompt)) return "(prompt vuoto — %ai <prompt>)";
        return completer.Complete("", prompt);
    }

    /// <summary>Streaming %ai (Fase 3): invoca il completer con <c>onToken</c> per ogni chunk,
    /// quando il provider implementa <see cref="Basic.AI.Rag.IStreamingLlmCompleter"/>; altrimenti
    /// fallback a <c>Complete</c> (un solo chunk). Ritorna il testo COMPLETO. Mai stub: senza
    /// completer attivo → eccezione esplicita.</summary>
    public string AiStream(string prompt, Action<string> onToken)
    {
        var completer = Basic.AI.Rag.LlmCompleterResolver.Default
            ?? throw new InvalidOperationException(
                "%ai richiede un provider LLM attivo (AI REGISTER … / provider configurato). Nessun completer impostato.");
        if (string.IsNullOrWhiteSpace(prompt)) return "(prompt vuoto — %ai <prompt>)";

        if (completer is Basic.AI.Rag.IStreamingLlmCompleter streaming)
        {
            var sb = new StringBuilder();
            foreach (var chunk in streaming.CompleteStream("", prompt))
            {
                sb.Append(chunk);
                onToken(chunk);
            }
            return sb.ToString();
        }
        var full = completer.Complete("", prompt);
        onToken(full);
        return full;
    }

    // --- %chart: comando BASIC grafico CHART (BAR/LINE/PIE) eseguito come cella BASIC ma
    // SENZA PCodeVM: delega al driver grafico attivo dell'host corrente. Per il notebook
    // headless il risultato è la descrizione testuale del comando emesso (senza display SDL
    // il disegno non ha destinazione — errore chiaro, mai stub).
    private string ExecuteChartMagic(string arg, string cellId)
    {
        if (string.IsNullOrWhiteSpace(arg)) return "(chart vuoto — %chart BAR data | LINE ... | PIE ...)";
        // La cella chart contiene una sequenza di comandi CHART/GRAPHICS BASIC: la eseguiamo
        // con il percorso BASIC standard (il kernel la tratta come una normale cella code).
        var r = Execute(cellId + "#chart", arg);
        if (!r.Ok) return $"✗ {r.Error}";
        return r.Output.Length > 0 ? r.Output : $"✓ chart emesso: {arg}";
    }

    // --- %vdb: VectorDB per-notebook (SEMANTIC_SEARCH — piano §3 Fase 2). Una collection
    // persistente per kernel su SQLite in-memory, indiciata con l'embedding provider ATTIVO
    // (EmbeddingProviderResolver.Default). Comandi:
    //   %vdb add <sourceId> = <texto>    indicizza (upsert per sourceId)
    //   %vdb search <query> [topK]        ricerca semantica (default topK=5)
    //   %vdb count                        numero di documenti indicizzati
    //   %vdb clear                        svuota la collection
    // Grammar: una collection sola per kernel (nome "nb") — come un VectorDB dedicato al
    // notebook, senza il wiring alias/driver di AIVECTOR (qui basta la connection diretta).
    private SqliteConnection? _vdb;
    private long _vdbCollectionId = -1;

    private string ExecuteVdbMagic(string arg)
    {
        if (string.IsNullOrWhiteSpace(arg)) return "(vdb — %vdb add <id> = <testo> | search <q> [topK] | count | clear)";
        var parts = arg.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries);
        var cmd = parts[0].ToLowerInvariant();
        var rest = parts.Length > 1 ? parts[1].Trim() : "";
        switch (cmd)
        {
            case "add": return VdbAdd(rest);
            case "search": return VdbSearch(rest);
            case "count": return $"documenti indicizzati: {VdbCount()}";
            case "clear": VdbClear(); return "vdb: collection svuotata";
            default:
                // "sourceId = testo" è la forma breve (add implicito)
                var eq = arg.IndexOf('=');
                return eq > 0 ? VdbAdd(arg) : "(vdb: comando sconosciuto — add | search | count | clear)";
        }
    }

    private (SqliteConnection Conn, long CollectionId) EnsureVdb()
    {
        if (_vdb == null)
        {
            _vdb = new SqliteConnection("Data Source=:memory:");
            _vdb.Open();
            Basic.SQLite.Vector.SqlVectorSchema.EnsureSchema(_vdb);
            // provider di default deterministico se nessun embedding provider reale è attivo
            // (stesso default che il PCodeVm installa per le xUnit — MAI rete implicita).
            var prov = Basic.SQLite.Vector.EmbeddingProviderResolver.Default
                       ??= new Basic.SQLite.Vector.FixtureEmbeddingProvider();
            var dims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
            var indexer = new Basic.SQLite.Vector.SqlVectorIndexer(_vdb, prov);
            _vdbCollectionId = indexer.EnsureCollection("nb", dims, Basic.SQLite.Vector.VectorMetric.Cosine);
        }
        return (_vdb, _vdbCollectionId);
    }

    private string VdbAdd(string rest)
    {
        var eq = rest.IndexOf('=');
        if (eq <= 0) return "(vdb add: serve 'sourceId = testo')";
        var sourceId = rest[..eq].Trim();
        var text = rest[(eq + 1)..].Trim();
        if (sourceId.Length == 0 || text.Length == 0) return "(vdb add: sourceId e testo obbligatori)";
        var (conn, cid) = EnsureVdb();
        var prov = Basic.SQLite.Vector.EmbeddingProviderResolver.Default!;
        var indexer = new Basic.SQLite.Vector.SqlVectorIndexer(conn, prov);
        var outcome = indexer.IndexText(cid, sourceId, text);
        return $"✓ {sourceId}: {outcome}";
    }

    private string VdbSearch(string rest)
    {
        var (conn, cid) = EnsureVdb();
        // topK opzionale come ultimo token numerico; il resto è la query
        var query = rest;
        var topK = 5;
        var lastSpace = rest.LastIndexOf(' ');
        if (lastSpace > 0 && int.TryParse(rest[(lastSpace + 1)..], out var k) && k > 0)
        {
            topK = k;
            query = rest[..lastSpace].Trim();
        }
        if (query.Length == 0) return "(vdb search: query vuota)";
        var prov = Basic.SQLite.Vector.EmbeddingProviderResolver.Default!;
        var search = new Basic.SQLite.Vector.SqlVectorSearch(conn, prov);
        var resp = search.SearchByText(cid, query, topK);
        if (resp.Status == Basic.SQLite.Vector.VectorSearchStatus.InsufficientEvidence || resp.Results.Count == 0)
            return "(nessun risultato)";
        var sb = new StringBuilder();
        foreach (var h in resp.Results)
        {
            var snippet = h.Text.Length > 90 ? h.Text[..90] + "…" : h.Text;
            sb.AppendLine($"[{h.SourceId}] score={h.Score:F4}  {snippet.Replace('\n', ' ')}");
        }
        return sb.ToString().TrimEnd();
    }

    private int VdbCount()
    {
        var (conn, cid) = EnsureVdb();
        var prov = Basic.SQLite.Vector.EmbeddingProviderResolver.Default!;
        return new Basic.SQLite.Vector.SqlVectorIndexer(conn, prov).CountEmbeddings(cid);
    }

    private void VdbClear()
    {
        // su :memory: svuotare = ricreare la connessione (schema + collection si rigenerano lazy)
        try { _vdb?.Dispose(); } catch { /* best-effort */ }
        _vdb = null;
        _vdbCollectionId = -1;
    }

    /// <summary>Celle note al kernel in ordine di DEFINIZIONE, con source e dipendenze.</summary>
    public IReadOnlyList<CellInfo> Cells()
        => _cellOrder
            .Where(id => _cache.ContainsKey(id))
            .Select(id => new CellInfo(id, _cache[id].Source, _cache[id].Reads, _cache[id].Writes))
            .ToList();

    /// <summary>Ordine di esecuzione run-all: topologico stabile — la cella che LEGGE un nome
    /// va DOPO la cella che lo SCRIVE. A parità di vincoli vince l'ordine di definizione
    /// (semantica Jupyter: dall'alto in basso). Cicli/dipendenze irrisolvibili: flush in
    /// ordine di definizione, nessun errore (è il comportamento REPL: l'utente ricompila).</summary>
    public IReadOnlyList<string> Order()
    {
        var cells = Cells();
        var byId = cells.ToDictionary(c => c.CellId, c => c, StringComparer.Ordinal);
        var order = new List<string>();
        var done = new HashSet<string>(StringComparer.Ordinal);
        var pending = cells.Select(c => c.CellId).ToList();

        while (pending.Count > 0)
        {
            var progressed = false;
            foreach (var id in pending.ToList())
            {
                var reads = byId[id].Reads;
                var hasUnsatisfiedDep = cells.Any(c => c.CellId != id
                                                       && !done.Contains(c.CellId)
                                                       && c.Writes.Overlaps(reads));
                if (!hasUnsatisfiedDep)
                {
                    order.Add(id);
                    done.Add(id);
                    pending.Remove(id);
                    progressed = true;
                }
            }
            if (!progressed) { order.AddRange(pending); break; }
        }
        return order;
    }

    /// <summary>Run-all: esegue le celle in ordine topologico (%order), rispettando la cache
    /// per-cell. Ritorna i risultati per cellId nell'ordine di esecuzione.</summary>
    public IReadOnlyList<(string CellId, CellResult Result)> RunAll()
    {
        var results = new List<(string, CellResult)>();
        foreach (var id in Order())
            results.Add((id, Execute(id, _cache[id].Source)));
        return results;
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _cache.Clear();
        _cellOrder.Clear();
        try { _sql?.Dispose(); } catch { /* best-effort */ }
        _sql = null;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Formato .gmnb — load/save (piano §3: JSON con le celle in ordine + kernel_state)
    // ─────────────────────────────────────────────────────────────────────────────

    private sealed record GmnbCell(string CellId, string Source);
    private sealed record GmnbFile(int Version, List<GmnbCell> Cells, Dictionary<string, object?> KernelState);

    /// <summary>Salva il notebook in formato .gmnb (JSON): celle in ordine di definizione +
    /// snapshot delle variabili globali (kernel_state). Il LOAD ricostruisce ri-eseguendo le
    /// celle (più robusto del ripristino diretto della bag statica, i SymbolId sono allocati
    /// in ordine dal compile lazy — un restore puntuale non è affidabile tra processi).</summary>
    public void Save(string path)
    {
        var cells = _cellOrder
            .Where(id => _sources.ContainsKey(id))
            .Select(id => new GmnbCell(id, _sources[id]))
            .ToList();
        var state = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var (name, id) in _kernelGlobals)
            state[name] = SerializeValue(Basic.IlBackend.IlRuntime.LoadGlobal(id));
        var doc = new GmnbFile(1, cells, state);
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        // Versioning (Fase 3 — piano §3: autosave + .gmnb.bak): prima di sovrascrivere,
        // l'ultima versione esistente diventa <path>.bak (una sola copia di backup).
        if (File.Exists(path))
        {
            try { File.Copy(path, path + ".bak", overwrite: true); } catch { /* best-effort */ }
        }
        File.WriteAllText(path, System.Text.Json.JsonSerializer.Serialize(
            doc, new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
    }

    /// <summary>Carica un .gmnb: reset del kernel + ri-esecuzione delle celle in ordine di
    /// definizione (lo stato si ricostruisce da sé). Ritorna i risultati cella-per-cella;
    /// una cella che fallisce non blocca le successive (il resto viene comunque eseguito).
    /// KernelState salvato è usato solo come riferimento/validazione, non ripristinato
    /// direttamente (vedi Save per il perché).</summary>
    public IReadOnlyList<(string CellId, CellResult Result)> Load(string path)
    {
        var doc = System.Text.Json.JsonSerializer.Deserialize<GmnbFile>(
            File.ReadAllText(path),
            new System.Text.Json.JsonSerializerOptions { PropertyNameCaseInsensitive = true })
            ?? throw new InvalidOperationException($"formato .gmnb non valido: {path}");
        Reset();
        var results = new List<(string, CellResult)>();
        foreach (var c in doc.Cells)
            results.Add((c.CellId, Execute(c.CellId, c.Source)));
        return results;
    }

    private static object? SerializeValue(object? v) => v switch
    {
        null => null,
        bool or double or float or int or long or short or byte or decimal or string => v,
        _ => v.ToString(),
    };

    /// <summary>Export HTML autoportante e stampabile (Fase 3): ogni cella con source (code o
    /// magic) e, se eseguita, il suo ultimo output (✓ verde / ✗ rosso). CSS @media print per
    /// la stampa → il PDF si ottiene dal browser (Stampa → Salva come PDF): il progetto non
    /// ha una libreria PDF writer C# (solo PdfPig, lettore), quindi l'HTML è l'artefatto
    /// canonico print-ready.</summary>
    public string ExportHtml()
    {
        var sb = new StringBuilder();
        sb.AppendLine("<!DOCTYPE html><html lang=\"it\"><head><meta charset=\"utf-8\">");
        sb.AppendLine("<title>GMBASIC Notebook</title><style>");
        sb.AppendLine("body{font:14px/1.45 ui-monospace,Menlo,monospace;margin:0;background:#fff;color:#1a1a1a;padding:24px}");
        sb.AppendLine("h1{font-size:18px;border-bottom:2px solid #333;padding-bottom:6px}");
        sb.AppendLine(".cell{margin:10px 0 18px;border:1px solid #ccc;border-radius:6px;overflow:hidden}");
        sb.AppendLine(".cell .idx{background:#eee;padding:3px 10px;font-size:11px;color:#555}");
        sb.AppendLine(".cell pre{background:#f7f7f7;margin:0;padding:8px 12px;white-space:pre-wrap}");
        sb.AppendLine(".cell .out{border-top:1px solid #ccc;padding:6px 12px;white-space:pre-wrap;background:#f2fbf2}");
        sb.AppendLine(".cell .out.err{background:#fdf0f0;color:#a00}");
        sb.AppendLine("@media print{.cell{page-break-inside:avoid}}");
        sb.AppendLine("</style></head><body>");
        sb.AppendLine("<h1>GMBASIC Notebook</h1>");
        var n = 0;
        foreach (var id in _cellOrder)
        {
            if (!_sources.TryGetValue(id, out var source)) continue;
            n++;
            sb.AppendLine($"<div class=\"cell\"><div class=\"idx\">cella {n} ({id})</div>");
            sb.Append("<pre>").Append(HtmlEscape(source)).AppendLine("</pre>");
            if (_lastOutput.TryGetValue(id, out var output) && output.Length > 0)
            {
                var err = _lastOk.TryGetValue(id, out var ok) && !ok;
                sb.Append($"<div class=\"out{(err ? " err" : "")}\">").Append(HtmlEscape(output)).AppendLine("</div>");
            }
            sb.AppendLine("</div>");
        }
        sb.AppendLine("<footer style=\"margin-top:24px;font-size:11px;color:#888\">Esportato da GMBASIC Notebook — stampa per ottenere il PDF</footer>");
        sb.AppendLine("</body></html>");
        return sb.ToString();
    }

    /// <summary>Scrive ExportHtml su file.</summary>
    public void ExportHtmlFile(string path)
    {
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(path, ExportHtml());
    }

    /// <summary>Export PDF VERO (opzione D, 2026-09-08): l'HTML print-ready viene reso in PDF dal
    /// Chromium del repo (headless --print-to-pdf — motore provato, stesso di Page.printToPDF).
    /// Nessuna nuova dependency (PdfPig resta reader). Se il CDP è disponibile si preferisce
    /// PrintToPdfAsync; la CLI headless è il percorso deterministico (Page.printToPDF via WS sul
    /// page target non è affidabile su tutte le sessioni CDP).</summary>
    public void ExportPdfFile(string path)
    {
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        var tmp = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "gmb_nb_" + Guid.NewGuid().ToString("N") + ".html");
        System.IO.File.WriteAllText(tmp, ExportHtml());
        try
        {
            var chrome = FindChromeBinary();
            if (chrome is null)
                throw new InvalidOperationException("ExportPdf: Chromium non trovato (" + System.IO.Path.Combine(CommonLib.GmbiPaths.LibsDir, "chromium") + "/<rid>) — esegui 'gmb setup' / launch_chromium.sh");
            var psi = new System.Diagnostics.ProcessStartInfo
            {
                FileName = chrome,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                Arguments = $"--headless --disable-gpu --no-sandbox --print-to-pdf=\"{path}\" \"file://{tmp}\""
            };
            using var proc = System.Diagnostics.Process.Start(psi)!;
            proc.WaitForExit();
            if (!System.IO.File.Exists(path) || new System.IO.FileInfo(path).Length == 0)
                throw new InvalidOperationException("ExportPdf: Chromium non ha prodotto il PDF (exit " + proc.ExitCode + ")");
        }
        finally { try { System.IO.File.Delete(tmp); } catch { } }
    }

    private static string? FindChromeBinary()
    {
        var names = new[] { "Google Chrome for Testing", "Chromium", "chrome", "chromium" };
        var roots = new[]
        {
            System.IO.Path.Combine(CommonLib.GmbiPaths.LibsDir, "chromium"),   // cache utente
            System.IO.Path.Combine(AppContext.BaseDirectory, "chromium"),
        };
        var rids = new[] { "osx-arm64", "osx-x64", "linux-x64", "linux-arm64", "win-x64" };
        foreach (var root in roots)
            foreach (var rid in rids)
            {
                var baseDir = System.IO.Path.Combine(root, rid);
                if (!System.IO.Directory.Exists(baseDir)) continue;
                foreach (var f in System.IO.Directory.EnumerateFiles(baseDir, "*", System.IO.SearchOption.AllDirectories))
                {
                    var fn = System.IO.Path.GetFileName(f);
                    foreach (var n in names)
                        if (fn.Equals(n, StringComparison.OrdinalIgnoreCase))
                        {
                            try { if (System.IO.File.Exists(f)) return f; } catch { }
                        }
                }
            }
        return null;
    }

    private static string FindRepoRoot()
    {
        var d = new System.IO.DirectoryInfo(AppContext.BaseDirectory);
        while (d != null) { if (System.IO.File.Exists(System.IO.Path.Combine(d.FullName, "GMBASIC.sln"))) return d.FullName; d = d.Parent; }
        return AppContext.BaseDirectory;
    }

    private static string HtmlEscape(string s) =>
        s.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;");


    /// <summary>Esporta il notebook come programma BASIC eseguibile (.bas): le celle CODE in
    /// ordine di definizione, concatenate; le celle MAGIC (interattive: %sql/%ai/%chart/%md/
    /// %vdb) NON sono portabili in un .bas autonomo → diventano un commento REM esplicito.
    /// L'exportato è compilabile con Orchestrator (test di validazione in suite).</summary>
    public void ExportBasic(string path)
    {
        var sb = new StringBuilder();
        sb.AppendLine("' GMBASIC Notebook — export .bas (celle in ordine di definizione)");
        foreach (var id in _cellOrder)
        {
            if (!_sources.TryGetValue(id, out var source)) continue;
            if (TryParseMagic(source) is { } magic)
            {
                sb.AppendLine($"REM --- cella {id}: magia {magic.Kind} (interattiva, non esportabile in .bas)");
                continue;
            }
            sb.AppendLine($"' --- cella {id} ---");
            sb.AppendLine(source);
            sb.AppendLine();
        }
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(path, sb.ToString());
    }
}
