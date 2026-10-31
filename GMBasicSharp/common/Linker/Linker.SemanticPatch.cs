// FILE: Interpreter.Linker.SemanticPatch.cs — §4.10 SEMAPRAX write-side.
// Applica i `SemanticPatchStmt` (`AI PATCH "target", "rev" … END PATCH`) revision-bound:
// se la revisione corrisponde, il corpo della procedura target viene SOSTITUITO dal corpo del patch;
// altrimenti il patch è RIFIUTATO (no-op). Gli stmt-patch vengono poi rimossi dai corpi.

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>Applica i semantic patch (write-side) revision-bound. Da chiamare DOPO il linking,
    /// PRIMA del codegen. Rimuove gli stmt-patch dai corpi (diventano no-op).</summary>
    public static void ApplySemanticPatches(BasicProgram program)
    {
        if (program is null) return;

        // 1) raccogli i patch da TUTTI i corpi delle procedure.
        var patches = new List<SemanticPatchStmt>();
        foreach (var mod in program.LoadedModules.Values)
            foreach (var d in mod.Declarations)
                if (d is ProcedureDefinition pd)
                    CollectAndStrip(pd.Body, patches, out _);
        // 2) se non ci sono patch, nulla da fare (ma rimuoviamo comunque eventuali stmt-patch: fatto sopra).
        string? rev = SemanticQueryFolder?.Invoke(program, TokenKind.AiRevision, "", 0);
        if (patches.Count == 0) return;

        // 3) per ogni patch: gate di revisione -> sostituisci il corpo della procedura target.
        foreach (var pt in patches)
        {
            if (rev is null || !string.Equals(pt.ExpectedRev, rev, StringComparison.Ordinal)) continue; // rifiutato
            foreach (var path in program.LoadedModules.Keys.ToList())
            {
                var mod = program.LoadedModules[path];
                var decls = mod.Declarations;
                bool changed = false;
                for (int i = 0; i < decls.Length; i++)
                {
                    if (decls[i] is ProcedureDefinition tpd &&
                        string.Equals(tpd.OriginalName.Lexeme, pt.Target, StringComparison.OrdinalIgnoreCase))
                    {
                        var patchedBody = pt.Body;
                        // Preserva il terminatore EndProcedureStmt del corpo ORIGINALE: è da lì che
                        // PCode/VM-C emettono l'epilogo (return-slot + RETURN). Senza, il corpo patchato
                        // "cade" nella procedura successiva -> ricorsione infinita a runtime. IL non ne
                        // soffre (epilogo implicito), ma la parità richiede lo stesso AST per tutti.
                        if (tpd.Body.Length > 0 && tpd.Body[^1] is EndProcedureStmt endProc &&
                            (patchedBody.Length == 0 || patchedBody[^1] is not EndProcedureStmt))
                            patchedBody = patchedBody.Add(endProc);
                        var newPd = tpd with { Body = patchedBody };
                        decls = decls.SetItem(i, newPd);
                        changed = true;
                        // La SymbolTable conserva il ProcedureDefinition ORIGINALE nel DeclarationNode:
                        // PCode/VM-C/C99 compilano via ctx.GetAllProcedures() (che legge la SymbolTable)
                        // e vedrebbero il corpo STALE. Aggiorna anche il simbolo corrispondente.
                        foreach (var sym in program.Symbols.FindSymbolsByName(pt.Target))
                            if (sym.DeclarationNode is ProcedureDefinition spd &&
                                ReferenceEquals(spd, tpd))
                                program.Symbols.UpdateSymbolDeclaration(sym.Id, newPd);
                    }
                }
                if (changed)
                    program.LoadedModules[path] = new BasicModule(mod.FilePath, mod.IsStrict, mod.SourceMap,
                        mod.Statements, mod.Imports, decls, mod.LibraryName);
            }
        }

        // 4) rimuovi gli stmt-patch residui dai corpi (già applicati o rifiutati) -> no-op.
        foreach (var path in program.LoadedModules.Keys.ToList())
        {
            var mod = program.LoadedModules[path];
            var decls = mod.Declarations;
            bool changed = false;
            for (int i = 0; i < decls.Length; i++)
                if (decls[i] is ProcedureDefinition pd)
                {
                    var nb = CollectAndStrip(pd.Body, null, out bool stripped);
                    if (stripped) { decls = decls.SetItem(i, pd with { Body = nb }); changed = true; }
                }
            if (changed)
                program.LoadedModules[path] = new BasicModule(mod.FilePath, mod.IsStrict, mod.SourceMap,
                    mod.Statements, mod.Imports, decls, mod.LibraryName);
        }
    }

    /// <summary>Restituisce il corpo con i `SemanticPatchStmt` rimossi; li raccoglie in <paramref name="into"/>
    /// (se non null). <paramref name="stripped"/> = true se almeno uno stmt è stato rimosso.
    /// Scende anche nei blocchi annidati (IF/loop/FOR/SELECT).</summary>
    private static ImmutableArray<Stmt> CollectAndStrip(ImmutableArray<Stmt> body, List<SemanticPatchStmt>? into, out bool stripped)
    {
        stripped = false;
        var b = ImmutableArray.CreateBuilder<Stmt>();
        foreach (var s in body)
        {
            if (s is SemanticPatchStmt sp) { into?.Add(sp); stripped = true; continue; }
            b.Add(s);
        }
        return stripped ? b.ToImmutable() : body;
    }

}
