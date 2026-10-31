// FILE: AI.Tests.BertWordPieceTokenizerGoldenTests
// Fase 1 del todo Tokenizer-WordPiece-Unicode-e-Retrieval-Sparse-Learned: ORACOLO ESTERNO.
// Confronta BertWordPieceTokenizer contro gli id dorati prodotti dalla libreria HF `tokenizers`
// (fixture versionata samples/Fixtures/tokenizer/tokenizer_golden.json, generata da
// tools/gen_tokenizer_golden.py). Regola della nota §3: l'oracolo NON è un'altra nostra
// implementazione (C vs C# vs llama.cpp) ma il software di riferimento del modello.
// Criterio di accettazione: tutti gli accoppiamenti esatti id-per-id, con il fallimento che
// stampa atteso/ottenuto. Copre D1 (BasicTokenizer Unicode-conforme) e D2 (troncamento che
// preserva [CLS]/[SEP] e assegna i token_type_ids DOPO il taglio).
using System.Text.Json;
using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class BertWordPieceTokenizerGoldenTests
{
    private static string FixtureDir()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !Directory.Exists(Path.Combine(dir.FullName, "samples", "Fixtures")))
            dir = dir.Parent;
        Assert.NotNull(dir);
        return Path.Combine(dir!.FullName, "samples", "Fixtures", "tokenizer");
    }

    private static readonly JsonDocument Golden =
        JsonDocument.Parse(File.ReadAllText(Path.Combine(FixtureDir(), "tokenizer_golden.json")));

    private static BertWordPieceTokenizer Tok(string vocabName) =>
        BertWordPieceTokenizer.Load(Path.Combine(FixtureDir(), $"vocab_{vocabName}.txt"));

    private static JsonElement[] Section(string name) =>
        [.. Golden.RootElement.GetProperty(name).EnumerateArray()];

    public static TheoryData<string> CaseIds()
    {
        var data = new TheoryData<string>();
        foreach (var c in Section("cases")) data.Add(c.GetProperty("id").GetString()!);
        return data;
    }

    public static TheoryData<string> PairIds()
    {
        var data = new TheoryData<string>();
        foreach (var c in Section("pairs")) data.Add(c.GetProperty("id").GetString()!);
        return data;
    }

    public static TheoryData<string> TruncIds()
    {
        var data = new TheoryData<string>();
        foreach (var c in Section("truncation")) data.Add(c.GetProperty("id").GetString()!);
        return data;
    }

    private static JsonElement Find(string section, string id) =>
        Section(section).First(e => e.GetProperty("id").GetString() == id);

    private static long[] Ids(JsonElement e) => [.. e.GetProperty("ids").EnumerateArray().Select(x => x.GetInt64())];

    private static string Fmt(IEnumerable<long> ids) => "[" + string.Join(", ", ids) + "]";

    [Theory(DisplayName = "Golden: BasicTokenize id-per-id contro oracolo HF")]
    [MemberData(nameof(CaseIds))]
    public void BasicTokenize_MatchesOracle(string caseId)
    {
        var c = Find("cases", caseId);
        var expected = c.GetProperty("basic_tokens").EnumerateArray().Select(x => x.GetString()!).ToArray();
        var actual = BertWordPieceTokenizer.BasicTokenize(c.GetProperty("text").GetString()!).ToArray();
        actual.Should().Equal(expected,
            $"BasicTokenize diverge dall'oracolo HF su '{caseId}' (testo: {c.GetProperty("text").GetString()}) — atteso {Fmt2(expected)}, ottenuto {Fmt2(actual)}");
    }

    [Theory(DisplayName = "Golden: Encode (ids) id-per-id contro oracolo HF")]
    [MemberData(nameof(CaseIds))]
    public void Encode_MatchesOracle(string caseId)
    {
        var c = Find("cases", caseId);
        var expected = Ids(c);
        var tok = Tok(c.GetProperty("vocab").GetString()!);
        var enc = tok.Encode(c.GetProperty("text").GetString()!, expected.Length);
        enc.InputIds.Should().Equal(expected,
            $"Encode diverge dall'oracolo HF su '{caseId}' — atteso {Fmt(expected)}, ottenuto {Fmt(enc.InputIds)}");
        enc.AttentionMask.Should().OnlyContain(m => m == 1, "nessun padding atteso con maxLength == ids.Length");
    }

    [Theory(DisplayName = "Golden: EncodePair (ids + token_type_ids) contro oracolo HF")]
    [MemberData(nameof(PairIds))]
    public void EncodePair_MatchesOracle(string pairId)
    {
        var c = Find("pairs", pairId);
        var expected = Ids(c);
        var expectedTypes = c.GetProperty("token_type_ids").EnumerateArray().Select(x => x.GetInt64()).ToArray();
        var tok = Tok(c.GetProperty("vocab").GetString()!);
        var enc = tok.EncodePair(c.GetProperty("query").GetString()!, c.GetProperty("document").GetString()!, expected.Length);
        enc.InputIds.Should().Equal(expected,
            $"EncodePair diverge dall'oracolo HF su '{pairId}' — atteso {Fmt(expected)}, ottenuto {Fmt(enc.InputIds)}");
        enc.TokenTypeIds.Should().Equal(expectedTypes,
            $"token_type_ids divergono su '{pairId}' (segmento 1 deve iniziare dopo il [SEP] della query)");
    }

    [Theory(DisplayName = "Golden: troncamento singola — [CLS]/[SEP] preservati, ids esatti")]
    [MemberData(nameof(TruncIds))]
    public void Truncation_Single_MatchesOracle(string truncId)
    {
        var c = Find("truncation", truncId);
        var expected = Ids(c);
        var maxLength = c.GetProperty("max_length").GetInt32();
        var tok = Tok(c.GetProperty("vocab").GetString()!);
        var enc = tok.Encode(c.GetProperty("text").GetString()!, maxLength);
        enc.InputIds.Should().Equal(expected,
            $"troncamento diverge su '{truncId}' — atteso {Fmt(expected)}, ottenuto {Fmt(enc.InputIds)}");
        enc.InputIds[0].Should().Be(2, "[CLS] deve sopravvivere al troncamento");
        enc.InputIds[maxLength - 1].Should().Be(3, "[SEP] deve sopravvivere al troncamento (difetto D2)");
    }

    [Fact(DisplayName = "D2: pair con query in overflow — marker preservati, confine segmento corretto")]
    public void EncodePair_QueryOverflow_KeepsMarkersAndSegmentBoundary()
    {
        var tok = Tok("en_uncased");
        var longQuery = string.Join(' ', Enumerable.Repeat("the quick brown fox", 10));   // 40 parole
        var enc = tok.EncodePair(longQuery, "hello world", 16);

        enc.InputIds[0].Should().Be(2, "[CLS] sempre in testa");
        var lastActive = enc.AttentionMask.ToList().LastIndexOf(1);
        enc.InputIds[lastActive].Should().Be(3, "ultimo token attivo = [SEP] del documento");
        enc.InputIds.Take(lastActive + 1).Should().Contain(3, "il [SEP] della query deve sopravvivere");
        var boundary = enc.TokenTypeIds.Take(lastActive + 1).ToList().IndexOf(1);
        boundary.Should().BeGreaterThan(0, "il segmento documento deve esistere");
        enc.InputIds[boundary - 1].Should().Be(3, "il segmento 1 inizia subito dopo il [SEP] della query");
        enc.TokenTypeIds.Skip(lastActive + 1).Should().OnlyContain(t => t == 0, "padding a tipo 0");
        enc.AttentionMask.Skip(lastActive + 1).Should().OnlyContain(m => m == 0, "mask spento sul padding");
        enc.InputIds.Where((t, i) => enc.AttentionMask[i] == 1 && t == 0).Should().BeEmpty(
            "nessun id [PAD]=0 in posizione attiva");
    }

    [Fact(DisplayName = "D2: maxLength minimi — [CLS][SEP] sempre presenti")]
    public void Encode_DegenerateMaxLengths_KeepMarkers()
    {
        var tok = Tok("en_uncased");
        var enc2 = tok.Encode("hello world rocket launch", 2);
        enc2.InputIds.Should().Equal([2, 3]);
        var enc3 = tok.Encode("hello world rocket launch", 3);
        enc3.InputIds.Should().Equal([2, 5, 3]);   // [CLS] hello [SEP]
    }

    private static string Fmt2(IEnumerable<string> s) => "[" + string.Join(", ", s.Select(x => $"'{x}'")) + "]";
}
