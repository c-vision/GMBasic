// StdLib.Ui.cs — UI namespace per CUA (UI.FIND/CLICK)

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Ui — namespace 'UI'
 * 4 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   Click
 *   Find
 *   FindAll
 *   Type
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateUiNamespace()
    {
        const string ns = "UI";
        var pub = SymbolFlags.IsPublic;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.UiFind, "Find"), [P("target", false)], new StringType(), TokenKind.UiFind, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.UiFindAll, "FindAll"), [P("target", false)], new ArrayType(new StringType()), TokenKind.UiFindAll, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.UiClick, "Click"), [P("target", false)], new BitType(), TokenKind.UiClick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.UiType, "Type"), [P("target", false), P("text", false)], new BitType(), TokenKind.UiType, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
