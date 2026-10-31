// FILE: StdLib.Grep.cs
// Namespace GREP — ricerca file (motore C condiviso gmb_grep, non reimplementato).
// GREP.MATCHES/COUNT/FILES: funzioni headless che ritornano testo/numero.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Grep — namespace 'GREP'
 * 3 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (3):
 *   COUNT
 *   FILES
 *   MATCHES
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateGrepNamespace()
    {
        const string namespaceName = "GREP";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        var procedures = new ProcedureDefinition[]
        {
            // GREP.MATCHES(pattern$, path$) -> ARRAY di stringhe ("file:line:col:text" per hit)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GrepMatches, "MATCHES"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "pattern"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
                ],
                new ArrayType(new StringType()), TokenKind.GrepMatches),

            // GREP.COUNT(pattern$, path$) -> LONG
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GrepCount, "COUNT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "pattern"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.GrepCount),

            // GREP.FILES(pattern$, path$) -> ARRAY di stringhe (nomi file distinti)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.GrepFiles, "FILES"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "pattern"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
                ],
                new ArrayType(new StringType()), TokenKind.GrepFiles),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
