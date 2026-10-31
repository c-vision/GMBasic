// FILE: CommonLib.CommandLineArgs.cs

using System.Collections.Immutable;

namespace CommonLib;

/// <summary>Risultati del parsing degli argomenti della riga di comando in modo fortemente tipizzato.</summary>
public sealed record CommandLineArgs
{
    /// <summary>Percorso del file script BASIC da eseguire.</summary>
    public string? FileName { get; init; }
    /// <summary>Se true, non apre alcuna console (headless).</summary>
    public bool NoConsole { get; init; }
    /// <summary>Se true, usa la console SDL.</summary>
    public bool UseSdl { get; init; }
    /// <summary>Se true, usa la console di sistema.</summary>
    public bool UseSystemConsole { get; init; }
    /// <summary>Se true, usa la modalità C64.</summary>
    public bool UseC64Mode { get; init; }
    /// <summary>Larghezza griglia caratteri opzionale.</summary>
    public int? GridWidth { get; init; }
    /// <summary>Altezza griglia caratteri opzionale.</summary>
    public int? GridHeight { get; init; }
    /// <summary>Porta per il debug remoto.</summary>
    public int? DebugPort { get; init; }
    /// <summary>Comando BASIC da eseguire all'avvio.</summary>
    public string? RunCommand { get; init; }
    /// <summary>Abilita log verbose del lexer.</summary>
    public bool VerboseLexer { get; init; }
    /// <summary>Abilita log verbose del parser.</summary>
    public bool VerboseParser { get; init; }
    /// <summary>Abilita log verbose dell'executor.</summary>
    public bool VerboseExecutor { get; init; }
    /// <summary>Abilita log verbose dell'orchestrator.</summary>
    public bool VerboseOrchestrator { get; init; }
    /// <summary>Abilita log verbose del linker.</summary>
    public bool VerboseLinker { get; init; }
    /// <summary>Definizioni di costanti per il preprocessore.</summary>
    public ImmutableDictionary<string, string?> Defines { get; init; } = ImmutableDictionary<string, string?>.Empty;

    /// <summary>Abilita la P-CODE VM esplicitamente (anche per .bas).</summary>
    public bool UsePCodeVm { get; init; }
    /// <summary>Forza l'uso dell'AST Executor (anche per .prg).</summary>
    public bool UseAst { get; init; }
    /// <summary>DLL managed + apphost stub (-m / -managed): richiede .NET installato.</summary>
    public bool Managed { get; init; }
    /// <summary>Singolo binario nativo NativeAOT (-n / -native): nessun runtime .NET.</summary>
    public bool Native { get; init; }
    /// <summary>True se é un file .prg (P-CODE bytecode).</summary>
    public bool IsPrgMode => FileName?.EndsWith(".prg", StringComparison.OrdinalIgnoreCase) == true;

    /// <summary>True se é stato specificato un file script da eseguire.</summary>
    public bool IsScriptMode => !string.IsNullOrEmpty(FileName);
}
