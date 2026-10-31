// FILE: Interpreter.StdLib.Network.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Network — namespace 'NET'
 * 8 funzioni | 2 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (8):
 *   CONN
 *   GETHOSTIP
 *   LOCALHOST
 *   LOCALPORT
 *   PEEK
 *   RECV
 *   REMOTEIP
 *   REMOTEPORT
 * COMANDI (2):
 *   SETOPTION
 *   SHUTDOWN
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- NET NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'NET'.
    /// QUESTA VERSIONE È CORRETTA e segue il pattern dei dispatcher.
    /// </summary>
    private void CreateNetNamespace()
    {
        const string namespaceName = "NET";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        RegisterCommand(TokenKind.Shutdown, "SHUTDOWN", namespaceName, Parser.ParseShutdownStatement);
        RegisterCommand(TokenKind.SetOption, "SETOPTION", namespaceName, Parser.ParseSetOptionStatement);




        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Conn, "CONN"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new BitType(), TokenKind.Conn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Recv, "RECV"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.Recv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RemoteIp, "REMOTEIP"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new StringType(), TokenKind.RemoteIp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RemotePort, "REMOTEPORT"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.RemotePort),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LocalPort, "LOCALPORT"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.LocalPort),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Host, "LOCALHOST"), [],
                new StringType(), TokenKind.Host),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GetHostIp, "GETHOSTIP"),
                [new Parameter(-1, T(TokenKind.Identifier, "hostname"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.GetHostIp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Peek, "PEEK"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "numBytes"), new LongType(), optionalFlag, null, false)
                ],
                new StringType(), TokenKind.Peek),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}