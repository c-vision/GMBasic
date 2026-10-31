// FILE: Interpreter.Compiler.Compiler.Crypto.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni crittografiche.
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione KEYGEN.
    /// </summary>
    private void VisitKeyGenStatement(KeyGenStmt stmt)
    {
        // Compila l'espressione dell'algoritmo (es. "RSA") e, se presente, la dimensione chiave.
        // KeySize era ignorata (bug: mai compilata), la VM usava sempre un default hardcoded.
        VisitExpression(stmt.Algorithm);
        var hasKeySize = stmt.KeySize != null;
        if (hasKeySize) VisitExpression(stmt.KeySize!);

        // Emette la syscall. La VM lascerà due valori sulla stack:
        // prima l'handle pubblico, poi quello privato.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.KEY_GEN); EmitByte((byte)(hasKeySize ? 2 : 1));

        // Assegna gli handle alle variabili di destinazione.
        // Poiché sono sulla stack in ordine [pubblico, privato],
        // dobbiamo assegnarli in ordine inverso.
        AssignVariable(stmt.PrivateKeyHandle, isLet: false);
        AssignVariable(stmt.PublicKeyHandle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione KEYCLOSE.
    /// </summary>
    private void VisitKeyCloseStatement(KeyCloseStmt stmt)
    {
        // Compila l'espressione dell'handle da chiudere.
        VisitExpression(stmt.KeyHandle);

        // Emette la syscall.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.KEY_CLOSE); EmitByte((byte)1);
    }
}