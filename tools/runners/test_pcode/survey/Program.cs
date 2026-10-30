using System;
using System.Linq;
using System.Text.RegularExpressions;
using Basic;
using Compiler;

class Program
{
    static int Main()
    {
        var stdlib = new StdLib();
        var seen = new System.Collections.Generic.Dictionary<string, (string ns, TokenKind tk)>(StringComparer.OrdinalIgnoreCase);
        foreach (var (ns, procs) in stdlib.BuiltinProcedures)
            foreach (var p in procs)
                if (p is NativeProcedureDefinition np)
                    seen.TryAdd(np.Name.Lexeme, (ns, np.BuiltinKind));
        Console.WriteLine($"canoniche (nomi unici): {seen.Count}");
        // mapping Compiler
        var compiler = Regex.Replace(File.ReadAllText("common/Compiler/Compiler.Expressions.Functions.cs"), @"//[^\n]*", "");
        var tkmap = new System.Collections.Generic.HashSet<string>();
        foreach (Match m in Regex.Matches(compiler, @"TokenKind\.([A-Za-z0-9_]+)\s*,\s*BuiltInFunction\.([A-Za-z0-9_]+)"))
            tkmap.Add(m.Groups[1].Value);
        // funzioni canoniche con BuiltinKind NON nel mapping
        var nomap = seen.Where(kv => !tkmap.Contains(kv.Value.tk.ToString())).ToList();
        Console.WriteLine($"BuiltinKind TokenKind senza BuiltInFunction nel mapping: {nomap.Count}");
        foreach (var (n, (ns, tk)) in nomap.Take(25))
            Console.WriteLine($"  {n} ({ns}) tk={tk}");
        return 0;
    }
}
