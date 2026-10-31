// test_vmrun — esegue un sample .bas sulla VM/C (gmb_vm_host) via `vmtest`, con
// timeout garantito + cleanup completo (Chromium CDP 9222, gmb_vm_host, profili /tmp).
//
// Uso:
//   dotnet run --project test_vmrun -- <file.bas> [timeout_secondi]
//
// Perché esiste: il `vmtest` nudo (Conformance) lascia il processo appeso e i residui
// (Chromium, gmb_vm_host) in giro quando il sample usa browser/WS/FETCH. Questo wrapper
// avvia il browser CDP, kill il processo dopo il timeout e ripulisce tutto.
using System.Diagnostics;
using System.Linq;
using System.Net.Http;

var file = args.Length > 0 ? args[0] : null;
if (file is null)
{
    Console.Error.WriteLine("uso: test_vmrun <file.bas> [timeout_secondi]");
    return 1;
}
var procArgs = "";   /* argomenti del PROGRAMMA: dopo il file, inoltrati a `vmtest` → gmb_vm_host */
{
    /* Convenzione: `<file.bas> [timeout_secondi] [argomenti del programma...]`.
     * Se il 2° argomento non è un numero, TUTTI gli argomenti dopo il file sono del programma
     * (così `test_vmrun x.bas --out y` passa --out al programma, non lo scarta).
     * Parità con la gamba IL: COMMAND(1) = primo argomento dopo il file. */
    var startIdx = 0;
    if (args.Length > 1 && int.TryParse(args[1], out _)) startIdx = 2; else startIdx = 1;
    if (args.Length > startIdx)
        procArgs = " " + string.Join(" ", args.Skip(startIdx).Select(a => "\"" + a.Replace("\"", "\\\"") + "\""));
}
var timeoutSec = args.Length > 1 && int.TryParse(args[1], out var t) ? t : 20;
if (!File.Exists(file)) { Console.Error.WriteLine($"file non trovato: {file}"); return 1; }
var fileFull = Path.GetFullPath(file);   /* il child gira con CWD=root: path assoluto */
// CWD = cartella del sample (come test_ilrun): gli output/input relativi ("work/...") risolvono.
{
    var dir = Path.GetDirectoryName(Path.GetFullPath(file));
    if (!string.IsNullOrEmpty(dir)) Directory.SetCurrentDirectory(dir);
}

// root del repo = risale fino a trovare GMBasic.sln
var root = AppContext.BaseDirectory;
while (root is not null && !File.Exists(Path.Combine(root, "GMBasic.sln"))
       && !(File.Exists(Path.Combine(root, "GMBasicSharp", "GMBasicSharp.slnx"))
            && Directory.Exists(Path.Combine(root, "GMBasic"))))
    root = Path.GetDirectoryName(root);
if (root is null) { Console.Error.WriteLine("root GMBasic non trovata"); return 1; }
// Anti-stale: `gmb_vm_host` è linkato BY-PATH (nessuna dipendenza di build verso i sorgenti C),
// quindi un binario vecchio esegue il runtime vecchio IN SILENZIO. Caso reale 2026-10-26:
// `system/commands.bas` dava solo `cc-ok` su VM/C per un host non ricostruito (falso negativo).
// La logica vive in VmHostFreshness.cs, condivisa con la suite di parità xUnit.
string host;
try { host = GmbTooling.VmHostFreshness.Ensure(root); }
catch (Exception ex) { Console.Error.WriteLine(ex.Message); return 1; }
Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", "1");
Environment.SetEnvironmentVariable("GMBASIC_VM_HOST", host);

// 1. avvia il browser CDP SOLO se il sample lo usa (BROWSER./CDP/9222): per gli altri test
// l'attesa del /json costava fino a 30 s e faceva sembrare "appeso" l'harness.
Process? browser = null;
var srcText = string.Empty;
try { srcText = File.ReadAllText(fileFull); } catch { }
var needsBrowser = srcText.Contains("BROWSER.", StringComparison.Ordinal)
                || srcText.Contains("9222", StringComparison.Ordinal)
                || srcText.Contains("CDP", StringComparison.Ordinal);
try
{
    if (!needsBrowser) throw new InvalidOperationException("skip: sample senza browser");
    browser = Process.Start(new ProcessStartInfo("bash", "plugins/browser/launch_chromium.sh 9222")
    {
        WorkingDirectory = root,
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
    });
    using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(1) };
    for (var i = 0; i < 30; i++)
    {
        try { var r = await http.GetAsync("http://127.0.0.1:9222/json/version"); if (r.IsSuccessStatusCode) break; }
        catch { /* browser non ancora su */ }
        Thread.Sleep(1000);
    }
}
catch { /* browser già su o launch non disponibile: si prosegue */ }

// 2. esegue il vmtest con il timeout.
// NB: si lancia la DLL compilata (non `dotnet run`): il wrapper `dotnet run` non inoltra
// stdin in modo affidabile e l'input scriptato andrebbe perso. Build incrementale prima.
var confProj = Path.Combine(root, "tools", "runners", "GMBASIC.Conformance", "GMBASIC.Conformance.csproj");
var buildPsi = new ProcessStartInfo("dotnet", $"build \"{confProj}\" -v q --nologo")
{
    WorkingDirectory = root,
    RedirectStandardOutput = true,
    RedirectStandardError = true,
    UseShellExecute = false,
};
using (var bp = Process.Start(buildPsi))
{
    if (bp is null) { Console.Error.WriteLine("build del conformance non avviata"); return 1; }
    var bo = bp.StandardOutput.ReadToEnd();
    var be = bp.StandardError.ReadToEnd();
    bp.WaitForExit();
    if (bp.ExitCode != 0)
    {
        Console.Error.WriteLine("build GMBASIC.Conformance FALLITA:");
        Console.Error.WriteLine(string.IsNullOrWhiteSpace(be) ? bo : be);
        return 1;
    }
}
var projDir = Path.GetDirectoryName(confProj)!;
var confDll = Directory.GetFiles(Path.Combine(projDir, "bin"), "GMBASIC.Conformance.dll",
                                 SearchOption.AllDirectories)
                      .OrderByDescending(f => File.GetLastWriteTimeUtc(f))
                      .FirstOrDefault();
if (confDll is null) { Console.Error.WriteLine("GMBASIC.Conformance.dll non trovata dopo la build"); return 1; }

var psi = new ProcessStartInfo("dotnet", $"\"{confDll}\" vmtest \"{fileFull}\"" + procArgs)
{
    WorkingDirectory = root,
    RedirectStandardOutput = true,
    RedirectStandardError = true,
    UseShellExecute = false,
};
psi.Environment["GMBASIC_HEADLESS"] = "1";
psi.Environment["GMBASIC_VM_HOST"] = host;
// Parità col lato C# (stessa convenzione di tests/GMBASIC.IlBackend.Tests/MiniTestsParityTests):
// il runner gestito usa SOLO i provider fixture, non l'intero config/providers.json. Senza,
// gmb_vm_host carica providers.json e i sample che dipendono dal SET dei provider (es.
// ProviderWeightedRotation) divergono per ambiente, non per codice.
psi.Environment["GMBASIC_TEST_FIXTURES"] = "1";
// STDIN SCRIPTATO: se stdin è una pipe/file (es. `printf 'ab\r' | test_vmrun file.bas`)
// lo inoltriamo al child (vmtest → gmb_vm_host) byte per byte. Se stdin è un terminale
// non tocchiamo nulla: si comporta come prima.
var forwardStdin = Console.IsInputRedirected;
// Il pipe non attraversa i processi .NET (handle non ereditato dal VM host): se stdin è
// rediretto leggiamo lo script QUI e lo passiamo al child tramite l'hook già usato dalle
// altre gambe (GMB_TUI_SCRIPT → typeahead TUI del runtime), che non dipende da fd ereditati.
if (forwardStdin)
{
    /* ATTENZIONE: con stdin rediretto ma NESSUN produttore che chiude (pipe aperta dal
     * chiamante) `ReadToEnd()` blocca per sempre. Si legge con un timeout breve: se non
     * arrivano dati si prosegue senza script (comportamento identico a prima). */
    try
    {
        var src = Console.OpenStandardInput();
        var readTask = Task.Run(() =>
        {
            using var sr = new StreamReader(src, System.Text.Encoding.UTF8, detectEncodingFromByteOrderMarks: false);
            return sr.ReadToEnd();
        });
        if (readTask.Wait(500) && readTask.Result.Length > 0)
            psi.Environment["GMB_TUI_SCRIPT"] = readTask.Result;
        else
            try { src.Dispose(); } catch { }   /* sblocca il lettore rimasto appeso */
    }
    catch { /* stdin non leggibile: si prosegue senza script */ }
}
// TORCH: libgmb_torch ha una LC_RPATH stale verso il torch di una virtualenv (stub) → dyld
// carica il torch sbagliato e il processo ABORTA (SIGSEGV, "Arresto anomalo del processo
// host"). Indichiamo la dir ML col torch corretto (GMB_TORCH_PATH) e, su macOS, la search
// path dyld (DYLD_LIBRARY_PATH) — devono essere presenti al LANCIO del processo VM.
{
    var rid = System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture switch
    {
        System.Runtime.InteropServices.Architecture.Arm64 => OperatingSystem.IsMacOS() ? "osx-arm64" : "linux-arm64",
        System.Runtime.InteropServices.Architecture.X64 => OperatingSystem.IsMacOS() ? "osx-x64" : "linux-x64",
        _ => "osx-arm64",
    };
    // Cache utente `~/.gmbasic/libs` (default di GmbiPaths.LibsDir): i componenti stanno in
    // `<cache>/<componente>/<rid>/` (ml = torch), le librerie in `<cache>/<rid>/`; in sviluppo
    // si aggiunge `GMBasic/build/libs/<rid>/`. `libs-runtime` e `Resources/libs` non esistono
    // più nel checkout (2026-10-30).
    var libsCache = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmbasic", "libs");
    var mlDir = Path.Combine(libsCache, "ml", rid);
    var libsDir = Path.Combine(libsCache, rid);
    var buildDir = Path.Combine(root, "GMBasic", "build", "libs", rid);
    psi.Environment["GMB_TORCH_PATH"] = Environment.GetEnvironmentVariable("GMB_TORCH_PATH") ?? mlDir;
    if (OperatingSystem.IsMacOS())
    {
        var dirs = new[] { mlDir, libsDir, buildDir }.Where(Directory.Exists);
        var prev = Environment.GetEnvironmentVariable("DYLD_LIBRARY_PATH");
        var joined = string.Join(":", dirs);
        psi.Environment["DYLD_LIBRARY_PATH"] = string.IsNullOrEmpty(prev) ? joined : $"{joined}:{prev}";
    }
}

var p = Process.Start(psi);
if (p is null) { Console.Error.WriteLine("impossibile avviare il vmtest"); return 1; }
var outTask = p.StandardOutput.ReadToEndAsync();
var errTask = p.StandardError.ReadToEndAsync();
var timedOut = !p.WaitForExit(timeoutSec * 1000);
if (timedOut)
{
    try { p.Kill(true); } catch { /* già terminato */ }
    Console.Error.WriteLine($"!! TIMEOUT ({timeoutSec}s) — processo kill");
}
var stdout = await outTask;
var stderr = await errTask;
var status = timedOut ? 124 : p.ExitCode;

// 3. Cleanup limitato ai processi avviati da QUESTO wrapper. vmtest attende il proprio
// gmb_vm_host; in caso di timeout Kill(true) sopra termina soltanto il suo albero.
// Un pkill globale poteva uccidere anche gli host di altre esecuzioni (exit=137).
try { if (browser is { HasExited: false }) browser.Kill(entireProcessTree: true); } catch { }
browser?.Dispose();

Console.Write(stdout);
if (stderr.Length > 0) Console.Error.Write(stderr);
Console.Error.WriteLine($"exit={status}");
return timedOut ? 124 : status;
