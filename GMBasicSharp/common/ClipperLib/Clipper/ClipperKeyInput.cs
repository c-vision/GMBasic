using CommonLib.Consoles.Tui;
using CommonLib.Screens;

namespace ClipperLib.Clipper;

/// <summary>
/// Sorgente di tasti a basso livello usata da <see cref="TerminalReader"/>.
///
/// SCOPO: dissaccare il driver TUI (TerminalReader) dal lettore console fisico, così
/// l'host (console / SDL / headless / runtime con <c>IKeyboardService</c>) può iniettare
/// la propria sorgente senza che ClipperLib dipenda dal runtime. Il default è
/// <see cref="ConsoleKeySource"/> (System.Console), che preserva il percorso legacy.
///
/// Vedi TODO "(open) - TerminalReader-Via-KeyboardService" (item input #6).
/// </summary>
public interface IClipperKeySource
{
    /// <summary>True se l'input è reindirizzato (pipe/file, niente terminale interattivo).</summary>
    bool InputRedirected { get; }

    /// <summary>True se c'è almeno un tasto disponibile senza bloccare.</summary>
    bool KeyAvailable { get; }

    /// <summary>Legge un singolo tasto (bloccante).</summary>
    ConsoleKeyInfo ReadKey(bool intercept);
}

/// <summary>Sorgente default basata su <see cref="System.Console"/> (comportamento legacy invariato).</summary>
public sealed class ConsoleKeySource : IClipperKeySource
{
    public bool InputRedirected => Console.IsInputRedirected;
    public bool KeyAvailable => Console.KeyAvailable;
    public ConsoleKeyInfo ReadKey(bool intercept) => Console.ReadKey(intercept);
}

/// <summary>Holder statico iniettabile della sorgente di TerminalReader.</summary>
public static class ClipperKeyInput
{
    /// <summary>
    /// Sorgente attiva. Default = <see cref="ConsoleKeySource"/>. L'host (repl/runtime/SDL)
    /// può settarla quando esiste un <c>IKeyboardService</c>; si reimposta a Console appena
    /// l'host smette di essere attivo (vedi <see cref="UseConsole"/>).
    /// </summary>
    public static IClipperKeySource Current { get; set; } = new ConsoleKeySource();

    /// <summary>Reimposta la sorgente al default console.</summary>
    public static void UseConsole() => Current = new ConsoleKeySource();
}
