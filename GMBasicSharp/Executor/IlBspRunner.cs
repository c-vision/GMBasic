// Executor/IlBspRunner.cs — path BSP sul BACKEND IL (GMB_BSP_BACKEND=il).
// IlBackendRuntime (dove vive BspDispatch.ServeBspPage) non può referenziare Executor per
// layering, quindi l'host C# registra qui il compilatore BSP→IL: compila il BASIC BSP in un
// BasicProgram (Orchestrator.CompileFromString) e poi in un assembly IL caricabile in-process
// (IlProgramCompiler.CompileToAssembly). A ogni richiesta ServeBspPageIlBackend semina i globali
// BSP e invoca la entry generata (lineare per pagine top-level, driver GPM v2 se may-park).
using System.Reflection;
using System.Runtime.CompilerServices;
using Basic;
using Basic.IlBackend;
using Basic.IlBackend.PCode;

namespace Basic.ExecutorV2;

/// <summary>
/// Registra il compilatore BSP→IL su <see cref="BspDispatch"/>. Attivo automaticamente al
/// caricamento dell'assembly Executor (che la REPL referenzia): l'hook resta però LATO P-Code
/// fino a quando una pagina .bsp viene servita con <c>GMB_BSP_BACKEND=il</c>.
/// </summary>
public static class IlBspRunner
{
#pragma warning disable CA2255 // ModuleInitializer intenzionale: registra il compilatore all'load dell'assembly
    [ModuleInitializer]
    internal static void Register()
    {
        BspDispatch.SetIlBspCompiler((basicCode, fileName) =>
        {
            var program = Orchestrator.CompileFromString(basicCode, fileName, null);
            // skipEntryInitialization: la pagina gira in-process dentro il server — la entry
            // NON deve rifare PrepareGraphicsHost/host/ResetProgramState (svuoterebbe i globali
            // del server: un FETCH della pagina verso la stessa API risponderebbe vuoto).
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "BspIl_" + Guid.NewGuid().ToString("N"), out _,
                types: null, skipEntryInitialization: true);
            using var ms = new MemoryStream();
            asmBuilder.Save(ms);
            return (Assembly.Load(ms.ToArray()), program);
        });
    }
#pragma warning restore CA2255
}
