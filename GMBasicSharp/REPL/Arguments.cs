using System.Collections.Immutable;

namespace Basic.Executor;

public static class ArgsParser
{
    public static CommandLineArgs Parse(string[] args)
    {
        var arguments = new List<string>(args);
        var definesBuilder = ImmutableDictionary.CreateBuilder<string, string?>(StringComparer.OrdinalIgnoreCase);

        bool FindAndRemoveFlag(string flag, string? alias = null) =>
            arguments.Remove(flag) || (alias != null && arguments.Remove(alias));

        var envDefines = Environment.GetEnvironmentVariable("GMBASIC_DEFINES");
        if (!string.IsNullOrWhiteSpace(envDefines))
            foreach (var d in envDefines.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                var parts = d.Split('=', 2);
                definesBuilder[parts[0]] = parts.Length == 2 ? parts[1] : "true";
            }

        var noConsole = FindAndRemoveFlag("-noconsole");
        // Console di avvio: SDL (finestra) SOLO se il programma usa la grafica, se richiesto
        // esplicitamente (--sdl / -c64) o nella REPL interattiva (nessun file). Prima era SDL
        // SEMPRE: un `PRINT "HELLO WORLD"` caricava libgmb_gfx e con essa SDL2/3, mixer e codec
        // audio, crypto/ssl, zstd/brotli/lzma/lz4, MoltenVK (~26 MB) per scrivere una riga.
        var noSdlFlag = FindAndRemoveFlag("-c", "-console");
        var forceSdl = FindAndRemoveFlag("--sdl");
        var useSystemConsole = FindAndRemoveFlag("-s");
        var useC64Mode = FindAndRemoveFlag("-c64");
        var usePCode = FindAndRemoveFlag("--pcode");
        var useAst = FindAndRemoveFlag("--ast");
        var native = FindAndRemoveFlag("-n", "--native");
        var managed = FindAndRemoveFlag("-m", "--managed");
        // -full: distribuzione managed AUTONOMA (closure copiata davvero + framework .NET
        // accanto). Senza, il percorso di esecuzione usa i symlink (nessun costo su disco).
        var full = FindAndRemoveFlag("-full", "--full");
        var verboseExecutor = FindAndRemoveFlag("-e", "-executor");
        var noOptimize = FindAndRemoveFlag("--no-optimize");
        var dumpIl = FindAndRemoveFlag("--dump-il");

        // -bsp <dir>: pre-compila le pagine BSP di una cartella (Program.ParseArgs la legge,
        // ma ArgsParser deve consumare -bsp E il suo valore per non trattarlo come file).
        string? compileBspDir = null;
        var bspIdx = arguments.IndexOf("-bsp");
        if (bspIdx >= 0)
        {
            arguments.RemoveAt(bspIdx);
            if (bspIdx < arguments.Count && !arguments[bspIdx].StartsWith('-'))
            {
                compileBspDir = arguments[bspIdx];
                arguments.RemoveAt(bspIdx);
            }
        }

        var gpmMode = C99GpmMode.Always;
        var gpmFlagIdx = arguments.FindIndex(a => a.Equals("-gpm") || a.StartsWith("-gpm:", StringComparison.OrdinalIgnoreCase));
        if (gpmFlagIdx >= 0)
        {
            var raw = arguments[gpmFlagIdx].StartsWith("-gpm:", StringComparison.OrdinalIgnoreCase)
                ? arguments[gpmFlagIdx]["-gpm:".Length..]
                : (gpmFlagIdx + 1 < arguments.Count ? arguments[gpmFlagIdx + 1] : null);
            if (raw != null && Enum.TryParse<C99GpmMode>(raw, true, out var parsed))
                gpmMode = parsed;
            arguments.RemoveAt(gpmFlagIdx);
            if (raw != null && gpmFlagIdx < arguments.Count && arguments[gpmFlagIdx] == raw)
                arguments.RemoveAt(gpmFlagIdx);
        }
        else
        {
            var envGpm = Environment.GetEnvironmentVariable("GMBASIC_GPM");
            if (!string.IsNullOrWhiteSpace(envGpm)
                && Enum.TryParse<C99GpmMode>(envGpm.Trim(), true, out var envParsed))
                gpmMode = envParsed;
        }

        var gridWidth = FindAndRemoveValue<int>("-w");
        var gridHeight = FindAndRemoveValue<int>("-h");
        var debugPort = FindAndRemoveValue<int>("--debug-tcp");

        // -size W,H (griglia a celle) e -font name,size: SOVRASCRIVONO i settings.
        // Coerenti con i flag console (-c/-s/-c64). Passati al runtime C via env.
        var sizeStr = FindAndRemoveStr("-size");
        if (!string.IsNullOrWhiteSpace(sizeStr))
        {
            var sp = sizeStr!.Split(',');
            if (sp.Length == 2 && int.TryParse(sp[0], out var sw) && int.TryParse(sp[1], out var sh))
            { gridWidth = sw; gridHeight = sh; }
        }
        var fontStr = FindAndRemoveStr("-font");
        string? fontName = null; int? fontSize = null;
        if (!string.IsNullOrWhiteSpace(fontStr))
        {
            var fp = fontStr!.Split(',');
            if (fp.Length >= 1 && !string.IsNullOrWhiteSpace(fp[0])) fontName = fp[0].Trim();
            if (fp.Length >= 2 && int.TryParse(fp[1], out var fsz)) fontSize = fsz;
        }

        // --target <wasm|native|c99> — V1.1 WASM: gmb prog.bas -target wasm -o prog.wasm
        string? target = null;
        var targetIdx = arguments.FindIndex(a => a == "--target" || a == "-target");
        if (targetIdx >= 0 && targetIdx + 1 < arguments.Count)
        {
            target = arguments[targetIdx + 1];
            arguments.RemoveAt(targetIdx + 1);
            arguments.RemoveAt(targetIdx);
        }
        // -o <output> — output per wasm/c99 (salvato in OutputFile)
        string? outputFile = null;
        var oIdx = arguments.IndexOf("-o");
        if (oIdx >= 0 && oIdx + 1 < arguments.Count)
        {
            outputFile = arguments[oIdx + 1];
            arguments.RemoveAt(oIdx + 1);
            arguments.RemoveAt(oIdx);
        }

        string? runCommand = null;
        var runIndex = arguments.IndexOf("--run");
        if (runIndex >= 0 && runIndex + 1 < arguments.Count)
        {
            runCommand = arguments[runIndex + 1];
            arguments.RemoveAt(runIndex + 1);
            arguments.RemoveAt(runIndex);
        }

        // Moduli IL esterni da caricare (ripetibile): --il-module <path.dll>. Il loader
        // generico li registra come namespace BASIC (IlModuleLoader.Load).
        var ilModules = new List<string>();
        for (var i = arguments.Count - 1; i >= 0; i--)
        {
            if (arguments[i] == "--il-module" && i + 1 < arguments.Count)
            {
                ilModules.Add(arguments[i + 1]);
                arguments.RemoveAt(i + 1);
                arguments.RemoveAt(i);
            }
        }
        ilModules.Reverse();

        for (var i = arguments.Count - 1; i >= 0; i--)
        {
            var arg = arguments[i];
            if (arg.StartsWith("-D", StringComparison.OrdinalIgnoreCase) && arg.Length > 2)
            {
                var d = arg[2..]; var parts = d.Split('=', 2);
                definesBuilder[parts[0]] = parts.Length == 2 ? parts[1] : "true";
                arguments.RemoveAt(i);
            }
        }

        // -noconsole: esecuzione SENZA console (headless). Ripristina la capacita' del vecchio
        // interprete (NoConsole -> Headless): GMBASIC_HEADLESS=1 salta SDL/video/host grafico e
        // la console interattiva. Programma eseguito in batch, nessuna finestra aperta.
        if (noConsole) Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", "1");

        // -size / -font -> env per il runtime C (override dei settings, come i flag console).
        if (gridWidth.HasValue && gridHeight.HasValue)
            Environment.SetEnvironmentVariable("GMB_CONSOLE_SIZE", $"{gridWidth.Value},{gridHeight.Value}");
        if (fontName != null || fontSize != null)
            Environment.SetEnvironmentVariable("GMB_CONSOLE_FONT", $"{(fontName ?? "")},{(fontSize ?? 0)}");

        var fileName = arguments.FirstOrDefault(a => !a.StartsWith('-'));

        // SDL solo se serve: file con grafica, --sdl, -c64 (console grafica) o REPL interattiva.
        var useSdl = !noConsole && (forceSdl || useC64Mode || noSdlFlag == false && fileName is null)
                     || (!noConsole && fileName is not null && !noSdlFlag && ProgramUsesGraphics(fileName));

        // Ripristino selezione console d'avvio (flag legacy -c/-s/-c64): mappa sul runtime C
        // via GMB_CONSOLE (0=%SDL default, 1=%C64, 2=%ANSI, 3=%SYSTEM). Solo se un flag e' presente
        // (non sovrascrive un GMB_CONSOLE impostato a mano).
        if (useC64Mode || useSystemConsole || !useSdl)
            Environment.SetEnvironmentVariable("GMB_CONSOLE", useC64Mode ? "1" : (!useSdl ? "2" : "3"));

        return new CommandLineArgs
        {
            FileName = fileName, NoConsole = noConsole, UseSdl = useSdl,
            UseSystemConsole = useSystemConsole, UseC64Mode = useC64Mode,
            GridWidth = gridWidth, GridHeight = gridHeight, DebugPort = debugPort,
            FontName = fontName, FontSize = fontSize,
            VerboseExecutor = verboseExecutor, Defines = definesBuilder.ToImmutable(),
            RunCommand = runCommand, UsePCodeVm = usePCode, UseAst = useAst, Managed = managed, Full = full,
            Native = native, GpmMode = gpmMode, NoOptimize = noOptimize, DumpIl = dumpIl,
            CompileBspDir = compileBspDir, IlModules = ilModules, Target = target, OutputFile = outputFile
        };

        string? FindAndRemoveStr(string flag)
        {
            var idx = arguments.IndexOf(flag);
            if (idx >= 0 && idx + 1 < arguments.Count)
            { var v = arguments[idx + 1]; arguments.RemoveAt(idx + 1); arguments.RemoveAt(idx); return v; }
            return null;
        }

        T? FindAndRemoveValue<T>(string flag) where T : struct
        {
            var idx = arguments.IndexOf(flag);
            if (idx >= 0 && idx + 1 < arguments.Count)
                try { var v = (T?)Convert.ChangeType(arguments[idx + 1], typeof(T)); arguments.RemoveAt(idx + 1); arguments.RemoveAt(idx); return v; } catch { }
            return null;
        }
    }
    /// <summary>Il programma usa la GRIGLIA GRAFICA? Se sì la console deve essere SDL (finestra).
    /// Scansione CONSERVATIVA del sorgente: prima parola di ogni riga (commenti BASIC esclusi).
    /// In caso di dubbio/errore di lettura ritorna true (meglio una finestra in più che comandi
    /// grafici silenziosamente no-op).</summary>
    private static bool ProgramUsesGraphics(string path)
    {
        try
        {
            var full = Path.GetFullPath(path);
            if (!File.Exists(full)) return true;
            if (new FileInfo(full).Length > 4 * 1024 * 1024) return true;   // troppo grande: prudenza
            var graphics = new[]
            {
                "SCREEN", "GRAPHICS", "SPRITE", "SPRITESET", "PSET", "PRESET", "CIRCLE", "PAINT",
                "DRAW", "POLYGON", "PCOPY", "SCREENSET", "PALETTE", "MOUSE", "WINDOW", "IMAGE",
                "BITMAP", "TILE", "FONT", "PLOT", "CHART", "VIDEO", "SCROLL", "POINT", "LINE"
            };
            foreach (var raw in File.ReadLines(full))
            {
                var line = raw;
                var comment = line.IndexOf('\'');
                if (comment >= 0) line = line[..comment];
                line = line.Trim();
                if (line.Length == 0) continue;
                var first = line.Split(new[] { ' ', '\t', '(' }, StringSplitOptions.RemoveEmptyEntries)[0]
                                 .ToUpperInvariant();
                if (Array.IndexOf(graphics, first) >= 0) return true;
            }
            return false;
        }
        catch { return true; }
    }

}

public class CommandLineArgs
{
    public string? FileName { get; set; }
    public bool NoConsole { get; set; }

    public bool UseSdl { get; set; }
    public bool UseSystemConsole { get; set; }
    public bool UseC64Mode { get; set; }
    public int? GridWidth { get; set; }
    public int? GridHeight { get; set; }
    public string? FontName { get; set; }   // -font name,size
    public int? FontSize { get; set; }
    public int? DebugPort { get; set; }
    public bool VerboseExecutor { get; set; }
    public ImmutableDictionary<string, string?> Defines { get; set; } = ImmutableDictionary<string, string?>.Empty;
    public string? RunCommand { get; set; }
    public bool UsePCodeVm { get; set; }
    public bool UseAst { get; set; }
    public bool Managed { get; set; }
    public bool Native { get; set; }
    /// <summary>Distribuzione managed completa: closure copiata (non symlink) + framework .NET
    /// accanto all'eseguibile. Default false = esecuzione/iterazione (symlink, nessun costo).</summary>
    public bool Full { get; set; }
    public bool NoOptimize { get; set; }
    public bool DumpIl { get; set; }
    public string? CompileBspDir { get; set; }
    public List<string> IlModules { get; set; } = [];
    public C99GpmMode GpmMode { get; set; } = C99GpmMode.Always;
    public string? Target { get; set; }
    public string? OutputFile { get; set; }
    public bool IsScriptMode => FileName != null;
    public bool IsPrgMode => FileName != null && FileName.EndsWith(".prg", StringComparison.OrdinalIgnoreCase);
}
