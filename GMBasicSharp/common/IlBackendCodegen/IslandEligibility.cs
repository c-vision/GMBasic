// FILE: Basic.IlBackend.IslandEligibility.cs
// ISLAND ELIGIBILITY — selettore compile-time: decide se una procedura può essere
// eseguita nativamente come "isola" (leaf IL senza pre-empt) dal ponte GPM⇄IL.
//
// Una procedura è native-eligible SOLO se il suo corpo non contiene alcun
// punto di pre-empt (§16 del modello concorrente): un statement che possa
// parcheggiare il task (SEND/RECV/GO/GO SELECT/WAITGROUP/YIELD/SLEEP),
// che vada a I/O bloccante (console input, rete, HTTP) o che gestisca corsie
// di controllo non mappabili in IL. La decisione è STABILE a compile-time —
// mai runtime, mai euristica che cambia esito tra run diverse.

using System.Collections.Immutable;

namespace Basic.IlBackend;

/// <summary>
/// Predicato <see cref="IsNativeEligible(FunctionDefinition)"/>: cammina il corpo di una
/// procedura e ritorna false la prima volta che incontra uno statement non native-eligible
/// (un punto che, nell'esecuzione a isole, dovrebbe cedere alla VM). Funzione pura di
/// confine per il bridge: NON esegue IL, NON tocca il task.
/// </summary>
public static class IslandEligibility
{
    /// <summary>Ritorna true se il corpo della procedura non contiene alcun punto di pre-empt.</summary>
    public static bool IsNativeEligible(FunctionDefinition definition)
        => HasNoPreempt(definition.Body);

    /// <summary>True per una lista di statement senza punti di pre-empt, scendendo nei corpi annidati.</summary>
    public static bool HasNoPreempt(IEnumerable<Stmt> body)
    {
        foreach (var stmt in body)
        {
            if (!IsEligibleStmt(stmt)) return false;
        }
        return true;
    }

    /// <summary>Devia ogni statement: ritorna false appena un nodo rende la procedura NON island-eligible.</summary>
    private static bool IsEligibleStmt(Stmt stmt) => stmt switch
    {
        // --- Punti di pre-empt cooperativo (NON isola nella VM bytecode, MA consentiti
        //     nelle isole IL: IslandRuntime parka il task senza blocco OS).
        GoProcedureStmt or GoSelectStmt or GoExitStmt
            or YieldStmt
            => false,

        // SEND/RECEIVE, SLEEP, WAIT/DONE in modalità isola: usa IslandRuntime.

        // --- I/O bloccante / attesa utente (non isola).
        InputStmt or LineInputStmt or WaitKeyStmt or InkeyStmt or GetKeyboardStmt
            => false,

        // --- Rete / HTTP (operazioni bloccanti, pendono su completazioni asincrone).
        TcpConnectStmt or TcpAcceptStmt or TcpRecvStmt or TcpSendStmt
            or UdpRecvStmt or UdpSendStmt or HttpFetchStmt or WebSocketConnectStmt
            => false,

        // --- Contenitori di statements: scendi ricorsivamente.
        IfStmt ifStmt => HasNoPreempt(ifStmt.IfClauses.SelectMany(c => c.Statements)) &&
                         (ifStmt.ElseClause is null || HasNoPreempt(ifStmt.ElseClause.Statements)),
        SelectStmt selectStmt => HasNoPreempt(selectStmt.CaseClauses.SelectMany(c => c.Statements)) &&
                                 (selectStmt.ElseClause is null || HasNoPreempt(selectStmt.ElseClause.Statements)),
        MatchStmt matchStmt => HasNoPreempt(matchStmt.Cases.SelectMany(c => c.Statements)),
        WithStmt withStmt => HasNoPreempt(withStmt.Body),
        UsingStmt usingStmt => HasNoPreempt(usingStmt.Body),
        TryCatchStmt tryCatch => HasNoPreempt(tryCatch.TryBody) &&
                                 HasNoPreempt(tryCatch.CatchClauses.SelectMany(c => c.Body)) &&
                                 (tryCatch.Finally is null || HasNoPreempt(tryCatch.Finally.Body)),
        ForEachStmt forEach => HasNoPreempt(forEach.Body),
        LocalProcedureStmt local => HasNoPreempt(local.Procedure.Body),

        // --- Tutto il resto (statement "flat" che l'IL backend gestisce): consentito.
        _ => true,
    };
}