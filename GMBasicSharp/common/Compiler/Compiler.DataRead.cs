// FILE: Interpreter.Compiler.Compiler.DataRead.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione DATA.
    /// Questa visita è speciale: non emette P-Code, ma popola la tabella
    /// dei dati nel CompiledChunk. Questo deve avvenire in un passaggio preliminare.
    /// </summary>
    private void VisitDataStatement(DataStmt stmt)
    {
        // Itera su ogni espressione nella clausola DATA.
        // Poiché i valori DATA devono essere costanti, li valutiamo qui.
        // Un vero compilatore avrebbe un "constant evaluator".
        // Per ora, ci aspettiamo che siano letterali.
        foreach (var valueExpr in stmt.Values)
        {
            switch (valueExpr)
            {
                case NumberExpr n: _chunk.Data.Add(n.Value); break;
                case StringExpr s: _chunk.Data.Add(s.Value); break;
            }
        }
    }

    /// <summary>
    /// Compila un'istruzione READ.
    /// </summary>
    private void VisitReadStatement(ReadStmt stmt)
    {
        foreach (var target in stmt.Variables)
        {
            // Emette l'istruzione per leggere il prossimo valore DATA.
            // La VM lo lascerà in cima alla stack.
            EmitByte((byte)OpCode.OP_READ);
            
            // Assegna il valore letto alla variabile di destinazione.
            // READ è un'assegnazione implicita.
            AssignVariable(target, isLet: false);
        }
    }

    /// <summary>
    /// Compila un'istruzione RESTORE [riga | etichetta]: imposta il puntatore DATA alla prima
    /// voce del pool successiva o uguale alla riga indicata (RESTORE nudo → posizione 0).
    /// Il target viene risolto a COMPILE-TIME in un indice del data pool (operando di OP_RESTORE):
    /// riga numerica via _dataLineToIndex, etichetta via la riga dell'etichetta.
    /// </summary>
    private void VisitRestoreStatement(RestoreStmt stmt)
    {
        var position = 0;
        if (stmt.Target is not null)
        {
            if (stmt.Target.Kind == TokenKind.Number && int.TryParse(stmt.Target.Lexeme, out var line))
            {
                position = GetDataPositionAtOrAfterLine(line);
            }
            else if (stmt.Target.Kind == TokenKind.Identifier
                     && _labelLineByLabel.TryGetValue(stmt.Target.Lexeme, out var labelLine))
            {
                position = GetDataPositionAtOrAfterLine(labelLine);
            }
            else
            {
                throw new NotSupportedException(
                    $"Compiler P-Code: RESTORE {stmt.Target.Lexeme} richiede un numero di riga o un'etichetta della stessa procedura.");
            }
        }
        EmitBytes((byte)OpCode.OP_RESTORE, (byte)(position >> 8), (byte)(position & 0xFF));
    }

    /// <summary>Indice nel data pool della prima voce DATA su/oltre la riga data
    /// (stessa semantica di GetDataPositionAtOrAfterLine del backend IL). Se nessuna riga
    /// DATA raggiunge il target, restituisce l'indice di fine pool (READ → OUT OF DATA).</summary>
    private int GetDataPositionAtOrAfterLine(int targetLine)
    {
        foreach (var (line, index) in _dataLineToIndex.OrderBy(kv => kv.Key))
        {
            if (line >= targetLine) return index;
        }
        return _chunk.Data.Count;
    }
}