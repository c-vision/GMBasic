// test_astdump — dumpa l'AST risolto di un file .bas (debug).
// Uso: dotnet run --project test_astdump -- <file.bas>
using System;
using System.Linq;

var file = args.Length > 0 ? args[0] : "samples/Functions/FunctionsAdvanced.bas";
var program = Basic.Orchestrator.Compile(Path.GetFullPath(file), null, false);
if (program?.MainModule != null)
    Basic.Parser.DumpASTHierarchical(program, showAllModules: false, useConsole: true);
else
    Console.WriteLine("compilazione fallita");
