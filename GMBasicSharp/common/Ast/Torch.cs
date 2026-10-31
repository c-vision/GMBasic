// FILE: Interpreter.Ast.Torch.cs
// Comandi legacy TORCH (sintassi a spazio, nessun valore di ritorno):
//   TORCH SET DEVICE "cpu" | "auto" | "cuda" | "rocm" | "mlx"
//   TORCH LOAD LIB
//   TORCH UNLOAD LIB
//   TORCH SET LIB PATH "dir"
//   TORCH INFO
// Complementano le funzioni qualificate TORCH.XXX(...) (lettura).

using System.Text;

namespace Basic;

public sealed record TorchSetDeviceStmt(Expr Device, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"TORCH SET DEVICE {Device}";
    public override AstNode DeepClone() => new TorchSetDeviceStmt((Expr)Device.DeepClone(), (Token)Token.DeepClone());
}

public sealed record TorchLoadLibStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "TORCH LOAD LIB";
    public override AstNode DeepClone() => new TorchLoadLibStmt((Token)Token.DeepClone());
}

public sealed record TorchUnloadLibStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "TORCH UNLOAD LIB";
    public override AstNode DeepClone() => new TorchUnloadLibStmt((Token)Token.DeepClone());
}

public sealed record TorchSetLibPathStmt(Expr LibPath, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"TORCH SET LIB PATH {LibPath}";
    public override AstNode DeepClone() => new TorchSetLibPathStmt((Expr)LibPath.DeepClone(), (Token)Token.DeepClone());
}

public sealed record TorchInfoStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "TORCH INFO";
    public override AstNode DeepClone() => new TorchInfoStmt((Token)Token.DeepClone());
}
