// FILE: REPL/CliDaemon.cs
// CLI `gmb daemon ...` — gestione dei daemon GMBasic come servizi del SO.
// Prima slice: registry in ~/.gmbasic/daemons/<name>/ + manifest.json + LaunchAgent macOS
// (launchd) via launchctl. Linux systemd --user e Windows in una fase successiva.
//
// Uso:
//   gmb daemon install <name> <file.bas> [--port N] [--no-start]
//   gmb daemon start|stop|restart|status <name>
//   gmb daemon logs <name> [--follow]
//   gmb daemon list
//   gmb daemon uninstall <name> [--purge]
using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace Basic.Executor;

internal static class CliDaemon
{
    private static string Home => Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    private static string RegistryRoot => Path.Combine(Home, ".gmbasic", "daemons");
    private static string DaemonDir(string name) => Path.Combine(RegistryRoot, name);
    private static string PlistPath(string name) => Path.Combine(Home, "Library", "LaunchAgents", $"it.gmb.{name}.plist");
    private static string Label(string name) => $"it.gmb.{name}";
    private static string Uid => Environment.GetEnvironmentVariable("UID") ?? "501";

    public static bool TryDispatch(string[] args)
    {
        if (args.Length < 2 || !args[0].Equals("daemon", StringComparison.OrdinalIgnoreCase)) return false;
        try
        {
            switch (args[1].ToLowerInvariant())
            {
                case "install": Install(args); return true;
                case "uninstall": case "delete": Uninstall(args); return true;
                case "start":
                    if (!File.Exists(PlistPath(args[2]))) { Console.Error.WriteLine($"[daemon] non installato: {args[2]}"); return true; }
                    Launchctl("bootstrap", $"gui/{Uid}", PlistPath(args[2]), silent: true);
                    Launchctl("kickstart", "-k", $"gui/{Uid}/{Label(args[2])}");
                    Thread.Sleep(1500); RecordPid(args[2]);
                    return true;
                case "stop": Launchctl("bootout", $"gui/{Uid}/{Label(args[2])}"); KillTree(ReadBas(args[2])); return true;
                case "restart":
                    Launchctl("bootout", $"gui/{Uid}/{Label(args[2])}", silent: true);
                    KillTree(ReadBas(args[2]));
                    Thread.Sleep(500);
                    Launchctl("bootstrap", $"gui/{Uid}", PlistPath(args[2]), silent: true);
                    Launchctl("kickstart", "-k", $"gui/{Uid}/{Label(args[2])}");
                    Thread.Sleep(1500); RecordPid(args[2]);
                    return true;
                case "status": Status(args); return true;
                case "logs": Logs(args); return true;
                case "list": List(); return true;
                default:
                    Console.Error.WriteLine("Uso: gmb daemon install|start|stop|restart|status|logs|list|uninstall ...");
                    return true;
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[daemon] errore: {ex.Message}");
            return true;
        }
    }

    private static void Install(string[] args)
    {
        if (args.Length < 4) { Console.Error.WriteLine("Uso: gmb daemon install <name> <file.bas> [--port N] [--no-start]"); return; }
        var name = args[2];
        var bas = Path.GetFullPath(args[3]);
        if (!File.Exists(bas)) { Console.Error.WriteLine($"[daemon] file non trovato: {bas}"); return; }
        var noStart = args.Contains("--no-start");
        var port = ArgInt(args, "--port", 0);

        Directory.CreateDirectory(DaemonDir(name));
        var logDir = Path.Combine(DaemonDir(name), "logs");
        Directory.CreateDirectory(logDir);

        var manifest = new Dictionary<string, object?>
        {
            ["name"] = name,
            ["file"] = bas,
            ["workdir"] = Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(bas))) ?? Environment.CurrentDirectory,
            ["port"] = port,
            ["plist"] = PlistPath(name),
            ["log_out"] = Path.Combine(logDir, "out.log"),
            ["log_err"] = Path.Combine(logDir, "err.log"),
            ["created_at"] = DateTime.UtcNow.ToString("o"),
        };
        File.WriteAllText(Path.Combine(DaemonDir(name), "manifest.json"),
            JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true }));

        var dotnet = FindDotnet();
        var replDll = Path.Combine(Environment.CurrentDirectory, "REPL", "bin", "Debug", "net10.0", "REPL.dll");
        var vmHost = Path.Combine(Environment.CurrentDirectory, "native", "gmb_vm_c", "build", "gmb_vm_host");
        var plist = $"""
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>{Label(name)}</string>
  <key>ProgramArguments</key>
  <array>
    <string>{dotnet}</string>
    <string>{replDll}</string>
    <string>{bas}</string>
  </array>
  <key>WorkingDirectory</key><string>{(string)manifest["workdir"]!}</string>
  <key>EnvironmentVariables</key>
  <dict><key>GMBASIC_VM_HOST</key><string>{vmHost}</string></dict>
  <key>RunAtLoad</key><true/>
  <key>StandardOutPath</key><string>{(string)manifest["log_out"]!}</string>
  <key>StandardErrorPath</key><string>{(string)manifest["log_err"]!}</string>
  <key>ProcessType</key><string>Background</string>
</dict>
</plist>
""";
        File.WriteAllText(PlistPath(name), plist);
        Launchctl("bootout", $"gui/{Uid}/{Label(name)}");   // se già presente
        KillTree(bas);                                      // niente orfani/duplicati
        Launchctl("bootstrap", $"gui/{Uid}", PlistPath(name));
        Console.WriteLine($"[daemon] installato: {name} (label {Label(name)})");
        Console.WriteLine($"[daemon] manifest: {Path.Combine(DaemonDir(name), "manifest.json")}");
        Console.WriteLine($"[daemon] plist: {PlistPath(name)}");
        if (!noStart) { Launchctl("kickstart", "-k", $"gui/{Uid}/{Label(name)}"); Thread.Sleep(1500); RecordPid(name); }
        Console.WriteLine($"[daemon] stato: {(IsRunning(name) ? "running" : "loaded")}");
    }

    private static void Uninstall(string[] args)
    {
        if (args.Length < 3) { Console.Error.WriteLine("Uso: gmb daemon uninstall <name> [--purge]"); return; }
        var name = args[2];
        Launchctl("bootout", $"gui/{Uid}/{Label(name)}");
        KillTree(ReadBas(name));
        if (File.Exists(PlistPath(name))) File.Delete(PlistPath(name));
        if (args.Contains("--purge") && Directory.Exists(DaemonDir(name))) Directory.Delete(DaemonDir(name), true);
        Console.WriteLine($"[daemon] rimosso: {name}{(args.Contains("--purge") ? " (purge)" : " (artefatti/log mantenuti)")}");
    }

    private static void Status(string[] args)
    {
        if (args.Length < 3) { Console.Error.WriteLine("Uso: gmb daemon status <name>"); return; }
        var name = args[2];
        var loaded = File.Exists(PlistPath(name));
        Console.WriteLine($"{name,-20} loaded={loaded} running={IsRunning(name)} label={Label(name)}");
    }

    private static void Logs(string[] args)
    {
        if (args.Length < 3) { Console.Error.WriteLine("Uso: gmb daemon logs <name> [--follow]"); return; }
        var name = args[2];
        var outLog = Path.Combine(DaemonDir(name), "logs", "out.log");
        var errLog = Path.Combine(DaemonDir(name), "logs", "err.log");
        if (args.Contains("--follow")) { TailFollow(outLog, errLog); return; }
        if (File.Exists(outLog)) Console.WriteLine(File.ReadAllText(outLog));
        if (File.Exists(errLog)) Console.Error.WriteLine(File.ReadAllText(errLog));
    }

    private static void List()
    {
        if (!Directory.Exists(RegistryRoot)) { Console.WriteLine("(nessun daemon)"); return; }
        foreach (var d in Directory.GetDirectories(RegistryRoot).OrderBy(x => x))
        {
            var n = Path.GetFileName(d);
            Console.WriteLine($"{n,-20} loaded={File.Exists(PlistPath(n))} running={IsRunning(n)}");
        }
    }

    /// <summary>PID del job launchd (dotnet) dal manifest/launchctl.</summary>
    private static int JobPid(string name)
    {
        var (_, o) = Launchctl("print", $"gui/{Uid}/{Label(name)}", silent: true);
        foreach (var line in o.Split('\n'))
        {
            var t = line.Trim();
            if (t.StartsWith("pid = ") && int.TryParse(t[6..], out var v)) return v;
        }
        return 0;
    }

    /// <summary>Primo figlio (gmb_vm_host) di un PID.</summary>
    private static int ChildPid(int parent)
    {
        try
        {
            var psi = new ProcessStartInfo("pgrep") { RedirectStandardOutput = true };
            psi.ArgumentList.Add("-P"); psi.ArgumentList.Add(parent.ToString());
            using var p = Process.Start(psi)!;
            var o = p.StandardOutput.ReadToEnd().Trim().Split('\n');
            p.WaitForExit();
            if (o.Length > 0 && int.TryParse(o[0], out var v)) return v;
        }
        catch { }
        return 0;
    }

    /// <summary>Registra nel manifest il PID del processo-app (vm_host) per uno stop mirato.</summary>
    private static void RecordPid(string name)
    {
        var jp = JobPid(name);
        var cp = jp > 0 ? ChildPid(jp) : 0;
        var pid = cp > 0 ? cp : jp;
        if (pid <= 0) return;
        try
        {
            var mf = Path.Combine(DaemonDir(name), "manifest.json");
            using var doc = JsonDocument.Parse(File.ReadAllText(mf));
            var dict = JsonSerializer.Deserialize<Dictionary<string, object?>>(doc.RootElement.GetRawText()) ?? new();
            dict["pid"] = pid;
            File.WriteAllText(mf, JsonSerializer.Serialize(dict, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    private static int ManifestPid(string name)
    {
        try
        {
            var mf = Path.Combine(DaemonDir(name), "manifest.json");
            if (!File.Exists(mf)) return 0;
            using var doc = JsonDocument.Parse(File.ReadAllText(mf));
            return doc.RootElement.TryGetProperty("pid", out var p) && p.TryGetInt32(out var v) ? v : 0;
        }
        catch { return 0; }
    }

    /// <summary>Uccide SOLO il daemon: PID registrato (vm_host, SIGTERM → stop ordinato) + il launcher.</summary>
    private static void KillTree(string bas)
    {
        var name = Path.GetFileNameWithoutExtension(bas);
        var appPid = ManifestPid(name);
        if (appPid > 0) { Run("kill", "-TERM", appPid.ToString()); Thread.Sleep(400); Run("kill", "-9", appPid.ToString()); }
        try
        {
            var psi = new ProcessStartInfo("pgrep") { RedirectStandardOutput = true };
            psi.ArgumentList.Add("-f"); psi.ArgumentList.Add(bas);
            using var p = Process.Start(psi)!;
            var pids = p.StandardOutput.ReadToEnd().Split('\n', StringSplitOptions.RemoveEmptyEntries);
            p.WaitForExit();
            foreach (var pid in pids)
            {
                Run("pkill", "-P", pid);   // figli (gmb_vm_host)
                Run("kill", "-9", pid);
            }
        }
        catch { /* best effort */ }
    }

    private static void Run(string cmd, params string[] args)
    {
        try
        {
            var psi = new ProcessStartInfo(cmd) { RedirectStandardOutput = true, RedirectStandardError = true };
            foreach (var a in args) psi.ArgumentList.Add(a);
            using var p = Process.Start(psi);
            p?.WaitForExit(2000);
        }
        catch { /* best effort */ }
    }

    private static bool IsRunning(string name)
    {
        var (rc, _) = Launchctl("print", $"gui/{Uid}/{Label(name)}", silent: true);
        return rc == 0;
    }

    private static (int rc, string output) Launchctl(string verb, string arg1, string? arg2 = null, bool silent = false)
    {
        var psi = new ProcessStartInfo("launchctl") { RedirectStandardOutput = true, RedirectStandardError = true };
        psi.ArgumentList.Add(verb);
        psi.ArgumentList.Add(arg1);
        if (arg2 != null) psi.ArgumentList.Add(arg2);
        using var p = Process.Start(psi)!;
        var o = p.StandardOutput.ReadToEnd() + p.StandardError.ReadToEnd();
        p.WaitForExit();
        if (!silent && p.ExitCode != 0 && verb != "bootout")
            Console.Error.WriteLine($"[launchctl {verb}] rc={p.ExitCode}: {o.Trim()}");
        return (p.ExitCode, o);
    }

    private static void TailFollow(params string[] files)
    {
        var tasks = files.Where(File.Exists).Select(f => Task.Run(() =>
        {
            using var fs = new FileStream(f, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            fs.Seek(0, SeekOrigin.End);
            using var sr = new StreamReader(fs);
            while (true) { var line = sr.ReadLine(); if (line != null) Console.WriteLine(line); else Thread.Sleep(200); }
        })).ToArray();
        Task.WaitAll(tasks);
    }

    /// <summary>Legge il campo file dal manifest del daemon (per KillTree).</summary>
    private static string ReadBas(string name)
    {
        try
        {
            var mf = Path.Combine(DaemonDir(name), "manifest.json");
            if (!File.Exists(mf)) return "";
            using var doc = JsonDocument.Parse(File.ReadAllText(mf));
            return doc.RootElement.TryGetProperty("file", out var f) ? f.GetString() ?? "" : "";
        }
        catch { return ""; }
    }

    private static int ArgInt(string[] args, string key, int def)
    {
        var i = Array.IndexOf(args, key);
        return i >= 0 && i + 1 < args.Length && int.TryParse(args[i + 1], out var v) ? v : def;
    }

    private static string FindDotnet()
    {
        foreach (var p in new[] { "/usr/local/share/dotnet/dotnet" })
            if (File.Exists(p)) return p;
        return "/usr/bin/env";   // fallback: env dotnet (ProgramArguments avrà "dotnet" dopo)
    }
}
