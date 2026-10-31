// FILE: Basic.Runtime.IServices.ISandboxService.cs

namespace Basic.Runtime;

/// <summary>Servizio sandbox per il BASIC Runtime (esecuzione isolata).</summary>
public interface ISandboxService
{
    string Create(string os, string? image = null);
    SandboxExecResult Exec(string id, string command, int timeoutMs = 30000);
    byte[] Screenshot(string id);
    void Mouse(string id, string action, int x, int y);
    void Keyboard(string id, string text);
    void CopyTo(string id, string localPath, string remotePath);
    void Destroy(string id);
}

/// <summary>Risultato dell'esecuzione di un comando in sandbox.</summary>
public record SandboxExecResult(int ExitCode, string Stdout, string Stderr);
