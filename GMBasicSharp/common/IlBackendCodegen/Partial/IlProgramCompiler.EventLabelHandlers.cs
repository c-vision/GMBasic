// FILE: IlProgramCompiler.EventLabelHandlers.cs
//
// Handler evento (`ON WINDOW …` / `ON <ev> CONTROL …`) il cui target è un LABEL:
//  - label di MODULO (programma ibrido, label fuori dalle procedure)
//  - label LOCALE a una procedura (`FinestraChiusa:` dentro `SUB Main`)
//
// Il runtime registra un handler per SymbolId e lo invoca come metodo statico senza
// contesto (`IlRuntime.RegisterWindowHandlerBySymbol` → `Proc_<id>`): una label non è un
// metodo, quindi qui si SINTETIZZA un `FunctionDefinition` (void, nessun parametro) con il
// blocco della label, esattamente come fa l'emitter C99 nativo con `gmb_user_lfnev_<fn>_<label>`
// e come fa la VM/C spawnando un task all'indirizzo del label.
//
// Semantica delle variabili (identica sulle 4 gambe): il blocco vede i GLOBALI di modulo;
// una variabile che nella procedura era locale diventa una locale FRESCA (azzerata) del
// metodo sintetico, perché l'evento arriva senza frame del chiamante.
//
// Gli id dei metodi sintetici sono NEGATIVI e fuori dal range del SymbolTable: nessun'altra
// parte del compilatore li risolve nella SymbolTable (i controlli su `_currentReturnSymbolId`
// sono `>= 0`, quindi il return-slot void è correttamente assente).

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    /// <summary>Blocco di statement che segue la label <paramref name="label"/> (fino alla
    /// label successiva, a END SUB o a un RETURN senza valore — il RETURN è incluso, come
    /// nel taglio dell'emitter C99). null se la label non esiste in questo corpo.</summary>
    private static ImmutableArray<Stmt>? SliceLabelBlock(ImmutableArray<Stmt> body, string label)
    {
        var start = -1;
        for (var i = 0; i < body.Length; i++)
            if (body[i] is LabelStmt ls && string.Equals(ls.Label.Lexeme, label, StringComparison.OrdinalIgnoreCase))
            { start = i; break; }
        if (start < 0) return null;

        var block = ImmutableArray.CreateBuilder<Stmt>();
        for (var i = start + 1; i < body.Length; i++)
        {
            var stmt = body[i];
            if (stmt is LabelStmt) break;
            block.Add((Stmt)stmt.DeepClone());
            if (stmt is EndProcedureStmt) break;
            if (stmt is ReturnStmt rs && rs.Value is null) break;
        }
        return block.ToImmutable();
    }

    /// <summary>Nome della label usata come handler dallo statement evento, o null.</summary>
    private static string? EventHandlerLabel(Stmt stmt) => stmt switch
    {
        OnWindowStmt { Action: GosubAction g } => g.TargetLabel.Lexeme,
        OnWindowStmt { Action: GotoAction g2 } => g2.TargetLabel.Lexeme,
        ControlEventStmt ce when (ce.ActionKind ?? "").ToUpperInvariant() is "GOSUB" or "GOTO"
            => string.IsNullOrWhiteSpace(ce.Target) ? null : ce.Target,
        _ => null
    };

    /// <summary>Aggiunge alle definizioni di procedura i metodi sintetici per gli handler
    /// evento che puntano a una label (vedi commento di file). Idempotente: se non ci sono
    /// handler con label restituisce l'array originale.</summary>
    internal static ImmutableArray<FunctionDefinition> AddEventLabelHandlerDefinitions(
        BasicProgram program, ImmutableArray<FunctionDefinition> definitions)
    {
        var procedureNames = new HashSet<string>(definitions.Select(d => d.OriginalName.Lexeme),
            StringComparer.OrdinalIgnoreCase);
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var added = new List<FunctionDefinition>();
        var nextId = -1000000;

        void Scan(ImmutableArray<Stmt> body)
        {
            if (body.IsDefaultOrEmpty) return;
            for (var i = 0; i < body.Length; i++)
            {
                var stmt = body[i];
                var label = EventHandlerLabel(stmt);
                if (label is null) continue;
                // SUB/FUNCTION omonima: la risoluzione per nome esistente la trova già.
                if (procedureNames.Contains(label)) continue;
                if (!seen.Add(label)) continue;
                var block = SliceLabelBlock(body, label);
                if (block is null) continue;

                var labelToken = new Token(TokenKind.Identifier, label, null, stmt.Position);
                var methodToken = new Token(TokenKind.Identifier,
                    "__EVLBL_" + label.Replace("$", "").Replace("%", "") + "_" + (-nextId), null, stmt.Position);
                var bodyWithExit = block.Value.Add(new EndProcedureStmt(labelToken));
                added.Add(new FunctionDefinition(
                    nextId--,
                    methodToken,
                    ImmutableArray<Parameter>.Empty,
                    new VoidType(),
                    bodyWithExit,
                    SymbolFlags.IsPublic | SymbolFlags.IsSynthetic)
                { OriginalName = labelToken });
            }
        }

        foreach (var definition in definitions) Scan(definition.Body);

        // Codice di modulo (label fuori dalle procedure): le statement sono indicizzate per
        // riga; l'ordine di esecuzione è quello delle chiavi.
        if (program.MainModule is not null)
            foreach (var line in program.MainModule.Statements.OrderBy(kv => kv.Key))
                Scan(line.Value.ToImmutableArray());

        return added.Count == 0 ? definitions : definitions.AddRange(added);
    }
}
