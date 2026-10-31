using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: CommonLib.Keyboard.IKeyboardService.cs


namespace CommonLib.Keyboard;

/// <summary>Servizio tastiera unificato per VM, Console e Scripting.</summary>
public interface IKeyboardService : IDisposable
{
    // ═══ Sincrono (VM / BRT) ═══
    
    /// <summary>Legge un singolo tasto (bloccante).</summary>
    TerminalKeyInfo ReadKey();

    /// <summary>Legge un singolo tasto con timeout.</summary>
    TerminalKeyInfo? ReadKey(int timeoutMs);

    /// <summary>Legge una linea di testo (bloccante, con prompt opzionale).</summary>
    string? ReadLine(string? prompt);

    /// <summary>Controlla se un tasto è disponibile (non bloccante).</summary>
    TerminalKeyInfo? TryReadKey();

    /// <summary>Coda un tasto per la prossima lettura (per test/automazione).</summary>
    void SendKey(char key);

    // ═══ Hook ═══

    /// <summary>Registra un hook per una combinazione di tasti.</summary>
    void RegisterHook(KeyCombination keyCombination, Action action);

    /// <summary>Rimuove un hook registrato.</summary>
    void UnregisterHook(KeyCombination keyCombination);

    /// <summary>Rimuove tutti gli hook.</summary>
    void UnregisterAllHooks();

    /// <summary>Verifica se un hook è registrato.</summary>
    bool IsHookRegistered(KeyCombination keyCombination);

    // ═══ Async (Console loop) ═══

    /// <summary>Richiesta di input bloccante attualmente attiva.</summary>
    IBlockingRequest? ActiveBlockingRequest { get; }

    /// <summary>Evento scatenato quando viene richiesta un'interruzione (es. Ctrl+C).</summary>
    event Action? BreakRequested;

    /// <summary>Avvia il servizio di ascolto.</summary>
    void Start();

    /// <summary>Ferma il servizio di ascolto.</summary>
    void Stop();

    /// <summary>Tenta di leggere un tasto dal buffer in modo non bloccante.</summary>
    ConsoleKeyInfo? TryReadKey(bool convertToUppercase);

    /// <summary>Attende in modo asincrono il prossimo tasto.</summary>
    ValueTask<ConsoleKeyInfo> ReadKeyAsync(CancellationToken cancellationToken = default, bool convertToUppercase = false);

    /// <summary>Legge una riga di testo con prompt ed editing.</summary>
    Task<string?> ReadLineAsync(IConsole console, string prompt,
        Func<char, string, (bool process, char convertedChar)>? charConversionRule,
        CancellationToken cancellationToken = default);

    /// <summary>Svuota il buffer dei tasti in attesa.</summary>
    ValueTask ClearBufferAsync();

    /// <summary>Mette in pausa il servizio.</summary>
    void Pause();

    /// <summary>Riprende il servizio.</summary>
    void Resume();

    /// <summary>Resetta lo stato del servizio.</summary>
    Task ResetAsync();
}
