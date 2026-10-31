// Diagnostici distinti per chiamate non risolte: "simbolo non trovato" vs "simbolo ambiguo"
// (regola del compilatore professionale), su entrambi i codegen C#:
//  - percorso bytecode (Compiler.Compiler, usato da PCode/VM chunk)
//  - percorso IL (IlProgramCompiler)
// Verifica empirica: testa i MESSAGGI effettivi prodotti.
using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class UnresolvedDiagnosticsTests
{
    private static void CompileBytecode(string src) =>
        new global::Compiler.Compiler().Compile(Basic.Orchestrator.CompileFromString(src, "unres.bas", null, verbose: false));

    private static void CompileIl(string src)
    {
        var program = Basic.Orchestrator.CompileFromString(src, "unres.bas", null, verbose: false);
        var asmBuilder = Basic.IlBackend.IlProgramCompiler.CompileToAssembly(program, "Unres_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new System.IO.MemoryStream();
        asmBuilder.Save(ms);
    }

    [Fact]
    public void QualifiedNotFound_Bytecode_IsNotFound()
    {
        var ex = Assert.ThrowsAny<Exception>(() => CompileBytecode("""
            IMPORT AI
            SUB Main()
              DIM x AS STRING
              x = AI.SIMBOLOINESISTENTE("ciao")
              PRINT x
            END SUB
            """));
        Assert.Contains("Simbolo non trovato", ex.Message);
        Assert.DoesNotContain("ambigua", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void QualifiedNotFound_Il_IsNotFound()
    {
        var ex = Assert.ThrowsAny<Exception>(() => CompileIl("""
            IMPORT AI
            SUB Main()
              DIM x AS STRING
              x = AI.SIMBOLOINESISTENTE("ciao")
              PRINT x
            END SUB
            """));
        Assert.Contains("Simbolo non trovato", ex.Message);
        Assert.DoesNotContain("ambigua", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    private static UnresolvedCallExpr MakeUnresolved(UnresolvedReason reason, string name)
        => new UnresolvedCallExpr(
            new global::Basic.Token(global::Basic.TokenKind.Identifier, name, null, global::Basic.SourcePosition.Unknown),
            ImmutableArray.Create<global::Basic.Expr>(),
            new global::Basic.StringType(),
            reason,
            ImmutableArray.Create<object>());

    [Fact]
    public void Diagnostics_Distinguiscono_Ambiguous_From_NotFound()
    {
        var amb = UnresolvedCallDiagnostics.Build(MakeUnresolved(UnresolvedReason.AmbiguousBuiltinFunctions, "CELL"));
        Assert.Contains("Chiamata ambigua", amb);
        Assert.Contains("CELL", amb);
        Assert.Contains("qualifica", amb, StringComparison.OrdinalIgnoreCase);
        Assert.DoesNotContain("Simbolo non trovato", amb);

        var nf = UnresolvedCallDiagnostics.Build(MakeUnresolved(UnresolvedReason.FunctionNotFound, "FOO"));
        Assert.Contains("Simbolo non trovato", nf);
        Assert.Contains("FOO", nf);
        Assert.DoesNotContain("ambigua", nf, StringComparison.OrdinalIgnoreCase);

        var notImported = UnresolvedCallDiagnostics.Build(MakeUnresolved(UnresolvedReason.BuiltinNamespaceNotImported, "BAR"));
        Assert.Contains("Simbolo non trovato", notImported);
        Assert.Contains("IMPORT", notImported);
    }

    [Fact]
    public void Bare_UnknownBuiltin_Is_NotFound()
    {
        // Nome nudo di libreria sconosciuto (nessun builtin/variabile): errore chiaro "non definito".
        var ex = Assert.ThrowsAny<Exception>(() => CompileBytecode("""
            SUB Main()
              DIM x AS DOUBLE
              x = NESSUNAFUNZIONEESISTENTE(1)
              PRINT x
            END SUB
            """));
        Assert.Contains("non definito", ex.Message, StringComparison.OrdinalIgnoreCase);
        Assert.DoesNotContain("ambigua", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void BareBuiltin_NamespaceNotImported_Gives_Hint_At_Link()
    {
        // SEARCHMULTI è una funzione di libreria SOLO nel namespace AI (non importato): il nome
        // nudo deve dare l'hint "namespace non importato / qualifica / IMPORT" — a LINK-TIME,
        // dove lo scope è completo (le FUNCTION/SUB utente omonime si risolvono prima, niente
        // shadowing: regressione SciPy 'y' evitata).
        var ex = Assert.ThrowsAny<Exception>(() => CompileBytecode("""
            SUB Main()
              DIM r AS STRING
              r = SEARCHMULTI("q", 3)
              PRINT r
            END SUB
            """));
        Assert.Contains("SEARCHMULTI", ex.Message);
        Assert.Contains("namespace non importato", ex.Message, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("IMPORT", ex.Message);
        Assert.DoesNotContain("ambigua", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    // NOTA (harness artificiale E_AMBIGUOUS end-to-end): oggi il PARSER non produce mai
    // UnresolvedReason.AmbiguousBuiltinFunctions per coppie REALI (le duplicazioni tra namespace
    // condividono lo stesso TokenKind/alias e collassano; le firme incompatibili filtrano).
    // Provate senza esito: MATH+TORCH (MAX/MIN/EXP/ABS), DBF+ISAM/RDD (EOF/BOF/DELETED/FLOCK),
    // CELL/CELLSTR CONSOLE+GRAPHICS. Il harness sotto inietta due candidati GENUINAMENTE ambigui
    // (stesso nome, stessa firma, TokenKind diversi) tramite la seam del registry (P0-lite).
    private sealed class RegistryWithAmbiguity : global::Basic.IParserRegistry
    {
        private readonly IParserRegistry _inner = global::Basic.StdLib.Instance;
        public RegistryWithAmbiguity()
        {
            var math = _inner.BuiltinProcedures["MATH"].OfType<NativeProcedureDefinition>()
                .First(p => p.OriginalName.Lexeme.Equals("ABS", StringComparison.OrdinalIgnoreCase));
            var name = new global::Basic.Token(global::Basic.TokenKind.Identifier, "SYNAMB", null, global::Basic.SourcePosition.Unknown);
            NativeProcedureDefinition Syn(int id, global::Basic.TokenKind kind) =>
                new NativeProcedureDefinition(id, math.Flags, name, math.Parameters, math.ReturnType,
                    kind, IsFunction: math.IsFunction, ExecutionKind: math.ExecutionKind);
            var c1 = Syn(-101, global::Basic.TokenKind.Sin);
            var c2 = Syn(-102, global::Basic.TokenKind.Cos);

            var procs = new Dictionary<string, IEnumerable<ProcedureDefinition>>();
            foreach (var (ns, defs) in _inner.BuiltinProcedures) procs[ns] = defs;
            procs["CONSOLE"] = procs["CONSOLE"].Concat(new ProcedureDefinition[] { c1 }).ToList();
            procs["MATH"] = procs["MATH"].Concat(new ProcedureDefinition[] { c2 }).ToList();
            BuiltinProcedures = procs;

            var byName = new Dictionary<string, List<string>>();
            foreach (var (k, v) in _inner.FunctionNameToNamespacesMap) byName[k] = v;
            byName["SYNAMB"] = new List<string> { "CONSOLE", "MATH" };
            FunctionNameToNamespacesMap = byName;

            AllBuiltinFunctionNames = new HashSet<string>(_inner.AllBuiltinFunctionNames) { "SYNAMB" };
            BuiltinFunctionProvidersMap = new Dictionary<TokenKind, List<string>>(_inner.BuiltinFunctionProvidersMap);
            CommandNameToTokenKindMap = new Dictionary<string, TokenKind>(_inner.CommandNameToTokenKindMap);
        }

        public HashSet<string> AllBuiltinFunctionNames { get; }
        public Dictionary<TokenKind, List<string>> BuiltinFunctionProvidersMap { get; }
        public Dictionary<string, TokenKind> CommandNameToTokenKindMap { get; }
        public Dictionary<string, List<string>> FunctionNameToNamespacesMap { get; }
        public IReadOnlyDictionary<string, IEnumerable<ProcedureDefinition>> BuiltinProcedures { get; }
    }

    [Fact]
    public void Registry_Seam_Materializes_AmbiguousCandidates_And_Ambiguity_Is_HardError()
    {
        // P0-lite: la seam CompileFromStringWithRegistry materializza i builtin DAL registry
        // fornito nei Symbol; e il choke a fine link trasforma l'ambiguità VERA in hard-error.
        const string safe = "SUB Main()\nEND SUB\n";
        var reg = new RegistryWithAmbiguity();
        var (compilerSafe, program) = global::Basic.Pipeline.SourcePipeline.CompileFromStringWithRegistry(safe, "safe.bas", reg);

        var c1 = program.Symbols.GetProceduresForNamespace("CONSOLE").OfType<NativeProcedureDefinition>()
            .Where(p => p.OriginalName.Lexeme.Equals("SYNAMB", StringComparison.OrdinalIgnoreCase)).ToList();
        var c2 = program.Symbols.GetProceduresForNamespace("MATH").OfType<NativeProcedureDefinition>()
            .Where(p => p.OriginalName.Lexeme.Equals("SYNAMB", StringComparison.OrdinalIgnoreCase)).ToList();
        Assert.Single(c1);
        Assert.Single(c2);
        Assert.NotEqual(c1[0].BuiltinKind, c2[0].BuiltinKind);
        Assert.Null(Record.Exception(() => compilerSafe.Compile(program)));

        // I due candidati risolvono QUALIFICATI per namespace (materializzazione ok).
        foreach (var ns in new[] { "CONSOLE", "MATH" })
        {
            var srcQ = $"SUB Main()\n DIM x AS DOUBLE\n x = {ns}.SYNAMB(1)\nEND SUB\n";
            var (cQ, progQ) = global::Basic.Pipeline.SourcePipeline.CompileFromStringWithRegistry(srcQ, "ambq.bas", reg);
            Assert.Null(Record.Exception(() => cQ.Compile(progQ)));
        }

        // Nome nudo con due candidati diversi-kind -> hard-error E_AMBIGUOUS al LINK (choke unico).
        const string amb = """
            SUB Main()
              DIM x AS DOUBLE
              x = SYNAMB(1)
              PRINT x
            END SUB
            """;
        var ex = Assert.ThrowsAny<Exception>(() =>
            global::Basic.Pipeline.SourcePipeline.CompileFromStringWithRegistry(amb, "amb.bas", reg));
        Assert.Contains("Chiamata ambigua", ex.Message);
        Assert.Contains("qualifica", ex.Message, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("CONSOLE", ex.Message);
        Assert.Contains("MATH", ex.Message);
    }
}
