// FILE: Interpreter.Compiler.Compiler.Scope.Dynamic.cs
// =========================================================================================
// SCOPE DINAMICO (DYNAMIC) — pre-scan dei nomi + emissione dei builtin OP_DYN_*.
//
// La visibilità di una variabile DYNAMIC segue la CATENA DI CHIAMATA a runtime, non lo
// scope lessicale (spec Variables-Scope §2.5, caso Clipper/xBase). Una lettura/scrittura "a
// cascata" (una procedura che usa un nome dichiarato DYNAMIC da un CHIAMANTE, senza averlo
// dichiarato) deve quindi risolversi sullo scope dinamico: l'indicizzazione è per NOME, non
// per simbolo (stessa scelta dei backend C99/IL).
//
// Il runtime (P-Code VM e VM/C) tiene le dichiarazioni sui frame DI CHIAMATA: la catena dei
// frame È la catena dello scope dinamico, quindi alla pop del frame le dichiarazioni
// spariscono da sole (ripristino automatico) — nessun push/pop esplicito nel bytecode.
// File di dominio (AGENTS §0quater): non appesantisce il Compiler principale.
// =========================================================================================

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Nomi dichiarati DYNAMIC nel programma (nome normalizzato → true se STRING),
    /// pre-scansionati una volta sola. Vedi <see cref="CollectDynamicNames"/>.</summary>
    private readonly Dictionary<string, bool> _dynamicNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Vero se il nome è una variabile DYNAMIC dichiarata ovunque nel programma.</summary>
    private bool IsDynamicName(string normalizedName) => _dynamicNames.ContainsKey(normalizedName);

    /// <summary>Vero se l'accesso al nome va instradato sullo scope dinamico: è un nome DYNAMIC
    /// e NON è ombreggiato da un upvalue/locale/globale genuino della procedura corrente (in tal
    /// caso vince quello, parità con C99CodeEmitter.TryEmitDynamicRead).</summary>
    private bool ShouldUseDynamicScope(string normalizedName)
        => IsDynamicName(normalizedName)
            && ResolveUpvalueByName(normalizedName) == -1
            && ResolveLocal(normalizedName) == -1
            && !GlobalSlots.ContainsKey(normalizedName);

    /// <summary>Pre-scan di tutte le dichiarazioni DYNAMIC: procedure, LOCAL SUB/FUNCTION e corpo
    /// del modulo principale. Da chiamare una volta a inizio compilazione (ordine di
    /// compilazione indipendente: una lettura a cascata può precedere la dichiarazione).</summary>
    private void CollectDynamicNames(ICompilationContext ctx)
    {
        _dynamicNames.Clear();

        void ScanStmts(IEnumerable<Stmt>? stmts)
        {
            if (stmts is null) return;
            foreach (var s in stmts)
            {
                switch (s)
                {
                    case DeclarationStmt ds when ds.Keyword.Kind == TokenKind.Dynamic:
                        foreach (var dv in ds.Declarations)
                        {
                            var nm = NormalizeIdentifierName(dv.NameCleaned.Lexeme);
                            var isStr = dv.DataType is StringType || dv.InitialValue is StringExpr;
                            _dynamicNames[nm] = _dynamicNames.TryGetValue(nm, out var prev) ? prev || isStr : isStr;
                        }
                        break;
                    case IfStmt isr:
                        foreach (var c in isr.IfClauses) ScanStmts(c.Statements);
                        ScanStmts(isr.ElseClause?.Statements);
                        break;
                    case SelectStmt selr:
                        foreach (var c in selr.CaseClauses) ScanStmts(c.Statements);
                        ScanStmts(selr.ElseClause?.Statements);
                        break;
                    case MatchStmt mcr:
                        foreach (var c in mcr.Cases) ScanStmts(c.Statements);
                        break;
                    case WithStmt wsr: ScanStmts(wsr.Body); break;
                    case UsingStmt usr: ScanStmts(usr.Body); break;
                    case TryCatchStmt tcr:
                        ScanStmts(tcr.TryBody);
                        foreach (var c in tcr.CatchClauses) ScanStmts(c.Body);
                        ScanStmts(tcr.Finally?.Body);
                        break;
                    case ForEachStmt fer: ScanStmts(fer.Body); break;
                    case LocalProcedureStmt lpr: ScanStmts(lpr.Procedure.Body); break;
                }
            }
        }

        foreach (var proc in ctx.GetAllProcedures()) ScanStmts(proc.Body);
        if (ctx.MainModule != null)
            foreach (var (_, stmts) in ctx.MainModule.Statements)
                ScanStmts(stmts);
    }

    /// <summary>Dichiara il nome DYNAMIC nel frame corrente (valore già in cima allo stack).</summary>
    private void EmitDynamicDeclare(string lexeme)
    {
        EmitByte((byte)OpCode.OP_DYN_DECLARE);
        EmitShort((ushort)AddConstant(NormalizeIdentifierName(lexeme)));
    }

    /// <summary>Legge il nome DYNAMIC dalla catena dei frame di chiamata (default "" / 0.0 quando
    /// assente, parità con gmb_dscope_get_string/get_double).</summary>
    private void EmitDynamicLoad(string lexeme)
    {
        var nm = NormalizeIdentifierName(lexeme);
        EmitByte((byte)OpCode.OP_DYN_LOAD);
        EmitShort((ushort)AddConstant(nm));
        EmitByte((byte)(_dynamicNames.TryGetValue(nm, out var isStr) && isStr ? 1 : 0));
    }

    /// <summary>Scrive il nome DYNAMIC (valore già in cima allo stack) nel frame più interno che
    /// lo definisce, altrimenti nel frame corrente.</summary>
    private void EmitDynamicStore(string lexeme)
    {
        EmitByte((byte)OpCode.OP_DYN_STORE);
        EmitShort((ushort)AddConstant(NormalizeIdentifierName(lexeme)));
    }
}
