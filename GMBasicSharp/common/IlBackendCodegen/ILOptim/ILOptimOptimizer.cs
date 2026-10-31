// IlBackend/ILOptim/ILOptimOptimizer.cs — Post-compilation optimization via ILOptim

using ILOptim;
using ILOptim.AsmIO;
using ILOptim.Passes;

namespace Basic.IlBackend.ILOptim;

public static class ILOptimOptimizer
{
    public static (bool Success, int MethodCount, string? Error) Optimize(string assemblyPath)
    {
        try
        {
            using var resolver = new ModuleResolver();
            resolver.AddTrustedSearchPaths();

            var module = resolver.Load(assemblyPath);

            var comp = new Compilation(module, new ConsoleLogger(), new CompilationSettings());
            var pm = new PassManager { Compilation = comp };

            pm.AddPasses()
                .Apply<SimplifyCFG>()
                .Apply<SsaPromotion>();

            pm.AddPasses(applyIndependently: true)
                .Apply<InlineMethods>();

            pm.AddPasses()
                .Apply<SimplifyInsts>()
                .Apply<SimplifyCFG>()
                .Apply<DeadCodeElim>()
                .Apply<ScalarReplacement>()
                .Apply<LoopStrengthReduction>();

            var methods = PassManager.GetCandidateMethodsFromIL(module, (_, m) => m.Module == module);
            pm.Run(methods);

            module.Save(assemblyPath, savePdb: false);

            return (true, methods.Count, null);
        }
        catch (Exception ex)
        {
            return (false, 0, ex.Message);
        }
    }
}
