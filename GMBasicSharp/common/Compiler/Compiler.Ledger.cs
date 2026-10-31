// FILE: Interpreter.Compiler.Compiler.Ledger.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per tutte le istruzioni del sottosistema LEDGER.
/// </summary>
public sealed partial class Compiler
{
    private const byte LEDGER_ADD_POSTING_ARG_COUNT = 2; // Conto, Importo

    /// <summary>Emette l'IMPORTO di un posting Ledger come UN solo valore (il syscall si
    /// aspetta Conto+Importo). Un ForeignAmountExpr ("100.00 USD @ 0.95") è importo × tasso
    /// (il motore bilancia in valuta base): senza questo, VisitExpression(ForeignAmountExpr)
    /// pushava DUE valori (Amount+ExchangeRate) e il conteggio argomenti del syscall andava
    /// fuori → LEDGER_ADD/EDIT fallivano (ID 0). Parità IlProgramCompiler.LedgerAmountValue.</summary>
    private void EmitLedgerPostingAmount(Expr? amount)
    {
        if (amount is ForeignAmountExpr fa)
        {
            VisitExpression(fa.Amount);
            if (fa.ExchangeRate is not null)
            {
                VisitExpression(fa.ExchangeRate);
                EmitByte((byte)OpCode.OP_MULTIPLY);
            }
        }
        else if (amount is not null)
        {
            VisitExpression(amount);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // auto-bilanciamento
        }
    }

    /// <summary>
    /// Compila un'istruzione LEDGER OPEN.
    /// </summary>
    private void VisitLedgerOpenStatement(LedgerOpenStmt stmt)
    {
        VisitExpression(stmt.FilePath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_OPEN); EmitByte((byte)1);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER ADD.
    /// </summary>
    private void VisitLedgerAddStatement(LedgerAddStmt stmt)
    {
        // Handle
        VisitExpression(stmt.Handle);
        // Date
        VisitExpression(stmt.Date);
        // Description
        VisitExpression(stmt.Description);

        // Postings: Conto, Importo
        foreach (var posting in stmt.Postings)
        {
            VisitExpression(posting.Account);
            EmitLedgerPostingAmount(posting.Amount);
        }

        // Numero totale di argomenti sulla stack: Handle + Date + Desc + (Postings * 2)
        var argCount = (byte)(3 + stmt.Postings.Length * LEDGER_ADD_POSTING_ARG_COUNT);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_ADD); EmitByte((byte)(argCount));

        AssignVariable(stmt.NewIdVariable, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER REGISTER.
    /// </summary>
    private void VisitLedgerRegisterStatement(LedgerRegisterStmt stmt)
    {
        VisitExpression(stmt.Handle);
        if (stmt.Query is not null) VisitExpression(stmt.Query);
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Nessuna query
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_REGISTER); EmitByte((byte)2);
        AssignVariable(stmt.DestinationArray, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER BALANCE.
    /// </summary>
    private void VisitLedgerBalanceStatement(LedgerBalanceStmt stmt)
    {
        VisitExpression(stmt.Handle);
        if (stmt.Query is not null) VisitExpression(stmt.Query);
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Nessuna query
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_BALANCE); EmitByte((byte)2);
        AssignVariable(stmt.DestinationArray, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER GET.
    /// </summary>
    private void VisitLedgerGetStatement(LedgerGetStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.TransactionId);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_GET); EmitByte((byte)2);
        AssignVariable(stmt.DestinationArray, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER EDIT.
    /// </summary>
    private void VisitLedgerEditStatement(LedgerEditStmt stmt)
    {
        // Handle
        VisitExpression(stmt.Handle);
        // Transaction ID
        VisitExpression(stmt.TransactionId);
        // Date
        VisitExpression(stmt.Date);
        // Description
        VisitExpression(stmt.Description);

        // Postings
        foreach (var posting in stmt.Postings)
        {
            VisitExpression(posting.Account);
            EmitLedgerPostingAmount(posting.Amount);
        }

        var argCount = (byte)(4 + stmt.Postings.Length * LEDGER_ADD_POSTING_ARG_COUNT);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_EDIT); EmitByte((byte)(argCount));

        AssignVariable(stmt.NewIdVariable, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER DELETE.
    /// </summary>
    private void VisitLedgerDeleteStatement(LedgerDeleteStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.TransactionId);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_DELETE); EmitByte((byte)2); EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER SAVE.
    /// </summary>
    private void VisitLedgerSaveStatement(LedgerSaveStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_SAVE); EmitByte((byte)1); EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER CLOSE.
    /// </summary>
    private void VisitLedgerCloseStatement(LedgerCloseStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_CLOSE); EmitByte((byte)1); EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER PACK.
    /// </summary>
    private void VisitLedgerPackStatement(LedgerPackStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_PACK); EmitByte((byte)1); EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER ACCOUNTS.
    /// </summary>
    private void VisitLedgerAccountsStatement(LedgerAccountsStmt stmt)
    {
        VisitExpression(stmt.Handle);
        if (stmt.RootAccount is not null) VisitExpression(stmt.RootAccount);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_ACCOUNTS); EmitByte((byte)2);
        AssignVariable(stmt.DestinationArray, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione LEDGER VALIDATE.
    /// </summary>
    private void VisitLedgerValidateStatement(LedgerValidateStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_VALIDATE); EmitByte((byte)1);
        AssignVariable(stmt.DestinationVariable, isLet: false);
    }

    private void VisitLedgerBudgetStatement(LedgerBudgetStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Account);
        VisitExpression(stmt.Amount);
        VisitExpression(stmt.Period);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_BUDGET); EmitByte((byte)4); EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitLedgerVarianceStatement(LedgerVarianceStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Account);
        VisitExpression(stmt.Period);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_VARIANCE); EmitByte((byte)3);
        AssignVariable(stmt.DestinationVariable, isLet: false);
    }

    private void VisitLedgerPlotStatement(LedgerPlotStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Query);
        VisitExpression(stmt.ChartType);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_PLOT); EmitByte((byte)3);
        AssignVariable(stmt.DestinationVariable, isLet: false);
    }

    private void VisitLedgerImportStatement(LedgerImportStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.CsvPath);
        VisitExpression(stmt.MappingRules);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LEDGER_IMPORT); EmitByte((byte)3);
        if (stmt.DestinationVariable is not null)
        {
            AssignVariable(stmt.DestinationVariable, isLet: false);
        }
    }
}