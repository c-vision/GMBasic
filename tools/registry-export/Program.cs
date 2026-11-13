// gmb-registry-export — P0 del monorepo.
//
// Unico generatore dei derivati del registry, SENZA Python:
//   canonical.registry.json (C# + copia in shared/spec/registry)
//   inventory.json, comparison-existing.json, coverage.json
//   stdlib_tokens.csv (+ unique) e copia in shared/spec/registry
//   gmb_registry.gen.h, CanonicalLookup.g.cs
//   gmb_pconst_gen.inc
//   gmb_ns_lookup.h, gmb_fn_lookup.h
//   gmb_builtin_codes.h (nome/Ns.Nome -> codice BuiltInFunction, per i frammenti EVAL/&)
//   gmb_ns_types.h (entrambe le copie del compiler C)
//   gmb_tokenkind.h (sync additivo da TokenKind.cs)
//   gmb_db_tokenkind_gen.h (ordinali Top/Bottom)
//
// Uso:
//   registry-export                  → canonical JSON su stdout
//   registry-export --all            → rigenera tutto
//   registry-export --check          → non scrive; exit 1 se un derivato diverge
//   registry-export --check-enum-ids → gate ID duplicati (exit 1 se collide)
//   registry-export --inventory <out>
//
// Nessun file viene toccato in modalità default (stdout).

using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Basic;

var argsList = args.ToList();
var repoRoot = FindMonorepoRoot();

if (argsList.Contains("--signatures"))
{
    // Catalogo FIRME per il brainvault in C (`gmb brain buildvault`): nome, tipo di ritorno
    // e parametri (nome, tipo, opzionale/byref/array). Generato dalla StdLib (fonte unica):
    // il runtime C lo legge da shared/spec/registry/stdlib-signatures.json.
    var reg0 = (IParserRegistry)Basic.StdLib.Instance;
    var sigs = CollectSignatures(reg0);
    var sigJson = SignaturesJson(sigs);
    var sigOut = argsList.SkipWhile(a => a != "--signatures").Skip(1).FirstOrDefault();
    if (sigOut is null || sigOut.StartsWith('-'))
        sigOut = Path.Combine(repoRoot, "shared/spec/registry/stdlib-signatures.json");
    File.WriteAllText(sigOut, sigJson);
    Console.Error.WriteLine($"wrote {sigOut} ({sigs.Count} procedure)");
    return 0;
}
if (argsList.Contains("--check-enum-ids")) return CheckEnumIds(repoRoot);
if (argsList.Contains("--builtin-codes"))
{
    // Tabella NOME -> CODICE builtin per il compilatore dei FRAMMENTI EVAL/& (gmb_bytecode):
    // un builtin di namespace compilato a runtime finiva in OP_CALL_EXTERNAL, che l'host di
    // eval non risolve ("MACRO ERROR: chiamata esterna non disponibile" su C99/VM-C mentre
    // IL/PCode davano il valore). Il CODICE è il valore di BuiltInFunction (= il numero che
    // `gmb_builtin_dispatch` usa per trovare `gmb_bf_N`).
    var reg1 = (IParserRegistry)Basic.StdLib.Instance;
    var (procs1, _, _) = Collect(reg1);
    var codesOut = argsList.SkipWhile(a => a != "--builtin-codes").Skip(1).FirstOrDefault();
    if (codesOut is null || codesOut.StartsWith('-'))
        codesOut = Path.Combine(repoRoot, "GMBasic/native/gmb_bytecode/include/gmb_builtin_codes.h");
    var codesText = BuildBuiltinCodesH(procs1, Path.Combine(repoRoot, "GMBasicSharp"));
    var cur = File.Exists(codesOut) ? File.ReadAllText(codesOut) : null;
    if (cur == codesText) { Console.Error.WriteLine($"invariato: {Rel(repoRoot, codesOut)}"); return 0; }
    File.WriteAllText(codesOut, codesText);
    Console.Error.WriteLine($"wrote {Rel(repoRoot, codesOut)}");
    return 0;
}
if (argsList.Contains("--all")) return RunAll(repoRoot, write: true);
if (argsList.Contains("--check")) return RunAll(repoRoot, write: false);

var registry = (IParserRegistry)Basic.StdLib.Instance;
var (procs, cmds, consts) = Collect(registry);
var canonJson = CanonicalJson(procs, cmds, consts);

if (argsList.Count > 0 && argsList[0] == "--inventory")
{
    File.WriteAllText(argsList.Count > 1 ? argsList[1] : "inventory.json", InventoryJson(procs, registry));
    return 0;
}
if (argsList.Count == 0 || argsList[0] == "-") Console.WriteLine(canonJson);
else File.WriteAllText(argsList[0], canonJson);
return 0;

// ─────────────────────────────── collection ───────────────────────────────

// Nome breve leggibile del tipo (parità con BrainVault.FormatType del vecchio toolchain C#).
static string FormatType(DataType t) => t switch
{
    StringType => "string",
    DoubleType => "double",
    IntegerType => "int",
    LongType => "long",
    BitType => "bool",
    ByteType => "byte",
    ArrayType at => FormatType(at.ElementType) + "[]",
    DictType => "dict",
    _ => t.ToString(),
};

static (List<ProcRow> procs, List<CmdRow> cmds, List<ConstRow> consts) Collect(IParserRegistry reg)
{
    var procs = new List<ProcRow>();
    foreach (var ns in reg.BuiltinProcedures.Keys.OrderBy(x => x, StringComparer.Ordinal))
    {
        var defs = reg.BuiltinProcedures[ns]
            .OrderBy(p => p.OriginalName.Lexeme, StringComparer.Ordinal)
            .ToList();
        foreach (var p in defs)
        {
            if (p is not NativeProcedureDefinition n) continue;
            var req = n.Parameters.Count(x => !x.Flags.HasFlag(SymbolFlags.IsOptional) && !x.Flags.HasFlag(SymbolFlags.IsParamArray));
            procs.Add(new ProcRow
            {
                Ns = ns,
                Name = n.OriginalName.Lexeme,
                TokenKind = (int)n.BuiltinKind,
                KindName = n.BuiltinKind.ToString(),
                DisplayKind = n.OriginalName.Kind.ToString(),
                Fn = n.IsFunction,
                ArityMin = req,
                ArityMax = n.Parameters.Length,
                Pure = n.ExecutionKind == NativeExecutionKind.Pure
            });
        }
    }

    var consts = new List<ConstRow>();
    var std = Basic.StdLib.Instance;
    foreach (var ns in std.BuiltinConstants.Keys.OrderBy(x => x, StringComparer.Ordinal))
    {
        var cs = std.BuiltinConstants[ns]
            .OrderBy(c => c.OriginalToken.Lexeme, StringComparer.Ordinal)
            .ToList();
        foreach (var c in cs)
        {
            double? v = (c.InitialValue as NumberExpr)?.Value;
            if (v is null) continue;
            consts.Add(new ConstRow { Ns = ns, Name = c.OriginalToken.Lexeme, Value = v.Value });
        }
    }

    var cmds = reg.CommandNameToTokenKindMap
        .OrderBy(kv => kv.Key, StringComparer.Ordinal)
        .Select(kv => new CmdRow { Name = kv.Key, TokenKind = (int)kv.Value, KindName = kv.Value.ToString() })
        .ToList();

    return (procs, cmds, consts);
}

static string CanonicalJson(List<ProcRow> procs, List<CmdRow> cmds, List<ConstRow> consts)
{
    var withIds = procs.Select((p, i) => new
    {
        p.Ns, p.Name, Id = i + 1, p.TokenKind, p.KindName, p.Fn, p.ArityMin, p.ArityMax, p.Pure
    });
    var constWithIds = consts.Select((c, i) => new { c.Ns, c.Name, Id = 2000000 + i + 1, c.Value });
    var cmdRows = cmds.Select((c, i) => new { c.Name, Id = 1000000 + i + 1, c.TokenKind, c.KindName });

    var doc = new
    {
        Schema = "gmb.registry@1",
        Generated = "gmb-registry-export",
        Note = "id procedure = ordinale stabile su (ns,name); comandi = offset 1000000; costanti = offset 2000000. Append-only: mai riusare id.",
        Procedures = withIds,
        Commands = cmdRows,
        Constants = constWithIds,
        Count = procs.Count,
        CommandsCount = cmds.Count,
        ConstantsCount = consts.Count
    };

    return JsonSerializer.Serialize(doc, new JsonSerializerOptions
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    });
}

static string InventoryJson(List<ProcRow> procs, IParserRegistry reg)
{
    var byName = procs.GroupBy(r => r.Name, StringComparer.OrdinalIgnoreCase)
        .Where(g => g.Select(x => x.Ns).Distinct().Count() > 1)
        .ToList();
    var multiKindInNs = procs.GroupBy(r => r.Ns.ToUpperInvariant() + "\u001f" + r.Name.ToUpperInvariant())
        .Where(g => g.Select(x => x.TokenKind).Distinct().Count() > 1)
        .ToList();
    var distinctKinds = procs.Select(r => r.TokenKind).Distinct().Count();
    return JsonSerializer.Serialize(new
    {
        count = procs.Count,
        namespaces = reg.BuiltinProcedures.Count,
        distinctTokenKinds = distinctKinds,
        namesInMultipleNamespaces = byName.Count,
        namesInMultipleNamespacesSample = byName.Take(20).Select(g => g.Key).ToList(),
        sameNsNameDifferentKinds = multiKindInNs.Select(g => g.Key.Replace('\u001f', '.')).Take(20).ToList()
    }, new JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
}

// Catalogo FIRME (per il brainvault in C): generato dalla StdLib, stessa fonte del registry.
static List<SigRow> CollectSignatures(IParserRegistry reg)
{
    var sigs = new List<SigRow>();
    foreach (var ns0 in reg.BuiltinProcedures.Keys.OrderBy(x => x, StringComparer.Ordinal))
    {
        foreach (var p0 in reg.BuiltinProcedures[ns0].OrderBy(p => p.OriginalName.Lexeme, StringComparer.Ordinal))
        {
            if (p0 is not NativeProcedureDefinition n0) continue;
            sigs.Add(new SigRow
            {
                Ns = ns0,
                Name = n0.OriginalName.Lexeme,
                Fn = n0.IsFunction,
                Return = FormatType(n0.ReturnType),
                Params = n0.Parameters.Select(x => new SigParam
                {
                    Name = x.OriginalName.Lexeme,
                    Type = FormatType(x.DataType),
                    Optional = x.Flags.HasFlag(SymbolFlags.IsOptional),
                    ByRef = x.Flags.HasFlag(SymbolFlags.IsByRef),
                    Array = x.Flags.HasFlag(SymbolFlags.IsArray),
                }).ToList(),
            });
        }
    }
    return sigs;
}

static string SignaturesJson(List<SigRow> sigs) =>
    JsonSerializer.Serialize(new { schema = "gmb.stdlib-signatures@1", procedures = sigs },
        new JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase }) + "\n";

// ─────────────────────────────── --all / --check ───────────────────────────────

static int RunAll(string root, bool write)
{
    var gs = Path.Combine(root, "GMBasicSharp");
    var gb = Path.Combine(root, "GMBasic");
    var sh = Path.Combine(root, "shared");
    var reg = (IParserRegistry)Basic.StdLib.Instance;
    var (procs, cmds, consts) = Collect(reg);

    // Sync ADDITIVO dell.enum C: va calcolato PRIMA dei lookup (che verificano i membri).
    var tkPath = Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_tokenkind.h");
    var tkText = SyncTokenKindH(File.ReadAllText(tkPath), gs, out var added);
    var cMembers = CMembers(gb, tkText);

    var outputs = new List<(string path, string text)>();
    outputs.Add((tkPath, tkText));
    var canon = CanonicalJson(procs, cmds, consts);
    outputs.Add((Path.Combine(gs, "common/Registry/canonical.registry.json"), canon));
    outputs.Add((Path.Combine(sh, "spec/registry/canonical.registry.json"), canon));
    outputs.Add((Path.Combine(gs, "common/Registry/inventory.json"), InventoryJson(procs, reg)));

    outputs.Add((Path.Combine(sh, "spec/registry/stdlib-signatures.json"), SignaturesJson(CollectSignatures(reg))));

    var csv = BuildCsv(procs);
    outputs.Add((Path.Combine(gs, "common/Registry/stdlib_tokens.csv"), csv));
    outputs.Add((Path.Combine(sh, "spec/registry/stdlib_tokens.csv"), csv));
    outputs.Add((Path.Combine(gs, "common/Registry/stdlib_tokens_unique.csv"), BuildCsv(procs, unique: true)));
    var surface = BuildSurfaceCsv(procs, cmds, consts);
    outputs.Add((Path.Combine(gs, "common/Registry/surface.csv"), surface));
    outputs.Add((Path.Combine(sh, "spec/registry/surface.csv"), surface));

    outputs.Add((Path.Combine(gs, "common/Registry/comparison-existing.json"), BuildComparison(gs, gb, cMembers, procs)));
    outputs.Add((Path.Combine(gs, "common/Registry/coverage.json"), BuildCoverage(gs, procs, cmds)));

    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gen/gmb_registry.gen.h"), BuildRegistryGenH(procs, cmds, consts)));
    outputs.Add((Path.Combine(gs, "common/Registry/generated/CanonicalLookup.g.cs"), BuildCanonicalLookupCs(procs, cmds, consts)));
    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gen/gmb_pconst_gen.inc"), BuildPconstInc(consts)));
    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_ns_lookup.h"), BuildNsLookupH(procs, cMembers)));
    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_fn_lookup.h"), BuildFnLookupH(procs, cMembers)));
    // Nome (e Ns.Nome) -> CODICE BuiltInFunction: lo usa il compilatore dei FRAMMENTI EVAL/&
    // (gmb_bytecode) per emettere OP_CALL_BUILTIN invece di OP_CALL_EXTERNAL.
    outputs.Add((Path.Combine(gb, "native/gmb_bytecode/include/gmb_builtin_codes.h"), BuildBuiltinCodesH(procs, gs)));
    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_ns_types.h"), BuildNsTypesH(reg)));
    outputs.Add((Path.Combine(gb, "native/gmb_compiler_c/include/gmb_ns_types.h"), BuildNsTypesH(reg)));
    outputs.Add((Path.Combine(gb, "native/gmb_db_c/include/gmb_db_tokenkind_gen.h"), BuildDbTokenKindH(gs)));

    int diverged = 0;
    foreach (var (path, text) in outputs)
    {
        var cur = File.Exists(path) ? File.ReadAllText(path) : null;
        if (cur == text) continue;
        if (!write)
        {
            Console.Error.WriteLine($"DRIFT: {Rel(root, path)}");
            diverged++;
            continue;
        }
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, text);
        Console.Error.WriteLine($"wrote {Rel(root, path)}");
    }

    if (!write) return diverged == 0 ? 0 : 1;
    if (added.Count > 0)
        Console.Error.WriteLine($"tokenkind C: aggiunti {added.Count} membri: {string.Join(", ", added)}");
    return 0;
}

// ─────────────────────────────── CSV ───────────────────────────────

static string BuildCsv(List<ProcRow> procs, bool unique = false)
{
    var rows = procs
        .OrderBy(p => p.Ns, StringComparer.Ordinal)
        .ThenBy(p => p.Name, StringComparer.Ordinal)
        .ThenBy(p => p.KindName, StringComparer.Ordinal)
        .ToList();
    if (unique)
    {
        var seen = new HashSet<(string, string)>();
        rows = rows.Where(r => seen.Add((r.Ns, r.Name))).ToList();
    }

    var sb = new StringBuilder();
    sb.Append("#,namespace,name,TokenKind,BuiltinKind\r\n");
    for (int i = 0; i < rows.Count; i++)
    {
        var r = rows[i];
        sb.Append(i + 1).Append(',').Append(r.Ns).Append(',').Append(r.Name).Append(',')
          .Append(r.DisplayKind).Append(",TokenKind.").Append(r.KindName).Append("\r\n");
    }
    return sb.ToString();
}

static string BuildSurfaceCsv(List<ProcRow> procs, List<CmdRow> cmds, List<ConstRow> consts)
{
    var sb = new StringBuilder();
    sb.Append("kind,ns,name,kindName\n");
    foreach (var p in procs.OrderBy(p => p.Ns, StringComparer.Ordinal).ThenBy(p => p.Name, StringComparer.Ordinal))
        sb.Append("procedure,").Append(p.Ns).Append(',').Append(p.Name).Append(',').Append(p.DisplayKind).Append('\n');
    foreach (var c in cmds.OrderBy(c => c.Name, StringComparer.Ordinal))
        sb.Append("command,,").Append(c.Name).Append(',').Append(c.KindName).Append('\n');
    foreach (var k in consts.OrderBy(c => c.Ns, StringComparer.Ordinal).ThenBy(c => c.Name, StringComparer.Ordinal))
    {
        var name = k.Name.StartsWith('%') ? k.Name[1..] : k.Name;
        sb.Append("constant,").Append(k.Ns).Append(',').Append(name).Append(",Constant\n");
    }
    return sb.ToString();
}

// ─────────────────────────────── generated C / C# ───────────────────────────────

static string BuildRegistryGenH(List<ProcRow> procs, List<CmdRow> cmds, List<ConstRow> consts)
{
    var sb = new StringBuilder();
    sb.Append("/* gmb_registry.gen.h — GENERATO da canonical.registry.json (P0, registry-export).\n");
    sb.Append(" * Lookup con gli ID CANONICI (procedure 1.. / comandi 1000000+ / costanti 2000000+).\n");
    sb.Append(" * arityMin/arityMax (2026-09-24): arità accettate dal builtin — usate dal check di\n");
    sb.Append(" * shadowing nei frontend (una ridefinizione utente con arità contestata è errore).\n");
    sb.Append(" * Consumato dal check shadowing di gmb_c99_emit.c (il resto resta per la fase di\n");
    sb.Append(" * allineamento/rimozione enum, non wired nei build). */\n");
    sb.Append("#ifndef GMB_REGISTRY_GEN_H\n#define GMB_REGISTRY_GEN_H\n");
    sb.Append("typedef struct { const char* ns; const char* name; int id; int arity_min; int arity_max; } gmb_reg_proc_t;\n");
    sb.Append("static const gmb_reg_proc_t gmb_reg_procs[] = {\n");
    for (int i = 0; i < procs.Count; i++)
    {
        var p = procs[i];
        sb.Append($"    {{\"{p.Ns}\", \"{p.Name}\", {i + 1}, {p.ArityMin}, {p.ArityMax}}},\n");
    }
    sb.Append("};\n");
    sb.Append("typedef struct { const char* name; int id; } gmb_reg_cmd_t;\n");
    sb.Append("static const gmb_reg_cmd_t gmb_reg_cmds[] = {\n");
    for (int i = 0; i < cmds.Count; i++)
        sb.Append($"    {{\"{cmds[i].Name}\", {1000000 + i + 1}}},\n");
    sb.Append("};\n");
    sb.Append("typedef struct { const char* name; int id; double v; } gmb_reg_const_t;\n");
    sb.Append("static const gmb_reg_const_t gmb_reg_consts[] = {\n");
    for (int i = 0; i < consts.Count; i++)
    {
        var name = consts[i].Name.StartsWith('%') ? consts[i].Name[1..] : consts[i].Name;
        sb.Append($"    {{\"{name}\", {2000000 + i + 1}, {Fmt(consts[i].Value)}}},\n");
    }
    sb.Append("};\n#endif\n");
    return sb.ToString();
}

static string BuildCanonicalLookupCs(List<ProcRow> procs, List<CmdRow> cmds, List<ConstRow> consts)
{
    var sb = new StringBuilder();
    sb.Append("// <auto-generated> CanonicalLookup.g.cs — GENERATO da canonical.registry.json (P0).\n");
    sb.Append("// Lookup con gli ID CANONICI (procedure 1.. / comandi 1000000+ / costanti 2000000+).\n");
    sb.Append("// Non incluso nei build C#: consumato dalla fase di allineamento/rimozione enum.\n");
    sb.Append("namespace Basic;\npublic static partial class CanonicalLookup\n{\n");
    sb.Append("    public readonly record struct Proc(string Ns, string Name, int Id);\n");
    sb.Append("    public static readonly Proc[] Procedures =\n    [\n");
    for (int i = 0; i < procs.Count; i++)
        sb.Append($"        new(\"{procs[i].Ns}\", \"{procs[i].Name}\", {i + 1}),\n");
    sb.Append("    ];\n");
    sb.Append("    public readonly record struct Cmd(string Name, int Id);\n");
    sb.Append("    public static readonly Cmd[] Commands =\n    [\n");
    for (int i = 0; i < cmds.Count; i++)
        sb.Append($"        new(\"{cmds[i].Name}\", {1000000 + i + 1}),\n");
    sb.Append("    ];\n");
    sb.Append("    public readonly record struct Const(string Name, int Id, double Value);\n");
    sb.Append("    public static readonly Const[] Constants =\n    [\n");
    for (int i = 0; i < consts.Count; i++)
    {
        var name = consts[i].Name.StartsWith('%') ? consts[i].Name[1..] : consts[i].Name;
        sb.Append($"        new(\"{name}\", {2000000 + i + 1}, {Fmt(consts[i].Value)}),\n");
    }
    sb.Append("    ];\n}\n");
    return sb.ToString();
}

static string BuildPconstInc(List<ConstRow> consts)
{
    var sb = new StringBuilder();
    sb.Append("/* gmb_pconst_gen.inc — GENERATO da canonical.registry.json (gmb-registry-export).\n");
    sb.Append(" * Costanti globali %% per il compiler C: sostituisce/estende il sottoinsieme manuale\n");
    sb.Append($" * (CHAT/RERANK/BROWSER/K_*) con TUTTE le {consts.Count} costanti della superficie canonica. */\n");
    foreach (var c in consts.OrderBy(r => r.Ns, StringComparer.Ordinal).ThenBy(r => r.Name, StringComparer.Ordinal))
    {
        var name = c.Name.StartsWith('%') ? c.Name[1..] : c.Name;
        sb.Append($"    {{\"{name}\", {Fmt(c.Value)}}},\n");
    }
    return sb.ToString();
}

static string BuildNsLookupH(List<ProcRow> procs, HashSet<string> members)
{
    var seen = new HashSet<(string, string)>();
    var rows = new List<(string ns, string name, string tk)>();
    foreach (var p in procs.OrderBy(p => p.Ns, StringComparer.OrdinalIgnoreCase).ThenBy(p => p.Name, StringComparer.OrdinalIgnoreCase))
    {
        var key = (p.Ns.ToLowerInvariant(), p.Name.ToLowerInvariant());
        if (seen.Add(key)) rows.Add((p.Ns, p.Name, p.DisplayKind));
    }
    var missing = rows.Where(r => !members.Contains("GMB_TOK_" + r.tk)).Select(r => r.tk).Distinct().ToList();
    if (missing.Count > 0) throw new InvalidOperationException("TokenKind senza membro enum C: " + string.Join(", ", missing));

    var sb = new StringBuilder();
    sb.Append("/* gmb_ns_lookup.h — lookup canonico (namespace, lexeme di registro) → TokenKind.\n");
    sb.Append(" * GENERATO da common/Registry/stdlib_tokens.csv (fonte di verità: NativeProcedure della StdLib).\n");
    sb.Append(" * Usato dal parser per risolvere NS.FUNZIONE: la qualifica seleziona la riga (ns, nome)\n");
    sb.Append(" * e produce il DispatchToken canonico — anche quando il lexeme è condiviso tra namespace\n");
    sb.Append(" * (SHARPERATIO → SharpeRatio in METRICS, PortfolioSharpeRatio in PORTFOLIO; EOF, GET, ...).\n");
    sb.Append(" * Il namespace è parte della chiave; MAI dispatch per lexeme. Il TokenKind è il VALORE\n");
    sb.Append(" * enum C (GMB_TOK_*) emesso a compile-time — nessuna risoluzione per nome a runtime.\n");
    sb.Append(" * Richiede gmb_tokenkind.h incluso prima. Rigenerare: tools/registry-export --all. */\n");
    sb.Append("#ifndef GMB_NS_LOOKUP_H\n#define GMB_NS_LOOKUP_H\n\n");
    sb.Append("typedef struct { const char* ns; const char* name; gmb_tokenkind_t tk; } gmb_ns_lookup_row_t;\n");
    sb.Append("static const gmb_ns_lookup_row_t gmb_ns_lookup[] = {\n");
    foreach (var r in rows) sb.Append($"    {{ \"{r.ns}\", \"{r.name}\", GMB_TOK_{r.tk} }},\n");
    sb.Append("};\n#define GMB_NS_LOOKUP_COUNT ((int)(sizeof(gmb_ns_lookup)/sizeof(gmb_ns_lookup[0])))\n#endif\n");
    return sb.ToString();
}

static string BuildFnLookupH(List<ProcRow> procs, HashSet<string> members)
{
    // Nome-enum che NON è una grafia di superficie (decisione utente 2026-10-27).
    // La regola generale resta "i nomi enum canonici sono chiavi" (sotto), ma per questi
    // token il nome C# è un identificatore interno, non una grafia BASIC: senza
    // l'esclusione comparirebbe in gmb_fn_lookup.h come alias accanto alla grafia
    // approvata (MutexTryLock accanto a TRYLOCK, GetTempDirFunc accanto a GETTEMPDIR).
    // Elenco ESPLICITO e chiuso: ogni aggiunta richiede l'approvazione dell'utente.
    var noEnumNameKey = new HashSet<string>(StringComparer.Ordinal)
    {
        "MutexTryLock",     // grafia approvata: TRYLOCK
        "GetTempDirFunc",   // grafia approvata: GETTEMPDIR (resta anche TempDir)
        "GetTempFileFunc",  // grafia approvata: GETTEMPFILE (resta anche TempFile)
    };
    var enumRows = new Dictionary<string, string>(StringComparer.Ordinal);
    foreach (var p in procs)
    {
        if (noEnumNameKey.Contains(p.DisplayKind)) continue;
        enumRows[p.DisplayKind] = "GMB_TOK_" + p.DisplayKind;
    }

    var lex = new Dictionary<string, string?>(StringComparer.OrdinalIgnoreCase);
    var lexOrig = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
    foreach (var p in procs)
    {
        var k = p.Name;
        if (lex.TryGetValue(k, out var cur))
        {
            if (cur != p.DisplayKind) lex[k] = null;
        }
        else
        {
            lex[k] = p.DisplayKind;
            lexOrig[k] = p.Name;
        }
    }
    var enumLower = enumRows.Keys.ToDictionary(k => k, k => k, StringComparer.OrdinalIgnoreCase);
    var entries = new Dictionary<string, string>(StringComparer.Ordinal);
    foreach (var (tk, cname) in enumRows) entries[tk] = cname;
    foreach (var (k, tk) in lex)
    {
        if (tk is null) continue;
        if (enumLower.TryGetValue(k, out var e) && e != tk) continue;
        if (!entries.ContainsKey(lexOrig[k])) entries[lexOrig[k]] = "GMB_TOK_" + tk;
    }
    var missing = entries.Values.Where(v => !members.Contains(v)).Distinct().ToList();
    if (missing.Count > 0) throw new InvalidOperationException("TokenKind senza membro enum C: " + string.Join(", ", missing));

    var sb = new StringBuilder();
    sb.Append("/* gmb_fn_lookup.h — mappa piatta nome → TokenKind per il PARSER (frontiera).\n");
    sb.Append(" * GENERATA da common/Registry/stdlib_tokens.csv (stessa fonte di gmb_ns_lookup.h). Chiavi:\n");
    sb.Append(" * nomi enum canonici (sempre) + grafie StdLib NON ambigue (un solo token ovunque).\n");
    sb.Append(" * I nomi ambigui tra namespace (SHARPERATIO, SEARCH, GET, ...) NON ci sono: si\n");
    sb.Append(" * risolvono solo qualificati via gmb_ns_lookup. Direttiva 2026-09-08: niente\n");
    sb.Append(" * gmb_tokenkind_from_name a valle. Rigenerare: tools/registry-export --all. */\n");
    sb.Append("#ifndef GMB_FN_LOOKUP_H\n#define GMB_FN_LOOKUP_H\n\n#include \"gmb_tokenkind.h\"\n#include <strings.h>\n\n");
    sb.Append("typedef struct { const char* k; gmb_tokenkind_t tk; } gmb_fn_lookup_row_t;\n");
    sb.Append("static const gmb_fn_lookup_row_t gmb_fn_lookup[] = {\n");
    foreach (var k in entries.Keys.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
        sb.Append($"    {{ \"{k}\", {entries[k]} }},\n");
    sb.Append("};\n\n");
    sb.Append("/* Nome (grafie StdLib non ambigue o nome enum canonico) → TokenKind; Unknown altrove. */\n");
    sb.Append("static inline gmb_tokenkind_t gmb_fn_tok(const char* n)\n{\n");
    sb.Append("    if (!n) return GMB_TOK_Unknown;\n");
    sb.Append("    for (size_t i = 0; i < sizeof(gmb_fn_lookup) / sizeof(gmb_fn_lookup[0]); i++)\n");
    sb.Append("        if (strcasecmp(n, gmb_fn_lookup[i].k) == 0) return gmb_fn_lookup[i].tk;\n");
    sb.Append("    return GMB_TOK_Unknown;\n}\n#endif\n");
    return sb.ToString();
}

static string BuildNsTypesH(IParserRegistry reg)
{
    var rows = new List<string>();
    var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    foreach (var (ns, procs) in reg.BuiltinProcedures.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
        foreach (var pr in procs)
            if (pr is NativeProcedureDefinition np && np.BuiltinKind != 0)
            {
                var tk = np.BuiltinKind.ToString();
                if (!seen.Add(tk)) continue;
                int isStr = (np.ReturnType is Basic.StringType
                             || np.ReturnType is Basic.ArrayType { ElementType: Basic.StringType }) ? 1 : 0;
                rows.Add($"    {{ GMB_TOK_{tk}, {isStr} }},");
            }

    var sb = new StringBuilder();
    sb.Append("/* gmb_ns_types.h — tipo di ritorno per DispatchToken (generato dalla StdLib). */\n");
    sb.Append("#ifndef GMB_NS_TYPES_H\n#define GMB_NS_TYPES_H\n");
    sb.Append("typedef struct { gmb_tokenkind_t tk; int is_str; } gmb_ns_type_row_t;\n");
    sb.Append("static const gmb_ns_type_row_t gmb_ns_types[] = {\n");
    foreach (var r in rows) sb.Append(r).Append('\n');
    sb.Append("};\n#define GMB_NS_TYPES_COUNT ((int)(sizeof(gmb_ns_types)/sizeof(gmb_ns_types[0])))\n#endif\n");
    return sb.ToString();
}

static string BuildBuiltinCodesH(List<ProcRow> procs, string gs)
{    // Il CODICE è quello che il compilatore C# emette davvero (OP_CALL_BUILTIN N): la mappa
    // TokenKind → BuiltInFunction di `Compiler.Expressions.Functions.cs` + il valore dell'enum
    // `Compiler.BuiltInFunction`. NON `NativeProcedureDefinition.BuiltinKind` (è un altro
    // numbering: Angle2d = 666 lì, 52 nell'enum usato dalla VM → "builtin 666 non implementato").
    var codes = TokenKindBuiltinCodes(gs);
    // Chiavi: il NOME di superficie (`Angle2d`) e la forma QUALIFICATA (`VECTOR.Angle2d`),
    // così il compilatore può preferire la qualificata quando il parser l'ha registrata
    // (`SQL.FETCH` ≠ `HTTP.FETCH`). Ordine deterministico: Ns poi Name; un nome nudo
    // condiviso da più namespace tiene la PRIMA riga (namespace in ordine alfabetico).
    var keys = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
    foreach (var p in procs.OrderBy(x => x.Ns, StringComparer.OrdinalIgnoreCase)
                           .ThenBy(x => x.Name, StringComparer.OrdinalIgnoreCase))
    {
        if (string.IsNullOrEmpty(p.Name)) continue;
        if (!codes.TryGetValue(p.DisplayKind, out var code)) continue;
        keys.TryAdd(p.Name, code);
        keys.TryAdd(p.Ns + "." + p.Name, code);
    }

    var sb = new StringBuilder();
    sb.Append("/* gmb_builtin_codes.h — GENERATO da `tools/registry-export --builtin-codes`.\n");
    sb.Append(" * NON modificare a mano.\n *\n");
    sb.Append(" * Mappa NOME di superficie (e forma qualificata `Ns.Nome`) -> CODICE builtin della VM,\n");
    sb.Append(" * cioè il valore di `BuiltInFunction` che `gmb_builtin_dispatch` usa per trovare\n");
    sb.Append(" * `gmb_bf_<N>`. La usa il compilatore dei FRAMMENTI EVAL/& (gmb_bytecode): senza,\n");
    sb.Append(" * un builtin di namespace compilato A RUNTIME finiva in OP_CALL_EXTERNAL e l'host di\n");
    sb.Append(" * eval rispondeva \"MACRO ERROR: chiamata esterna non disponibile\" (IL/PCode davano il\n");
    sb.Append(" * valore). I codici sono gli stessi del compilatore C# (fonte unica: la StdLib). */\n");
    sb.Append("#ifndef GMB_BUILTIN_CODES_H\n#define GMB_BUILTIN_CODES_H\n");
    sb.Append("typedef struct { const char* name; int code; } gmb_builtin_code_row_t;\n");
    sb.Append("static const gmb_builtin_code_row_t gmb_builtin_codes[] = {\n");
    foreach (var kv in keys.OrderBy(k => k.Key, StringComparer.OrdinalIgnoreCase))
        sb.Append($"    {{ \"{kv.Key}\", {kv.Value} }},\n");
    sb.Append("};\n");
    sb.Append("#define GMB_BUILTIN_CODES_COUNT ((int)(sizeof(gmb_builtin_codes)/sizeof(gmb_builtin_codes[0])))\n");
    sb.Append("#endif\n");
    return sb.ToString();
}

/// <summary>TokenKind (nome) → codice BuiltInFunction, letti dalla mappa del compilatore C#
/// (`Compiler.Expressions.Functions.cs`) e dal valore dell'enum (`BuiltInFunction.cs` via
/// reflection): è lo STESSO numero che finisce in OP_CALL_BUILTIN, quindi il codice giusto per
/// la VM. `NativeProcedureDefinition.BuiltinKind` è un altro numbering (NON usarlo).</summary>
static Dictionary<string, int> TokenKindBuiltinCodes(string gs)
{
    var map = new Dictionary<string, int>(StringComparer.Ordinal);
    var path = Path.Combine(gs, "common/Compiler/Compiler.Expressions.Functions.cs");
    if (!File.Exists(path)) return map;
    var text = File.ReadAllText(path);
    foreach (Match m in Regex.Matches(text, @"\{\s*TokenKind\.(\w+)\s*,\s*BuiltInFunction\.(\w+)\s*\}"))
    {
        var tk = m.Groups[1].Value;
        var bf = m.Groups[2].Value;
        if (map.ContainsKey(tk)) continue;
        if (Enum.TryParse<Compiler.BuiltInFunction>(bf, out var v)) map[tk] = (int)v;
    }
    return map;
}

static string BuildDbTokenKindH(string gs){
    var values = TokenKindValues(gs);
    var wanted = new (string Const, string Member)[] { ("GMB_DB_TOK_TOP", "Top"), ("GMB_DB_TOK_BOTTOM", "Bottom") };
    var width = wanted.Max(w => w.Const.Length);
    var sb = new StringBuilder();
    sb.Append("/* native/gmb_db_c/include/gmb_db_tokenkind_gen.h — GENERATO, non modificare a mano.\n");
    sb.Append(" *\n * Sorgente: common/Ast/TokenKind.cs (enum piatto: l'ordinale e' la POSIZIONE del\n");
    sb.Append(" * membro). E' lo stesso numero che il compilatore C# mette nel chunk per\n");
    sb.Append(" * `stmt.Location.Kind` (EmitConstant((double)kind)), quindi il C deve usarlo identico.\n");
    sb.Append(" *\n * Rigenera:  tools/registry-export --all\n");
    sb.Append(" * Verifica:  tools/registry-export --check   (exit 1 = drift)\n */\n");
    sb.Append("#ifndef GMB_DB_TOKENKIND_GEN_H\n#define GMB_DB_TOKENKIND_GEN_H\n\n");
    foreach (var (c, m) in wanted)
        sb.Append($"#define {c.PadRight(width)} {values[m],-5} /* TokenKind.{m} */\n");
    sb.Append("\n#endif /* GMB_DB_TOKENKIND_GEN_H */\n");
    return sb.ToString();
}

static string SyncTokenKindH(string text, string gs, out List<string> added)
{
    var have = CMembers(gs, text).ToHashSet(StringComparer.Ordinal);
    var csMembers = CsEnumMembers(File.ReadAllText(Path.Combine(gs, "common/Ast/TokenKind.cs")));
    added = csMembers.Where(n => !have.Contains("GMB_TOK_" + n)).ToList();
    if (added.Count > 0)
    {
        var anchor = "} gmb_tokenkind_t;";
        var i = text.IndexOf(anchor, StringComparison.Ordinal);
        if (i < 0) throw new InvalidOperationException("gmb_tokenkind.h: chiusura enum non trovata");
        var block = string.Join("\n", added.Select(n => "    GMB_TOK_" + n + ",")) + "\n";
        text = text[..i] + block + text[i..];
        have = CMembers(gs, text).ToHashSet(StringComparer.Ordinal);
    }

    var m = Regex.Match(text, @"#define GMB_TOK_COUNT (\d+)");
    if (!m.Success) throw new InvalidOperationException("GMB_TOK_COUNT non trovato");
    text = text[..m.Index] + $"#define GMB_TOK_COUNT {have.Count}" + text[(m.Index + m.Length)..];
    return text;
}

// ─────────────────────────────── reports ───────────────────────────────

static string BuildComparison(string gs, string gb, HashSet<string> cMembers, List<ProcRow> procs)
{
    var tokCs = CsEnumMembers(File.ReadAllText(Path.Combine(gs, "common/Ast/TokenKind.cs"))).ToHashSet(StringComparer.Ordinal);
    var bfText = File.ReadAllText(Path.Combine(gs, "common/Compiler/BuiltInFunction.cs"));
    var bfVals = BuiltInFunctionValues(bfText);
    var bfMembers = EnumMembers(bfText, "BuiltInFunction").ToHashSet(StringComparer.Ordinal);
    var tokC = cMembers.Select(m => m.StartsWith("GMB_TOK_") ? m[8..] : m).ToHashSet(StringComparer.Ordinal);
    var canonicalKinds = procs.Select(p => p.KindName).Distinct().ToList();
    var missingCsTk = canonicalKinds.Where(k => !tokCs.Contains(k)).OrderBy(x => x, StringComparer.Ordinal).ToList();

    var dups = bfVals.GroupBy(kv => kv.Value).Where(g => g.Count() > 1)
        .ToDictionary(g => g.Key, g => g.Select(x => x.Key).ToList());

    int fromName = 0;
    var compilerDir = Path.Combine(gb, "native/gmb_compiler_c");
    if (Directory.Exists(compilerDir))
        foreach (var f in Directory.EnumerateFiles(compilerDir, "*.*", SearchOption.AllDirectories))
        {
            var name = Path.GetFileName(f);
            if (f.Contains("/build") || name == "gmb_tokenkind.c") continue;
            if (!name.EndsWith(".c") && !name.EndsWith(".h")) continue;
            fromName += Regex.Matches(File.ReadAllText(f), "gmb_tokenkind_from_name").Count;
        }

    var report = new
    {
        canonical = new { count = procs.Count, namespaces = procs.Select(p => p.Ns).Distinct().Count() },
        @enum_cs = new
        {
            tokenkind_members = tokCs.Count,
            builtinfunction_members = bfMembers.Count,
            builtinfunction_explicit_values = bfVals.Count,
            builtinfunction_dup_code_groups = dups.Count,
            builtinfunction_dup_members = dups.Values.Sum(v => v.Count)
        },
        builtinfunction_dup_codes_sample = dups.Take(10).Select(kv => new { code = kv.Key, names = kv.Value.Take(4).ToList() }).ToList(),
        @enum_c = new { tokenkind_header_members = tokC.Count },
        c_from_name_uses_excl_impl = fromName,
        canonical_kindNames_not_in_cs_tokenkind = missingCsTk.Count,
        kindNames_missing_sample = missingCsTk.Take(25).ToList()
    };
    return JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
}

static string BuildCoverage(string gs, List<ProcRow> procs, List<CmdRow> cmds)
{
    var backends = new (string Name, string File)[]
    {
        ("IL", "common/BackendSupport/Registry/IL.cs"),
        ("PCodeVM", "common/BackendSupport/Registry/PCodeVM.cs"),
        ("VMC", "common/BackendSupport/Registry/VMC.cs"),
        ("C99Native", "common/BackendSupport/Registry/C99Native.cs"),
    };
    var sets = backends.ToDictionary(b => b.Name,
        b => Regex.Matches(File.ReadAllText(Path.Combine(gs, b.File)), "Register\\([^,]+,\\s*\"([^\"]+)\"")
            .Select(m => m.Groups[1].Value.ToLowerInvariant()).ToHashSet());

    var rows = new List<Dictionary<string, object>>();
    for (int i = 0; i < procs.Count; i++)
    {
        var p = procs[i];
        var row = new Dictionary<string, object>
        {
            ["id"] = i + 1, ["ns"] = p.Ns, ["name"] = p.Name, ["kind"] = p.KindName
        };
        foreach (var b in backends)
            row[b.Name] = sets[b.Name].Contains(p.Name.ToLowerInvariant()) || sets[b.Name].Contains(p.KindName.ToLowerInvariant());
        rows.Add(row);
    }
    for (int i = 0; i < cmds.Count; i++)
    {
        var c = cmds[i];
        var row = new Dictionary<string, object>
        {
            ["id"] = 1000000 + i + 1, ["ns"] = "", ["name"] = c.Name, ["kind"] = c.KindName
        };
        foreach (var b in backends)
            row[b.Name] = sets[b.Name].Contains(c.Name.ToLowerInvariant());
        rows.Add(row);
    }

    var counts = backends.ToDictionary(b => b.Name, b => rows.Count(r => (bool)r[b.Name]));
    var notImpl = backends.ToDictionary(b => b.Name,
        b => rows.Where(r => !(bool)r[b.Name]).Take(8)
            .Select(r => (string)r["ns"] == "" ? (string)r["name"] : $"{r["ns"]}.{r["name"]}").ToList());
    var report = new
    {
        total_canonical = rows.Count,
        scope = "DECLARED by BackendSupport/Registry (name-match; NON misurato su dispatch reali)",
        declared_per_backend = counts,
        declared_not_present_sample = notImpl,
        rows
    };
    return JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
}

// ─────────────────────────────── enum helpers / gate ───────────────────────────────

static int CheckEnumIds(string root)
{
    var gs = Path.Combine(root, "GMBasicSharp");
    var gb = Path.Combine(root, "GMBasic");
    var errors = new List<string>();

    var tk = CsEnumMembers(File.ReadAllText(Path.Combine(gs, "common/Ast/TokenKind.cs")));
    foreach (var g in tk.GroupBy(x => x, StringComparer.Ordinal).Where(g => g.Count() > 1))
        errors.Add($"TokenKind duplicato: {g.Key}");

    var bf = BuiltInFunctionValues(File.ReadAllText(Path.Combine(gs, "common/Compiler/BuiltInFunction.cs")));
    foreach (var g in bf.GroupBy(kv => kv.Value).Where(g => g.Count() > 1))
        errors.Add($"BuiltInFunction id {g.Key} duplicato: {string.Join(", ", g.Select(x => x.Key))}");

    var cMembers = CMembers(gb);
    foreach (var g in cMembers.GroupBy(x => x, StringComparer.Ordinal).Where(g => g.Count() > 1))
        errors.Add($"TokenKind C duplicato: {g.Key}");

    var cCount = Regex.Match(File.ReadAllText(Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_tokenkind.h")), @"#define GMB_TOK_COUNT (\d+)");
    if (cCount.Success && int.Parse(cCount.Groups[1].Value) < cMembers.Count)
        errors.Add($"GMB_TOK_COUNT={cCount.Groups[1].Value} ma l'enum C ha {cMembers.Count} membri");

    if (errors.Count == 0)
    {
        Console.WriteLine($"gate enum IDs: OK (TokenKind {tk.Count}, BuiltInFunction {bf.Count}, C {cMembers.Count})");
        return 0;
    }
    foreach (var e in errors) Console.Error.WriteLine("ERRORE: " + e);
    return 1;
}

static List<string> CsEnumMembers(string text) => EnumMembers(text, "TokenKind");

static List<string> EnumMembers(string text, string enumName)
{
    var m = Regex.Match(text, @"\benum\s+" + Regex.Escape(enumName) + @"\s*\{");
    if (!m.Success) return [];
    int i = m.Index + m.Length - 1, depth = 0, start = i + 1, end = -1;
    for (; i < text.Length; i++)
    {
        if (text[i] == '{') depth++;
        else if (text[i] == '}' && --depth == 0) { end = i; break; }
    }
    if (end < 0) return [];
    var body = text[start..end];
    body = Regex.Replace(body, @"/\*.*?\*/", " ", RegexOptions.Singleline);
    body = Regex.Replace(body, @"//[^\n]*", "");
    var names = new List<string>();
    foreach (var part in body.Split(','))
    {
        var p = part.Trim();
        if (p.Length == 0) continue;
        var eq = p.IndexOf('=');
        if (eq >= 0) p = p[..eq].Trim();
        if (Regex.IsMatch(p, "^[A-Za-z_][A-Za-z0-9_]*$")) names.Add(p);
    }
    return names;
}

static string Fmt(double v) => v.ToString("G17", CultureInfo.InvariantCulture).Replace("E", "e");

static Dictionary<string, int> BuiltInFunctionValues(string text)
{
    var d = new Dictionary<string, int>(StringComparer.Ordinal);
    foreach (Match m in Regex.Matches(text, @"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([0-9]+)\s*,?\s*$", RegexOptions.Multiline))
        d[m.Groups[1].Value] = int.Parse(m.Groups[2].Value);
    return d;
}

static Dictionary<string, int> TokenKindValues(string gs)
{
    var members = CsEnumMembers(File.ReadAllText(Path.Combine(gs, "common/Ast/TokenKind.cs")));
    var values = new Dictionary<string, int>(StringComparer.Ordinal);
    for (int i = 0; i < members.Count; i++) values[members[i]] = i;
    return values;
}

static HashSet<string> CMembers(string gb, string? text = null)
{
    text ??= File.ReadAllText(Path.Combine(gb, "native/gmb_compiler_c/frontend/include/gmb_tokenkind.h"));
    var end = text.IndexOf("} gmb_tokenkind_t;", StringComparison.Ordinal);
    if (end < 0) return new HashSet<string>(StringComparer.Ordinal);
    var start = text.LastIndexOf('{', end);
    if (start < 0) return new HashSet<string>(StringComparer.Ordinal);
    var body = text[(start + 1)..end];
    body = Regex.Replace(body, @"/\*.*?\*/", " ", RegexOptions.Singleline);
    body = Regex.Replace(body, @"//[^\n]*", "");
    return Regex.Matches(body, @"\bGMB_TOK_[A-Za-z0-9_]+\b")
        .Select(m => m.Value).ToHashSet(StringComparer.Ordinal);
}

static string FindMonorepoRoot()
{
    // Marker reali del monorepo. NON `.gitmodules`: i submodule sono stati assorbiti il
    // 2026-10-30 e quel file non esiste più (la sua assenza faceva fallire il tool).
    var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
    while (dir is not null)
    {
        if (Directory.Exists(Path.Combine(dir.FullName, "GMBasic")) &&
            Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp")) &&
            Directory.Exists(Path.Combine(dir.FullName, "shared")))
            return dir.FullName;
        dir = dir.Parent;
    }
    throw new InvalidOperationException("registry-export: root del monorepo non trovata (cercato GMBasic + GMBasicSharp + shared)");
}

static string Rel(string root, string path) => Path.GetRelativePath(root, path);

sealed class ConstRow
{
    public string Ns { get; set; } = "";
    public string Name { get; set; } = "";
    public double Value { get; set; }
}

sealed class SigParam
{
    public string Name { get; set; } = "";
    public string Type { get; set; } = "";
    public bool Optional { get; set; }
    public bool ByRef { get; set; }
    public bool Array { get; set; }
}

sealed class SigRow
{
    public string Ns { get; set; } = "";
    public string Name { get; set; } = "";
    public bool Fn { get; set; }
    public string Return { get; set; } = "";
    public List<SigParam> Params { get; set; } = new();
}

sealed class ProcRow
{
    public string Ns { get; set; } = "";
    public string Name { get; set; } = "";
    public int TokenKind { get; set; }
    public string KindName { get; set; } = "";
    public string DisplayKind { get; set; } = "";
    public bool Fn { get; set; }
    public int ArityMin { get; set; }
    public int ArityMax { get; set; }
    public bool Pure { get; set; }
}

sealed class CmdRow
{
    public string Name { get; set; } = "";
    public int TokenKind { get; set; }
    public string KindName { get; set; } = "";
}
