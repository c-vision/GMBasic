// Executor/ConsoleProgressBar.cs — renderer di progresso su una riga console,
// riusabile da CLI e REPL. Usa ModelDownloader.DownloadProgress. Senza TTY si
// disattiva (solo l'esito finale). Pattern: '■' per la parte fatta.

using System;
using Basic.ModelDownload;

namespace Basic.Executor;

public sealed class ConsoleProgressBar
{
    private readonly bool _tty;
    private readonly int _width;
    private string _last = "";

    public ConsoleProgressBar(int width = 40)
    {
        _tty = !Console.IsOutputRedirected && !Console.IsErrorRedirected;
        _width = Math.Clamp(width, 10, 60);
    }

    public bool Enabled => _tty;

    public void Report(ModelDownloader.DownloadProgress p)
    {
        if (!_tty) return;
        var pct = Math.Clamp(p.Percent, 0, 100);
        var filled = (int)Math.Round(_width * pct / 100.0);
        var bar = new string('█', filled).PadRight(_width, '░');
        var line = $"[{bar}] {pct,3:F0}%  {Fmt(p.BytesDone),9}/{Fmt(p.BytesTotal),9}  {p.CurrentFile ?? ""}";
        if (line.Length > _last.Length) line = line.PadRight(_last.Length);
        Console.Write("\r" + line);
        _last = line;
    }

    public void Finish()
    {
        if (_tty && _last.Length > 0) Console.WriteLine();
        _last = "";
    }

    private static string Fmt(long b) => b >= 1 << 20 ? $"{b / (double)(1 << 20):F0}MB" : $"{b}B";
}
