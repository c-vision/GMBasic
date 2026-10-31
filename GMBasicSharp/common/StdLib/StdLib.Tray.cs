// FILE: StdLib.Tray.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Tray — namespace 'TRAY'
 * 10 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (10):
 *   TRAYAVAILABLE
 *   TRAYBACKEND
 *   TRAYCREATE
 *   TRAYITEM
 *   TRAYLAST
 *   TRAYPOLL
 *   TRAYREMOVE
 *   TRAYSELFTEST
 *   TRAYSEP
 *   TRAYTOOLTIP
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRAY NAMESPACE (icona nella barra del SO) ---
    // Implementazione UNICA nella libreria C del runtime condiviso
    // (native/gmb_runtime_c/src/gmb_tray*.c): macOS NSStatusItem, Windows
    // Shell_NotifyIcon, Linux SNI (no-op finché non implementato).
    // I backend C la chiamano direttamente; IL/PCodeVM via P/Invoke
    // (CommonLib/TrayUtils.cs). Se il bridge non è disponibile → no-op con
    // TRAYAVAILABLE()=0 e TRAYBACKEND()="none" (nessun errore).
    // Main thread; gli handler degli item vanno dispatchati come task GPM.
    // ========================================================================

    private void CreateTrayNamespace()
    {
        const string namespaceName = "TRAY";
        const SymbolFlags pub = SymbolFlags.IsPublic;

        var iconParam = new Parameter(-1, T(TokenKind.Identifier, "icon"), new StringType(), SymbolFlags.None, null, false);
        var tipParam = new Parameter(-1, T(TokenKind.Identifier, "tooltip"), new StringType(), SymbolFlags.None, null, false);
        var idParam = new Parameter(-1, T(TokenKind.Identifier, "id"), new LongType(), SymbolFlags.None, null, false);
        var labelParam = new Parameter(-1, T(TokenKind.Identifier, "label"), new StringType(), SymbolFlags.None, null, false);
        var textParam = new Parameter(-1, T(TokenKind.Identifier, "text"), new StringType(), SymbolFlags.None, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayCreate, "TRAYCREATE"),
                [iconParam, tipParam], new LongType(), TokenKind.TrayCreate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayItem, "TRAYITEM"),
                [idParam, labelParam], new LongType(), TokenKind.TrayItem, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TraySep, "TRAYSEP"),
                [], new LongType(), TokenKind.TraySep, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayTooltip, "TRAYTOOLTIP"),
                [textParam], new LongType(), TokenKind.TrayTooltip, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayPoll, "TRAYPOLL"),
                [], new LongType(), TokenKind.TrayPoll, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayRemove, "TRAYREMOVE"),
                [], new LongType(), TokenKind.TrayRemove, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayBackend, "TRAYBACKEND"),
                [], new StringType(), TokenKind.TrayBackend, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TraySelfTest, "TRAYSELFTEST"),
                [], new LongType(), TokenKind.TraySelfTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayAvailable, "TRAYAVAILABLE"),
                [], new LongType(), TokenKind.TrayAvailable, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TrayLast, "TRAYLAST"),
                [], new LongType(), TokenKind.TrayLast, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
