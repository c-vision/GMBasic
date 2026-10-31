// FILE: Interpreter.Compiler.Arrays.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione REDIM.
    /// Questa versione corretta utilizza il SymbolId direttamente dall'AST,
    /// eliminando le ricerche basate su stringhe e la creazione di target fittizi.
    /// </summary>
    private void VisitRedimStatement(RedimStmt stmt)
    {
        // Caso speciale: REDIM C = {val1, val2, ...} — nessuna dimensione esplicita
        if (stmt.Dimensions.Length == 0 && stmt.Initializer != null)
        {
            // Create array from initializer literal
            if (stmt.Initializer is ArrayLiteralExpr lit)
            {
                // Create array from initializer literal — size = exact element count
                EmitConstant(lit.Elements.Length - 1.0);
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                EmitByte((byte)(1));
                // INIT_ARRAY will fill elements; remaining are initialized to 0
                int idx = 0;
                foreach (var el in lit.Elements) {
                    VisitExpression(el);
                    idx++;
                }
                EmitByte((byte)OpCode.OP_INIT_ARRAY);
                EmitShort((ushort)idx);
            }
            else
            {
                VisitExpression(stmt.Initializer);
                // Ensure we have an array on the stack (wrap if needed)
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                EmitByte((byte)(0));
            }
            DoRedimStore(stmt.ArrayTarget);
            return;
        }
        
        // Standard REDIM with dimensions
        // Save old array length before REDIM (needed for PRESERVE + initializer)
        bool needSaveLen = stmt.Preserve != null && stmt.Initializer != null;
        int savedLenSlot = -1;
        if (needSaveLen && stmt.ArrayTarget is VariableTarget vt0)
        {
            savedLenSlot = _locals.Count;
            AddLocal(new Token(TokenKind.Identifier, "__rdm_len", null, stmt.RedimToken.Position));
            LoadVariable(vt0);
            EmitByte((byte)OpCode.OP_ARRAY_LEN);
            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)savedLenSlot);
            EmitByte((byte)OpCode.OP_POP);
        }
        
        if (stmt.ArrayTarget is VariableTarget vtRedim)
            LoadVariable(vtRedim);
        
        foreach (var dim in stmt.Dimensions)
            VisitExpression(dim);

        // Emit class descriptor for UDT arrays so REDIM creates __class__-tagged dicts
        ushort udtDescIdx = ushort.MaxValue;
        var arrType = InferExpressionTypeFromTarget(stmt.ArrayTarget);
        if (arrType is ArrayType redimArrType && redimArrType.ElementType is UserDefinedType redimUdtType)
        {
            var descKey = redimUdtType.Name.Lexeme + "#" + redimUdtType.SymbolId;
            if (_compiledClassDescriptors.TryGetValue(descKey, out var udtDesc))
                udtDescIdx = (ushort)AddConstant(udtDesc);
        }

        var opCode = stmt.Preserve is not null ? OpCode.OP_REDIM_PRESERVE : OpCode.OP_REDIM;
        EmitByte((byte)opCode);
        EmitByte((byte)stmt.Dimensions.Length);
        EmitShort(udtDescIdx);
        
        // After REDIM PRESERVE + initializer: populate new elements
        if (savedLenSlot >= 0 && stmt.Initializer is ArrayLiteralExpr litInit2)
        {
            int arrSlot = _locals.Count;
            AddLocal(new Token(TokenKind.Identifier, "__rdm_a", null, stmt.RedimToken.Position));
            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)arrSlot);
            int ii = 0;
            foreach (var el in litInit2.Elements)
            {
                VisitExpression(el);  // push value (deepest)
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)arrSlot);  // push arr
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)savedLenSlot);
                EmitConstant((double)ii);
                EmitByte((byte)OpCode.OP_ADD);  // push index = oldLen + i (top)
                EmitBytes((byte)OpCode.OP_STORE_ELEMENT, 1);
                ii++;
            }
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)arrSlot);
        }
        
        DoRedimStore(stmt.ArrayTarget);
    }
    
    private void DoRedimStore(Target target)
    {
        if (target is VariableTarget vt)
        {
            var localIdx2 = ResolveLocal(vt.NameCleaned.Lexeme);
            if (localIdx2 != -1) { EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx2); } else
            {
                var normalizedName = NormalizeIdentifierName(vt.NameCleaned.Lexeme);
                if (_globals.TryGetValue(normalizedName, out var gIdx))
                {
                    EmitByte((byte)OpCode.OP_DEFINE_GLOBAL);
                    EmitShort(gIdx);
                }
            }
        }
    }
    
    /// <summary>
    /// Compila un'istruzione AADD.
    /// </summary>
    private void VisitAAddStatement(AAddStmt stmt)
    {
        // AADD(array, valore)
        PushTargetByRef(stmt.Array);
        VisitExpression(stmt.Value);
        
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AADD);
        EmitByte((byte)(2)); // 2 argomenti
        EmitByte((byte)OpCode.OP_POP); // AADD non restituisce un valore.
    }

    /// <summary>
    /// Compila un'istruzione AINS.
    /// </summary>
    private void VisitAInsStatement(AInsStmt stmt)
    {
        // AINS(array, posizione)
        PushTargetByRef(stmt.Array);
        VisitExpression(stmt.PositionExpr);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AINS);
        EmitByte((byte)(2));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione ADEL.
    /// </summary>
    private void VisitADelStatement(ADelStmt stmt)
    {
        // ADEL(array, posizione)
        PushTargetByRef(stmt.Array);
        VisitExpression(stmt.PositionExpr);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.ADEL);
        EmitByte((byte)(2));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione ASORT.
    /// </summary>
    private void VisitASortStatement(ASortStmt stmt)
    {
        // ASORT(array, [compareFunction], [order])
        PushTargetByRef(stmt.Array);
        
        if (stmt.CompareFunction is not null) VisitExpression(stmt.CompareFunction);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        if (stmt.Order is not null) VisitExpression(stmt.Order);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.ASORT);
        EmitByte((byte)(3));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione ASHUFFLE (mescola l'array originale in place).
    /// </summary>
    private void VisitAShuffleStatement(AShuffleStmt stmt)
    {
        PushTargetByRef(stmt.Array);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.ASHUFFLE);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione AFILL.
    /// </summary>
    private void VisitAFillStatement(AFillStmt stmt)
    {
        // AFILL(array, valore, [start], [count])
        PushTargetByRef(stmt.Array);
        VisitExpression(stmt.Value);
        
        if (stmt.Start is not null) VisitExpression(stmt.Start);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        if (stmt.Count is not null) VisitExpression(stmt.Count);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AFILL);
        EmitByte((byte)(4));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione ACOPY.
    /// </summary>
    private void VisitACopyStatement(ACopyStmt stmt)
    {
        // ACOPY(sorgente, destinazione, [start_sorg], [count], [start_dest])
        // L'array sorgente viene letto, quello di destinazione viene modificato.
        LoadVariable(stmt.Source);
        PushTargetByRef(stmt.Destination);

        if (stmt.SourceStart is not null) VisitExpression(stmt.SourceStart);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        if (stmt.Count is not null) VisitExpression(stmt.Count);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        if (stmt.DestStart is not null) VisitExpression(stmt.DestStart);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.ACOPY);
        EmitByte((byte)(5));
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione AEVAL.
    /// </summary>
    private void VisitAEvalStatement(AEvalStmt stmt)
    {
        // AEVAL(array, actionFunction [, arity])
        // La subroutine 'actionFunction' potrebbe modificare gli elementi dell'array,
        // quindi l'array deve essere passato per riferimento per rendere le modifiche persistenti.
        PushTargetByRef(stmt.Array);
        VisitExpression(stmt.ActionFunction);

        // Arità del block (parametri dichiarati): il runtime passa (elemento, indice) SOLO
        // ai block a 2+ parametri — il prologo lega gli argomenti in ordine inverso, quindi
        // passare 2 argomenti a un block a 1 parametro legherebbe l'INDICE a quel parametro
        // (regressione arrays18: Tot := Tot + n riceveva 0,1,2 invece di 10,20,30).
        var arity = 1;
        switch (stmt.ActionFunction)
        {
            case LambdaExpr lam:
                arity = lam.Parameters.Length;
                break;
            case FunctionReferenceExpr fre:
                var procDef = _procedureMap.Values.FirstOrDefault(p => p.SymbolId == fre.SymbolId);
                if (procDef != null) arity = procDef.Parameters.Length;
                break;
            case VariableExpr ve:
                for (var i = _locals.Count - 1; i >= 0; i--)
                {
                    if (_locals[i].Name.Lexeme.Equals(ve.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase)
                        && _locals[i].Type is FunctionPointerType fpt)
                    {
                        arity = fpt.ParameterTypes.Length;
                        break;
                    }
                }
                break;
        }
        EmitConstant((double)arity);

        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.AEVAL);
        EmitByte((byte)(3));
        EmitByte((byte)OpCode.OP_POP);
    }
    
    /// <summary>
    /// Metodo helper per pushare un riferimento a un target (variabile o array).
    /// Necessario per le funzioni array che modificano l'array originale.
    /// </summary>
    private void PushTargetByRef(Target target)
    {
        if (target is not VariableTarget vt)
            // ArrayElementTarget è anche valido se gli indici sono vuoti
            if (target is not ArrayElementTarget aet || aet.Indices.Any())
                throw new Exception("Array function target must be a simple variable.");

        var varName = GetTargetToken(target);
        var localIndex = ResolveLocal(varName.Lexeme);
        if (localIndex != -1)
        {
            EmitByte((byte)OpCode.OP_PUSH_REF_LOCAL); EmitShort((ushort)localIndex);
        }
        else
        {
            var globalIndex = ResolveGlobal(varName.Lexeme);
            EmitBytes((byte)OpCode.OP_PUSH_REF_GLOBAL);
            EmitShort(globalIndex);
        }
    }
}