// P0 — contratto del manifest canonico (common/Registry/canonical.registry.json):
// invarianti strutturali che il registry deve rispettare per essere la fonte unica:
//  - id univoci e monotoni per spazio (procedure 1.. ; comandi 1000001+ ; costanti 2000001+);
//  - ordinamento deterministico (ns,name);
//  - nessuna coppia (ns,name) duplicata; nessun kindName ripetuto oltre gli alias consentiti.
// Il gate di idempotenza (tools/registry-export/check.sh) copre la rigenerazione; qui si fissa
// il contratto a livello di test.
using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class RegistryCanonicalTests
{
    private static readonly JsonDocument Doc = Load();

    private static JsonDocument Load()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) dir = dir.Parent;
        var p = Path.Combine(dir!.FullName, "GMBasicSharp", "common", "Registry", "canonical.registry.json");
        Assert.True(File.Exists(p), "canonical.registry.json mancante: " + p);
        return JsonDocument.Parse(File.ReadAllText(p));
    }

    [Fact]
    public void Ids_Monotonic_And_Unique_Per_Space()
    {
        foreach (var (arr, min) in new[]
                 { (Doc.RootElement.GetProperty("procedures"), 1),
                   (Doc.RootElement.GetProperty("commands"), 1000001),
                   (Doc.RootElement.GetProperty("constants"), 2000001) })
        {
            var ids = arr.EnumerateArray().Select(e => e.GetProperty("id").GetInt32()).ToList();
            Assert.Equal(ids.OrderBy(x => x), ids);
            Assert.Equal(ids.Distinct().Count(), ids.Count);
            Assert.All(ids, id => Assert.True(id >= min && id < min + 1000000, $"id fuori spazio: {id}"));
        }
    }

    [Fact]
    public void Procedures_Sorted_Deterministic()
    {
        // Il canonico deve essere ORDINATO deterministicamente (ns,name,kind). Le righe con la
        // stessa (ns,name,kind,arity) sono OVERLOAD PER TIPO (es. ISAM.APPENDMANY: fileNumber vs
        // alias + records) — legittimi, non duplicazioni da correggere.
        var procs = Doc.RootElement.GetProperty("procedures")
            .EnumerateArray()
            .Select(e => (Ns: e.GetProperty("ns").GetString()!, Name: e.GetProperty("name").GetString()!,
                Kind: e.GetProperty("tokenKind").GetInt32()))
            .ToList();
        var sorted = procs.OrderBy(x => x.Ns, StringComparer.Ordinal)
            .ThenBy(x => x.Name, StringComparer.Ordinal)
            .ThenBy(x => x.Kind);
        Assert.Equal(sorted, procs);
        Assert.True(procs.Count >= 1200);
    }

    [Fact]
    public void Sections_Present_And_Consistent()
    {
        var root = Doc.RootElement;
        Assert.True(root.GetProperty("procedures").GetArrayLength() > 0);
        Assert.True(root.GetProperty("commands").GetArrayLength() > 0);
        Assert.True(root.GetProperty("constants").GetArrayLength() > 0);
        Assert.Equal(root.GetProperty("procedures").GetArrayLength(), root.GetProperty("count").GetInt32());
        Assert.Equal(root.GetProperty("commands").GetArrayLength(), root.GetProperty("commandsCount").GetInt32());
        Assert.Equal(root.GetProperty("constants").GetArrayLength(), root.GetProperty("constantsCount").GetInt32());
        Assert.Equal("gmb.registry@1", root.GetProperty("schema").GetString());
    }

    [Fact]
    public void BuiltInFunction_Has_No_Duplicate_Codes()
    {
        // Blocca il fix (c9a40cd70): 0 gruppi di codici duplicati in BuiltInFunction.cs
        // (misura in comparison-existing.json, rigenerato da compare_existing.py).
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) dir = dir.Parent;
        var p = Path.Combine(dir!.FullName, "GMBasicSharp", "common", "Registry", "comparison-existing.json");
        using var doc = JsonDocument.Parse(File.ReadAllText(p));
        Assert.Equal(0, doc.RootElement.GetProperty("enum_cs")
            .GetProperty("builtinfunction_dup_code_groups").GetInt32());
    }

    [Fact]
    public void CConstantsInclude_Matches_Canonical()
    {
        // Parità cross-mondo (P0): l'include C delle costanti (gmb_pconst_gen.inc, wired nei
        // compilatori) deve avere ESATTAMENTE le stesse costanti del manifest canonico.
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) dir = dir.Parent;
        var inc = Path.Combine(dir!.FullName, "GMBasic", "native", "gmb_compiler_c", "frontend", "include", "gen", "gmb_pconst_gen.inc");
        Assert.True(File.Exists(inc), "gmb_pconst_gen.inc mancante: " + inc);
        var lines = File.ReadAllLines(inc);
        var incRows = new List<(string Name, double Value)>();
        foreach (var ln in lines)
        {
            var m = System.Text.RegularExpressions.Regex.Match(ln.Trim(), @"^\{\""([^\""]+)\"",\s*([-+0-9.eE]+)\},$");
            if (m.Success)
                incRows.Add((m.Groups[1].Value, double.Parse(m.Groups[2].Value, System.Globalization.CultureInfo.InvariantCulture)));
        }
        var canonConsts = Doc.RootElement.GetProperty("constants")
            .EnumerateArray()
            .Select(c => (Name: c.GetProperty("name").GetString()!.TrimStart('%'),
                          Value: c.GetProperty("value").GetDouble()))
            .ToList();
        Assert.Equal(canonConsts.Count, incRows.Count);
        Assert.Equal(canonConsts.OrderBy(x => x.Name).Select(x => x.Name),
            incRows.OrderBy(x => x.Name).Select(x => x.Name));
        var byName = canonConsts.ToDictionary(x => x.Name);
        foreach (var r in incRows)
            Assert.Equal(byName[r.Name].Value, r.Value, 9); // precisione ~1e-9
    }

    [Fact]
    public void GeneratedCsLookup_Matches_Canonical()
    {
        // Parità cross-mondo (P0): CanonicalLookup.g.cs (generato) deve contenere ESATTAMENTE
        // una riga per ogni procedura/comando/costante del manifest canonico.
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) dir = dir.Parent;
        var g = Path.Combine(dir!.FullName, "GMBasicSharp", "common", "Registry", "generated", "CanonicalLookup.g.cs");
        Assert.True(File.Exists(g), "CanonicalLookup.g.cs mancante: " + g);
        var text = File.ReadAllText(g);
        var root = Doc.RootElement;
        var expected = root.GetProperty("procedures").GetArrayLength()
                     + root.GetProperty("commands").GetArrayLength()
                     + root.GetProperty("constants").GetArrayLength();
        Assert.Equal(expected, System.Text.RegularExpressions.Regex.Matches(text, @"\bnew\(").Count);
        Assert.Contains("\"SearchMulti\"", text); // nome canonico esatto (PascalCase)
        Assert.Contains("\"AUTO\"", text);        // costante %AUTO (senza prefisso %)
    }
}
