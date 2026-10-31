// FILE: Interpreter.StdLib.Concurrency.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Concurrency — namespace 'CONCURRENCY'
 * 3 funzioni | 11 comandi | 3 tipi | 0 costanti
 *
 * FUNZIONI (3):
 *   SleepAsync
 *   TRYLOCK
 *   TaskYield
 * COMANDI (11):
 *   CHANNEL
 *   COND
 *   DEFER
 *   DONE
 *   GO
 *   MUTEX
 *   ONCE
 *   RECEIVE
 *   SEND
 *   SLEEP
 *   WAIT
 * TIPI (3):
 *   CHANNEL
 *   ONCE
 *   WAITGROUP
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CONCURRENCY NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'CONCURRENCY'.
    /// </summary>
    private void CreateConcurrencyNamespace()
    {
        const string namespaceName = "CONCURRENCY";

        // --- Registrazione dei Comandi (Legacy e Qualificata) ---
        RegisterCommand(TokenKind.Go, "GO", namespaceName, Parser.ParseGoStatement);
        // GO non ha una registrazione qualificata come comando puro (solo GO FUNCTION come espressione)

        RegisterCommand(TokenKind.Wait, "WAIT", namespaceName, Parser.ParseWaitStatement);

        RegisterCommand(TokenKind.Done, "DONE", namespaceName, Parser.ParseDoneStatement);

        RegisterCommand(TokenKind.Send, "SEND", namespaceName, Parser.ParseSendStatement);

        RegisterCommand(TokenKind.Receive, "RECEIVE", namespaceName, Parser.ParseReceiveStatement);

        RegisterCommand(TokenKind.Channel, "CHANNEL", namespaceName, Parser.ParseChannelStatement);

        RegisterCommand(TokenKind.Sleep, "SLEEP", namespaceName, Parser.ParseSleepStatement);

        RegisterCommand(TokenKind.Defer, "DEFER", namespaceName, Parser.ParseDeferStatement);
        RegisterCommand(TokenKind.Mutex, "MUTEX", namespaceName, Parser.ParseMutexStatement);
        // MUTEX.TRYLOCK(mu) → BIT (parità Go mu.TryLock(), Go 1.18) — funzione nel namespace MUTEX.
        RegisterBuiltinNamespace("MUTEX",
            [new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MutexTryLock, "TRYLOCK"),
                [new Parameter(-1, T(TokenKind.Identifier, "mu"), new MutexType(), SymbolFlags.None, null, false)],
                new BitType(), TokenKind.MutexTryLock, false)],
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase));
        RegisterCommand(TokenKind.Cond, "COND", namespaceName, Parser.ParseCondStatement);
        RegisterCommand(TokenKind.Once, "ONCE", namespaceName, Parser.ParseOnceStatement);

        // Funzioni async
        var sleepAsyncParam = new Parameter(-1, T(TokenKind.Identifier, "ms"), new IntegerType(),
            SymbolFlags.None, null, false);
        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Sleep, "SleepAsync"),
                ImmutableArray.Create(sleepAsyncParam), new StringType(), TokenKind.Sleep, false),
            // TaskYield: funzione VOID usata come `AWAIT TaskYield()` (ConcurrencyAsync.bas) —
            // DISTINTA dal comando statement YIELD nei generatori.
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Yield, "TaskYield"),
                ImmutableArray<Parameter>.Empty, new VoidType(), TokenKind.Yield, true)
        );

        // Registrazione dei tipi di dato del namespace
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "WAITGROUP", new WaitGroupType() },
            { "CHANNEL", new ChannelType(new InferredType()) },
            { "ONCE", new OnceType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}