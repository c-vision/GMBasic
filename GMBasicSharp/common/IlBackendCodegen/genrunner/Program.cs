// Runner di TEST: genera la DLL di un programma .bas con il NUOVO codegen
// (IlBackendCodegen → GMBasic.IlBackend codegen-only) che referenzia il runtime
// separato (GMBasic.Runtime.IL). Serve a misurare il binario nativo senza codegen.
using System;
using Basic;

class GenRunner
{
    static void Main(string[] args)
    {
        var source = File.ReadAllText(args[0]);
        var fileName = Path.GetFileName(args[0]);
        var program = Orchestrator.CompileFromString(source, fileName, null, false);
        if (program?.MainModule == null) { Console.Error.WriteLine("compile failed"); Environment.Exit(1); }

        var outDir = args.Length > 1 ? args[1] : ".";
        Directory.CreateDirectory(outDir);
        var baseName = Path.GetFileNameWithoutExtension(args[0]);
        var dll = Basic.IlBackend.IlProgramCompiler.Compile(program, args[0], outDir, baseName, null);
        Console.WriteLine($"DLL: {dll}");
    }
}
