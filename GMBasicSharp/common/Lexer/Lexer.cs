// FILE: Interpreter.Lexer.cs

using System.Collections.Immutable;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text;

// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable InvertIf
namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
                     GUIDA DI RIFERIMENTO A GM-BASIC (Rev. Definitiva)
                                     REGOLE LESSICALI (LEXER)
------------------------------------------------------------------------------------------------------------------------

Il Lexer è responsabile della scomposizione del codice sorgente in una sequenza di "parole" atomiche chiamate token.
Le sue regole definiscono la struttura di base del linguaggio a livello di caratteri.

1. IDENTIFICATORI E KEYWORD
=============================
- Case-Insensitive: Tutti gli identificatori e le keyword sono trattati in modo non sensibile alle maiuscole/minuscole.
(es. 'PRINT', 'Print', e 'print' sono equivalenti).
- Caratteri Ammessi: Devono iniziare con una lettera e possono essere seguiti da lettere, numeri o il carattere underscore '_'.
- Suffissi di Tipo: Un identificatore può terminare con un suffisso speciale per dichiararne implicitamente il tipo:
 - $: Stringa (STRING)
 - %: Intero a 16-bit (INTEGER)
 - !: Virgola mobile, precisione singola (SINGLE)
 - #: Virgola mobile, doppia precisione (DOUBLE)
 - &: Intero a 32-bit (LONG)
- Keyword Multi-Parola: Le keyword composte da più parole (es. 'END IF', 'LINE INPUT', 'RESTORE SCREEN') vengono
riconosciute come un singolo token e hanno la precedenza su quelle a parola singola.

2. LETTERALI
=============
- Stringhe Standard:
- Delimitatori: Devono iniziare e finire con un doppio apice (").
- Escape: Per includere un doppio apice all'interno di una stringa,
 usare una sequenza di due doppi apici ("").
- Multi-riga: Le stringhe standard non possono estendersi su più righe.
- Raw String (Multi-riga):
- Delimitatori: Iniziano e finiscono con tre doppi apici (""") o backtick (`).
- Contenuto: Tutto il testo tra i delimitatori, inclusi virgolette, commenti, direttive di pre-processore e a-capo,
  viene trattato come contenuto letterale senza alcuna interpretazione speciale.
- Formattazione: I caratteri di a-capo (\n) all'interno del sorgente vengono preservati nel valore finale della stringa,
  permettendo la creazione di stringhe multi-riga. Il pre-processore unisce tutte le righe fisiche che compongono la raw
  string in un'unica riga logica prima della tokenizzazione.

- Numerici:
- Decimali: Sequenza di cifre, con un punto (.) opzionale
 per la parte decimale.
- Esadecimali: Prefisso '0x' o '0X' seguito da cifre 0-9 e lettere A-F.
- Ottali: Prefisso '0o' o '0O' seguito da cifre 0-7.
- Binari: Prefisso '0b' o '0B' seguito da cifre 0-1.
- Tipi: Tutti i letterali numerici vengono inizialmente parsati come
 valori a 64-bit in virgola mobile (double).

3. COMMENTI
============
- Stile REM: La keyword 'REM' trasforma il resto della riga logica (fino al successivo a capo o ':') in un commento.
- Stile Apostrofo: Un apostrofo (') trasforma il resto della riga fisica (fino al successivo a capo) in un commento.
- Stile C# (riga singola): Un doppio slash (//) funziona come l'apostrofo, ignorando il resto della riga fisica.
- Stile C# (blocco): Il codice racchiuso tra /* e * / viene ignorato dal lexer. Può estendersi su più righe e non può
                   essere annidato.

4. SEPARATORI E CONTINUAZIONE DI RIGA
=====================================
- Spazi e Tab: Vengono usati per separare i token ma sono altrimenti ignorati.
- Due Punti (:): Separa più istruzioni sulla stessa riga logica.
- A Capo (\n, \r\n): Genera un token 'Newline' e termina un'istruzione, a meno che non si applichi una regola di continuazione.
- Continuazione di Riga Esplicita (_): Un carattere underscore (_) alla fine di una riga fisica (ignorando gli spazi finali)
indica che l'istruzione continua sulla riga successiva. Il Lexer ignora l'_ e l'a-capo, presentando al Parser un unico
flusso di token.
- Continuazione di Riga con Due Punti (:): Se una riga fisica inizia con due punti (ignorando gli spazi iniziali),
 viene considerata una continuazione della riga logica precedente. Il Lexer unisce le due righe come se fossero
 separate da un due punti sulla stessa riga.
- Continuazione di Riga Implicita (Operatori): Un a-capo viene ignorato automaticamente (non implementato nella fase
 di pre-processing attuale, ma gestito dal Parser) se il token precedente è un operatore binario
 (es. +, -, *, AND, OR, =), una virgola (,) o un punto (.).

5. NUMERI DI RIGA
==================
- Riconoscimento: Un numero viene trattato come un token 'LineNumber' solo ed esclusivamente se è il primo token
non-whitespace all'inizio di una riga logica.
- Formati: I numeri di riga possono essere specificati in formato decimale, esadecimale, ottale o binario.

6. OPERATORI E DELIMITATORI
=============================
- Aritmetici: +, -, *, /, \, ^, MOD
- Comparazione: =, <>, <, >, <=, >=
- Parentesi: (), [], {}
- Altri: , ; . # -> => @ &

7. DIRETTIVE DI PRE-PROCESSORE (COMPILAZIONE CONDIZIONALE)
===========================================================
Le direttive controllano quale parte del codice viene analizzata dal lexer. Devono trovarsi all'inizio di una riga logica
e non vengono eseguite se si trovano all'interno di commenti o stringhe.
- #DEF <simbolo> [valore]: Definisce un simbolo di compilazione, con un valore stringa opzionale.
- #UNDEF <simbolo>: Rimuove la definizione di un simbolo.
- #IFDEF <simbolo>: Include il blocco di codice successivo se il simbolo è definito.
- #IFUNDEF <simbolo>: Include il blocco di codice successivo se il simbolo NON è definito.
- #IF expr THEN / #ELSEIF expr THEN: Include il blocco se l'espressione costante è vera (non zero).
  Supporta: +, -, *, /, \, MOD, ^, AND, OR, NOT, XOR, =, <, >, <=, >=, <>, parentesi.
  I simboli definiti con #DEF possono essere usati come variabili numeriche.
- #ELSE: Alterna la condizione di inclusione all'interno di un blocco #IFDEF/#IFUNDEF/#IF.
- #ENDIF / #END IF: Termina un blocco di compilazione condizionale.
- #REGION "nome": Inizia una regione di codice collassabile (puramente decorativa).
- #END REGION: Termina una regione di codice.
*/
/// <summary>
///     Rappresenta l'analizzatore lessicale (lexer) per il linguaggio BASIC.
///     Trasforma il codice sorgente in una sequenza di token in modo efficiente.
/// </summary>
public sealed partial class Lexer
{
    /// <summary>
    /// La mappa che collega ogni riga del sorgente al suo numero di riga originale.
    /// QUESTA È L'UNICA FONTE DI VERITÀ PER IL CODICE SORGENTE.
    /// </summary>
    private readonly ImmutableArray<SourceLine> _sourceMap;

    /// <summary>
    /// L'indice della riga corrente che il lexer sta analizzando nell'array `_sourceMap`.
    /// </summary>
    private int _currentLineIndex;

    /// <summary>
    /// L'indice del carattere (colonna) corrente all'interno del testo della riga attuale (`_sourceMap[_currentLineIndex].Text`).
    /// </summary>
    private int _currentColumnIndex;

    /// <summary>
    /// L'indice della riga logica di inizio del token corrente in fase di scansione.
    /// </summary>
    private int _tokenStartLine;

    /// <summary>
    /// La colonna di inizio del token corrente in fase di scansione.
    /// </summary>
    private int _tokenStartColumn;

    /// <summary>
    /// Un flag di stato che indica se il lexer si trova all'inizio di una riga logica.
    /// </summary>
    private bool _startOfLogicalLine = true;

    /// <summary>
    /// Se true, disabilita il riconoscimento dei numeri di riga (`TokenKind.LineNumber`) anche
    /// all'inizio di una riga logica. Usato per il lexing JIT di frammenti isolati (singola
    /// espressione o istruzione, es. macro `&amp;expr` o segmenti di stringhe interpolate
    /// `$"{expr}"`), dove un letterale numerico come `0.856` non deve mai essere scambiato
    /// per un numero di riga (che consumerebbe solo la parte intera, spezzando il letterale).
    /// </summary>
    private readonly bool _disableLineNumbers;

    /// <summary>
    /// Modalità espressione: abilitata dal lexing JIT di espressioni isolate (es. filtri
    /// DB via <see cref="Parser.ParseSingleExpression"/>). In questa modalità l'apice singolo
    /// <c>'</c> è un delimitatore di stringa (parità col backend C, gmb_expr), NON un commento.
    /// Nei file sorgente normali <c>'</c> resta il carattere di commento documentato.
    /// </summary>
    private readonly bool _expressionMode;

    /// <summary>
    /// Un contatore per tracciare la profondità di annidamento delle parentesi graffe `{}`.
    /// </summary>
    private int _braceDepth;

    /// <summary>
    /// L'istanza del programma che fornisce il contesto di compilazione.
    /// </summary>
    private readonly IDefinesProvider _definesProvider;

    /// <summary>
    /// Ottiene o imposta un valore che indica se il logging dettagliato del lexer è attivo.
    /// </summary>
    public static bool Verbose { get; set; }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="Lexer"/>.
    /// </summary>
    public Lexer(ImmutableArray<SourceLine> sourceMap, IDefinesProvider definesProvider, bool verbose = false,
        bool disableLineNumbers = false, bool expressionMode = false)
    {
        _sourceMap = sourceMap;
        _definesProvider = definesProvider;
        Verbose = verbose;
        _disableLineNumbers = disableLineNumbers;
        _expressionMode = expressionMode;
        Log($"[LEXER.CTOR] Inizializzato Lexer con SourceMap di {_sourceMap.Length} righe, Verbose: {Verbose}");
    }

    /// <summary>
    /// Esegue le tre passate di pre-elaborazione in una singola iterazione sulla SourceMap
    /// per massima efficienza, utilizzando una macchina a stati a livello di carattere
    /// per gestire correttamente i contesti (stringhe, commenti, raw string).
    /// </summary>
    /// <returns>Una SourceMap contenente solo le righe logiche complete pronte per la tokenizzazione.</returns>
    private ImmutableArray<SourceLine> PhasePreProcessAll()
    {
        var outputBuilder = ImmutableArray.CreateBuilder<SourceLine>();
        // ponytail: (parentState, anyBranchTaken) — anyBranchTaken previene ELSEIF/ELSE dopo un branch true
        var ifStack = new Stack<(bool ParentState, bool AnyBranchTaken)>();
        var currentlyIncluding = true;

        var inMultiLineComment = false;
        var inRawString = false;
        var inBacktickString = false;

        SourceLine? currentLogicalLine = null;
        var currentLineBuilder = new StringBuilder();
        var pendingSpaceForContinuation = false;

        for (var lineIndex = 0; lineIndex < _sourceMap.Length; lineIndex++)
        {
            var line = _sourceMap[lineIndex];
            var rawText = line.Text;

            if (!inMultiLineComment && !inRawString)
            {
                var trimmedLine = rawText.TrimStart();
                if (trimmedLine.StartsWith('#'))
                {
                    var parts = trimmedLine.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
                    var directive = parts[0].ToUpperInvariant();

                    switch (directive)
                    {
                        case "#IFDEF" or "#IFUNDEF":
                            var symbolName = parts.Length > 1 ? parts[1] : "";
                            var conditionResult = _definesProvider.IsDefined(symbolName) ^ (directive == "#IFUNDEF");
                            ifStack.Push((currentlyIncluding, currentlyIncluding && conditionResult));
                            currentlyIncluding = currentlyIncluding && conditionResult;
                            break;
                        case "#IF":
                            var ifExprResult = EvaluatePreprocessorExpr(parts.AsSpan(1), _definesProvider, line);
                            ifStack.Push((currentlyIncluding, currentlyIncluding && ifExprResult));
                            currentlyIncluding = currentlyIncluding && ifExprResult;
                            break;
                        case "#ELSEIF":
                            if (ifStack.Count == 0)
                                throw new BasicException(ErrorCode.SyntaxError,
                                    new SourcePosition(-1, line.OriginalLineNumber, 1),
                                    "#ELSEIF senza #IF.", line);
                            var (elseifParent, elseifTaken) = ifStack.Pop();
                            var elseifExprResult = EvaluatePreprocessorExpr(parts.AsSpan(1), _definesProvider, line);
                            var elseifThisBranch = elseifParent && !elseifTaken && elseifExprResult;
                            currentlyIncluding = elseifThisBranch;
                            ifStack.Push((elseifParent, elseifTaken || elseifThisBranch));
                            break;
                        case "#ELSE":
                            if (ifStack.Count == 0)
                                throw new BasicException(ErrorCode.SyntaxError,
                                    new SourcePosition(-1, line.OriginalLineNumber, 1),
                                    "#ELSE senza #IFDEF/#IFUNDEF.", line);
                            var (elseParent, elseTaken) = ifStack.Pop();
                            currentlyIncluding = elseParent && !elseTaken;
                            ifStack.Push((elseParent, true));
                            break;
                        case "#ENDIF":
                            if (ifStack.Count == 0)
                                throw new BasicException(ErrorCode.SyntaxError,
                                    new SourcePosition(-1, line.OriginalLineNumber, 1),
                                    "#ENDIF senza #IFDEF/#IFUNDEF.", line);
                            currentlyIncluding = ifStack.Pop().ParentState;
                            break;
                        case "#END":
                            if (parts.Length > 1 && string.Equals(parts[1], "REGION", StringComparison.OrdinalIgnoreCase))
                            {
                                // #END REGION — decorativo, ignora
                            }
                            else if (parts.Length > 1 && string.Equals(parts[1], "IF", StringComparison.OrdinalIgnoreCase))
                            {
                                // #END IF — equivalente a #ENDIF
                                if (ifStack.Count == 0)
                                    throw new BasicException(ErrorCode.SyntaxError,
                                        new SourcePosition(-1, line.OriginalLineNumber, 1),
                                        "#END IF senza #IFDEF/#IFUNDEF.", line);
                                currentlyIncluding = ifStack.Pop().ParentState;
                            }
                            break;
                        case "#DEF" when currentlyIncluding:
                            if (parts.Length > 1)
                                _definesProvider.AddDefine(parts[1], parts.Length > 2 ? string.Join(" ", parts.Skip(2)) : null);
                            break;
                        case "#UNDEF" when currentlyIncluding:
                            if (parts.Length > 1) _definesProvider.RemoveDefine(parts[1]);
                            break;
                        case "#REGION" when currentlyIncluding:
                            // #REGION "nome" — decorativo, ignora
                            break;
                    }

                    continue;
                }
            }

            if (!currentlyIncluding) continue;

            var cleanLineBuilder = new StringBuilder();
            var inStandardString = false;
            var inSingleQuoteString = false;
            var i = 0;
            while (i < rawText.Length)
            {
                var span = rawText.AsSpan(i);
                if (inMultiLineComment)
                {
                    if (span.StartsWith("*/"))
                    {
                        inMultiLineComment = false;
                        i += 2;
                    }
                    else i++;

                    continue;
                }

                if (inRawString)
                {
                    if (span.StartsWith("\"\"\""))
                    {
                        inRawString = false;
                        cleanLineBuilder.Append("\"\"\"");
                        i += 3;
                    }
                    else
                    {
                        cleanLineBuilder.Append(rawText[i]);
                        i++;
                    }

                    continue;
                }

                if (inBacktickString)
                {
                    var ch = rawText[i];
                    if (ch == '`')
                    {
                        inBacktickString = false;
                    }
                    cleanLineBuilder.Append(ch);
                    i++;
                    continue;
                }

                if (inStandardString)
                {
                    if (rawText[i] == '"')
                    {
                        if (i + 1 < rawText.Length && rawText[i + 1] == '"')
                        {
                            cleanLineBuilder.Append("\"\"");
                            i += 2;
                            continue;
                        }

                        inStandardString = false;
                    }

                    cleanLineBuilder.Append(rawText[i]);
                    i++;
                    continue;
                }

                if (inSingleQuoteString)
                {
                    if (rawText[i] == '\'')
                    {
                        if (i + 1 < rawText.Length && rawText[i + 1] == '\'')
                        {
                            cleanLineBuilder.Append("''");
                            i += 2;
                            continue;
                        }

                        inSingleQuoteString = false;
                    }

                    cleanLineBuilder.Append(rawText[i]);
                    i++;
                    continue;
                }

                var currentChar = rawText[i];

                if (span.StartsWith("/*"))
                {
                    inMultiLineComment = true;
                    i += 2;
                }
                else if (currentChar == '`')
                {
                    inBacktickString = true;
                    cleanLineBuilder.Append('`');
                    i++;
                }
                else if (span.StartsWith("\"\"\""))
                {
                    inRawString = true;
                    cleanLineBuilder.Append("\"\"\"");
                    i += 3;
                }
                else if (currentChar == '"')
                {
                    inStandardString = true;
                    cleanLineBuilder.Append(currentChar);
                    i++;
                }
                else if (_expressionMode && currentChar == '\'')
                {
                    // Modalità espressione (filtri DB): l'apice singolo è un delimitatore di
                    // stringa, parità col backend C (gmb_expr). Nei file sorgente resta un commento.
                    inSingleQuoteString = true;
                    cleanLineBuilder.Append(currentChar);
                    i++;
                }
                else if (currentChar == '\'' || span.StartsWith("//") || IsRemKeyword(rawText, i)) break;
                else
                {
                    cleanLineBuilder.Append(currentChar);
                    i++;
                }
            }

            var textToAppend = cleanLineBuilder.ToString();

            if (currentLogicalLine == null)
            {
                if (string.IsNullOrWhiteSpace(textToAppend))
                {
                    outputBuilder.Add(line with { Text = string.Empty });
                    continue;
                }

                currentLogicalLine = line;
            }
            else
            {
                if (pendingSpaceForContinuation)
                {
                    currentLineBuilder.Append(' ');
                }
            }

            currentLineBuilder.Append(textToAppend);

            var continues = false;
            if (inRawString || inBacktickString)
            {
                currentLineBuilder.Append('\n');
                continues = true;
            }
            else
            {
                var trimmedLogicalContent = currentLineBuilder.ToString().TrimEnd();
                if (trimmedLogicalContent.EndsWith('_'))
                {
                    currentLineBuilder.Length = trimmedLogicalContent.Length - 1;
                    continues = true;
                    pendingSpaceForContinuation = true;
                }
                else
                {
                    pendingSpaceForContinuation = false;
                    var nextLineStartsColon = lineIndex + 1 < _sourceMap.Length &&
                                              _sourceMap[lineIndex + 1].Text.TrimStart().StartsWith(':');
                    if (nextLineStartsColon) continues = true;
                }
            }

            if (!continues)
            {
                outputBuilder.Add(currentLogicalLine with { Text = currentLineBuilder.ToString().TrimEnd() });
                currentLogicalLine = null;
                currentLineBuilder.Clear();
            }
        }

        if (currentLogicalLine != null)
            outputBuilder.Add(currentLogicalLine with { Text = currentLineBuilder.ToString().TrimEnd() });

        if (ifStack.Count > 0)
        {
            var lastLine = _sourceMap.Last();
            throw new BasicException(ErrorCode.SyntaxError, new SourcePosition(-1, lastLine.OriginalLineNumber, 1),
                "Blocchi #IF... non chiusi. Atteso #ENDIF.", lastLine);
        }

        if (inMultiLineComment)
        {
            var lastLine = _sourceMap.Last();
            throw new BasicException(ErrorCode.SyntaxError,
                new SourcePosition(-1, lastLine.OriginalLineNumber, lastLine.Text.Length),
                "Commento multiriga /* non chiuso.", lastLine);
        }

        if (inRawString)
        {
            var lastLine = _sourceMap.Last();
            throw new BasicException(ErrorCode.SyntaxError,
                new SourcePosition(-1, lastLine.OriginalLineNumber, lastLine.Text.Length),
                "Raw string \"\"\" non chiusa.", lastLine);
        }

        if (inBacktickString)
        {
            var lastLine = _sourceMap.Last();
            throw new BasicException(ErrorCode.SyntaxError,
                new SourcePosition(-1, lastLine.OriginalLineNumber, lastLine.Text.Length),
                "Raw string backtick non chiusa.", lastLine);
        }

        return outputBuilder.ToImmutable();

        // La funzione locale viene definita qui per essere nello scope dell'intero metodo.
        bool IsRemKeyword(string text, int index)
        {
            if (text.Length < index + 3) return false;
            if (!text.AsSpan(index, 3).Equals("REM", StringComparison.OrdinalIgnoreCase)) return false;
            if (text.Length > index + 3 && char.IsLetterOrDigit(text[index + 3])) return false;
            return index == 0 || char.IsWhiteSpace(text[index - 1]) || text[index - 1] == ':';
        }
    }

    /// <summary>
    /// Esegue la scansione dell'intero codice sorgente e restituisce una lista di token.
    /// Agisce come orchestratore: esegue la pre-elaborazione completa e poi
    /// delega la tokenizzazione a un metodo interno specializzato.
    /// </summary>
    public IReadOnlyList<Token> ScanTokens()
    {
        Log("[LEXER.ScanTokens] Avvio FASE 1/2/3: Pre-elaborazione Unificata.");

        var logicalLinesMap = PhasePreProcessAll();
        Log($"[LEXER.ScanTokens] Pre-elaborazione completata. Totale righe logiche: {logicalLinesMap.Length}.");

        // --- BLOCCO DI DEBUG (Mostra le righe logiche unite) ---
        Log("\n--- OUTPUT UNIFICATO (Righe Logiche) ---");
        Log("==========================================================");
        foreach (var line in logicalLinesMap)
        {
            Log($"[L:{line.OriginalLineNumber:D3}] {line.Text}");
        }

        Log("==========================================================");
        // --- FINE BLOCCO DI DEBUG ---

        // Se non ci sono righe da processare, restituisce solo il token EOF.
        if (!logicalLinesMap.Any())
        {
            return new List<Token> { new(TokenKind.Eof, "", null, new SourcePosition(-1, 1, 1)) };
        }

        // Crea una nuova istanza del lexer con le righe logiche pulite
        // e chiama il metodo di tokenizzazione interno.
        var finalLexer = new Lexer(logicalLinesMap, _definesProvider, Verbose, _disableLineNumbers, _expressionMode);
        return finalLexer.TokenizeInternal();
    }

    /// <summary>
    /// Esegue il loop di tokenizzazione principale su una SourceMap che contiene
    /// solo righe logiche complete. Questo metodo viene chiamato internamente da ScanTokens.
    /// VERSIONE CORRETTA: La logica di gestione dei newline è stata centralizzata e resa robusta.
    /// </summary>
    /// <returns>La lista finale di token.</returns>
    private List<Token> TokenizeInternal()
    {
        var tokens = new List<Token>();

        while (!IsAtEnd())
        {
            // Resetta lo stato per ogni nuova riga logica.
            _startOfLogicalLine = true;
            _currentColumnIndex = 0;
            var lineText = _sourceMap[_currentLineIndex].Text;

            // Loop per consumare tutti i token su una riga logica.
            while (_currentColumnIndex < lineText.Length)
            {
                MarkTokenStart();
                var tokenStartCol = _currentColumnIndex;
                var tokenStartLine = _currentLineIndex;

                var c = PeekChar();
                if (c is ' ' or '\t' or '\r')
                {
                    AdvanceChar();
                    continue; // Salta gli spazi bianchi.
                }

                var token = ScanTokenInternal();
                tokens.Add(token);
            }

            // Dopo aver processato tutti i caratteri di una riga, aggiungi sempre un Newline.
            // Questo ora è l'unico punto in cui un Newline viene aggiunto esplicitamente alla fine di una riga.
            MarkTokenStart(); // Imposta la posizione per il token Newline
            tokens.Add(CreateToken(TokenKind.Newline));
            _currentLineIndex++;
        }

        // Aggiunge il token finale di End-Of-File.
        var eofPosition = _sourceMap.Any()
            ? new SourcePosition(_sourceMap[^1].FileId, _sourceMap[^1].OriginalLineNumber,
                _sourceMap[^1].Text.Length + 1)
            : new SourcePosition(-1, 1, 1);
        tokens.Add(new Token(TokenKind.Eof, string.Empty, null, eofPosition));

        return tokens;
    }

    /// <summary>
    /// Esegue la scansione del prossimo token dal sorgente.
    /// QUESTA VERSIONE È CORRETTA: distingue tra stringhe normali (") e raw string (""").
    /// </summary>
    private Token ScanTokenInternal()
    {
        // Gestisce i numeri di riga solo all'inizio di una riga logica.
        if (_startOfLogicalLine && !_disableLineNumbers && char.IsDigit(PeekChar()))
        {
            return ScanLineNumber();
        }

        _startOfLogicalLine = false; // Qualsiasi altro token resetta il flag.

        var c = AdvanceChar();
        switch (c)
        {
            case '(': return CreateToken(TokenKind.LeftParen);
            case ')': return CreateToken(TokenKind.RightParen);
            case '[': return CreateToken(TokenKind.LeftBracket);
            case ']': return CreateToken(TokenKind.RightBracket);
            case '{':
                _braceDepth++;
                return CreateToken(TokenKind.LeftBrace);
            case '}':
                if (_braceDepth > 0) _braceDepth--;
                return CreateToken(TokenKind.RightBrace);
            case '+': return CreateToken(Match('+') ? TokenKind.PlusPlus : TokenKind.Plus);
            case '-':
                return Match('-')
                    ? CreateToken(TokenKind.MinusMinus)
                    : CreateToken(Match('>') ? TokenKind.Arrow : TokenKind.Minus);
            case '*': return CreateToken(TokenKind.Multiply);
            case '/': return CreateToken(TokenKind.Divide);
            case '\\': return CreateToken(TokenKind.IntegerDivide);
            case '^': return Match('^') ? CreateToken(TokenKind.BitXor) : CreateToken(TokenKind.Power);
            case ',': return CreateToken(TokenKind.Comma);
            case ';': return CreateToken(TokenKind.Semicolon);
            case '?': return CreateToken(Match('?') ? TokenKind.QuestionMarkQuestion : TokenKind.QuestionMark);
            case ':':
                // Riconosce l'operatore di assegnazione inline :=
                if (Match('=')) return CreateToken(TokenKind.InlineAssign);
                // La gestione del DictColon ora si basa solo sul contesto delle graffe.
                return _braceDepth > 0
                    ? CreateToken(TokenKind.DictColon)
                    : CreateToken(TokenKind.Colon);
            case '.':
                // Dopo che lo switch ha consumato il '.', MatchKeyword controlla
                // i caratteri successivi. Questa è la logica corretta e robusta.
                if (MatchKeyword("T.")) return CreateToken(TokenKind.True);
                if (MatchKeyword("F.")) return CreateToken(TokenKind.False);
                if (MatchKeyword("AND.")) return CreateToken(TokenKind.And);
                if (MatchKeyword("OR.")) return CreateToken(TokenKind.Or);
                if (MatchKeyword("NOT.")) return CreateToken(TokenKind.Not);
                // Fallback: se nessuno dei match ha successo, era solo un punto.
                return CreateToken(TokenKind.Dot);
            case '|': return Match('|') ? CreateToken(TokenKind.BitOr) : CreateToken(TokenKind.Bar);
            case '#': return CreateToken(TokenKind.Hash);
            case '&': return Match('&') ? CreateToken(TokenKind.BitAnd) : CreateToken(TokenKind.Ampersand);
            case '!':
                // '!=' resta NotEqual; '!' da solo e' il NOT BITWISE (il NOT logico e' la keyword).
                if (Match('=')) return CreateToken(TokenKind.NotEqual);
                return CreateToken(TokenKind.BitNot);
            case '=':
                // Riconosce l'operatore di uguaglianza esatta ==
                if (Match('=')) return CreateToken(TokenKind.ExactlyEqual);
                return CreateToken(Match('>') ? TokenKind.FatArrow : TokenKind.Equal);
            case '<':
                return CreateToken(Match('>')
                    ? TokenKind.NotEqual
                    : Match('=')
                        ? TokenKind.LessEqual
                        : TokenKind.LessThan);
            case '>': return CreateToken(Match('=') ? TokenKind.GreaterEqual : TokenKind.GreaterThan);

            case '"':
                if (PeekChar() == '"' && PeekNextChar() == '"')
                {
                    AdvanceChar();
                    AdvanceChar();
                    return ScanRawTripleQuoteString();
                }

                return ScanString('"');

            case '\'':
                // Stringa con apice singolo '...' — parità col C (gmb_expr accetta '...' e "..."
                // nei filtri DB). Escape del delimitatore raddoppiato ('').
                return ScanString('\'');

            case '`':
                return ScanBackTickString();

            case '$':
                // Stringa interpolata: $"..."
                if (PeekChar() == '"')
                {
                    AdvanceChar(); // Consuma il "
                    return ScanInterpolatedString();
                }
                // Altrimenti è un suffisso di tipo (es. nome$), gestito come identificatore.
                _currentColumnIndex--;
                return ScanIdentifierOrKeyword();

            case '%':
                // Costante stile PowerBASIC: %NOME (il nome inizia sempre con una lettera).
                // Stesso trucco già usato sopra per '$': IsAlphaNumeric include già '%' come
                // carattere valido di CONTINUAZIONE di un identificatore (usato oggi solo per il
                // suffisso finale "nome%" = INTEGER), quindi rimandare indietro e richiamare
                // ScanIdentifierOrKeyword lo fa consumare come primo carattere del lessema,
                // producendo un Identifier con lessema "%NOME" — nessun'altra logica da toccare.
                if (char.IsLetter(PeekChar()) || PeekChar() == '_')
                {
                    _currentColumnIndex--;
                    return ScanIdentifierOrKeyword();
                }
                {
                    var errLineP = _sourceMap.Any() ? _sourceMap[_currentLineIndex].OriginalLineNumber : 1;
                    Error(errLineP, _currentColumnIndex,
                        "Il nome di una costante '%nome' deve iniziare con una lettera.");
                    return null;
                }

            // Rimosso ' ' e '\t' da qui, gestiti in TokenizeInternal
            case '@': return CreateToken(TokenKind.AtSign);
            default:
                if (char.IsLetter(c) || c == '_')
                {
                    _currentColumnIndex--;
                    return ScanIdentifierOrKeyword();
                }

                if (char.IsDigit(c))
                {
                    _currentColumnIndex--;
                    return ScanNumber();
                }

                var errLine2 = _sourceMap.Any() ? _sourceMap[_currentLineIndex].OriginalLineNumber : 1;
                Error(errLine2, _currentColumnIndex, $"Carattere inatteso: '{c}'.");
                return null;
        }
    }

    /// <summary>
    /// Analizza un letterale raw string ("""), che può contenere newline e apici.
    /// Questo metodo viene chiamato dopo che il delimitatore di apertura """ è stato consumato.
    /// </summary>
    private Token ScanRawTripleQuoteString()
    {
        var stringBuilder = new StringBuilder();
        while (PeekChar() != '\0') // Loop until the end of the current logical line
        {
            // Cerca il delimitatore di chiusura """
            if (PeekChar() == '"' && PeekNextChar() == '"' && PeekCharAtOffset(2) == '"')
            {
                // Trovato delimitatore di chiusura.
                AdvanceChar();
                AdvanceChar();
                AdvanceChar();
                return CreateToken(TokenKind.StringLiteral, stringBuilder.ToString());
            }

            stringBuilder.Append(AdvanceChar());
        }

        // Se il loop termina, significa che abbiamo raggiunto la fine della riga logica
        // senza trovare il delimitatore di chiusura.
        var errorLine = _sourceMap.Any() ? _sourceMap[_tokenStartLine].OriginalLineNumber : 1;
        Error(errorLine, _tokenStartColumn, "Raw string literal non terminata.");
        return null; // Irraggiungibile
    }

    /// <summary>
    /// Analiza un literal raw string delimitato da un singolo backtick (`).
    /// Viene chiamato dopo che il backtick iniziale è stato consumato.
    /// </summary>
    private Token ScanBackTickString()
    {
        var stringBuilder = new StringBuilder();

        while (PeekChar() != '\0') // Loop until the end of the current logical line
        {
            // Cerca il delimitatore di chiusura `
            if (PeekChar() == '`')
            {
                AdvanceChar(); // Consuma il backtick
                return CreateToken(TokenKind.StringLiteral, stringBuilder.ToString());
            }

            stringBuilder.Append(AdvanceChar());
        }

        // Se il loop termina, significa che abbiamo raggiunto la fine della riga logica
        var errorLine = _sourceMap.Any() ? _sourceMap[_tokenStartLine].OriginalLineNumber : 1;
        Error(errorLine, _tokenStartColumn, "Raw string literal non terminata (manca il backtick).");
        return null; // Irraggiungibile
    }

    /// <summary>
    /// Analizza un letterale stringa standard, delimitato da doppi apici (").
    /// </summary>
    private Token ScanString(char delimiter)
    {
        var stringBuilder = new StringBuilder();
        while (true)
        {
            if (IsAtEnd() || PeekChar() == '\n' || PeekChar() == '\r')
            {
                var errorLine = _sourceMap.Any() ? _sourceMap[_tokenStartLine].OriginalLineNumber : 1;
                Error(errorLine, _tokenStartColumn, "Letterale stringa non terminato.");
            }

            if (PeekChar() == delimiter)
            {
                // Gestisce l'escape del delimitatore raddoppiato ("" o '').
                if (PeekNextChar() == delimiter)
                {
                    AdvanceChar(); // Consuma il primo delimitatore
                    stringBuilder.Append(AdvanceChar()); // Consuma e aggiunge il secondo
                }
                else
                {
                    // È il delimitatore di chiusura, esci dal loop.
                    break;
                }
            }
            else
            {
                // Aggiungi il carattere corrente alla stringa.
                stringBuilder.Append(AdvanceChar());
            }
        }

        // Consuma il delimitatore di chiusura finale.
        AdvanceChar();

        return CreateToken(TokenKind.StringLiteral, stringBuilder.ToString());
    }

    /// <summary>
    /// Analizza una stringa interpolata $"..." e ne estrae il template.
    /// Il template viene memorizzato nel lexeme del token come testo puro.
    /// Il parser e l'esecutore si occuperanno di segmentarlo in parti letterali ed espressioni.
    /// Formato: "testo{expr}testo{expr:format}testo"
    /// </summary>
    private Token ScanInterpolatedString()
    {
        var sb = new StringBuilder();
        int braceDepth = 0;

        while (true)
        {
            if (IsAtEnd() || PeekChar() == '\n' || PeekChar() == '\r')
            {
                var errorLine = _sourceMap.Any() ? _sourceMap[_tokenStartLine].OriginalLineNumber : 1;
                Error(errorLine, _tokenStartColumn, "Stringa interpolata non terminata.");
                break;
            }

            var c = PeekChar();

            // Traccia la profondità delle parentesi graffe
            if (c == '{' && PeekNextChar() != '{')
            {
                braceDepth++;
                AdvanceChar();
                sb.Append(c);
                continue;
            }
            if (c == '}' && PeekNextChar() != '}' && braceDepth > 0)
            {
                braceDepth--;
                AdvanceChar();
                sb.Append(c);
                continue;
            }

            // Chiusura della stringa (solo fuori da {...})
            if (c == '"' && braceDepth == 0)
            {
                AdvanceChar();
                break;
            }

            // Escape {{ → { letterale
            if (c == '{' && PeekNextChar() == '{')
            {
                AdvanceChar(); AdvanceChar();
                sb.Append(''); // Carattere speciale per { letterale
                continue;
            }
            // Escape }} → } letterale
            if (c == '}' && PeekNextChar() == '}')
            {
                AdvanceChar(); AdvanceChar();
                sb.Append(''); // Carattere speciale per } letterale
                continue;
            }

            AdvanceChar();
            sb.Append(c);
        }

        return CreateToken(TokenKind.InterpolatedStringLiteral, sb.ToString());
    }

    /// <summary>
    /// Analizza un numero di riga.
    /// </summary>
    private Token ScanLineNumber()
    {
        // Gestisce i formati 0x, 0o, 0b per i numeri di riga
        if (PeekChar() == '0' && "xXoObB".Contains(PeekNextChar()))
        {
            var value = ParseIntegerLiteral();
            _startOfLogicalLine = false;
            return CreateToken(TokenKind.LineNumber, value);
        }

        // Consuma le cifre del numero di riga
        while (char.IsDigit(PeekChar())) AdvanceChar();

        var lexeme = GetLexeme();
        if (double.TryParse(lexeme, NumberStyles.Float, CultureInfo.InvariantCulture, out var valueDecimal))
        {
            // Un numero di riga consuma lo stato "inizio riga logica"
            _startOfLogicalLine = false;
            return CreateToken(TokenKind.LineNumber, valueDecimal);
        }

        // Se il parsing fallisce, potrebbe non essere un numero di riga valido (caso limite).
        // Ripristiniamo la posizione per tentare di leggerlo come un identificatore.
        _currentLineIndex = _tokenStartLine;
        _currentColumnIndex = _tokenStartColumn;
        return ScanIdentifierOrKeyword();
    }

    /// <summary>
    /// Analizza un letterale numerico (intero o in virgola mobile) all'interno del codice.
    /// </summary>
    private Token ScanNumber()
    {
        // Gestisce i formati 0x, 0o, 0b
        if (PeekChar() == '0' && "xXoObB".Contains(PeekNextChar()))
        {
            var value = ParseIntegerLiteral();
            return CreateToken(TokenKind.Number, value);
        }

        // Consuma la parte intera
        while (char.IsDigit(PeekChar())) AdvanceChar();

        // Consuma la parte decimale, se presente
        if (PeekChar() == '.' && char.IsDigit(PeekNextChar()))
        {
            AdvanceChar(); // Consuma il '.'
            while (char.IsDigit(PeekChar())) AdvanceChar();
        }

        // Esponente decimale (2026-11-04, parità col frontend C — gmb_lexer.c):
        // `1E30`, `1.5e-3`, `2.0E+9`. Senza questo, il `E30` restava un identificatore e
        // l'assegnazione veniva TRONCATA in silenzio (`x = 1.0E30` → 1) oppure falliva con
        // "Token 'E30' inatteso", a seconda del contesto: PCode/IL/VM-C divergevano dal
        // backend nativo C99 (che già accettava gli esponenti).
        if (PeekChar() is 'e' or 'E')
        {
            var signOffset = PeekCharAtOffset(1) is '+' or '-' ? 1 : 0;
            if (char.IsDigit(PeekCharAtOffset(1 + signOffset)))
            {
                AdvanceChar();                        // 'e' / 'E'
                if (signOffset == 1) AdvanceChar();   // segno
                while (char.IsDigit(PeekChar())) AdvanceChar();
            }
        }

        // Ottiene il testo del numero e tenta di parsarlo
        var lexeme = GetLexeme();
        if (double.TryParse(lexeme, NumberStyles.Float, CultureInfo.InvariantCulture, out var valueDecimal))
        {
            return CreateToken(TokenKind.Number, valueDecimal);
        }

        var errorLine = _sourceMap.Any() ? _sourceMap[_tokenStartLine].OriginalLineNumber : 1;
        Error(errorLine, _tokenStartColumn, $"Letterale numerico non valido: '{lexeme}'.");
        return null; // Irraggiungibile
    }

    /// <summary>
    /// Parsa un letterale intero in base 16 (esadecimale), 8 (ottale) o 2 (binario).
    /// </summary>
    private double ParseIntegerLiteral()
    {
        AdvanceChar(); // Consuma 'x', 'o', 'b'
        var baseChar = char.ToUpperInvariant(AdvanceChar());
        var baseValue = baseChar switch
        {
            'X' => 16, 'O' => 8, 'B' => 2, _ => throw new InvalidOperationException("Invalid base specifier.")
        };

        // Usa MarkTokenStart e GetLexeme() nel modo standard.
        MarkTokenStart();
        while (char.IsLetterOrDigit(PeekChar()))
        {
            AdvanceChar();
        }

        var numberPart = GetLexeme();

        try
        {
            return Convert.ToInt64(numberPart, baseValue);
        }
        catch (FormatException)
        {
            var errorLine = _sourceMap[_tokenStartLine].OriginalLineNumber;
            Error(errorLine, _tokenStartColumn, $"Cifre non valide per un numero in base {baseValue}.");
        }
        catch (OverflowException)
        {
            var errorLine = _sourceMap[_tokenStartLine].OriginalLineNumber;
            Error(errorLine, _tokenStartColumn, "Il numero è troppo grande per essere rappresentato.",
                ErrorCode.Overflow);
        }

        return 0; // Irraggiungibile
    }

    /// <summary>
    /// Analizza un identificatore o una keyword.
    /// </summary>
    private Token ScanIdentifierOrKeyword()
    {
        // La gestione delle keyword multi-parola è ora centralizzata in MatchKeyword.
        if (MatchKeyword("RESTORE SCREEN")) return CreateToken(TokenKind.RestoreScreen);
        if (MatchKeyword("END INTERFACE")) return CreateToken(TokenKind.EndInterface);
        if (MatchKeyword("END GENERATOR")) return CreateToken(TokenKind.EndGenerator);
        if (MatchKeyword("END FUNCTION")) return CreateToken(TokenKind.EndFunction);
        if (MatchKeyword("END PROPERTY")) return CreateToken(TokenKind.EndProperty);
        if (MatchKeyword("END OPERATOR")) return CreateToken(TokenKind.EndOperator);
        if (MatchKeyword("SAVE SCREEN")) return CreateToken(TokenKind.SaveScreen);
        if (MatchKeyword("READ WRITE")) return CreateToken(TokenKind.ReadWrite);
        if (MatchKeyword("END SELECT")) return CreateToken(TokenKind.EndSelect);
        if (MatchKeyword("END MATCH")) return CreateToken(TokenKind.EndMatch);
        if (MatchKeyword("LINE INPUT")) return CreateToken(TokenKind.LineInput);
        if (MatchKeyword("END CLASS")) return CreateToken(TokenKind.EndClass);
        if (MatchKeyword("END ENUM")) return CreateToken(TokenKind.EndEnum);
        if (MatchKeyword("END TYPE")) return CreateToken(TokenKind.EndType);
        if (MatchKeyword("END SUB")) return CreateToken(TokenKind.EndFunction);
        if (MatchKeyword("END GET")) return CreateToken(TokenKind.EndGet);
        if (MatchKeyword("END SET")) return CreateToken(TokenKind.EndSet);
        if (MatchKeyword("END TRY")) return CreateToken(TokenKind.EndTry);
        if (MatchKeyword("END IF")) return CreateToken(TokenKind.EndIf);
        if (MatchKeyword("END WITH")) return CreateToken(TokenKind.EndWith);
        if (MatchKeyword("END USING")) return CreateToken(TokenKind.EndUsing);
        if (MatchKeyword("END TICK")) return CreateToken(TokenKind.EndTick);
        if (MatchKeyword("END FN")) return CreateToken(TokenKind.EndFunction);
        if (MatchKeyword("SPRITEDEF END")) return CreateToken(TokenKind.EndSpritedef);

        // Consuma tutti i caratteri validi per un identificatore.
        while (IsAlphaNumeric(PeekChar()))
        {
            AdvanceChar();
        }

        // Ottiene il testo del token appena consumato.
        var text = GetLexeme();

        // Determina se il testo è una keyword o un semplice identificatore.
        var tokenKind = Keywords.GetValueOrDefault(text, TokenKind.Identifier);

        // Dopo un identificatore, non siamo più all'inizio di una riga logica.
        _startOfLogicalLine = false;

        return CreateToken(tokenKind);
    }

    /// <summary>
    /// Controlla e consuma una keyword, gestendo anche il caso in cui la keyword
    /// si trovi a cavallo di un'interruzione di riga (in caso di codice malformato).
    /// </summary>
    /// <param name="keyword">La keyword da cercare.</param>
    /// <param name="lookaheadOnly">Se true, non consuma i caratteri ma esegue solo il controllo.</param>
    private bool MatchKeyword(string keyword, bool lookaheadOnly = false)
    {
        var length = keyword.Length;

        // Esegue un controllo carattere per carattere, gestendo la possibilità
        // che la keyword sia spezzata su più righe (caso limite).
        for (var i = 0; i < length; i++)
        {
            var peekedChar = PeekCharAtOffset(i);
            // Se incontriamo la fine del file prima della fine della keyword, non è una corrispondenza.
            if (peekedChar == '\0') return false;
            // Confronto case-insensitive.
            if (char.ToUpperInvariant(peekedChar) != char.ToUpperInvariant(keyword[i]))
            {
                return false;
            }
        }

        // Dopo la keyword, ci deve essere un carattere non alfanumerico
        // per considerarla una parola intera.
        if (IsAlphaNumeric(PeekCharAtOffset(length)))
        {
            return false;
        }

        // Se è solo un lookahead, abbiamo finito.
        if (lookaheadOnly) return true;

        // Altrimenti, consuma i caratteri della keyword.
        for (var i = 0; i < length; i++)
        {
            AdvanceChar();
        }

        return true;
    }

    /// <summary>
    /// Verifica se il lexer ha raggiunto la fine del sorgente.
    /// </summary>
    private bool IsAtEnd() => _currentLineIndex >= _sourceMap.Length;

    /// <summary>
    /// Restituisce il carattere corrente senza consumarlo. Opera solo sulla riga logica corrente.
    /// </summary>
    /// <returns>Il carattere corrente, o '\0' se alla fine della riga logica.</returns>
    private char PeekChar()
    {
        if (IsAtEnd() || _currentColumnIndex >= _sourceMap[_currentLineIndex].Text.Length)
        {
            return '\0';
        }

        return _sourceMap[_currentLineIndex].Text[_currentColumnIndex];
    }

    /// <summary>
    /// Restituisce il carattere successivo a quello corrente senza consumarlo. Opera solo sulla riga logica corrente.
    /// </summary>
    /// <returns>Il carattere successivo, o '\0' se alla fine della riga logica.</returns>
    private char PeekNextChar()
    {
        if (IsAtEnd() || _currentColumnIndex + 1 >= _sourceMap[_currentLineIndex].Text.Length)
        {
            return '\0';
        }

        return _sourceMap[_currentLineIndex].Text[_currentColumnIndex + 1];
    }

    /// <summary>
    /// Restituisce il carattere a un dato scostamento dalla posizione corrente, senza consumare nulla. 
    /// Opera solo sulla riga logica corrente.
    /// </summary>
    /// <returns>Il carattere all'offset specificato, o '\0' se la posizione è fuori dai limiti della riga logica.</returns>
    private char PeekCharAtOffset(int offset)
    {
        if (IsAtEnd()) return '\0';

        var peekIndex = _currentColumnIndex + offset;
        var lineText = _sourceMap[_currentLineIndex].Text;

        if (peekIndex < 0 || peekIndex >= lineText.Length)
        {
            return '\0';
        }

        return lineText[peekIndex];
    }

    /// <summary>
    /// Consuma il carattere corrente e avanza la posizione (versione "stupida").
    /// </summary>
    private char AdvanceChar()
    {
        if (IsAtEnd()) return '\0';
        var line = _sourceMap[_currentLineIndex];
        if (_currentColumnIndex >= line.Text.Length)
        {
            // Fine della riga, consuma il '\n' virtuale
            _currentLineIndex++;
            _currentColumnIndex = 0;
            return '\n';
        }

        var charToReturn = line.Text[_currentColumnIndex];
        _currentColumnIndex++;
        return charToReturn;
    }

    /// <summary>
    /// Controlla se il carattere corrente corrisponde a quello atteso. Se sì, lo consuma.
    /// </summary>
    private bool Match(char expected)
    {
        // Se il carattere corrente non corrisponde, restituisce false senza fare nulla.
        if (PeekChar() != expected) return false;

        // Se corrisponde, consuma il carattere e restituisce true.
        AdvanceChar();
        return true;
    }

    /// <summary>
    /// Verifica se un carattere è alfanumerico o un simbolo valido per identificatori.
    /// </summary>
    private static bool IsAlphaNumeric(char c) => char.IsLetterOrDigit(c) || c is '$' or '%' or '!' or '#' or '_';

    /// <summary>
    /// Memorizza la posizione corrente (riga e colonna) come inizio del token.
    /// </summary>
    private void MarkTokenStart()
    {
        _tokenStartLine = _currentLineIndex;
        _tokenStartColumn = _currentColumnIndex;
    }

    /// <summary>
    /// Ottiene il lexema del token corrente, utilizzando la posizione di inizio memorizzata.
    /// Questa versione corretta gestisce i rari casi multi-riga senza leggere oltre il necessario.
    /// </summary>
    private string GetLexeme()
    {
        if (_tokenStartLine >= _sourceMap.Length) return string.Empty;

        // Caso 99%: il lexema è sulla stessa riga.
        if (_tokenStartLine == _currentLineIndex)
        {
            var lineText = _sourceMap[_tokenStartLine].Text;
            // Se la colonna di inizio è oltre la fine, non c'è lexema.
            if (_tokenStartColumn >= lineText.Length)
            {
                // Se la colonna corrente è la stessa, è un lexema vuoto. Se è maggiore, è un newline.
                return _currentColumnIndex > _tokenStartColumn ? "\n" : string.Empty;
            }

            var length = _currentColumnIndex - _tokenStartColumn;
            return lineText.Substring(_tokenStartColumn, length);
        }

        // Caso raro: il lexema si estende su più righe (es. commento non terminato).
        var sb = new StringBuilder();
        sb.Append(_sourceMap[_tokenStartLine].Text[_tokenStartColumn..]); // Parte finale della prima riga

        for (var i = _tokenStartLine + 1; i < _currentLineIndex; i++)
        {
            sb.Append('\n').Append(_sourceMap[i].Text); // Righe intermedie complete
        }

        if (_currentLineIndex < _sourceMap.Length)
        {
            sb.Append('\n');
            if (_currentColumnIndex > 0)
            {
                sb.Append(_sourceMap[_currentLineIndex].Text[.._currentColumnIndex]); // Parte iniziale dell'ultima riga
            }
        }

        return sb.ToString();
    }

    /// <summary>
    /// Crea un nuovo oggetto <see cref="Token"/> utilizzando lo stato corrente del lexer.
    /// </summary>
    /// <remarks>
    /// CORREZIONE CRITICA: Questo metodo ora legge il numero di riga originale direttamente
    /// dalla `SourceMap` utilizzando l'indice `_currentSourceMapIndex` come unica fonte di verità.
    /// Questo elimina ogni possibilità di desincronizzazione del numero di riga.
    /// </remarks>
    private Token CreateToken(TokenKind kind, object? literal = null)
    {
        var lexeme = GetLexeme();
        var tokenColumn = _tokenStartColumn + 1;

        var originalLineNumber = 1; // Default di sicurezza
        var fileId = -1; // Default di sicurezza

        if (_tokenStartLine >= 0 && _tokenStartLine < _sourceMap.Length)
        {
            var sourceLine = _sourceMap[_tokenStartLine];
            originalLineNumber = sourceLine.OriginalLineNumber;
            fileId = sourceLine.FileId;
        }

        var position = new SourcePosition(fileId, originalLineNumber, tokenColumn);

        return new Token(kind, lexeme, literal, position);
    }

    /// <summary>
    /// Logga un messaggio se la modalità verbose è attiva.
    /// </summary>
    private static void Log(string message, bool force = false)
    {
        if (Verbose || force) Console.WriteLine(message);
    }

    /// <summary>
    /// Lancia un'eccezione di parsing arricchita con informazioni sulla posizione.
    /// </summary>
    /// <param name="line">Il numero di riga originale in cui si è verificato l'errore.</param>
    /// <param name="column">La colonna in cui si è verificato l'errore.</param>
    /// <param name="message">Il messaggio di errore specifico.</param>
    /// <param name="code">Il codice di errore specifico.</param>
    [DoesNotReturn]
    private void Error(int line, int column, string message, ErrorCode code = ErrorCode.SyntaxError)
    {
        SourceLine? sourceLocation = null;
        var fileId = -1;

        if (_currentLineIndex >= 0 && _currentLineIndex < _sourceMap.Length)
        {
            sourceLocation = _sourceMap[_currentLineIndex];
            fileId = sourceLocation.FileId;
        }

        var position = new SourcePosition(fileId, line, column);

        throw new BasicException(code, position, message, sourceLocation);
    }

    /// <summary>
    /// Stampa il contesto del codice per il debugging.
    /// </summary>
    [SuppressMessage("CodeQuality", "IDE0051:Remove unused private members",
        Justification = "Utile per il debugging futuro.")]
    // ReSharper disable once UnusedMember.Local
    private void PrintCodeContext(int lines = 5)
    {
        if (IsAtEnd())
        {
            Console.WriteLine("\n=== Context code: At End Of File ===\n");
            return;
        }

        // Calcola le righe di contesto da mostrare
        var startLine = Math.Max(0, _currentLineIndex - lines);
        var endLine = Math.Min(_sourceMap.Length - 1, _currentLineIndex + lines);

        var sb = new StringBuilder();
        sb.AppendLine("\n=== Context code ===");

        // Itera sulle righe della SourceMap per costruire lo snippet
        for (var i = startLine; i <= endLine; i++)
        {
            var line = _sourceMap[i];
            sb.AppendLine($"[{line.OriginalLineNumber:D4}] {line.Text}");

            // Se è la riga corrente, aggiungi l'indicatore di posizione '^'
            if (i != _currentLineIndex) continue;
            // Il prefisso è "[NNNN] " (6 caratteri) + la colonna corrente
            const int prefixLength = 6;
            sb.Append(new string(' ', prefixLength + _currentColumnIndex));
            sb.AppendLine("^");
        }

        sb.AppendLine("====================\n");
        Console.WriteLine(sb.ToString());
    }

    /// <summary>
    /// Valuta un'espressione costante a compile-time per le direttive #IF e #ELSEIF.
    /// Supporta numeri interi, simboli #DEF, operatori aritmetici, logici e di confronto.
    /// </summary>
    private static bool EvaluatePreprocessorExpr(ReadOnlySpan<string> parts, IDefinesProvider program, SourceLine line)
    {
        // Ricostruisce l'espressione dalla linea originale, rimuovendo "#IF" o "#ELSEIF" e "THEN" finale
        var text = line.Text.Trim();
        // Trova il primo spazio dopo la direttiva
        var spaceIdx = text.IndexOf(' ');
        if (spaceIdx < 0) return false;
        var exprStr = text.Substring(spaceIdx).Trim();
        // Rimuove "THEN" finale se presente (case-insensitive)
        if (exprStr.Length >= 4 && exprStr.EndsWith("THEN", StringComparison.OrdinalIgnoreCase))
            exprStr = exprStr.Substring(0, exprStr.Length - 4).Trim();
        else if (exprStr.Length >= 5 && exprStr.EndsWith(" THEN", StringComparison.OrdinalIgnoreCase))
            exprStr = exprStr.Substring(0, exprStr.Length - 5).Trim();

        if (string.IsNullOrEmpty(exprStr)) return false;

        var result = EvalPreprocessorExpr(exprStr, program);
        return result != 0;
    }

    /// <summary>
    /// Parser/valutatore ricorsivo per espressioni costanti del pre-processore.
    /// Grammatica: logicalExpr ('OR' logicalExpr)*
    /// </summary>
    private static long EvalPreprocessorExpr(string expr, IDefinesProvider program)
    {
        var tokens = TokenizePreprocessorExpr(expr, program);
        var pos = 0;
        var result = ParseLogicalOr(tokens, ref pos);
        return result;
    }

    private static long ParseLogicalOr(List<PPToken> tokens, ref int pos)
    {
        var left = ParseLogicalAnd(tokens, ref pos);
        while (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator && tokens[pos].Text == "OR")
        {
            pos++;
            var right = ParseLogicalAnd(tokens, ref pos);
            left = (left != 0 || right != 0) ? 1 : 0;
        }
        return left;
    }

    private static long ParseLogicalAnd(List<PPToken> tokens, ref int pos)
    {
        var left = ParseComparison(tokens, ref pos);
        while (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator && tokens[pos].Text == "AND")
        {
            pos++;
            var right = ParseComparison(tokens, ref pos);
            left = (left != 0 && right != 0) ? 1 : 0;
        }
        return left;
    }

    private static long ParseComparison(List<PPToken> tokens, ref int pos)
    {
        var left = ParseAddSub(tokens, ref pos);
        if (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator)
        {
            var op = tokens[pos].Text;
            if (op is "=" or "<>" or "<" or ">" or "<=" or ">=")
            {
                pos++;
                var right = ParseAddSub(tokens, ref pos);
                left = op switch
                {
                    "=" => left == right ? 1 : 0,
                    "<>" => left != right ? 1 : 0,
                    "<" => left < right ? 1 : 0,
                    ">" => left > right ? 1 : 0,
                    "<=" => left <= right ? 1 : 0,
                    ">=" => left >= right ? 1 : 0,
                    _ => left
                };
            }
        }
        return left;
    }

    private static long ParseAddSub(List<PPToken> tokens, ref int pos)
    {
        var left = ParseMulDiv(tokens, ref pos);
        while (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator)
        {
            var op = tokens[pos].Text;
            if (op is "+" or "-" or "XOR")
            {
                pos++;
                var right = ParseMulDiv(tokens, ref pos);
                left = op switch
                {
                    "+" => left + right,
                    "-" => left - right,
                    "XOR" => left ^ right,
                    _ => left
                };
            }
            else break;
        }
        return left;
    }

    private static long ParseMulDiv(List<PPToken> tokens, ref int pos)
    {
        var left = ParseUnary(tokens, ref pos);
        while (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator)
        {
            var op = tokens[pos].Text;
            if (op is "*" or "/" or "\\" or "MOD" or "<<" or ">>")
            {
                pos++;
                var right = ParseUnary(tokens, ref pos);
                left = op switch
                {
                    "*" => left * right,
                    "/" => right != 0 ? left / right : 0,
                    "\\" => right != 0 ? left / right : 0,
                    "MOD" => right != 0 ? left % right : 0,
                    "<<" => left << (int)right,
                    ">>" => left >> (int)right,
                    _ => left
                };
            }
            else break;
        }
        return left;
    }

    private static long ParseUnary(List<PPToken> tokens, ref int pos)
    {
        if (pos < tokens.Count && tokens[pos].Type == PPTokenType.Operator)
        {
            var op = tokens[pos].Text;
            if (op is "-" or "NOT")
            {
                pos++;
                var val = ParseUnary(tokens, ref pos);
                return op == "-" ? -val : (val == 0 ? 1 : 0);
            }
        }
        return ParsePrimary(tokens, ref pos);
    }

    private static long ParsePrimary(List<PPToken> tokens, ref int pos)
    {
        if (pos >= tokens.Count) return 0;

        if (tokens[pos].Type == PPTokenType.Number)
        {
            var val = tokens[pos].NumValue;
            pos++;
            return val;
        }

        if (tokens[pos].Type == PPTokenType.LeftParen)
        {
            pos++;
            var val = ParseLogicalOr(tokens, ref pos);
            if (pos < tokens.Count && tokens[pos].Type == PPTokenType.RightParen)
                pos++;
            return val;
        }

        return 0;
    }

    private enum PPTokenType { Number, Operator, LeftParen, RightParen }

    private readonly struct PPToken(PPTokenType type, string text, long numValue)
    {
        public PPTokenType Type { get; } = type;
        public string Text { get; } = text;
        public long NumValue { get; } = numValue;
    }

    /// <summary>
    /// Tokenizza un'espressione del pre-processore in numeri, operatori, parentesi e simboli.
    /// I simboli noti (definiti con #DEF) vengono risolti al loro valore numerico.
    /// </summary>
    private static List<PPToken> TokenizePreprocessorExpr(string expr, IDefinesProvider program)
    {
        var tokens = new List<PPToken>();
        var i = 0;
        while (i < expr.Length)
        {
            if (char.IsWhiteSpace(expr[i])) { i++; continue; }

            if (expr[i] == '(') { tokens.Add(new PPToken(PPTokenType.LeftParen, "(", 0)); i++; continue; }
            if (expr[i] == ')') { tokens.Add(new PPToken(PPTokenType.RightParen, ")", 0)); i++; continue; }

            // Operatori multi-carattere
            if (i + 1 < expr.Length)
            {
                var twoChar = expr.Substring(i, 2);
                if (twoChar is "<=" or ">=" or "<>")
                {
                    tokens.Add(new PPToken(PPTokenType.Operator, twoChar, 0));
                    i += 2;
                    continue;
                }
                if (twoChar is "<<" or ">>")
                {
                    tokens.Add(new PPToken(PPTokenType.Operator, twoChar, 0));
                    i += 2;
                    continue;
                }
            }

            var ch = expr[i];
            if (ch is '+' or '-' or '*' or '/' or '\\' or '=' or '<' or '>')
            {
                tokens.Add(new PPToken(PPTokenType.Operator, ch.ToString(), 0));
                i++;
                continue;
            }

            // Numeri
            if (char.IsDigit(ch) || (ch == '-' && i + 1 < expr.Length && char.IsDigit(expr[i + 1])))
            {
                var start = i;
                if (ch == '-') i++;
                while (i < expr.Length && char.IsDigit(expr[i])) i++;
                if (long.TryParse(expr.Substring(start, i - start), out var num))
                {
                    tokens.Add(new PPToken(PPTokenType.Number, "", num));
                }
                continue;
            }

            // Identificatori (keyword operatori o simboli #DEF)
            if (char.IsLetter(ch) || ch == '_')
            {
                var start = i;
                while (i < expr.Length && (char.IsLetterOrDigit(expr[i]) || expr[i] == '_')) i++;
                var word = expr.Substring(start, i - start);

                // Keyword operatori
                var upper = word.ToUpperInvariant();
                if (upper is "AND" or "OR" or "NOT" or "XOR" or "MOD")
                {
                    tokens.Add(new PPToken(PPTokenType.Operator, upper, 0));
                }
                else if (program.IsDefined(word))
                {
                    var defValue = program.GetDefineValue(word);
                    if (defValue != null && long.TryParse(defValue, out var defNum))
                        tokens.Add(new PPToken(PPTokenType.Number, "", defNum));
                    else
                        tokens.Add(new PPToken(PPTokenType.Number, "", 1)); // simbolo definito = 1
                }
                else
                {
                    tokens.Add(new PPToken(PPTokenType.Number, "", 0)); // simbolo non definito = 0
                }
                continue;
            }

            i++;
        }
        return tokens;
    }
}