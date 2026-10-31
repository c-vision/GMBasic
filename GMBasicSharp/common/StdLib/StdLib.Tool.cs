// StdLib.Tool.cs — TOOL namespace per CUA (TOOL.SEARCH)

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Tool — namespace 'TOOL'
 * 2 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (2):
 *   Register
 *   Search
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateToolNamespace()
    {
        const string ns = "TOOL";
        var pub = SymbolFlags.IsPublic;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ToolSearch, "Search"), [P("query", false)], new StringType(), TokenKind.ToolSearch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ToolRegister, "Register"), [P("name", false), P("description", false)], new BitType(), TokenKind.ToolRegister, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
