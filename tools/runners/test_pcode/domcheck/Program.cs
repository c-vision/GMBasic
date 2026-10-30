using System;
using System.IO;

class Program
{
    static int Main(string[] args)
    {
        var file = args.Length > 0 ? args[0] : null;
        string src;
        if (file != null) src = File.ReadAllText(file);
        else src = args.Length > 1 ? args[1] : throw new Exception("servono file o sorgente");
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_domcheck_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(tmp, src);
        var program = Basic.Orchestrator.Compile(tmp, null, false, "il");
        if (program?.MainModule == null) { Console.WriteLine("COMPILATION FAILED"); try { File.Delete(tmp); } catch {} return 1; }
        var chunk = new global::Compiler.Compiler().Compile(program);
        Basic.IlBackend.IlRuntime.SetCurrent(new Basic.Runtime.HeadlessRuntimeHost(Console.Out, new Basic.Runtime.RuntimeConfig()));
        var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
        Console.WriteLine("---"); Console.WriteLine("SUCCESS: " + r.Success); Console.WriteLine(r.Output);
        try { File.Delete(tmp); } catch {}
        return r.Success ? 0 : 1;
    }
}
