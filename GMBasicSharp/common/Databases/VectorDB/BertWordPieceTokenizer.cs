// FILE: Basic.SQLite.Vector.BertWordPieceTokenizer
using System.Globalization;
using System.Text;

namespace Basic.SQLite.Vector;

/// <summary>Single- or pair-sequence encoding for BERT-family models.</summary>
internal sealed class EncodedSequence(long[] inputIds, long[] attentionMask, long[]? tokenTypeIds = null)
{
    public long[] InputIds { get; } = inputIds;
    public long[] AttentionMask { get; } = attentionMask;
    public long[] TokenTypeIds { get; } = tokenTypeIds ?? new long[inputIds.Length];
    public int Length => InputIds.Length;
}

/// <summary>BERT WordPiece tokenizer shared by <see cref="OnnxEmbeddingProvider"/> (single-sequence
/// encoding) and <see cref="OnnxCrossEncoderReranker"/> (pair encoding:
/// <c>[CLS] query [SEP] document [SEP]</c>).</summary>
/// <remarks>
/// Spec comportamentale del BasicTokenizer HF (transformers/tokenizers, uncased, strip_accents),
/// VERIFICATA contro l'oracolo versionato: <c>samples/Fixtures/tokenizer/tokenizer_golden.json</c>
/// (generato da <c>tools/gen_tokenizer_golden.py</c> con la libreria HF <c>tokenizers</c> — oracolo
/// esterno, non una nostra seconda lettura della spec). Test: <c>BertWordPieceTokenizerGoldenTests</c>.
/// Upgrade path: model-specific tokenizer hook (e.g. SentencePiece) if a non-BERT model is ever needed.
/// </remarks>
internal sealed class BertWordPieceTokenizer(Dictionary<string, int> vocab, int clsId, int sepId, int unkId)
{
    /// <summary>Limite HF <c>max_input_chars_per_word</c>: parole più lunghe → [UNK] diretto.</summary>
    private const int MaxInputCharsPerWord = 100;

    public static BertWordPieceTokenizer Load(string vocabPath)
    {
        var lines = File.ReadAllLines(vocabPath);
        var vocab = new Dictionary<string, int>(lines.Length, StringComparer.Ordinal);
        for (var i = 0; i < lines.Length; i++)
            vocab[lines[i]] = i;

        return new BertWordPieceTokenizer(
            vocab,
            Lookup(vocab, "[CLS]", 101),
            Lookup(vocab, "[SEP]", 102),
            Lookup(vocab, "[UNK]", 100));
    }

    public EncodedSequence Encode(string text, int maxLength)
    {
        var tokens = new List<int>(maxLength) { clsId };
        foreach (var word in BasicTokenize(text))
        {
            foreach (var piece in WordPiece(word))
                tokens.Add(piece);
        }

        tokens.Add(sepId);
        if (tokens.Count > maxLength)
        {
            // Troncamento canonico BERT: [CLS] + primi (maxLength-2) token + [SEP] — i marker
            // restano SEMPRE (prima tagliava via [SEP]: difetto D2, todo Tokenizer-WordPiece §2).
            if (maxLength >= 2)
            {
                var keep = maxLength - 2;
                tokens.RemoveRange(1 + keep, tokens.Count - 2 - keep);
            }
            else
            {
                tokens.RemoveRange(maxLength, tokens.Count - maxLength);   // maxLength < 2: degenere
            }
        }

        return Pad(tokens, maxLength);
    }

    /// <summary>Cross-encoder pair encoding: <c>[CLS] query [SEP] document [SEP]</c>, with
    /// token_type_ids 0 over the query segment (incl. leading [CLS] and its [SEP]) and 1 over the
    /// document segment (incl. trailing [SEP]) — the standard BERT sentence-pair layout that
    /// cross-encoder rerankers (e.g. ms-marco-MiniLM) are trained on. Su overflow si taglia il
    /// CONTENUTO (prima il documento, poi la query se ancora non basta: longest-first), mai i
    /// marker; i type ids sono assegnati DOPO il taglio, sul confine reale.</summary>
    public EncodedSequence EncodePair(string query, string document, int maxLength)
    {
        var queryTokens = new List<int> { clsId };
        foreach (var word in BasicTokenize(query))
            foreach (var piece in WordPiece(word))
                queryTokens.Add(piece);
        queryTokens.Add(sepId);

        var docTokens = new List<int>();
        foreach (var word in BasicTokenize(document))
            foreach (var piece in WordPiece(word))
                docTokens.Add(piece);
        docTokens.Add(sepId);

        // Budget del documento (incluso il suo [SEP]): taglia il contenuto, preserva il [SEP] finale.
        var maxDocTokens = Math.Max(1, maxLength - queryTokens.Count);
        if (docTokens.Count > maxDocTokens)
            docTokens.RemoveRange(maxDocTokens - 1, docTokens.Count - maxDocTokens);

        // Query oltre il budget totale (documento già minimale): longest-first taglia la query,
        // preservando [CLS] (testa) e il suo [SEP] (coda).
        var maxQueryTokens = Math.Max(2, maxLength - docTokens.Count);
        if (queryTokens.Count > maxQueryTokens)
            queryTokens.RemoveRange(maxQueryTokens - 1, queryTokens.Count - maxQueryTokens);

        var queryLen = queryTokens.Count;
        var tokens = new List<int>(queryLen + docTokens.Count);
        tokens.AddRange(queryTokens);
        tokens.AddRange(docTokens);
        if (tokens.Count > maxLength)
            tokens.RemoveRange(maxLength, tokens.Count - maxLength);   // guardia: mai raggiunto coi budget sopra

        var encoded = Pad(tokens, maxLength);
        for (var i = queryLen; i < tokens.Count; i++)
            encoded.TokenTypeIds[i] = 1;
        return encoded;
    }

    private static EncodedSequence Pad(List<int> tokens, int maxLength)
    {
        var inputIds = new long[maxLength];
        var attentionMask = new long[maxLength];
        for (var i = 0; i < tokens.Count; i++)
        {
            inputIds[i] = tokens[i];
            attentionMask[i] = 1;
        }

        return new EncodedSequence(inputIds, attentionMask);
    }

    /// <summary>Greedy longest-match WordPiece (HF <c>WordpieceTokenizer</c>): se UN qualsiasi
    /// segmento fallisce, l'INTERA parola diventa un solo [UNK] — i pezzi già matchati si
    /// scartano (<c>is_bad → return [unk_token]</c>; verificato contro l'oracolo: hindi e
    /// control-chars nel golden). Parola &gt; 100 codepoint → [UNK] diretto.</summary>
    private IReadOnlyList<int> WordPiece(string token)
    {
        if (token.Length == 0)
            return [];

        // HF: len(chars) > max_input_chars_per_word (100 codepoint) → [UNK]
        if (CountRunes(token) > MaxInputCharsPerWord)
            return [unkId];

        var pieces = new List<int>();
        var start = 0;
        while (start < token.Length)
        {
            var end = token.Length;
            var found = -1;
            while (start < end)
            {
                var piece = start == 0 ? token[start..end] : "##" + token[start..end];
                if (vocab.TryGetValue(piece, out var id))
                {
                    found = id;
                    break;
                }

                end--;
            }

            if (found < 0)
                return [unkId];          // HF: is_bad → l'intera parola è [UNK]

            pieces.Add(found);
            start = end;
        }

        return pieces;
    }

    /// <summary>BasicTokenizer HF (uncased, strip_accents=true), nell'ordine di
    /// <c>transformers.BasicTokenizer._tokenize</c>: (1) <c>_clean_text</c> — scarta NUL/FFFD e
    /// le categorie C* (Cc eccetto \t\n\r, Cf, Co, Cn), \t\n\r e Zs → spazio; (2)
    /// <c>_tokenize_chinese_chars</c> — isola ogni ideogramma CJK; (3) split su whitespace;
    /// (4) per parola: NFD → scarta Mn (strip accents) → lowercase; (5) split su punteggiatura
    /// (range ASCII 33-47/58-64/91-96/123-126 O categoria P*, quindi '_' (Pc) È punteggiatura).
    /// Oracle: tokenizer_golden.json (HF tokenizers) — l'ordine NFD/lowercase è deciso dal golden.</summary>
    internal static IEnumerable<string> BasicTokenize(string text)
    {
        // (1) + (2): clean + isolamento CJK in un solo passaggio (per codepoint, surrogate-safe).
        var cleaned = new StringBuilder(text.Length);
        foreach (var cp in EnumerateCodePoints(text))
        {
            if (cp is 0 or 0xFFFD)
                continue;
            if (cp is '\t' or '\n' or '\r')
            {
                cleaned.Append(' ');
                continue;
            }

            var cat = CategoryOf(cp);
            if (cat is UnicodeCategory.Control or UnicodeCategory.Format
                     or UnicodeCategory.PrivateUse or UnicodeCategory.OtherNotAssigned)
                continue;                                            // C* → scartato (verificato su oracolo: anche Cf)
            if (cat == UnicodeCategory.SpaceSeparator)
            {
                cleaned.Append(' ');
                continue;
            }

            if (IsCjkIdeograph(cp))
                cleaned.Append(' ').Append(char.ConvertFromUtf32(cp)).Append(' ');
            else
                cleaned.Append(char.ConvertFromUtf32(cp));
        }

        // (3) split su whitespace + (4)+(5) per parola.
        foreach (var raw in cleaned.ToString().Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            var normalized = StripCombiningMarks(raw.Normalize(NormalizationForm.FormD)).ToLowerInvariant();
            var word = new StringBuilder(normalized.Length);
            foreach (var cp in EnumerateCodePoints(normalized))
            {
                if (IsPunctuation(cp))
                {
                    if (word.Length > 0)
                    {
                        yield return word.ToString();
                        word.Clear();
                    }

                    yield return char.ConvertFromUtf32(cp);
                    continue;
                }

                word.Append(char.ConvertFromUtf32(cp));
            }

            if (word.Length > 0)
                yield return word.ToString();
        }
    }

    private static string StripCombiningMarks(string nfd)
    {
        var sb = new StringBuilder(nfd.Length);
        foreach (var cp in EnumerateCodePoints(nfd))
            if (CategoryOf(cp) != UnicodeCategory.NonSpacingMark)
                sb.Append(char.ConvertFromUtf32(cp));
        return sb.ToString();
    }

    /// <summary>Range HF <c>_is_chinese_char</c>: ideografi CJK (UNI + estensioni), isolati come token singoli.</summary>
    private static bool IsCjkIdeograph(int cp) =>
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF)
        || (cp >= 0x20000 && cp <= 0x2A6DF) || (cp >= 0x2A700 && cp <= 0x2B73F)
        || (cp >= 0x2B740 && cp <= 0x2B81F) || (cp >= 0x2B820 && cp <= 0x2CEAF)
        || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x2F800 && cp <= 0x2FA1F);

    /// <summary>HF <c>_is_punctuation</c>: range ASCII oppure categoria Unicode P* (incluso '_' = Pc).</summary>
    private static bool IsPunctuation(int cp)
    {
        if ((cp >= 33 && cp <= 47) || (cp >= 58 && cp <= 64) || (cp >= 91 && cp <= 96) || (cp >= 123 && cp <= 126))
            return true;
        var cat = CategoryOf(cp);
        return cat is UnicodeCategory.ConnectorPunctuation or UnicodeCategory.DashPunctuation
                      or UnicodeCategory.OpenPunctuation or UnicodeCategory.ClosePunctuation
                      or UnicodeCategory.InitialQuotePunctuation or UnicodeCategory.FinalQuotePunctuation
                      or UnicodeCategory.OtherPunctuation;
    }

    /// <summary>Categoria Unicode per CODEPOINT (CharUnicodeInfo non ha overload int pubblico).</summary>
    private static UnicodeCategory CategoryOf(int cp) =>
        CharUnicodeInfo.GetUnicodeCategory(char.ConvertFromUtf32(cp), 0);

    private static IEnumerable<int> EnumerateCodePoints(string text)
    {
        for (var i = 0; i < text.Length; i++)
        {
            if (char.IsHighSurrogate(text[i]) && i + 1 < text.Length && char.IsLowSurrogate(text[i + 1]))
            {
                yield return char.ConvertToUtf32(text[i], text[i + 1]);
                i++;
            }
            else
            {
                yield return text[i];
            }
        }
    }

    private static int CountRunes(string s)
    {
        var n = 0;
        // ReSharper disable once LoopCanBeConvertedToQuery
        foreach (var _ in EnumerateCodePoints(s)) n++;
        return n;
    }

    private static int Lookup(Dictionary<string, int> vocab, string token, int fallback) =>
        vocab.TryGetValue(token, out var id) ? id : fallback;
}
