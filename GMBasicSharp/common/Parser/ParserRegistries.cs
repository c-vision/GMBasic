// FILE: Parser.ParserRegistries.cs

namespace Basic;

/// <summary>
/// Contiene le informazioni necessarie per la registrazione e il dispatching di un comando.
/// </summary>
/// <param name="ProvidingNamespace">Il namespace che fornisce questo comando (es. "ISAM", "DBF").</param>
/// <param name="ParseFunction">Un delegato al metodo del parser che analizza l'istruzione specifica.</param>
public sealed record CommandRegistration(
    string ProvidingNamespace,
    Func<Parser, string, Stmt> ParseFunction);

/// <summary>
/// Registri statici condivisi tra StdLib (che li popola) e Parser (che li consulta).
/// Separati per evitare dipendenza diretta del Parser da StdLib.
/// </summary>
public static class ParserRegistries
{
    /// <summary>
    /// Mappa un TokenKind a una lista di possibili comandi (per gestire il polimorfismo).
    /// Tutti i comandi sono registrati qui (solo sintassi legacy/non qualificata).
    /// </summary>
    public static Dictionary<TokenKind, List<CommandRegistration>> UnifiedCommandRegistry { get; } = new();

    /// <summary>Registro comandi per NOME/LESSEMA (non-keyword): evita di aggiungere
    /// TokenKind per comandi pixel (CHECK/RADIO/TEXTBOX/CONTROL/...). Il dispatch,
    /// se il token è un Identifier il cui lessema è qui, invoca il parser registrato.
    /// Chiave case-insensitive.</summary>
    public static Dictionary<string, List<CommandRegistration>> NamedCommandRegistry { get; } =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Dispatcher dei subcomandi per namespace, usati dal fallback "senza prefisso".
    /// Ogni voce tenta di parsare il comando corrente come subcomando del namespace.
    /// Il fallback prova SOLO i namespace attivi (importati o di default/settings),
    /// con backtracking: vince il primo parser che consuma l'istruzione per intero.
    /// </summary>
    public static readonly (string Namespace, Func<Parser, string, Stmt> Dispatcher)[] NamespaceSubcommandDispatchers =
    {
        ("DBF", Parser.ParseDbfSubcommand),
        ("ISAM", Parser.ParseIsamSubcommand),
        ("RDD", Parser.ParseRddSubcommand),
        ("SPEED", Parser.ParseSpeedSubcommand),
        ("LEDGER", Parser.ParseLedgerSubcommand),
        ("SQL", Parser.ParseSqlSubcommand),
        ("HTTP", Parser.ParseHttpSubcommand),
        ("CHART", Parser.ParseChartSubcommand),
        ("MOUSE", Parser.ParseMouseSubcommand),
        ("VECTORDB", Parser.ParseVectorDbSubcommand),
    };
}
