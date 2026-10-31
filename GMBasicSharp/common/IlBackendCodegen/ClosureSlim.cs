// FILE: IlBackendCodegen/ClosureSlim.cs
// Filtro della closure managed per la DISTRIBUZIONE (IlProgramCompiler.Compile senza
// linkRuntime). Non tocca l'esecuzione (`-m` usa i symlink: costo zero) né l'AOT (che ha il
// suo root descriptor): agisce solo quando si produce una cartella da distribuire.
//
// PERCHÉ. La closure copiata è "tutto ciò che sta accanto a REPL": 114 dll + i binari nativi
// di TUTTI i RID. Per un `PRINT` erano 262 MB, di cui ~116 MB di binari di ALTRI sistemi
// operativi (Android/iOS/Windows/Linux) — impossibili da caricare su questa macchina — e
// ~65 MB di pacchetti che il programma non può raggiungere (OpenXml/PdfPig/ClosedXML per
// CONVERT, onnxruntime, provider Oracle/Postgres/MySQL/Firebird/SqlServer).
//
// COME. Due segnali INDIPENDENTI; un gruppo si esclude solo se ENTRAMBI sono negativi:
//   1. il programma non usa nessun metodo del runtime con i prefissi del gruppo (analisi IL
//      della DLL generata: è ciò che vede anche il codice dentro SUB e moduli IMPORTati);
//   2. il sorgente del programma non contiene nessuna delle parole chiave del gruppo.
// Fail-open: profilo vuoto, analisi fallita o codice dinamico (&/EVAL/BSP) → nessun filtro.
// Ogni esclusione è STAMPATA da Compile (`[slim] ...`), mai silenziosa; `COMPILE ... -all`
// produce la closure completa.

namespace Basic.IlBackend;

/// <summary>Gruppi di dipendenze opzionali pesanti, con il criterio che li rende necessari.</summary>
public static class ClosureSlim
{
    /// <summary>Un gruppo escludibile. <paramref name="MethodPrefixes"/> (prefissi dei metodi
    /// del runtime, separati da spazio) e <paramref name="SourceKeywords"/> sono i due segnali
    /// che lo tengono in vita; <paramref name="FilePrefixes"/> e <paramref name="NativePrefixes"/>
    /// sono i file che si tolgono.</summary>
    public sealed record Group(string Label, string MethodPrefixes, string SourceKeywords,
        string[] FilePrefixes, string[] NativePrefixes);

    /// <summary>I gruppi noti. Aggiungerne uno = dichiarare (a) i metodi e (b) le parole chiave
    /// che lo rendono necessario, (c) i file che ne fanno parte, (d) l'etichetta del log.</summary>
    public static readonly Group[] Groups =
    [
        // CONVERT (documenti): ConversionLib risolve i tipi per nome (ConversionLazy), quindi
        // senza questi file l'uso di CONVERT produce un errore ESPLICITO, non silenzioso —
        // è lo stesso comportamento già scelto per l'AOT (che non compila ConversionLib).
        new("documenti/CONVERT (OpenXml, PdfPig, ClosedXML, RTF)",
            MethodPrefixes: "Convert Doc",
            SourceKeywords: "CONVERT TOMD TOHTML TOXLSX DOCX XLSX RTF",
            FilePrefixes: ["DocumentFormat.OpenXml", "ClosedXML", "UglyToad.PdfPig", "RtfPipe", "SixLabors.Fonts", "ConversionLib"],
            NativePrefixes: []),

        // onnxruntime: servito a VectorDB (embedding/reranker), ASR (Whisper) e provider AI
        // locali. È il file più pesante della closure (~39 MB).
        new("onnxruntime (AI/VectorDB/ASR/OCR)",
            MethodPrefixes: "Ai Vector Asr Doc Llm Train Torch Rag Embed Knowledge Ocr Whisper",
            SourceKeywords: "AI VECTORDB EMBED RERANK RAG ASR WHISPER OCR KNOWLEDGE",
            FilePrefixes: ["Microsoft.ML.OnnxRuntime"],
            NativePrefixes: ["libonnxruntime"]),

        // Provider database ADO: SqlDriver li trova per nome d'assemblaggio; se il programma
        // non fa SQL/DBF/ISAM/RDD, nessun provider viene mai cercato.
        new("provider SQL (Oracle, Postgres, MySQL, Firebird, SqlServer)",
            MethodPrefixes: "Sql Db Dbf Isam Rdd",
            SourceKeywords: "SQL DBF ISAM RDD ORACLE POSTGRES MYSQL FIREBIRD",
            FilePrefixes: ["Oracle.ManagedDataAccess", "Npgsql", "MySqlConnector", "FirebirdSql", "Microsoft.Data.SqlClient"],
            NativePrefixes: []),
    ];

    /// <summary>Gruppi che il programma NON può raggiungere. Vuoto se il profilo è vuoto
    /// (nessuna informazione: prudenza) o se un segnale dice che il gruppo serve.</summary>
    public static List<Group> Excluded(IlRuntimeProfile profile, string? sourceText = null)
    {
        var excluded = new List<Group>();
        if (profile.IsEmpty) return excluded;   // niente dati → nessun filtro
        var src = (sourceText ?? "").ToUpperInvariant();
        foreach (var g in Groups)
        {
            // Segnale 1: metodi del runtime usati dal programma compilato.
            var prefixes = g.MethodPrefixes.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            var usedByCode = profile.UsedMethods.Any(m =>
                prefixes.Any(p => m.StartsWith(p, StringComparison.Ordinal)));
            // Segnale 2: parole chiave nel sorgente (copre i casi in cui il metodo non è
            // distinguibile, es. chiamate dietro un wrapper comune).
            var inSource = g.SourceKeywords.Split(' ', StringSplitOptions.RemoveEmptyEntries)
                .Any(k => HasWord(src, k));
            if (!usedByCode && !inSource) excluded.Add(g);
        }
        return excluded;
    }

    /// <summary>Parola intera (non sottostringa): "AI" non deve combaciare con "MAIN",
    /// "RAG" con "FRAG" — un falso positivo qui costa solo peso, ma rende il filtro inutile.</summary>
    private static bool HasWord(string upperText, string keyword)
    {
        var from = 0;
        while (from <= upperText.Length - keyword.Length)
        {
            var i = upperText.IndexOf(keyword, from, StringComparison.Ordinal);
            if (i < 0) return false;
            var beforeOk = i == 0 || !char.IsLetterOrDigit(upperText[i - 1]);
            var end = i + keyword.Length;
            // una keyword che finisce con '.' o '_' (es. "AI.") termina lì per costruzione
            var afterOk = end >= upperText.Length
                          || !char.IsLetterOrDigit(upperText[end])
                          || keyword[^1] is '.' or '_';
            if (beforeOk && afterOk) return true;
            from = end;
        }
        return false;
    }

    /// <summary>True se il file (nome semplice) appartiene a un gruppo escluso.</summary>
    public static bool IsExcluded(string fileName, IReadOnlyList<Group> excluded)
    {
        foreach (var g in excluded)
            foreach (var p in g.FilePrefixes)
                if (fileName.StartsWith(p, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    /// <summary>True se il binario nativo (nome semplice) è di un gruppo escluso.</summary>
    public static bool IsNativeExcluded(string fileName, IReadOnlyList<Group> excluded)
    {
        foreach (var g in excluded)
            foreach (var p in g.NativePrefixes)
                if (fileName.StartsWith(p, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    /// <summary>RID accettabili per una distribuzione eseguita su <paramref name="rid"/>:
    /// il RID esatto, la sua famiglia (osx, linux, win), il livello portable (unix) e la
    /// cartella senza RID (`native`). Un binario di un ALTRO sistema operativo/architettura
    /// non può essere caricato: tenerlo è solo peso morto.</summary>
    public static HashSet<string> AcceptableRids(string rid)
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { rid, "native" };
        var family = rid.Split('-')[0];
        set.Add(family);
        if (family is "osx" or "linux" or "freebsd") set.Add("unix");
        if (family is "maccatalyst") set.Add("osx");
        return set;
    }
}
