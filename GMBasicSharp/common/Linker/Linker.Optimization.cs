// FILE: Interpreter.Linker.Optimization.cs

namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
                GUIDA ALL'OTTIMIZZAZIONE TRAMITE APPIATTIMENTO DEI CICLI (LOOP FLATTENING)
------------------------------------------------------------------------------------------------------------------------
Questa sezione del compilatore è dedicata a un'importante ottimizzazione: la trasformazione
di cicli `FOR...NEXT` annidati in un'unica macchina a stati lineare. Questo processo,
chiamato "Loop Flattening", riduce drasticamente l'overhead di gestione dei contesti di ciclo
della Virtual Machine, migliorando le performance in scenari con molte iterazioni.

Vengono presentati due approcci distinti con semantiche diverse.

========================================================================================================================
METODO 1: LoopFlatteningOptimizer (Stateful - Variabile di Ciclo Modificabile)
========================================================================================================================
Questo è l'approccio attualmente in uso. Dà priorità alla retrocompatibilità con i dialetti
BASIC tradizionali, dove la variabile contatore di un ciclo (es. `I` in `FOR I=...`)
può essere modificata all'interno del corpo del ciclo per alterarne il flusso.

PRINCIPIO DI FUNZIONAMENTO:
L'ottimizzatore genera una macchina a stati esplicita che simula il comportamento di
due cicli annidati usando le variabili originali (`I` e `J`) come stato. Non c'è un
contatore sintetico. L'inizializzazione, il controllo e l'incremento di `I` e `J`
vengono gestiti tramite istruzioni `LET`, `ForCheckAndJumpStmt` e `JumpStmt`.

ESEMPIO DI CODICE SORGENTE:
--------------------------
10 FOR I = 1 TO 2
20     PRINT "Outer:", I
30     FOR J = 1 TO 3
40         PRINT " Inner:", J
50     NEXT J
60 NEXT I
--------------------------

COSTRUZIONE DELL'AST APPIATTITO (CONCETTUALE):
Il parser prima scompone questo codice in istruzioni atomiche. L'ottimizzatore
identifica il pattern annidato e lo sostituisce con il seguente blocco AST:

1.  SETUP E INIZIALIZZAZIONE
    -   `EnterLoopStmt`:   Crea il contesto per l'intero blocco. Fondamentale per `EXIT FOR`.
                         Contiene le etichette per `CONTINUE` (salta a NEXT I) e `EXIT`.
    -   `LET I = 1`:       Inizializza la variabile del ciclo esterno *una sola volta*.
    -   `JUMP -> ___FOR_OUTER_CHECK_...`: Salta al primo controllo del ciclo esterno.

2.  MACCHINA A STATI DEL CICLO ESTERNO
    -   `LABEL ___FOR_OUTER_CHECK_...`: Punto di ingresso/ritorno per il ciclo esterno.
    -   `ForCheckAndJumpStmt I, ___EXIT_...`: Controlla `I` contro il suo limite. Se superato, esce.
    -   `PRINT "Outer:", I`: Esegue il corpo intermedio.
    -   `LET J = 1`:       Inizializza/Resetta la variabile del ciclo interno.

3.  MACCHINA A STATI DEL CICLO INTERNO
    -   `LABEL ___FOR_INNER_CHECK_...`: Punto di ingresso/ritorno per il ciclo interno.
    -   `ForCheckAndJumpStmt J, ___OUTER_INC_...`: Controlla `J` contro il suo limite. Se superato,
                                                 salta all'incremento del ciclo esterno.
    -   `PRINT " Inner:", J`: Esegue il corpo del ciclo interno.

4.  LOGICA DI INCREMENTO (SIMULAZIONE DEI `NEXT`)
    -   `LABEL ___FOR_INNER_INC_...`:   Etichetta per `CONTINUE` nel ciclo interno.
    -   `NEXT J`:                       Incrementa `J`.
    -   `JUMP -> ___FOR_INNER_CHECK_...`: Ritorna al controllo del ciclo interno.
    -   `LABEL ___FOR_OUTER_INC_...`:   Etichetta per `CONTINUE` nel ciclo esterno.
    -   `NEXT I`:                       Incrementa `I`.
    -   `JUMP -> ___FOR_OUTER_CHECK_...`: Ritorna al controllo del ciclo esterno.

5.  CHIUSURA
    -   `ExitLoopStmt`:    Chiude il contesto del ciclo.
    -   `LABEL ___EXIT_...`: Destinazione per `EXIT FOR` e per la fine naturale del ciclo.

VANTAGGIO: La modifica di `I` o `J` nel corpo del ciclo ha effetto, perché il controllo
           successivo (`ForCheckAndJumpStmt`) userà il valore modificato.
           
GESTIONE GOTO INTERNI:
L'ottimizzatore ora è consapevole dei numeri di riga. Se un `GOTO` punta a una riga che
verrebbe eliminata dal flattening, l'ottimizzatore la preserva aggiungendo un `RemStmt`
segnaposto. Questo garantisce che `RebuildLabelMap` possa registrare il numero di riga,
rendendo il `GOTO` valido a tempo di compilazione. A runtime, il GOTO salterà a
un'operazione innocua e l'esecuzione continuerà dal punto successivo, che è un
comportamento di fallback sicuro.
*/
/// <summary>
/// Contiene le classi e i metodi per le passate di ottimizzazione dell'AST
/// che avvengono durante la fase di compilazione.
/// </summary>
public static partial class Linker
{
    /// <summary>
    /// Ottimizzatore che trasforma cicli FOR annidati in cicli FOR singoli piatti,
    /// migliorando le prestazioni riducendo l'overhead di controllo dei loop.
    /// QUESTA VERSIONE È STATA CORRETTA PER ESSERE CONSERVATIVA E STABILE.
    /// </summary>
    public class LoopFlatteningOptimizer
    {
        private int _loopLabelCounter;

        /// <summary>
        /// Esegue un pass completo di flattening su una lista di statement.
        /// </summary>
        public IReadOnlyList<Stmt> FlattenLoopPass(IReadOnlyList<Stmt> statements)
        {
            var result = new List<Stmt>(statements.Count);
            var i = 0;
            while (i < statements.Count)
            {
                if (statements[i] is EnterLoopStmt enterLoopStmt)
                {
                    if (enterLoopStmt.StartToken.Kind == TokenKind.For)
                    {
                        var (isFlattenable, outerFor, innerFor, outerEnter, intermediateBody, innerBody, stmtsConsumed
                                ) =
                            CanFlattenAt(statements, i);

                        if (isFlattenable)
                        {
                            var flattenedBlock = GenerateStatefulFlattenedLoop(outerFor!, innerFor!, outerEnter!,
                                intermediateBody!, innerBody!);
                            result.AddRange(flattenedBlock);
                            i += stmtsConsumed;
                            continue;
                        }
                    }

                    var endOfBlockIndex = FindEndOfLoopBlock(statements, i, enterLoopStmt.ExitLabel);

                    if (endOfBlockIndex != -1)
                    {
                        for (var j = i; j <= endOfBlockIndex; j++)
                        {
                            result.Add(statements[j]);
                        }

                        i = endOfBlockIndex + 1;
                        continue;
                    }
                }

                result.Add(statements[i]);
                i++;
            }

            return result;
        }

        /// <summary>
        /// Esegue un pass completo di flattening su un dizionario di statement indicizzati per numero di riga.
        /// Utilizza una logica robusta per sincronizzare l'input originale con l'output trasformato e
        /// per preservare i numeri di riga interni al ciclo ottimizzato per garantire la validità dei GOTO.
        /// </summary>
        public Dictionary<int, List<Stmt>> FlattenStatementsByLine(Dictionary<int, List<Stmt>> statementsByLine)
        {
            var flatListWithLineNumbers = statementsByLine
                .OrderBy(kvp => kvp.Key)
                .SelectMany(kvp => kvp.Value.Select(stmt => (Line: kvp.Key, Stmt: stmt)))
                .ToList();

            var flatStatements = flatListWithLineNumbers.Select(t => t.Stmt).ToList();

            var resultList = new List<Stmt>();
            var i = 0;
            while (i < flatStatements.Count)
            {
                var currentStmt = flatStatements[i];
                if (currentStmt is EnterLoopStmt { StartToken.Kind: TokenKind.For })
                {
                    var (isFlattenable, outerFor, innerFor, outerEnter, intermediateBody, innerBody, stmtsConsumed) =
                        CanFlattenAt(flatStatements, i);

                    if (isFlattenable)
                    {
                        var flattenedBlock = GenerateStatefulFlattenedLoop(outerFor!, innerFor!, outerEnter!,
                            intermediateBody!, innerBody!);
                        resultList.AddRange(flattenedBlock);
                        i += stmtsConsumed;
                        continue;
                    }
                }

                resultList.Add(currentStmt);
                i++;
            }

            var newStatementsByLine = new Dictionary<int, List<Stmt>>();
            var resIdx = 0;
            var origIdx = 0;

            while (resIdx < resultList.Count)
            {
                if (origIdx >= flatListWithLineNumbers.Count)
                {
                    var lastLine = flatListWithLineNumbers.Count > 0 ? flatListWithLineNumbers.Last().Line : 0;
                    if (!newStatementsByLine.TryGetValue(lastLine, out var list))
                    {
                        list = [];
                        newStatementsByLine[lastLine] = list;
                    }

                    list.Add(resultList[resIdx]);
                    resIdx++;
                    continue;
                }

                var (originalLine, originalStmt) = flatListWithLineNumbers[origIdx];
                var currentResultStmt = resultList[resIdx];

                var isFlattenedBlockStart = false;
                var originalStmtsConsumed = 0;

                if (originalStmt is EnterLoopStmt { StartToken.Kind: TokenKind.For })
                {
                    var (flattenable, _, _, _, _, _, consumed) = CanFlattenAt(flatStatements, origIdx);
                    if (flattenable)
                    {
                        isFlattenedBlockStart = true;
                        originalStmtsConsumed = consumed;
                    }
                }

                if (isFlattenedBlockStart)
                {
                    if (currentResultStmt is EnterLoopStmt newEnter)
                    {
                        var endOfNewBlockIndex = FindEndOfLoopBlock(resultList, resIdx, newEnter.ExitLabel);
                        var newBlockCount = endOfNewBlockIndex - resIdx + 1;

                        if (!newStatementsByLine.TryGetValue(originalLine, out var targetList))
                        {
                            targetList = [];
                            newStatementsByLine[originalLine] = targetList;
                        }

                        for (var k = 0; k < newBlockCount; k++)
                        {
                            targetList.Add(resultList[resIdx + k]);
                        }

                        // Preserva i numeri di riga interni come REM.
                        for (var j = 1; j < originalStmtsConsumed; j++)
                        {
                            var (lineNumber, stmt) = flatListWithLineNumbers[origIdx + j];
                            var (prevLineNumber, _) = flatListWithLineNumbers[origIdx + j - 1];
                            if (lineNumber != prevLineNumber && !newStatementsByLine.ContainsKey(lineNumber))
                            {
                                newStatementsByLine[lineNumber] = [new RemStmt(new Token(TokenKind.Rem, "REM", "Optimized away", stmt.Position))];
                            }
                        }

                        resIdx += newBlockCount;
                        origIdx += originalStmtsConsumed;
                    }
                    else
                    {
                        AddToLine(newStatementsByLine, originalLine, currentResultStmt);
                        resIdx++;
                        origIdx++;
                    }
                }
                else
                {
                    AddToLine(newStatementsByLine, originalLine, currentResultStmt);
                    resIdx++;
                    origIdx++;
                }
            }

            return newStatementsByLine;
        }

        private void AddToLine(Dictionary<int, List<Stmt>> dict, int line, Stmt stmt)
        {
            if (!dict.TryGetValue(line, out var list))
            {
                list = [];
                dict[line] = list;
            }

            list.Add(stmt);
        }

        /// <summary>
        /// Trova l'indice dell'istruzione `LabelStmt` che corrisponde all'etichetta di uscita,
        /// che definisce la fine completa del blocco di un ciclo.
        /// </summary>
        private static int FindEndOfLoopBlock(IReadOnlyList<Stmt> stmts, int startIndex, Token exitLabel)
        {
            for (var i = startIndex; i < stmts.Count; i++)
            {
                if (stmts[i] is LabelStmt label && label.Label.Lexeme == exitLabel.Lexeme)
                {
                    return i;
                }
            }

            return -1;
        }
        
        /// <summary>
        /// Analizza una sequenza di statement per determinare se contengono una coppia
        /// di cicli FOR immediatamente annidati e senza codice intermedio, idonei per l'appiattimento.
        /// </summary>
        private static (bool, ForStmt?, ForStmt?, EnterLoopStmt?, List<Stmt>?, List<Stmt>?, int) CanFlattenAt(
            IReadOnlyList<Stmt> stmts, int startIndex)
        {
            // Pattern atteso:
            // 1. EnterLoopStmt (esterno)
            // 2. ForStmt (esterno)
            // 3. LabelStmt (start esterno)
            // 4. ForCheckAndJumpStmt (check esterno)
            // 5. EnterLoopStmt (interno)  <-- DEVE ESSERE QUI
            // 6. ForStmt (interno)
            
            if (startIndex + 6 > stmts.Count) return (false, null, null, null, null, null, 0);

            if (stmts[startIndex] is not EnterLoopStmt outerEnter || stmts[startIndex + 1] is not ForStmt outerFor)
                return (false, null, null, null, null, null, 0);
            
            // Cerca il ciclo interno ESATTAMENTE dopo l'header del ciclo esterno
            if (stmts[startIndex + 4] is not EnterLoopStmt innerEnter || stmts[startIndex + 5] is not ForStmt innerFor)
                return (false, null, null, null, null, null, 0);
            
            var endOfOuterLoopIndex = FindEndOfLoopBlock(stmts, startIndex, outerEnter.ExitLabel);
            if (endOfOuterLoopIndex == -1) return (false, null, null, null, null, null, 0);

            var endOfInnerLoopIndex = FindEndOfLoopBlock(stmts, startIndex + 4, innerEnter.ExitLabel);
            if (endOfInnerLoopIndex == -1) return (false, null, null, null, null, null, 0);

            // Verifica che non ci sia codice intermedio
            if (endOfInnerLoopIndex + 4 != endOfOuterLoopIndex)
            {
                 // C'è codice tra NEXT J e NEXT I, non appiattibile
                 return (false, null, null, null, null, null, 0);
            }
            
            if (!IsValidForFlattening(outerFor, innerFor))
                return (false, null, null, null, null, null, 0);

            var bodyStartIndex = startIndex + 4 + 4; // Header esterno + header interno
            var bodyEndIndex = endOfInnerLoopIndex - 4;
            if (bodyEndIndex < bodyStartIndex) bodyEndIndex = bodyStartIndex;

            var innerBody = stmts.Skip(bodyStartIndex).Take(bodyEndIndex - bodyStartIndex).ToList();
            var intermediateBody = new List<Stmt>(); // Nessun corpo intermedio permesso

            var stmtsConsumed = endOfOuterLoopIndex - startIndex + 1;
            return (true, outerFor, innerFor, outerEnter, intermediateBody, innerBody, stmtsConsumed);
        }

        /// <summary>
        /// Genera il loop appiattito stateful che sostituisce i due loop annidati originali.
        /// </summary>
        private List<Stmt> GenerateStatefulFlattenedLoop(ForStmt outerFor, ForStmt innerFor,
            EnterLoopStmt outerEnter, List<Stmt> intermediateBody, List<Stmt> innerBody,
            Dictionary<int, int>? internalLineLabels = null)
        {
            var replacement = new List<Stmt>();

            var outerCheckLabel = GenerateLoopLabel("FOR_OUTER_CHECK");
            var innerCheckLabel = GenerateLoopLabel("FOR_INNER_CHECK");
            var innerIncrementLabel = GenerateLoopLabel("FOR_INNER_INC");

            var outerIncrementLabel = outerEnter.ContinueLabel;
            var exitLabel = outerEnter.ExitLabel;

            replacement.Add(new EnterLoopStmt(outerEnter.StartToken, outerIncrementLabel, exitLabel));
            replacement.Add(new LetStmt([new Assignment(outerFor.Variable, outerFor.Start)], null));
            replacement.Add(new JumpStmt(outerCheckLabel));

            // GenerateStatefulLoopStructure ora è inline per gestire le label
            var outerVarTarget = outerFor.Variable;
            var innerVarTarget = innerFor.Variable;
            
            replacement.Add(new LabelStmt(outerCheckLabel));
            replacement.Add(new ForCheckAndJumpStmt(outerVarTarget.NameCleaned, exitLabel));
            replacement.AddRange(intermediateBody.Select(s => (Stmt)s.DeepClone()));
            replacement.Add(new LetStmt([new Assignment(innerVarTarget, innerFor.Start)], null));
            replacement.Add(new LabelStmt(innerCheckLabel));
            replacement.Add(new ForCheckAndJumpStmt(innerVarTarget.NameCleaned, outerIncrementLabel));
            replacement.AddRange(innerBody.Select(s => (Stmt)s.DeepClone()));
            replacement.Add(new LabelStmt(innerIncrementLabel));
            replacement.Add(new NextStmt(innerVarTarget.NameCleaned));
            replacement.Add(new JumpStmt(innerCheckLabel));
            replacement.Add(new LabelStmt(outerIncrementLabel));
            replacement.Add(new NextStmt(outerVarTarget.NameCleaned));
            replacement.Add(new JumpStmt(outerCheckLabel));

            replacement.Add(new ExitLoopStmt(outerEnter.StartToken));
            replacement.Add(new LabelStmt(exitLabel));

            return replacement;
        }

        /*/// <summary>
        /// Genera la macchina a stati completa per il ciclo appiattito.
        /// </summary>
        private static void GenerateStatefulLoopStructure(List<Stmt> statements, ForStmt outerFor, ForStmt innerFor,
            List<Stmt> intermediateBody, List<Stmt> innerBody,
            Token outerCheckLabel, Token innerCheckLabel, Token innerIncrementLabel, Token outerIncrementLabel,
            Token exitLabel)
        {
            var outerVarTarget = outerFor.Variable;
            var innerVarTarget = innerFor.Variable;

            statements.Add(new LabelStmt(outerCheckLabel));
            statements.Add(new ForCheckAndJumpStmt(outerVarTarget.OriginalToken, exitLabel));
            statements.AddRange(intermediateBody.Select(s => (Stmt)s.DeepClone()));
            statements.Add(new LetStmt([new Assignment(innerVarTarget, innerFor.Start)], null));
            statements.Add(new LabelStmt(innerCheckLabel));
            statements.Add(new ForCheckAndJumpStmt(innerVarTarget.OriginalToken, outerIncrementLabel));
            statements.AddRange(innerBody.Select(s => (Stmt)s.DeepClone()));
            statements.Add(new LabelStmt(innerIncrementLabel));
            statements.Add(new NextStmt(innerVarTarget.OriginalToken));
            statements.Add(new JumpStmt(innerCheckLabel));
            statements.Add(new LabelStmt(outerIncrementLabel));
            statements.Add(new NextStmt(outerVarTarget.OriginalToken));
            statements.Add(new JumpStmt(outerCheckLabel));
        }*/

        /// <summary>
        /// Verifica se due loop FOR sono compatibili per il flattening.
        /// </summary>
        private static bool IsValidForFlattening(ForStmt outerFor, ForStmt innerFor)
        {
            if (ExpressionDependsOnVariable(innerFor.End, outerFor.Variable.NameCleaned.Lexeme))
                return false;
            if (!IsSimpleStep(outerFor.Step) || !IsSimpleStep(innerFor.Step))
                return false;
            return IsSimpleAffineExpr(outerFor.Start) && IsSimpleAffineExpr(outerFor.End) &&
                   IsSimpleAffineExpr(innerFor.Start) && IsSimpleAffineExpr(innerFor.End);
        }

        /// <summary>
        /// Verifica se un'espressione dipende da una specifica variabile.
        /// </summary>
        private static bool ExpressionDependsOnVariable(AstNode node, string variableName)
        {
            return node switch
            {
                VariableExpr varExpr => string.Equals(varExpr.NameCleaned.Lexeme, variableName,
                    StringComparison.OrdinalIgnoreCase),
                BinaryExpr binExpr => ExpressionDependsOnVariable(binExpr.Left, variableName) ||
                                      ExpressionDependsOnVariable(binExpr.Right, variableName),
                UnaryExpr unExpr => ExpressionDependsOnVariable(unExpr.Right, variableName),
                GroupingExpr groupExpr => ExpressionDependsOnVariable(groupExpr.Expression, variableName),
                FunctionExpr funcExpr => funcExpr.Args.Any(arg => ExpressionDependsOnVariable(arg, variableName)),
                ArrayAccessExpr arrExpr => arrExpr.Indices.Any(idx => ExpressionDependsOnVariable(idx, variableName)),
                LetStmt letStmt => letStmt.Assignments.Any(a => ExpressionDependsOnVariable(a.Value, variableName)),
                _ => false
            };
        }

        /// <summary>
        /// Verifica se lo step è semplice (null o costante intera).
        /// </summary>
        private static bool IsSimpleStep(Expr? stepExpr)
        {
            switch (stepExpr)
            {
                case null:
                    return true;
                case NumberExpr numExpr:
                {
                    var intValue = Math.Round(numExpr.Value);
                    const double tolerance = 1e-10;
                    return Math.Abs(numExpr.Value - intValue) < tolerance;
                }
                default:
                    return false;
            }
        }

        /// <summary>
        /// Verifica se un'espressione è affine semplice.
        /// </summary>
        private static bool IsSimpleAffineExpr(Expr expr)
        {
            return expr switch
            {
                NumberExpr => true,
                VariableExpr => true,
                BinaryExpr binExpr when IsArithmeticOperator(binExpr.Op.Kind) =>
                    IsSimpleAffineExpr(binExpr.Left) && IsSimpleAffineExpr(binExpr.Right),
                _ => false
            };
        }

        /// <summary>
        /// Determina se un tipo di token rappresenta un operatore aritmetico di base.
        /// Questo helper viene utilizzato da <see cref="IsSimpleAffineExpr"/> per verificare
        /// la validità delle espressioni nei limiti dei cicli `FOR` prima di tentare l'appiattimento.
        /// </summary>
        /// <param name="tokenKind">Il <see cref="TokenKind"/> da controllare.</param>
        /// <returns>True se il token è +, -, * o /; altrimenti, false.</returns>
        private static bool IsArithmeticOperator(TokenKind tokenKind)
        {
            return tokenKind is TokenKind.Plus or TokenKind.Minus or TokenKind.Multiply or TokenKind.Divide;
        }

        /// <summary>
        /// Genera una label univoca per il loop.
        /// </summary>
        private Token GenerateLoopLabel(string prefix)
        {
            var labelName = $"___{prefix}_{_loopLabelCounter++}";
            return new Token(TokenKind.Identifier, labelName, null, SourcePosition.Unknown);
        }
    }
}