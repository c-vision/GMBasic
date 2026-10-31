// FILE: Interpreter.Parser.Stmt.Torch.cs
// TORCH: comandi legacy a spazio (sintassi a spazio, nessun valore di ritorno):
//   TORCH SET DEVICE "cpu" | "auto" | "cuda" | "rocm" | "mlx"
//   TORCH LOAD LIB
//   TORCH UNLOAD LIB
//   TORCH SET LIB PATH "dir"
//   TORCH INFO
// Complementano le funzioni qualificate TORCH.XXX(...) (lettura).

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher d'ingresso per i comandi legacy `TORCH ...` (consuma la keyword).
    /// </summary>
    internal static Stmt ParseTorchStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Torch, "Internal parser error: Expected 'TORCH' keyword.");
        return ParseTorchSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Dispatcher dei subcomandi TORCH (chiamato dopo che 'TORCH' è stato consumato).
    /// </summary>
    internal static Stmt ParseTorchSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();

        if (commandToken.Kind == TokenKind.Set)
        {
            parser.Advance();
            if (parser.Match(TokenKind.Device))
                return ParseTorchSetDeviceStatement(parser, commandToken);
            if (parser.Match(TokenKind.Lib))
            {
                parser.Consume(TokenKind.Path, "Expected PATH after 'TORCH SET LIB'.");
                return ParseTorchSetLibPathStatement(parser, commandToken);
            }
            throw CreateError(parser.Peek(), "Expected DEVICE or 'LIB PATH' after 'TORCH SET'.");
        }

        if (commandToken.Kind == TokenKind.Load)
        {
            parser.Advance();
            parser.Consume(TokenKind.Lib, "Expected LIB after 'TORCH LOAD'.");
            return new TorchLoadLibStmt(commandToken);
        }

        if (commandToken.Kind == TokenKind.Unload)
        {
            parser.Advance();
            parser.Consume(TokenKind.Lib, "Expected LIB after 'TORCH UNLOAD'.");
            return new TorchUnloadLibStmt(commandToken);
        }

        // TORCH INFO — "INFO" è lexata come TokenKind.IsamInfo (keyword esistente);
        // la riusiamo qui come parola chiave del subcomando INFO.
        if (commandToken.Kind == TokenKind.IsamInfo)
        {
            parser.Advance();
            return new TorchInfoStmt(commandToken);
        }

        throw CreateError(commandToken,
            $"Expected SET, LOAD, UNLOAD or INFO after 'TORCH' (found '{commandToken.Lexeme}').");
    }

    /// <summary>
    /// <c>TORCH SET DEVICE "cpu"</c> — imposta il device attivo (auto/cpu/cuda/rocm/mlx).
    /// </summary>
    private static Stmt ParseTorchSetDeviceStatement(Parser parser, Token setToken)
    {
        var dev = parser.ParseExpression();
        return new TorchSetDeviceStmt(dev, setToken);
    }

    /// <summary>
    /// <c>TORCH SET LIB PATH "dir"</c> — imposta la directory delle librerie ML.
    /// </summary>
    private static Stmt ParseTorchSetLibPathStatement(Parser parser, Token setToken)
    {
        var path = parser.ParseExpression();
        return new TorchSetLibPathStmt(path, setToken);
    }
}
