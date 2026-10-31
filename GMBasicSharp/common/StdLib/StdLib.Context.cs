// FILE: Interpreter.StdLib.Context.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Context — namespace 'CONTEXT'
 * 5 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (5):
 *   BACKGROUND
 *   CANCEL
 *   CANCELLED
 *   DONE
 *   WITHCANCEL
 * TIPI (1):
 *   CONTEXT
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CONTEXT NAMESPACE (parità Go context.Context) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra il namespace 'CONTEXT' con la cancellazione propagata (parità Go
    /// context): BACKGROUND (radice), WITHCANCEL (figlio derivato), CANCEL (propagata ai
    /// figli), DONE (canale chiuso alla cancellazione), CANCELLED (BIT).
    /// </summary>
    private void CreateContextNamespace()
    {
        const string namespaceName = "CONTEXT";
        var ctxParam = new Parameter(-1, T(TokenKind.Identifier, "ctx"), new ContextType(),
            SymbolFlags.None, null, false);
        var parentParam = new Parameter(-1, T(TokenKind.Identifier, "parent"), new ContextType(),
            SymbolFlags.None, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // BACKGROUND() → nuovo contesto radice (parità Go context.Background()).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.ContextBackground, "BACKGROUND"),
                ImmutableArray<Parameter>.Empty, new ContextType(), TokenKind.ContextBackground, false),
            // WITHCANCEL(parent) → nuovo contesto figlio (parità Go context.WithCancel).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.ContextWithCancel, "WITHCANCEL"),
                ImmutableArray.Create(parentParam), new ContextType(), TokenKind.ContextWithCancel, false),
            // CANCEL(ctx) → cancella ctx e propaga ai figli (parità Go cancel()).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.ContextCancel, "CANCEL"),
                ImmutableArray.Create(ctxParam), new VoidType(), TokenKind.ContextCancel, true),
            // DONE(ctx) → canale chiuso alla cancellazione (parità Go ctx.Done()).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.ContextDone, "DONE"),
                ImmutableArray.Create(ctxParam), new ChannelType(new InferredType()), TokenKind.ContextDone, false),
            // CANCELLED(ctx) → BIT (parità Go ctx.Err() != nil).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.ContextCancelled, "CANCELLED"),
                ImmutableArray.Create(ctxParam), new BitType(), TokenKind.ContextCancelled, false)
        );

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "CONTEXT", new ContextType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
