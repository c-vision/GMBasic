// Agents/CodeRunner.cs — Seam del runner BASIC generato (CODE mode) per il mondo C#.
// Layering senza cicli: common/AI definisce l'interfaccia; l'HOST che possiede il pipeline
// di compilazione (progetto Executor/REPL, mondo C#) la registra in CodeRunner.Instance.
// AskCua la usa se presente; altrimenti errore esplicito. Nessuna condivisione col mondo C.
using System;

namespace Basic.Agents;

public interface ICodeRunner
{
    /// <summary>Compila ed esegue un programma BASIC (sorgente) nel mondo C#, ritornando
    /// l'output (o un messaggio d'errore esplicito).</summary>
    string RunBasic(string source);
}

public static class CodeRunner
{
    public static ICodeRunner? Instance { get; set; }
}
