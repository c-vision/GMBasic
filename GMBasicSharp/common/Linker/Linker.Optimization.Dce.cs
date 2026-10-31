// FILE: Interpreter.Linker.Optimization.Dce.cs
using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Pass di ottimizzazione AST: Dead Code Elimination (DCE).
    /// Identifica le procedure (SUB / FUNCTION) non raggiungibili a partire dagli entry point.
    /// </summary>
    public static class DeadCodeEliminator
    {
        public static void EliminateDeadCode(BasicProgram program)
        {
            var reachableSymbols = new HashSet<int>();
            var worklist = new Queue<int>();

            // 1. Identifica gli Entry Point del programma:
            // - Statement nei moduli caricati (top-level / codice numerato)
            // - Procedure con nome "Main" nello scope globale o marcate Public
            foreach (var module in program.LoadedModules.Values)
            {
                foreach (var stmtsList in module.Statements.Values)
                {
                    foreach (var stmt in stmtsList)
                    {
                        CollectReferencedSymbols(stmt, reachableSymbols, worklist);
                    }
                }
            }

            var allProcs = program.Symbols.GetAllProcedures().ToList();
            foreach (var proc in allProcs)
            {
                if (proc.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase) ||
                    proc.Flags.HasFlag(SymbolFlags.IsPublic))
                {
                    if (reachableSymbols.Add(proc.SymbolId))
                    {
                        worklist.Enqueue(proc.SymbolId);
                    }
                }
            }

            // 2. Transitive Closure: Esplora ricorsivamente tutte le procedure raggiungibili
            var procMap = allProcs.ToDictionary(p => p.SymbolId);
            while (worklist.Count > 0)
            {
                var currentId = worklist.Dequeue();
                if (procMap.TryGetValue(currentId, out var procDef))
                {
                    // Body è un parametro posizionale SENZA default nel record ProcedureDefinition,
                    // ma resta comunque default(ImmutableArray<Stmt>) (non .Empty) per le dichiarazioni
                    // SOLO-FIRMA che non hanno mai un corpo BASIC da eseguire — metodi ASTRATTI/di
                    // INTERFACCIA, delegate, DECLARE/FFI esterne. Un foreach diretto su un
                    // ImmutableArray<T> default lancia InvalidOperationException ("cannot be
                    // performed on a default instance") — mai raggiunto prima di DCE perché nessun
                    // altro codice iterava .Body su OGNI procedura raggiungibile indiscriminatamente.
                    // Bug scoperto: ClassesAbstract/ClassesInterfaces/FunctionsDelegates/
                    // SystemFFIPacked.bas (100% delle classi con almeno un membro PUBLIC) andavano
                    // in COMPILE ERROR non appena quel membro finiva nel worklist.
                    if (procDef.Body.IsDefault) continue;
                    foreach (var stmt in procDef.Body)
                    {
                        CollectReferencedSymbols(stmt, reachableSymbols, worklist);
                    }
                }
            }

            // 3. Marca ed eventualmente esclude le procedure non raggiunte (Dead Code)
            var deadProcedures = allProcs
                .Where(p => !reachableSymbols.Contains(p.SymbolId) &&
                            !p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase) &&
                            !p.Flags.HasFlag(SymbolFlags.IsPublic))
                .ToList();

            // 4. APPLICA l'eliminazione: rimuove le procedure morte dalle Declarations dei
            // moduli (un solo posto in cui vivono i FunctionDefinition/GeneratorDefinition).
            // Non si tocca la SymbolTable (i SymbolId restano allocati — indice stabile per
            // il resto del pipeline); è la lista delle DECLARATIONS che il back-end (P-Code/
            // IL) usa per materializzare le procedure, quindi la rimozione lì produce output
            // più compatti e salta la compilazione di codice mai raggiungibile.
            if (deadProcedures.Count > 0)
            {
                var deadIds = deadProcedures.Select(p => p.SymbolId).ToHashSet();
                foreach (var modulePath in program.LoadedModules.Keys.ToList())
                {
                    var module = program.LoadedModules[modulePath];
                    var kept = module.Declarations
                        .Where(d => !(d is ProcedureDefinition pd && deadIds.Contains(pd.SymbolId)))
                        .ToImmutableArray();
                    if (kept.Length != module.Declarations.Length)
                    {
                        var updatedModule = new BasicModule(
                            module.FilePath, module.IsStrict, module.SourceMap, module.Statements,
                            module.Imports, kept, module.LibraryName);
                        updatedModule.ExecutionOrder = module.ExecutionOrder;
                        program.LoadedModules[modulePath] = updatedModule;
                    }
                }
            }
        }

        private static void CollectReferencedSymbols(AstNode node, HashSet<int> reachableSymbols, Queue<int> worklist)
        {
            switch (node)
            {
                case BinaryExpr binary:
                    CollectReferencedSymbols(binary.Left, reachableSymbols, worklist);
                    CollectReferencedSymbols(binary.Right, reachableSymbols, worklist);
                    break;

                case UnaryExpr unary:
                    CollectReferencedSymbols(unary.Right, reachableSymbols, worklist);
                    break;

                case ExpressionStmt exprStmt:
                    CollectReferencedSymbols(exprStmt.Expression, reachableSymbols, worklist);
                    break;

                case LetStmt letStmt:
                    foreach (var assign in letStmt.Assignments)
                    {
                        CollectReferencedSymbols(assign.Value, reachableSymbols, worklist);
                    }
                    break;

                case PrintStmt printStmt:
                    foreach (var arg in printStmt.Arguments)
                    {
                        if (arg.Expression != null) CollectReferencedSymbols(arg.Expression, reachableSymbols, worklist);
                    }
                    break;

                case ReturnStmt returnStmt:
                    if (returnStmt.Value != null) CollectReferencedSymbols(returnStmt.Value, reachableSymbols, worklist);
                    break;

                case IfStmt ifStmt:
                    foreach (var clause in ifStmt.IfClauses)
                    {
                        CollectReferencedSymbols(clause.Condition, reachableSymbols, worklist);
                        foreach (var stmt in clause.Statements) CollectReferencedSymbols(stmt, reachableSymbols, worklist);
                    }
                    if (ifStmt.ElseClause != null)
                    {
                        foreach (var stmt in ifStmt.ElseClause.Statements) CollectReferencedSymbols(stmt, reachableSymbols, worklist);
                    }
                    break;

                case ForStmt forStmt:
                    CollectReferencedSymbols(forStmt.Start, reachableSymbols, worklist);
                    CollectReferencedSymbols(forStmt.End, reachableSymbols, worklist);
                    if (forStmt.Step != null) CollectReferencedSymbols(forStmt.Step, reachableSymbols, worklist);
                    break;

                // CHIAMATE: il cuore della raggiungibilità — senza questi case il closure
                // restava vuoto (nessuna procedura chiamata veniva mai marcata raggiungibile).
                case FunctionExpr fn:
                    if (reachableSymbols.Add(fn.SymbolId)) worklist.Enqueue(fn.SymbolId);
                    foreach (var arg in fn.Args) CollectReferencedSymbols(arg, reachableSymbols, worklist);
                    break;

                case FunctionReferenceExpr fre:
                    if (reachableSymbols.Add(fre.SymbolId)) worklist.Enqueue(fre.SymbolId);
                    break;

                case BuiltinFunctionCallExpr bce:
                    foreach (var arg in bce.Args) CollectReferencedSymbols(arg, reachableSymbols, worklist);
                    break;

                case UnresolvedCallExpr uce:
                    foreach (var arg in uce.Arguments) CollectReferencedSymbols(arg, reachableSymbols, worklist);
                    break;

                case NewExpr newExpr:
                    if (newExpr.ConstructorArgs is { } ctorArgs)
                        foreach (var arg in ctorArgs) CollectReferencedSymbols(arg, reachableSymbols, worklist);
                    if (newExpr.InitialValues is { } initValues)
                        foreach (var arg in initValues) CollectReferencedSymbols(arg, reachableSymbols, worklist);
                    break;

                // Walk ricorsivo generico: per qualunque nodo con figli non coperto sopra,
                // scendi per riflessione sui figli strutturali — evita di dimenticare un case.
                // Ciclo-safe: AstReflectiveWalk.ChildNodes salta le back-reference al genitore
                // (es. UserDefinedType.Parent, che altrimenti fa risalire l'albero all'infinito
                // — stesso bug di FindExplicitCallTo) e le ImmutableArray<T> mai inizializzate
                // (iterarle lancia InvalidOperationException).
                case AstNode other:
                    foreach (var child in AstReflectiveWalk.ChildNodes(other))
                        CollectReferencedSymbols(child, reachableSymbols, worklist);
                    break;
            }
        }
    }
}
