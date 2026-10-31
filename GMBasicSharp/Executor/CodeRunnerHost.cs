// Executor/CodeRunnerHost.cs — runner BASIC generato (CODE mode, mondo C#).
// Registra su Basic.Agents.CodeRunner.Instance un runner che compila da stringa
// (Orchestrator.CompileFromString) → assembly IL in-process → invoca la entry, catturando
// l'output della console. Pattern IlBspRunner; nessun legame col mondo C.
using System.IO;
using System.Runtime.CompilerServices;
using System.Reflection;
using Basic;
using Basic.Agents;
using Basic.IlBackend;

namespace Basic.ExecutorV2;

public static class CodeRunnerHost
{
#pragma warning disable CA2255
    [ModuleInitializer]
    internal static void Register()
    {
        CodeRunner.Instance = new Runner();
    }
#pragma warning restore CA2255

    private sealed class Runner : ICodeRunner
    {
        public string RunBasic(string source)
        {
            try
            {
                // Percorso PCode in-process (come test_ilrun): chunk + PCodeVmRunner.Run
                var program = Orchestrator.CompileFromString(source, "agent_code.bas", null);
                var chunk = new global::Compiler.Compiler().Compile(program);
                var result = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                return (result.Output ?? "").Trim();
            }
            catch (Exception ex)
            {
                return "error: " + ex.Message;
            }
        }
    }
}
