// FILE: Interpreter.StdLib.Memory.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Memory — namespace 'MEMORY'
 * 5 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (5):
 *   DELETE
 *   LIST
 *   LOAD
 *   SEARCH
 *   STORE
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateMemoryNamespace()
    {
        const string namespaceName = "MEMORY";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MemoryStore, "STORE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "value"), new AnyType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "longTerm"), new LongType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.MemoryStore),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MemoryLoad, "LOAD"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.MemoryLoad),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MemorySearch, "SEARCH"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "pattern"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.MemorySearch),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MemoryDelete, "DELETE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new StringType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.MemoryDelete),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MemoryList, "LIST"),
                [],
                new StringType(), TokenKind.MemoryList),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}