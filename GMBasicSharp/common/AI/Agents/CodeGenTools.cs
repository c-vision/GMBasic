// FILE: Basic.Agents.CodeGenTools
// B2.2 — CodeGenTools: WRITE/COMPILE/RUN + sandbox ASK/AUTO/YOLO per CodeGenLoop
// Usa reflection per Orchestrator/IlBackend per evitare dipendenza circolare AI → Executor/IlBackendRuntime

using System.IO;

namespace Basic.Agents;

public enum CodeGenSandbox { Ask, Auto, Yolo }

public static class CodeGenTools
{
    public static string ToolWrite(string path, string content, CodeGenSandbox sandbox = CodeGenSandbox.Auto)
    {
        if (string.IsNullOrWhiteSpace(path)) return "Error: path vuota";
        if (sandbox == CodeGenSandbox.Ask)
            Basic.Agents.AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, "CodeGen", $"WRITE {path} ({content.Length} chars) — sandbox Ask");
        try
        {
            var dir = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(path, content);
            return $"Wrote {content.Length} chars to {path}";
        }
        catch (Exception ex) { return $"Error WRITE: {ex.Message}"; }
    }

    private static object? InvokeOrchestratorCompile(string path)
    {
        var orchType = Type.GetType("Basic.Orchestrator, Executor");
        if (orchType == null) orchType = Type.GetType("Basic.Orchestrator, GMBasic");
        if (orchType == null)
        {
            // Fallback: l'Orchestrator può essere compilato NELL'assembly corrente — i progetti
            // di test includono Executor/Orchestrator.cs via <Compile Include=...>, quindi il
            // tipo NON vive in un assembly chiamato "Executor". Cerca tra gli assembly caricati
            // (parità AI VERIFY anche negli harness xUnit).
            foreach (var asm in System.AppDomain.CurrentDomain.GetAssemblies())
            {
                orchType = asm.GetType("Basic.Orchestrator", throwOnError: false);
                if (orchType != null) break;
            }
        }
        if (orchType == null) throw new InvalidOperationException("Orchestrator non trovato (Executor/GMBasic)");
        var method = orchType.GetMethod("Compile", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
        if (method == null) throw new InvalidOperationException("Orchestrator.Compile non trovato");
        return method.Invoke(null, new object[] { path, null, false, "il" });
    }

    public static (bool ok, string diagnostics) ToolCompile(string path)
    {
        if (!File.Exists(path)) return (false, $"File not found: {path}");
        try
        {
            InvokeOrchestratorCompile(path);
            return (true, "Compile OK");
        }
        catch (Exception ex)
        {
            var inner = ex.InnerException ?? ex;
            return (false, inner.Message);
        }
    }

    public static (bool ok, string output, string diagnostics) ToolRun(string path)
    {
        if (!File.Exists(path)) return (false, "", $"File not found: {path}");
        try
        {
            var prog = InvokeOrchestratorCompile(path);
            var ilCompilerType = Type.GetType("Basic.IlBackend.IlProgramCompiler, GMBasic.IlBackend");
            if (ilCompilerType == null) return (false, "", "IlProgramCompiler non trovato");
            var compileMethod = ilCompilerType.GetMethod("CompileToAssembly", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            if (compileMethod == null) return (false, "", "CompileToAssembly non trovato");
            var args = new object[] { prog!, "CodeGenRun_" + Guid.NewGuid().ToString("N"), null };
            var asmBuilder = compileMethod.Invoke(null, args);
            if (asmBuilder == null) return (false, "", "CompileToAssembly returned null");
            var tmp = Path.Combine(Path.GetTempPath(), "gmb_codegen_run");
            Directory.CreateDirectory(tmp);
            var dll = Path.Combine(tmp, Path.GetFileNameWithoutExtension(path) + ".dll");
            var saveMethod = asmBuilder.GetType().GetMethod("Save", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
            if (saveMethod == null) return (false, "", "Save non trovato");
            using (var fs = File.Create(dll)) saveMethod.Invoke(asmBuilder, new object[] { fs });
            var loaded = System.Reflection.Assembly.LoadFile(dll);
            var main = loaded.GetType("GeneratedProgram")?.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            if (main == null) return (false, "", "Main not found");
            var sw = new StringWriter();
            var old = Console.Out;
            Exception? inner = null;
            try
            {
                Console.SetOut(sw);
                var ilRuntimeType = Type.GetType("Basic.IlBackend.IlRuntime, GMBasic.Runtime.IL");
                var setCurrent = ilRuntimeType?.GetMethod("SetCurrent", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
                var hostType = Type.GetType("Basic.Runtime.HeadlessRuntimeHost, GMBasic.Runtime");
                var configType = Type.GetType("Basic.Runtime.RuntimeConfig, GMBasic.Runtime");
                if (setCurrent != null && hostType != null && configType != null)
                {
                    var host = Activator.CreateInstance(hostType, Console.Out, Activator.CreateInstance(configType));
                    setCurrent.Invoke(null, new object[] { host! });
                }
                main.Invoke(null, null);
            }
            catch (System.Reflection.TargetInvocationException tie) { inner = tie.InnerException; }
            finally { Console.SetOut(old); }
            if (inner != null) return (false, sw.ToString(), inner.Message);
            return (true, sw.ToString(), "Run OK");
        }
        catch (Exception ex)
        {
            var inner = ex.InnerException ?? ex;
            return (false, "", inner.Message);
        }
    }

    public static string ToolExecute(string path, string content, CodeGenSandbox sandbox = CodeGenSandbox.Auto)
    {
        var w = ToolWrite(path, content, sandbox);
        if (w.StartsWith("Error")) return w;
        var (okC, diagC) = ToolCompile(path);
        if (!okC) return $"Compile failed: {diagC}";
        var (okR, outR, diagR) = ToolRun(path);
        if (!okR) return $"Run failed: {diagR}\nOutput: {outR}";
        return outR;
    }
}
