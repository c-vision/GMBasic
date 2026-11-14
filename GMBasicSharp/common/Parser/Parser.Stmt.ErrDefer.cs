// FILE: Parser.Stmt.ErrDefer.cs
//
// ERRDEFER <stmt>  —  cleanup eseguito SOLO se la procedura esce per un errore
// (equivalente di `errdefer` di Zig).
//
//   SUB F()
//     ERRDEFER CLOSE #1          ' corre solo se F esce per un errore
//     ...
//   END SUB
//
// SEMANTICA. Il cleanup corre quando la PROCEDURA esce perché un errore la attraversa:
//   • errore catturato da un TRY/CATCH *dentro* la procedura → la procedura non esce →
//     il cleanup NON corre (come `errdefer` di Zig, legato all'uscita dal blocco);
//   • errore che sfugge alla procedura → cleanup e RILANCIO dello stesso errore.
// Ordine LIFO, come per DEFER.
//
// IMPLEMENTAZIONE (zero lavoro nei backend). Come nel frontend C
// (gmb_parser_stmt_errdefer.c), il corpo della procedura viene avvolto in
//
//   TRY
//     <corpo senza i marcatori ERRDEFER>
//   CATCH e AS ERR
//     <cleanup, in ordine inverso>
//     THROW                     ' rilancia l'errore corrente
//   END TRY
//
// cioè costrutti che esistono già su tutte le gambe (TRY/CATCH tipizzato, THROW nudo).
// La direttiva è una SOFT keyword: `errdefer` resta un nome di variabile utilizzabile.

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    private const string ErrDeferKeyword = "ERRDEFER";

    /// <summary>Stack degli ERRDEFER della procedura in parsing (LIFO per annidamento).</summary>
    private readonly Stack<List<Stmt>> _errDeferScopes = new();

    /// <summary>Contatore per rendere UNIVOCO il nome del binding del CATCH implicito di
    /// ERRDEFER (`__gmb_ed_e_<n>`): con lo stesso nome in due procedure il Linker risolveva
    /// il secondo su un simbolo non dichiarato e la compilazione falliva.</summary>
    private int _errDeferUid;

    /// <summary>Profondità del corpo di un GENERATOR in parsing: `ERRDEFER` non è supportato
    /// lì (il desugaring produce un TRY/CATCH, che il corpo di un generatore non può
    /// contenere) e va rifiutato con un errore ESPLICITO, non con un messaggio generico.</summary>
    private int _generatorBodyDepth;

    private static bool AtErrDeferDirective(Token t) =>
        t.Kind == TokenKind.Identifier
        && string.Equals(t.Lexeme, ErrDeferKeyword, StringComparison.OrdinalIgnoreCase);

    /// <summary>
    /// Parsa `ERRDEFER &lt;stmt&gt;`: lo statement NON entra nel corpo, viene registrato per
    /// essere eseguito nel CATCH implicito della procedura. Ritorna una lista vuota.
    /// </summary>
    private IReadOnlyList<AstNode> ParseErrDeferStatement()
    {
        var kw = Advance();                       // ERRDEFER
        // GENERATOR: il desugaring produce un TRY/CATCH, che il corpo di un generatore non
        // può contenere (il backend C lo rifiuta e i gestiti perderebbero il cleanup).
        // Errore ESPLICITO e specifico, invece del generico "a livello di modulo".
        if (_generatorBodyDepth > 0)
            throw CreateError(kw,
                "ERRDEFER non è supportato dentro un GENERATOR: il cleanup sul percorso " +
                "d'errore richiede un TRY/CATCH, che il corpo di un generatore non può contenere.");
        if (_errDeferScopes.Count == 0)
            throw CreateError(kw,
                "ERRDEFER è valido solo dentro una procedura (SUB/FUNCTION): " +
                "a livello di modulo non c'è un'uscita da proteggere.");
        if (Peek().Kind == TokenKind.Newline)
            throw CreateError(kw, "ERRDEFER: attesa un'istruzione da eseguire (es. ERRDEFER CLOSE #1).");

        // l'istruzione può essere su più righe (blocco): si parsano gli stessi nodi di una
        // riga logica e si registrano tutti.
        var tmp = new List<Stmt>();
        ParseLogicalLine(tmp, new List<AstNode>(), null);
        var top = _errDeferScopes.Peek();
        top.AddRange(tmp);
        return Array.Empty<AstNode>();
    }

    /// <summary>
    /// Avvolge il corpo della procedura nel TRY/CATCH dei cleanup, se ci sono ERRDEFER.
    /// Va chiamata con la lista degli ERRDEFER della procedura già estratta dallo stack.
    /// </summary>
    private List<Stmt> WrapErrDefers(List<Stmt> body, List<Stmt> errDefers, Token token)
    {
        if (errDefers.Count == 0) return body;

        var catchBody = ImmutableArray.CreateBuilder<Stmt>();
        // LIFO: l'ultimo ERRDEFER registrato corre per primo
        for (int i = errDefers.Count - 1; i >= 0; i--) catchBody.Add(errDefers[i]);

        // RILANCIO: `ERROR e.Number, e.Description`. Non `THROW` nudo: sul PCode
        // `VisitRethrowStatement` emette OP_THROW senza push, quindi l'errore arrivava al
        // CATCH esterno come 256/"Unspecified" (numero e descrizione persi). La forma ERROR
        // con i membri dell'errore catturato è portabile e già a parità sulle 4 gambe.
        // Nome del binding UNIVOCO per procedura: con lo stesso nome in due SUB il Linker
        // risolveva il secondo `__gmb_ed_e` su un simbolo non dichiarato (`#-1`) e il
        // `FieldAccessExpr` falliva con "la base è un valore costante" (bug reale: due
        // procedure con ERRDEFER => errore di compilazione su IL/PCode).
        var errName = new Token(TokenKind.Identifier, $"__gmb_ed_e_{++_errDeferUid}", null, token.Position);
        var errVarExpr = new VariableExpr(-1, errName, errName);
        var numField = new Token(TokenKind.Identifier, "Number", null, token.Position);
        var descField = new Token(TokenKind.Identifier, "Description", null, token.Position);
        catchBody.Add(new ErrorStmt(
            new FieldAccessExpr(errVarExpr, numField),
            new FieldAccessExpr(errVarExpr, descField),
            token));

        var errVar = new VariableDeclaration(
            -1, errName, errName, ImmutableArray<Expr>.Empty, new ErrType(), null, false, true);

        var clause = new CatchClause(errVar, null, catchBody.ToImmutable(), token);
        var tryToken = new Token(TokenKind.Try, "TRY", null, token.Position);
        var tryStmt = new TryCatchStmt(
            [.. body], ImmutableArray.Create(clause), null, tryToken);
        return [tryStmt];
    }
}
