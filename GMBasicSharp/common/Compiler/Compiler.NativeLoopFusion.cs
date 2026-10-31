// FILE: Interpreter.Compiler.Compiler.NativeLoopFusion.cs
//
// AMMISSIBILITÀ delle FUSIONI NATIVE che eseguono l'INTERO corpo di un FOR in un solo opcode:
//   * OP_NATIVE_LOOP_ACC           → `acc = acc +/-/*// f(I) [* m]`   (Compiler.Assign.cs)
//   * OP_NATIVE_STRING_APPEND_LOOP → `s$ = s$ + termine`              (Compiler.Assign.cs)
//
// PERCHÉ SERVE QUESTO FILTRO (bug reale, `Bugs/(open) - PCode-For-In-Function-Corpo-Loop.md`):
// quei due opcode NON eseguono "una iterazione ottimizzata", eseguono TUTTE le iterazioni del
// FOR da soli (fino a esaurire il range) e poi CONSUMANO lo stato del ciclo
// (ForState/for_active): sono quindi corretti SOLO se l'assegnazione È L'INTERO corpo del
// ciclo. Con anche una sola altra istruzione nel corpo il guasto è silenzioso e identico su
// PCodeVM e VM/C (stesso chunk, stesso opcode):
//     FOR s = 0 TO 3
//       PRINT "cpos"; s          ' eseguito UNA volta invece di 4
//       out$ = out$ + "x"        ' fuso → l'intero ciclo gira qui, contatore al valore finale
//     NEXT                       ' OP_FOR_LOOP: nessuno stato → il ciclo è già finito
// Sintomo: una sola riga stampata (`cpos 0`) e contatore che salta al valore finale (`s= 4`),
// mentre IL (che non ha questa fusione) stampa correttamente 4 righe. Nessun errore a runtime:
// risultato sbagliato in silenzio, quindi il filtro è OBBLIGATORIO, non un'ottimizzazione.
//
// COME: il parser abbassa `FOR...NEXT` in una lista PIATTA di statement
// (EnterLoopStmt → ForStmt → Label(FOR_START) → ForCheckAndJumpStmt → corpo →
//  Label(FOR_CONTINUE) → NextStmt → JumpStmt → ExitLoopStmt → Label(FOR_EXIT)), quindi il corpo
// non è più un contenitore AST: l'unico modo di sapere se l'assegnazione è l'unico statement
// del corpo è scorrere la lista FLAT prima dell'emissione (stesso schema del pre-scan C99:
// C99CodeEmitter.StrAcc.cs, che è già corretto per corpi misti). Qui
// si registra per IDENTITÀ di nodo AST l'assegnazione candidata; la fusione è ammessa SOLO per
// quelle.
//
// DIREZIONE CONSERVATIVA: una lista non pre-scansionata (o un corpo misto) NON viene fusa — si
// perde solo l'ottimizzazione, mai la correttezza: si ricade nella emissione normale
// (LOAD/ADD/STORE per iterazione), che è la semantica di riferimento.
//
// NOTA: le due TryEmit* restano additive e non toccano in alcun modo il lowering del FOR né
// OP_FOR_PREPARE/OP_FOR_LOOP.

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Assegnazioni che costituiscono l'INTERO corpo del proprio FOR (identità di nodo
    /// AST). Popolato da <see cref="PreScanWholeBodyNativeFusions"/>; letto da
    /// <see cref="IsWholeBodyNativeFusion"/>. Vedi intestazione del file per il motivo.</summary>
    private readonly HashSet<Assignment> _wholeBodyNativeFusions =
        new(ReferenceEqualityComparer.Instance);

    /// <summary>True se questa assegnazione è l'unico statement del corpo del proprio FOR, quindi
    /// le fusioni "tutto il ciclo in un opcode" non perdono l'esecuzione delle altre istruzioni
    /// del corpo (non ce ne sono). False = emissione normale (semantica di riferimento).</summary>
    private bool IsWholeBodyNativeFusion(Assignment assignment) =>
        _wholeBodyNativeFusions.Contains(assignment);

    /// <summary>
    /// Registra le assegnazioni ammesse alle fusioni native "tutto il ciclo in un opcode"
    /// analizzando la lista FLAT di statement NELL'ORDINE DI EMISSIONE (deve contenere il
    /// lowering completo di ogni FOR, come prodotto dal parser). ADDITIVO: se non riconosce
    /// nulla non registra nulla (nessuna fusione, emissione normale).
    /// </summary>
    private void PreScanWholeBodyNativeFusions(IReadOnlyList<Stmt> stmts)
    {
        for (var i = 0; i < stmts.Count; i++)
        {
            if (stmts[i] is not ForStmt f) continue;

            // Forma lowerata: [Label*] ForCheckAndJumpStmt subito dopo il ForStmt.
            var j = i + 1;
            while (j < stmts.Count && stmts[j] is LabelStmt) j++;
            if (j >= stmts.Count || stmts[j] is not ForCheckAndJumpStmt chk) continue;
            if (!chk.Variable.Lexeme.Equals(f.Variable.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase))
                continue;

            // NextStmt della STESSA variabile a profondità 0 (i FOR annidati si contano e
            // squalificano il candidato: il loro lowering sta dentro il corpo).
            var depth = 0;
            var nextIdx = -1;
            for (var k = j + 1; k < stmts.Count; k++)
            {
                if (stmts[k] is ForStmt) { depth++; continue; }
                if (stmts[k] is NextStmt nx)
                {
                    if (depth == 0)
                    {
                        if (nx.Variable.Lexeme.Equals(f.Variable.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase))
                            nextIdx = k;
                        break;
                    }
                    depth--;
                }
            }
            if (nextIdx < 0) continue;

            // Corpo: dal check (escluso) al NextStmt (escluso). Il LabelStmt FOR_CONTINUE emesso
            // dal parser PRECEDE immediatamente NEXT: è scaffolding del lowering, non
            // un'istruzione del corpo.
            var last = nextIdx - 1;
            if (last >= 0 && stmts[last] is LabelStmt) last--;

            // Deve restare UNA sola istruzione reale. I RemStmt (commenti) non emettono codice:
            // possono coesistere senza invalidare la fusione. Qualunque altra cosa — LabelStmt
            // utente, salti, DIM, PRINT — squalifica il candidato (direzione conservativa).
            var realCount = 0;
            LetStmt? onlyLet = null;
            for (var m = j + 1; m <= last; m++)
            {
                if (stmts[m] is RemStmt) continue;
                realCount++;
                if (onlyLet is null && stmts[m] is LetStmt ls) onlyLet = ls;
            }
            if (realCount != 1 || onlyLet is null || onlyLet.Assignments.Length != 1) continue;

            _wholeBodyNativeFusions.Add(onlyLet.Assignments[0]);
        }
    }
}
