// StdLib.Computer.cs — COMPUTER namespace (L0 Computer Use cross-OS, CUA §4)
// Implementazione reale nel ComputerProvider C (gmb_agent_runtime); la VM/C dispatcha
// via builtin 1381-1386. Backend IL/PCode: errore esplicito (nessuna logica duplicata).

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Computer — namespace 'COMPUTER'
 * 12 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (12):
 *   Click
 *   DoubleClick
 *   HotKey
 *   Key
 *   Move
 *   ScreenRecord
 *   ScreenRecordStart
 *   ScreenRecordStatus
 *   ScreenRecordStop
 *   Screenshot
 *   Type
 *   Windows
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateComputerNamespace()
    {
        const string ns = "COMPUTER";
        var pub = SymbolFlags.IsPublic;
        var xP = new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), SymbolFlags.None, null, false);
        var yP = new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), SymbolFlags.None, null, false);
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerScreenshot, "Screenshot"), [P("path", true)], new StringType(), TokenKind.ComputerScreenshot, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerClick, "Click"), [xP, yP], new DoubleType(), TokenKind.ComputerClick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerDoubleClick, "DoubleClick"), [xP, yP], new DoubleType(), TokenKind.ComputerDoubleClick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerHotKey, "HotKey"), [P("combo", false)], new DoubleType(), TokenKind.ComputerHotKey, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerMove, "Move"), [xP, yP], new DoubleType(), TokenKind.ComputerMove, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerType, "Type"), [P("text", false)], new DoubleType(), TokenKind.ComputerType, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerKey, "Key"), [P("keys", false)], new DoubleType(), TokenKind.ComputerKey, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerWindows, "Windows"), [], new StringType(), TokenKind.ComputerWindows, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerScreenRecord, "ScreenRecord"), [P("path", true), P("fps", true)], new StringType(), TokenKind.ComputerScreenRecord, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerScreenRecordStart, "ScreenRecordStart"), [P("path", false), P("fps", true)], new StringType(), TokenKind.ComputerScreenRecordStart, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerScreenRecordStop, "ScreenRecordStop"), [], new StringType(), TokenKind.ComputerScreenRecordStop, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ComputerScreenRecordStatus, "ScreenRecordStatus"), [], new StringType(), TokenKind.ComputerScreenRecordStatus, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
