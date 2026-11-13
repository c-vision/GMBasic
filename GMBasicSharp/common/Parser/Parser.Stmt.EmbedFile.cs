// FILE: Parser.Stmt.EmbedFile.cs
//
// EMBEDFILE "file" AS nome$  —  equivalente di `@embedFile` di Zig.
//
// Il contenuto del file diventa un LETTERALE stringa nel programma: la risorsa viaggia
// dentro l'eseguibile, non serve consegnarla accanto al binario e non c'è I/O a runtime.
//
// Direttiva COMPILE-TIME (come REQUIRE/IMPORT): non è un builtin, non arriva al runtime.
// Soft keyword: `EMBEDFILE` è riconosciuta solo se seguita da un letterale stringa, così
// resta un nome di variabile utilizzabile; stessa regola nel parser C
// (gmb_parser_stmt_embed.c), per la parità fra i due compilatori.
//
// Desugaring: un solo statement `nome$ = "<testo del file>"`. Nessun DIM: la variabile
// segue la regola delle assegnazioni implicite, identica sulle 4 gambe.
//
// Il file è TESTO UTF-8 (la stringa del linguaggio è testo: LEN conta i caratteri, non i
// byte). Un payload binario viene RIFIUTATO esplicitamente (byte NUL o UTF-8 non valido),
// mai incorporato troncato.

using System;
using System.IO;
using System.Text;

namespace Basic;

public sealed partial class Parser
{
    private const string EmbedFileKeyword = "EMBEDFILE";

    /// <summary>True se il token corrente apre una direttiva EMBEDFILE (soft keyword + stringa).</summary>
    private bool AtEmbedFileDirective()
    {
        var t = Peek();
        return t.Kind == TokenKind.Identifier
               && string.Equals(t.Lexeme, EmbedFileKeyword, StringComparison.OrdinalIgnoreCase)
               && At(1).Kind == TokenKind.StringLiteral;
    }

    /// <summary>
    /// Parsa `EMBEDFILE "file" AS nome$` e lo traduce in `nome$ = "<contenuto>"`.
    /// Il percorso è relativo alla cartella del programma (fallback: CWD).
    /// </summary>
    private IReadOnlyList<AstNode> ParseEmbedFileStatement()
    {
        Advance();                              // EMBEDFILE
        var pathToken = Advance();              // "file"
        var relPath = pathToken.Literal as string ?? string.Empty;

        Match(TokenKind.As);                    // AS opzionale (parità col parser C)
        // BASE64 (soft keyword, nessun TokenKind): payload BINARIO codificato in testo.
        var base64 = false;
        if (Peek().Kind == TokenKind.Identifier
            && string.Equals(Peek().Lexeme, "BASE64", StringComparison.OrdinalIgnoreCase))
        {
            base64 = true;
            Advance();
        }
        var nameToken = Advance();
        if (nameToken.Kind is not (TokenKind.Identifier or TokenKind.Id))
            throw CreateError(nameToken, "EMBEDFILE: atteso il nome della variabile (EMBEDFILE \"file\" AS nome$).");

        var text = LoadEmbedFileText(relPath, pathToken, base64);

        // Nome NORMALIZZATO come in ParsePrimaryAtom: NameSuffixed porta il `$`, NameCleaned no.
        // Il resolver/linker lavora sul nome pulito: passare il suffisso in entrambi i token
        // non trova il simbolo (la variabile restava non dichiarata → NIL a runtime).
        var (baseName, _) = SplitNameAndSuffix(nameToken.Lexeme);
        var cleanToken = nameToken with { Lexeme = baseName };

        var target = new VariableTarget(-1, nameToken, cleanToken);
        var value = new StringExpr(text, pathToken);
        // LetStmt (non ExpressionStmt+AssignmentExpr): è il nodo che il parser produce per
        // `x = expr` e l'unico che il linker tratta come assegnazione di una variabile
        // (dichiarazione implicita compresa). Con ExpressionStmt il simbolo restava non
        // risolto → NIL a runtime su IL/PCode.
        return [new LetStmt([new Assignment(target, value)], null)];
    }

    /// <summary>
    /// Legge il file come TESTO e lo restituisce. Errori espliciti di compilazione:
    /// file mancante, contenuto binario (NUL) o UTF-8 non valido.
    /// </summary>
    private string LoadEmbedFileText(string relPath, Token positionToken, bool base64 = false)
    {
        if (string.IsNullOrEmpty(relPath))
            throw CreateError(positionToken, "EMBEDFILE: percorso vuoto.");

        var candidates = new List<string>();
        var baseDir = string.IsNullOrEmpty(_entryFilePath) ? null : Path.GetDirectoryName(_entryFilePath);
        if (!string.IsNullOrEmpty(baseDir) && !Path.IsPathRooted(relPath))
            candidates.Add(Path.Combine(baseDir!, relPath));
        candidates.Add(relPath);

        string? used = null;
        foreach (var candidate in candidates)
        {
            if (File.Exists(candidate)) { used = candidate; break; }
        }
        if (used is null)
            throw CreateError(positionToken,
                $"EMBEDFILE: file non leggibile: '{relPath}' (cercato anche in '{baseDir ?? "."}').");

        byte[] bytes;
        try { bytes = File.ReadAllBytes(used); }
        catch (Exception ex)
        {
            throw CreateError(positionToken, $"EMBEDFILE: lettura fallita di '{used}': {ex.Message}");
        }

        // BASE64: payload QUALSIASI (binario compreso) codificato in testo ASCII.
        // Stessa codifica del frontend C (gmb_embed_load_base64): testo identico sulle 4 gambe.
        if (base64) return Convert.ToBase64String(bytes);

        var nul = Array.IndexOf(bytes, (byte)0);
        if (nul >= 0)
            throw CreateError(positionToken,
                $"EMBEDFILE: il file contiene un byte NUL (offset {nul}): non è testo UTF-8. " +
                "Per i payload binari usa la forma BASE64: EMBEDFILE \"file\" AS BASE64 nome$.");

        try
        {
            // decode STRETTO: le sequenze non valide sono un errore, non un '?'
            return new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true).GetString(bytes);
        }
        catch (DecoderFallbackException)
        {
            throw CreateError(positionToken,
                $"EMBEDFILE: '{relPath}' non è testo UTF-8 valido (byte fuori sequenza). " +
                "Per i payload binari usa la forma BASE64: EMBEDFILE \"file\" AS BASE64 nome$.");
        }
    }
}
