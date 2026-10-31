// FILE: IlBackendCodegen/Partial/IlProgramCompiler.ControlFlow.Branches.cs
// PARTIAL — dominio CONTROLFLOW/BRANCHES: label/branch/leave-out-of-try, ON ERROR
// target, matrix/vector command, increment, ternary. Migrato da
// IlProgramCompiler.ControlFlow.cs (2026-08-30 — split 6.0.1, file <= 1200 righe).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: branch/leave/matrix/vector/ternary.</summary>
public sealed partial class IlProgramCompiler
{
    private Label ResolveNamedLabel(Token token)
    {
        if (_namedLabels.TryGetValue(token.Lexeme, out var label)) return label;
        throw new NotSupportedException($"Backend IL: etichetta '{token.Lexeme}' non trovata.");
    }

    /// <summary>
    /// Risolve il target di GOTO/GOSUB: un numero di riga (Token numerico, cerca in
    /// _lineLabels) oppure un'etichetta con nome (cerca in _namedLabels).
    /// </summary>
    private Label ResolveJumpTarget(Token target)
    {
        if (target.Kind == TokenKind.Number)
        {
            var line = Convert.ToInt32(target.Literal);
            if (_lineLabels.TryGetValue(line, out var label)) return label;
            // GOTO verso una riga inesistente (idioma `GOTO 999` come uscita): salta al
            // label di uscita del metodo corrente (parità C99). Se non c'è un metodo in
            // corso di emissione, errore esplicito.
            if (_missingLineExitLabel is { } exitLabel) return exitLabel;
            throw new NotSupportedException($"Backend IL: GOTO verso la riga {line}, non trovata nel modulo.");
        }

        return ResolveNamedLabel(target);
    }

    /// <summary>
    /// A quale numero di riga appartiene il target di un salto — un Token numerico È già
    /// la riga; un'etichetta con nome la cerca in _namedLabelToLine (popolato solo da
    /// EmitEntryPointWithErrorHandling). Usato solo da EmitBranch quando ON ERROR è
    /// attivo — mai chiamato altrimenti.
    /// </summary>
    private int? ResolveJumpTargetLine(Token target)
    {
        if (target.Kind == TokenKind.Number) return Convert.ToInt32(target.Literal);
        return _namedLabelToLine.TryGetValue(target.Lexeme, out var line) ? line : null;
    }

    /// <summary>ON ERROR GOTO/GOSUB NomeSub: risolve il nome a una PROCEDURA senza parametri
    /// e registra un target sentinella negativo che il dispatcher usa per invocarla quando
    /// si verifica un errore (il P-Code salta all'indirizzo della procedura).</summary>
    private int? ResolveOnErrorProcedureTarget(Token target)
    {
        if (target.Kind != TokenKind.Identifier) return null;
        // FindSymbolsByName è case-sensitive (SymbolsByName) e il codice BASIC può usare
        // una grafia diversa (es. ON ERROR GOTO gestore_errori per SUB GestoreErrori):
        // risolve case-insensitive e normalizzando gli underscore (come il P-Code fa con
        // _procedureAddresses).
        var normalized = target.Lexeme.Replace("_", "");
        var definition = _procedureDefinitions.Values
            .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(normalized, StringComparison.OrdinalIgnoreCase));
        if (definition == null ||
            !_procedureMethods.TryGetValue(definition.SymbolId, out var method))
            return null;
        if (!method.IsStatic || method.GetParameters().Length != 0) return null;
        var sentinel = OnErrorProcedureTargetBase - _onErrorProcedureTargets.Count - 1;
        _onErrorProcedureTargets[sentinel] = method;
        return sentinel;
    }

    /// <summary>
    /// Emette un salto verso <paramref name="target"/> — un Br/Brtrue/Brfalse diretto in
    /// modalità normale (<paramref name="conditionalOpCode"/> null = incondizionato). Se
    /// ON ERROR GOTO/RESUME è attivo per questa entry point (<see cref="_onErrorDispatch"/>
    /// non null — vedi EmitEntryPointWithErrorHandling), un salto diretto non è più
    /// possibile: il codice è decomposto in un blocco try SEPARATO per ogni numero di
    /// riga (necessario per poter "rientrare" a una riga specifica dopo un errore, cosa
    /// che un singolo try che avvolge tutto non permetterebbe — vedi il commento di quel
    /// metodo), e IL vieta un Br diretto che attraversi il confine tra due blocchi try
    /// diversi (solo Leave può uscire da una regione protetta). Va quindi tradotto in
    /// "imposta lo stato alla riga di destinazione, poi Leave verso il dispatcher", che
    /// rientrerà nel chunk corretto.
    /// </summary>
    private void EmitBranch(Token target, OpCode? conditionalOpCode = null)
    {
        if (_onErrorDispatch is not { } dispatch)
        {
            // Vedi il commento su _activeTryScopes: un salto che esce dal corpo del TRY
            // corrente (target non tra le sue etichette/righe) non può essere un Br
            // diretto — solo Leave può attraversare il confine di una regione protetta.
            if (_activeTryScopes.Count > 0 && !IsTargetInInnermostTryScope(target))
            {
                EmitLeaveOutOfTry(target, conditionalOpCode);
                return;
            }
            _il.Emit(conditionalOpCode ?? OpCodes.Br, ResolveJumpTarget(target));
            return;
        }

        var targetLine = ResolveJumpTargetLine(target)
            ?? throw new NotSupportedException($"Backend IL: impossibile determinare la riga di destinazione del salto verso '{target.Lexeme}' con ON ERROR GOTO/RESUME attivo.");

        if (conditionalOpCode is null)
        {
            EmitSetStateAndLeave(dispatch.State, dispatch.LeaveTarget, targetLine);
            return;
        }

        var skipLabel = _il.DefineLabel();
        _il.Emit(conditionalOpCode.Value == OpCodes.Brtrue ? OpCodes.Brfalse : OpCodes.Brtrue, skipLabel);
        EmitSetStateAndLeave(dispatch.State, dispatch.LeaveTarget, targetLine);
        _il.MarkLabel(skipLabel);
    }

    /// <summary>Il target di un salto appartiene al corpo TRY più interno attualmente in
    /// compilazione? Se sì, un Br diretto resta legale (salto interno alla stessa
    /// regione protetta); vedi _activeTryScopes.</summary>
    private bool IsTargetInInnermostTryScope(Token target)
    {
        var scope = _activeTryScopes.Peek();
        return target.Kind == TokenKind.Number
            ? scope.Lines.Contains(Convert.ToInt32(target.Literal))
            : scope.Labels.Contains(target.Lexeme);
    }

    /// <summary>Emette Leave (mai Br) verso un target FUORI dal TRY corrente — Leave
    /// gestisce correttamente anche più livelli di try/finally annidati con una sola
    /// istruzione, quindi basta risolvere direttamente l'etichetta finale del salto
    /// (nessuna indirezione a stati/dispatcher, a differenza del pattern ON ERROR).</summary>
    private void EmitLeaveOutOfTry(Token target, OpCode? conditionalOpCode)
    {
        var resolvedTarget = ResolveJumpTarget(target);
        if (conditionalOpCode is null)
        {
            _il.Emit(OpCodes.Leave, resolvedTarget);
            return;
        }

        var skipLabel = _il.DefineLabel();
        _il.Emit(conditionalOpCode.Value == OpCodes.Brtrue ? OpCodes.Brfalse : OpCodes.Brtrue, skipLabel);
        _il.Emit(OpCodes.Leave, resolvedTarget);
        _il.MarkLabel(skipLabel);
    }

    private void EmitSetStateAndLeave(LocalBuilder stateLocal, Label leaveTarget, int targetLine)
    {
        _il.Emit(OpCodes.Ldc_I4, targetLine);
        _il.Emit(OpCodes.Stloc, stateLocal);
        _il.Emit(OpCodes.Leave, leaveTarget);
    }

    private void EmitMatrixCommandStatement(MatrixCommandStmt stmt)
    {
        var args = stmt.Arguments;
        switch (stmt.Command.Kind)
        {
            case TokenKind.LinAlgMatrixIdentityCommand:
            case TokenKind.LinAlgMatrixPushCommand:
            case TokenKind.LinAlgMatrixPopCommand:
                if (args.Length != 1) throw new NotSupportedException($"Backend IL: MATRIX.{stmt.Command.Lexeme} richiede 1 argomento.");
                EmitInt64Argument(args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(stmt.Command.Kind switch
                {
                    TokenKind.LinAlgMatrixIdentityCommand => nameof(IlRuntime.MatrixIdentity),
                    TokenKind.LinAlgMatrixPushCommand => nameof(IlRuntime.MatrixPush),
                    _ => nameof(IlRuntime.MatrixPop)
                }));
                return;

            case TokenKind.LinAlgMatrixTranslateCommand:
            case TokenKind.LinAlgMatrixRotateCommand:
            case TokenKind.LinAlgMatrixScaleCommand:
                if (args.Length != 4) throw new NotSupportedException($"Backend IL: MATRIX.{stmt.Command.Lexeme} richiede 4 argomenti.");
                EmitInt64Argument(args[0]);
                EmitDoubleArgument(args[1]);
                EmitDoubleArgument(args[2]);
                EmitDoubleArgument(args[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(stmt.Command.Kind switch
                {
                    TokenKind.LinAlgMatrixTranslateCommand => nameof(IlRuntime.MatrixTranslate),
                    TokenKind.LinAlgMatrixRotateCommand => nameof(IlRuntime.MatrixRotate),
                    _ => nameof(IlRuntime.MatrixScale)
                }));
                return;

            case TokenKind.LinAlgMatrixMultiplyCommand:
                if (args.Length != 3) throw new NotSupportedException("Backend IL: MATRIX.MULTIPLY richiede 3 argomenti.");
                foreach (var argument in args) EmitInt64Argument(argument);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixMultiplyInto)));
                return;

            // WORLD/VIEW/PROJECTION: la PCodeVM li implementa come WORLD_MATRIX_GPM/
            // VIEW_MATRIX_GPM/PROJECTION_MATRIX_GPM — setter della matrice corrente nel
            // GraphicsManager (usata dalle trasformazioni di rendering). In headless
            // (nessun renderer/GraphicsManager) il valore è un handle opaco: valuta
            // l'argomento (side-effect) e no-op, come il resto della grafica non-SDL.
            case TokenKind.LinAlgMatrixSetWorldCommand:
            case TokenKind.LinAlgMatrixSetViewCommand:
            case TokenKind.LinAlgMatrixSetProjectionCommand:
                if (args.Length != 1) throw new NotSupportedException($"Backend IL: MATRIX.{stmt.Command.Lexeme} richiede 1 argomento (handle).");
                EmitInt64Argument(args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixSetTransformMatrix)));
                return;

            default:
                throw new NotSupportedException($"Backend IL: comando MATRIX '{stmt.Command.Lexeme}' non nativo (dipende dal pipeline grafico).");
        }
    }

    private void EmitIncrementExpression(Target target, bool plus, bool postfix)
    {
        // ++t / t++ (e --): `t = t ± 1` poi il risultato (prefix=NUOVO, postfix=VECCHIO).
        // Vale per QUALUNQUE target (variabile, campo di classe/UDT, elemento di array):
        // `t = t ± 1` via i percorsi di assegnazione esistenti, poi rilettura di `t`.
        var tExpr = TargetToExpr(target);
        if (tExpr is null)
            throw new NotSupportedException($"Backend IL: ++/-- su target '{target.GetType().Name}' non supportato.");
        var opKind = plus ? TokenKind.Plus : TokenKind.Minus;
        var opTok = new Token(opKind, plus ? "+" : "-", null, target.Position);
        EmitTargetAssignment(target, new BinaryExpr(tExpr, opTok, new NumberExpr(1.0, opTok)));
        // Risultato: prefix = nuovo valore (rilettura); postfix = vecchio = nuovo ∓ 1.
        EmitDoubleArgument(TargetToExpr(target)!);
        if (postfix) { _il.Emit(OpCodes.Ldc_R8, 1.0); _il.Emit(plus ? OpCodes.Sub : OpCodes.Add); }
    }

    /// <summary>Converte un Target nel nodo Expr equivalente (LETTURA): variabile, elemento di
    /// array, campo (anche annidato) o accesso indicizzato. Null se non convertibile.</summary>
    private Expr? TargetToExpr(Target t) => t switch
    {
        VariableTarget vt => new VariableExpr(vt.SymbolId, vt.NameSuffixed, vt.NameCleaned),
        ArrayElementTarget aet => new ArrayAccessExpr(aet.SymbolId, aet.NameSuffixed, aet.NameCleaned, aet.Indices),
        FieldTarget ft => TargetToExpr(ft.Structure) is { } s ? new FieldAccessExpr(s, ft.OriginalToken) : null,
        IndexedTarget it => new IndexedAccessExpr(it.Base, it.Indices),
        _ => null,
    };

    /// <summary>Emette l'assegnazione a un Target da un'Expr — stessa dispatch di
    /// EmitLetStatement (usata da ++/--).</summary>
    private void EmitTargetAssignment(Target target, Expr value)
    {
        switch (target)
        {
            case VariableTarget vt: EmitStoreVariable(vt.SymbolId, value); break;
            case ArrayElementTarget aet: EmitArrayElementAssignment(aet, value); break;
            case FieldTarget ft: EmitFieldAssignment(ft, value); break;
            case IndexedTarget it: EmitIndexedTargetAssignment(it, value); break;
            default:
                throw new NotSupportedException(
                    $"Backend IL: assegnazione a target '{target.GetType().Name}' non supportata.");
        }
    }

    private void EmitTernaryExpression(TernaryExpr te)
    {
        // cond ? a : b — branch nativo come il P-Code (OP_JUMP_IF_FALSE/FORWARD).
        // Il tipo comune dei due rami: l'eventuale coerce la fa il call site (PRINT/
        // assegnazione) via InferType; qui si emette il valore grezzo di ogni ramo.
        EmitExpression(te.Condition);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ceq);
        var falseLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Brtrue, falseLabel);
        EmitExpression(te.TruePart);
        var endLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Br, endLabel);
        _il.MarkLabel(falseLabel);
        EmitExpression(te.FalsePart);
        _il.MarkLabel(endLabel);
    }

    private void EmitVectorCommandStatement(VectorCommandStmt stmt)
    {
        var args = stmt.Arguments;
        switch (stmt.Command.Kind)
        {
            case TokenKind.LinAlgVectorAddCommand:
            case TokenKind.LinAlgVectorSubCommand:
            case TokenKind.LinAlgVectorMulCommand:
            case TokenKind.LinAlgVectorDivCommand:
                if (args.Length != 2) throw new NotSupportedException($"Backend IL: VECTOR.{stmt.Command.Lexeme} richiede 2 argomenti.");
                // Due forme: (a) array DIRETTO (DIM V AS DOUBLE, AlgebraVectorOperations)
                // e (b) HANDLE long (VECTOR.CREATE, AlgebraTests). Discriminato per tipo
                // del 1° argomento: ArrayType → double[] diretto, altrimenti handle.
                var isArrayForm = NormalizeInferredType(InferType(args[0])) is ArrayType;
                if (isArrayForm) EmitExpression(args[0]);
                else EmitInt64Argument(args[0]);
                EmitCryptoObjectArgument(args[1]);
                var vop = stmt.Command.Kind switch
                {
                    TokenKind.LinAlgVectorAddCommand => nameof(IlRuntime.VectorAdd),
                    TokenKind.LinAlgVectorSubCommand => nameof(IlRuntime.VectorSub),
                    TokenKind.LinAlgVectorMulCommand => nameof(IlRuntime.VectorMul),
                    _ => nameof(IlRuntime.VectorDiv)
                };
                _il.Emit(OpCodes.Call, RuntimeMethod(vop, isArrayForm
                    ? [typeof(double[]), typeof(object)]
                    : [typeof(long), typeof(object)]));
                return;

            case TokenKind.LinAlgVectorSetCommand:
                if (args.Length != 2) throw new NotSupportedException("Backend IL: VECTOR.SET richiede 2 argomenti.");
                var isSetArray = NormalizeInferredType(InferType(args[0])) is ArrayType;
                if (isSetArray) EmitExpression(args[0]);
                else EmitInt64Argument(args[0]);
                EmitDoubleArgument(args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VectorSet), isSetArray
                    ? [typeof(double[]), typeof(double)]
                    : [typeof(long), typeof(double)]));
                return;

            case TokenKind.LinAlgVectorFillRandomCommand:
            case TokenKind.LinAlgVectorWrapCommand:
                if (args.Length != 3) throw new NotSupportedException($"Backend IL: VECTOR.{stmt.Command.Lexeme} richiede 3 argomenti.");
                var isFillArray = NormalizeInferredType(InferType(args[0])) is ArrayType;
                if (isFillArray) EmitExpression(args[0]);
                else EmitInt64Argument(args[0]);
                EmitDoubleArgument(args[1]);
                EmitDoubleArgument(args[2]);
                var fillName = stmt.Command.Kind == TokenKind.LinAlgVectorFillRandomCommand
                    ? nameof(IlRuntime.VectorFillRandom)
                    : nameof(IlRuntime.VectorWrap);
                _il.Emit(OpCodes.Call, RuntimeMethod(fillName, isFillArray
                    ? [typeof(double[]), typeof(double), typeof(double)]
                    : [typeof(long), typeof(double), typeof(double)]));
                return;

            case TokenKind.LinAlgVectorAddWrapCommand:
                if (args.Length != 4) throw new NotSupportedException("Backend IL: VECTOR.ADDWRAP richiede 4 argomenti.");
                var isAwArray = NormalizeInferredType(InferType(args[0])) is ArrayType;
                if (isAwArray) EmitExpression(args[0]);
                else EmitInt64Argument(args[0]);
                EmitCryptoObjectArgument(args[1]);
                EmitDoubleArgument(args[2]);
                EmitDoubleArgument(args[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VectorAddWrap), isAwArray
                    ? [typeof(double[]), typeof(object), typeof(double), typeof(double)]
                    : [typeof(long), typeof(object), typeof(double), typeof(double)]));
                return;

            case TokenKind.LinAlgVectorBounce2dCommand:
                if (args.Length != 4 && args.Length != 8)
                    throw new NotSupportedException("Backend IL: VECTOR.BOUNCE2D richiede 4 o 8 argomenti.");
                // BOUNCE2D: gli argomenti vettore possono essere VETTORI DIRETTI (double[],
                // DIM X#(MAX)) o HANDLE long (VECTOR.CREATE). Discriminato per TIPO del 1°
                // argomento (l'arity da sola è ambigua: anche la forma 8-arg può usare array,
                // es. GraphicsTexturesBenchmark "VECTOR BOUNCE2D X, Y, DX, DY, 0, 0, 640, 480").
                var isBounceArray = NormalizeInferredType(InferType(args[0])) is ArrayType;
                for (var index = 0; index < args.Length; index++)
                {
                    if (index < args.Length / 2)
                    {
                        if (isBounceArray) EmitExpression(args[index]);
                        else EmitInt64Argument(args[index]);
                    }
                    else EmitDoubleArgument(args[index]);
                }
                _il.Emit(OpCodes.Call, args.Length switch
                {
                    4 when isBounceArray =>
                        RuntimeMethod(nameof(IlRuntime.VectorBounce2D), typeof(double[]), typeof(double[]), typeof(double), typeof(double)),
                    4 =>
                        RuntimeMethod(nameof(IlRuntime.VectorBounce2D), typeof(long), typeof(long), typeof(double), typeof(double)),
                    8 when isBounceArray =>
                        RuntimeMethod(nameof(IlRuntime.VectorBounce2D), typeof(double[]), typeof(double[]), typeof(double[]), typeof(double[]), typeof(double), typeof(double), typeof(double), typeof(double)),
                    _ =>
                        RuntimeMethod(nameof(IlRuntime.VectorBounce2D), typeof(long), typeof(long), typeof(long), typeof(long), typeof(double), typeof(double), typeof(double), typeof(double))
                });
                return;

            default:
                throw new NotSupportedException($"Backend IL: comando VECTOR '{stmt.Command.Lexeme}' non ancora supportato.");
        }
    }
}
