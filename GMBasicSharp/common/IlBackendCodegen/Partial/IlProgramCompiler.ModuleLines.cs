// FILE: IlBackendCodegen/Partial/IlProgramCompiler.ModuleLines.cs
// PARTIAL — dominio MODULE LINES (BASIC legacy): righe NUMERATE di modulo raggiunte da un
// GOSUB dentro una PROCEDURA. Un salto CIL non può attraversare il confine di un metodo,
// quindi ogni riga-target diventa un METODO runner statico che esegue le righe da lì fino
// alla successiva riga-target (stesso modello dei `lfn_num_<riga>` del C99Native).
//
// Perché serve: in un programma "ibrido" (SUB Main + codice numerato di modulo) le righe
// numerate sono emesse nell'entry point, mentre la SUB Main è un metodo separato: il
// `GOSUB 100` dentro Main non aveva alcuna destinazione nel proprio metodo e ricadeva sul
// label di uscita ("riga non trovata") → la subroutine non veniva mai eseguita.

using System.Collections.Immutable;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    /// <summary>
    /// Runner per-programma (la compilazione crea un'istanza del compilatore per ogni
    /// metodo, tutte con lo stesso <see cref="BasicProgram"/>): riga numerata → metodo
    /// runner. La tabella vive quanto il programma compilato.
    /// </summary>
    private static readonly ConditionalWeakTable<BasicProgram, Dictionary<int, MethodBuilder>>
        ModuleLineRunners = new();

    private static Dictionary<int, MethodBuilder>? GetModuleLineRunnerMap(BasicProgram program)
        => ModuleLineRunners.TryGetValue(program, out var map) ? map : null;

    /// <summary>Catalogo (riga, statements) delle righe numerate legacy del modulo, in ordine.</summary>
    private static List<(int Line, List<Stmt> Stmts)> ModuleLegacyLines(BasicModule module)
    {
        var result = new List<(int, List<Stmt>)>();
        foreach (var line in module.ExecutionOrder)
        {
            if (line >= 1_000_000) continue; // righe sintetiche (codice non numerato)
            if (!module.Statements.TryGetValue(line, out var stmts)) continue;
            result.Add((line, stmts));
        }
        return result;
    }

    /// <summary>
    /// Crea i runner per OGNI riga numerata di modulo che è target di un `GOSUB <riga>`
    /// dentro una procedura (SUB/FUNCTION/metodo). Da chiamare DOPO la scoperta delle
    /// lambda (i corpi dei runner possono contenerne) e PRIMA della compilazione delle
    /// procedure.
    /// </summary>
    internal void PrepareModuleLineRunners(
        BasicModule mainModule,
        TypeBuilder programType,
        IlTypeContext types,
        Dictionary<int, MethodBuilder> procedureMethods,
        Dictionary<int, FunctionDefinition> procedureDefinitions,
        Dictionary<int, TypeBuilder> classTypes,
        Dictionary<int, FieldBuilder> classFields,
        Dictionary<int, FieldArrayInfo> classFieldArrays,
        Dictionary<int, List<ClassConstructorInfo>> classConstructors,
        Dictionary<int, ClassPropertyInfo> classProperties,
        Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        Dictionary<LambdaExpr, LambdaClosureInfo> lambdaInfos)
    {
        var legacy = ModuleLegacyLines(mainModule);
        if (legacy.Count == 0) return;
        var legacyLines = legacy.Select(l => l.Line).ToHashSet();

        // Target numerici dei GOSUB dentro le PROCEDURE (il GOSUB del codice di modulo è
        // già risolto con i label locali dell'entry point).
        var targets = new SortedSet<int>();
        foreach (var definition in procedureDefinitions.Values)
        {
            WalkAst(definition.Body.Cast<object>().ToList(), node =>
            {
                if (node is GosubStmt { Target.Kind: TokenKind.Number } gosub
                    && gosub.Target.Literal is not null)
                {
                    var line = Convert.ToInt32(gosub.Target.Literal);
                    if (legacyLines.Contains(line)) targets.Add(line);
                }
                return true;
            });
        }
        if (targets.Count == 0) return;

        var orderedTargets = targets.ToList();
        var map = new Dictionary<int, MethodBuilder>();
        ModuleLineRunners.Remove(_program);
        ModuleLineRunners.Add(_program, map);

        // Prima DEFINISCI tutti i metodi (un runner può chiamarne un altro), poi compila.
        foreach (var target in orderedTargets)
        {
            var runner = programType.DefineMethod(
                $"ModuleLines_{target}",
                MethodAttributes.Private | MethodAttributes.Static,
                types.Void,
                Type.EmptyTypes);
            map[target] = runner;
        }

        for (var i = 0; i < orderedTargets.Count; i++)
        {
            var target = orderedTargets[i];
            var next = i + 1 < orderedTargets.Count ? orderedTargets[i + 1] : int.MaxValue;
            var segments = legacy.Where(l => l.Line >= target && l.Line < next).ToList();
            var body = segments.SelectMany(s => s.Stmts).ToImmutableArray();
            var nameToken = new Token(TokenKind.Identifier, $"__modulelines_{target}",
                null, SourcePosition.Unknown);
            var synthetic = new FunctionDefinition(
                -1, nameToken, ImmutableArray<Parameter>.Empty, new VoidType(), body, SymbolFlags.None);
            var runnerCompiler = new IlProgramCompiler(
                _program, map[target].GetILGenerator(), types, procedureMethods,
                procedureDefinitions, classTypes, classFields, classFieldArrays, classConstructors,
                classProperties, classOperators, lambdaInfos, synthetic);
            runnerCompiler.EmitModuleLineRunnerBody(segments);
        }
    }

    /// <summary>
    /// Corpo di un runner: un label per ogni riga del segmento, statements in ordine, poi
    /// il `ret` finale (un RETURN della riga numerata esce dal runner e rientra al
    /// chiamante — è la semantica del GOSUB, parità C99Native).
    /// </summary>
    private void EmitModuleLineRunnerBody(List<(int Line, List<Stmt> Stmts)> segments)
    {
        _localByName.Clear();
        _currentLineLocal = _il.DeclareLocal(_types.Int32);
        var all = segments.SelectMany(s => s.Stmts).ToImmutableArray();
        foreach (var (line, stmts) in segments)
        {
            _lineLabels[line] = _il.DefineLabel();
            foreach (var stmt in stmts) PreScanLabels(stmt, line);
        }
        PreScanRedefinedTypes(all);
        InitializeGosubRuntime(all);
        _missingLineExitLabel = _il.DefineLabel();
        foreach (var (line, stmts) in segments)
        {
            _il.MarkLabel(_lineLabels[line]);
            foreach (var stmt in stmts) EmitStatement(stmt);
        }
        _il.MarkLabel(_missingLineExitLabel.Value);
        _il.Emit(OpCodes.Ret);
        _missingLineExitLabel = null;
    }

    /// <summary>
    /// Se <paramref name="line"/> è una riga numerata di modulo con un runner disponibile,
    /// emette la CHIAMATA al runner (usato dal GOSUB dentro una procedura). Ritorna false se
    /// la riga non è un target gestito da runner.
    /// </summary>
    private bool TryEmitModuleLineRunnerCall(Token target)
    {
        if (target.Kind != TokenKind.Number || target.Literal is null) return false;
        var runnerMap = GetModuleLineRunnerMap(_program);
        if (runnerMap is null) return false;
        var line = Convert.ToInt32(target.Literal);
        if (!runnerMap.TryGetValue(line, out var runner)) return false;
        _il.Emit(OpCodes.Call, runner);
        return true;
    }
}
