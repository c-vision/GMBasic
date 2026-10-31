// FILE: Interpreter.StdLib.Strings.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Strings — namespace 'STRINGS'
 * 48 funzioni | 0 comandi | 3 tipi | 0 costanti
 *
 * FUNZIONI (48):
 *   Asc
 *   Camel
 *   Chr
 *   Contains
 *   Dup
 *   EndsWith
 *   Format
 *   HTMLENCODE
 *   ISCREDITCARD
 *   ISEMAIL
 *   ISLENGTH
 *   ISREQUIRED
 *   ISURL
 *   Instr
 *   JSENCODE
 *   Join
 *   Kebab
 *   LCASE
 *   LTrim
 *   Left
 *   Len
 *   Lower
 *   Mid
 *   PadC
 *   PadL
 *   PadR
 *   Pascal
 *   Petscii
 *   RTrim
 *   Remove
 *   Repeat
 *   Replace
 *   Reverse
 *   Right
 *   Search
 *   Sentence
 *   Snake
 *   Space
 *   Split
 *   StartsWith
 *   Str
 *   Title
 *   Trim
 *   UCASE
 *   URLENCODE
 *   Upper
 *   WRAP
 * TIPI (3):
 *   RegexMatchType
 *   RegexType
 *   StringBuilderType
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- STRINGS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'STRINGS'.
    /// </summary>
    private void CreateStringsNamespace()
    {
        const string namespaceName = "STRINGS";
        var publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        var noFlags = SymbolFlags.None;
        var optionalFlag = SymbolFlags.IsOptional;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Left, "Left"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), noFlags, null, false)
            ], new StringType(), TokenKind.Left),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Right, "Right"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), noFlags, null, false)
            ], new StringType(), TokenKind.Right),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Mid, "Mid"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "start"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.Mid),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Len, "Len"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new InferredType(), noFlags, null, false)],
                new LongType(),
                TokenKind.Len),
            // LENB(s) — BYTE UTF-8 della stringa. LEN conta i CARATTERI (code point: "😀"=1),
            // LENB i BYTE della codifica ("😀"=4): servono entrambi (conteggio visibile vs
            // limiti di buffer/file/Content-Length). SIZEOF() NON è equivalente: sulle stringhe
            // vale -1 su tutte e 4 le gambe (verificato 2026-10-30).
            // Parametro a tipo INFERITO (come LEN) per la PARITÀ: con StringType un argomento
            // non-stringa dava 3 esiti diversi (errore C in compilazione, IL invalido, 0 su
            // VM/C). Ora ogni gamba restituisce -1 per un argomento non stringa.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LenB, "LenB"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new InferredType(), noFlags, null, false)],
                new LongType(),
                TokenKind.LenB),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Asc, "Asc"),
                [new Parameter(-1, T(TokenKind.Identifier, "character"), new StringType(), noFlags, null, false)],
                new LongType(),
                TokenKind.Asc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Str, "Str"),
                [new Parameter(-1, T(TokenKind.Identifier, "number"), new DoubleType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Str),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Chr, "Chr"),
                [new Parameter(-1, T(TokenKind.Identifier, "asciiCode"), new LongType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Chr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Instr, "Instr"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "haystack"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "needle"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "start"), new LongType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.Instr),
            // Overload Clipper: InStr(start, haystack, needle)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Instr, "Instr"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "start"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "haystack"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "needle"), new StringType(), noFlags, null, false)
            ], new LongType(), TokenKind.Instr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dup, "Dup"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "character"), new InferredType(), noFlags, null, false)
            ], new StringType(), TokenKind.Dup),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Space, "Space"),
                [new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Space),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Lower, "Lower"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Lower),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Upper, "Upper"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Upper),
            // Alias Clipper UCASE/LCASE: STESSA procedura di Upper/Lower, un solo nome in piu'.
            // Nessun TokenKind nuovo: una sola versione della funzione. Nella dichiarazione per
            // esteso ci sono DUE TokenKind diversi e non vanno confusi:
            //   - Name.Kind     = il TokenKind CANONICO della procedura: e' quello che il
            //                     generatore delle tabelle native (tools/registry-export) scrive
            //                     come token della grafia (`{ "UCASE", GMB_TOK_Upper }`), quindi
            //                     UCASE risolve sul token canonico, NON su un token proprio;
            //   - BuiltinKind   = la CHIAVE DI DISPATCH (ultimo argomento) su tutte le gambe.
            // (L'helper `T(TokenKind.X, "NOME")` nascondeva questa distinzione; la forma esplicita
            // no. Con Name.Kind = Identifier il generatore scriverebbe `GMB_TOK_Identifier`.)
            new NativeProcedureDefinition(
                SymbolId: -1,
                Flags: publicFlag,
                Name: new Token(TokenKind.Upper, "UCASE", null, SourcePosition.Unknown),
                Parameters: [new Parameter(-1, new Token(TokenKind.Identifier, "source", null, SourcePosition.Unknown), new StringType(), noFlags, null, false)],
                ReturnType: new StringType(),
                BuiltinKind: TokenKind.Upper),
            new NativeProcedureDefinition(
                SymbolId: -1,
                Flags: publicFlag,
                Name: new Token(TokenKind.Lower, "LCASE", null, SourcePosition.Unknown),
                Parameters: [new Parameter(-1, new Token(TokenKind.Identifier, "source", null, SourcePosition.Unknown), new StringType(), noFlags, null, false)],
                ReturnType: new StringType(),
                BuiltinKind: TokenKind.Lower),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Trim, "Trim"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Trim),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LTrim, "LTrim"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.LTrim),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RTrim, "RTrim"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.RTrim),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Reverse, "Reverse"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Reverse),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Title, "Title"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Title),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sentence, "Sentence"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Sentence),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Replace, "Replace"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "oldValue"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "newValue"), new StringType(), noFlags, null, false)
            ], new StringType(), TokenKind.Replace),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RepeatFunc, "Repeat"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "separator"), new StringType(), optionalFlag, null, false) 
            ], new StringType(), TokenKind.RepeatFunc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Pascal, "Pascal"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Pascal),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Camel, "Camel"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Camel),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Snake, "Snake"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Snake),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Kebab, "Kebab"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Kebab),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Wrap, "WRAP"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "lineLength"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "alignment"), new IntegerType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.Wrap),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Split, "Split"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "delimiter"), new StringType(), optionalFlag, null, false)
            ], new ArrayType(new StringType()), TokenKind.Split),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Join, "Join"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new ArrayType(new InferredType()), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "delimiter"), new StringType(), noFlags, null, false)
            ], new StringType(), TokenKind.Join),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Search, "Search"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pattern"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.Search),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Remove, "Remove"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "substringToRemove"), new StringType(), noFlags, null, false)
            ], new StringType(), TokenKind.Remove),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Petscii, "Petscii"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Petscii),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Contains, "Contains"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.Contains),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StartsWith, "StartsWith"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "prefix"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.StartsWith),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EndsWith, "EndsWith"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "suffix"), new StringType(), noFlags, null, false)
            ], new BitType(), TokenKind.EndsWith),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PadL, "PadL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "padChar"), new StringType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.PadL),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PadR, "PadR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "padChar"), new StringType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.PadR),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PadC, "PadC"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "length"), new LongType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "padChar"), new StringType(), optionalFlag, null, false)
            ], new StringType(), TokenKind.PadC),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Format, "Format"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "formatString"), new StringType(), noFlags, null, false)
            ], new StringType(), TokenKind.Format),
            // --- Security encoding (2026-09-02, ADR Security-Encoding-Namespace) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HtmlEncode, "HTMLENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.HtmlEncode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UrlEncode, "URLENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.UrlEncode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JsEncode, "JSENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.JsEncode),
            // --- Validation (2026-09-02): core condiviso CommonLib.SecurityValidation / gmb_validation_* ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsRequired, "ISREQUIRED"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.IsRequired, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsEmail, "ISEMAIL"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.IsEmail, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsUrl, "ISURL"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.IsUrl, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsCreditCard, "ISCREDITCARD"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.IsCreditCard, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsLength, "ISLENGTH"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "value"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "minLength"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "maxLength"), new LongType(), noFlags, null, false)
                ], new BitType(), TokenKind.IsLength, ExecutionKind: NativeExecutionKind.Pure)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "STRINGBUILDER", new StringBuilderType() },
            { "REGEX", new RegexType() },
            { "REGEXMATCH", new RegexMatchType() }
        }.ToImmutableDictionary(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);

        // --- SCHEMI PER TIPI NATIVI STRINGA ---
        var voidType = new VoidType();
        var longType = new LongType();
        var bitType = new BitType();
        var stringType = new StringType();
        var inferred = new InferredType();

        RegisterTypeSchema(typeof(StringBuilderType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Length", longType },
                { "Capacity", longType }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Append", voidType },
                { "AppendLine", voidType },
                { "AppendFormat", voidType },
                { "Insert", voidType },
                { "Replace", voidType },
                { "Remove", voidType },
                { "Clear", voidType },
                { "ToString", stringType }
            });

        RegisterTypeSchema(typeof(RegexType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Pattern", stringType }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "IsMatch", bitType },
                { "Match", new RegexMatchType() },
                { "Matches", new ListType(inferred) },
                { "Replace", stringType },
                { "Split", new ArrayType(stringType) }
            });

        RegisterTypeSchema(typeof(RegexMatchType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Value", stringType },
                { "Index", longType },
                { "Length", longType },
                { "Success", bitType }
            },
            null);
    }
}