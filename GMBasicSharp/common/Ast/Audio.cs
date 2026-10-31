// FILE: Interpreter.Ast.Audio.cs

namespace Basic;

/// <summary>
/// Waveform types for SOUND command.
/// </summary>
public enum WaveformType
{
    Sine = 0,
    Square = 1,
    Sawtooth = 2,
    Triangle = 3,
    Noise = 4,
    Pulse = 5,
}

/// <summary>
/// Filter types for SOUND.
/// </summary>
public enum FilterType
{
    Off = 0,
    LowPass = 1,
    HighPass = 2,
    BandPass = 3,
}

/// <summary>
/// Rappresenta l'istruzione SOUND per riprodurre un singolo tono.
/// Voice = null significa voice 0 (backward compat).
/// </summary>
[JsonDerivedType(typeof(SoundStmt), nameof(SoundStmt))]
public sealed record SoundStmt(Expr Frequency, Expr Duration, Expr? Volume, Expr? Waveform,
    Expr? Attack, Expr? Decay, Expr? Sustain, Expr? Release,
    Expr? Voice, Expr? PulseWidth, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString()
    {
        var s = Voice is not null ? $"ON {Voice}, " : "";
        s += $"{Frequency}, {Duration}";
        if (Volume is not null) s += $", V{Volume}";
        if (Waveform is not null) s += $", W{Waveform}";
        if (Attack is not null) s += $", A{Attack} D{Decay} S{Sustain} R{Release}";
        if (PulseWidth is not null) s += $", PW{PulseWidth}";
        return s;
    }

    public override AstNode DeepClone()
    {
        return new SoundStmt(
            (Expr)Frequency.DeepClone(),
            (Expr)Duration.DeepClone(),
            (Expr?)Volume?.DeepClone(),
            (Expr?)Waveform?.DeepClone(),
            (Expr?)Attack?.DeepClone(),
            (Expr?)Decay?.DeepClone(),
            (Expr?)Sustain?.DeepClone(),
            (Expr?)Release?.DeepClone(),
            (Expr?)Voice?.DeepClone(),
            (Expr?)PulseWidth?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Imposta parametri filtro per un voice audio.
/// SET FILTER voice, type, cutoff, resonance
/// </summary>
[JsonDerivedType(typeof(SetFilterStmt), nameof(SetFilterStmt))]
public sealed record SetFilterStmt(Expr Voice, Expr Type, Expr Cutoff, Expr Resonance, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        $"SET FILTER {Voice}, {Type}, {Cutoff}Hz, {Resonance}%";

    public override AstNode DeepClone() =>
        new SetFilterStmt(
            (Expr)Voice.DeepClone(), (Expr)Type.DeepClone(),
            (Expr)Cutoff.DeepClone(), (Expr)Resonance.DeepClone(),
            (Token)Token.DeepClone());
}

/// <summary>
/// Classe base astratta per le diverse modalità di esecuzione dei comandi audio.
/// </summary>
[JsonDerivedType(typeof(StandardPlayAction), nameof(StandardPlayAction))]
[JsonDerivedType(typeof(ConcurrentPlayAction), nameof(ConcurrentPlayAction))]
[JsonDerivedType(typeof(QueuePlayAction), nameof(QueuePlayAction))]
[JsonDerivedType(typeof(NamedPlayAction), nameof(NamedPlayAction))]
[JsonDerivedType(typeof(LoopPlayAction), nameof(LoopPlayAction))]
[JsonDerivedType(typeof(ControlAudioAction), nameof(ControlAudioAction))]
public abstract record AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione audio.
    /// </summary>
    /// <returns>Una nuova istanza di AudioAction che è una copia completa dell'originale.</returns>
    public abstract AudioAction DeepClone();
}

/// <summary>
/// Rappresenta l'azione di riproduzione musicale standard (bloccante).
/// </summary>
public sealed record StandardPlayAction(Expr MusicString) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di StandardPlayAction con una copia dell'espressione MusicString.</returns>
    public override AudioAction DeepClone()
    {
        return new StandardPlayAction((Expr)MusicString.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione di riproduzione concorrente.
/// </summary>
public sealed record ConcurrentPlayAction(ImmutableArray<Expr> MusicStrings) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di ConcurrentPlayAction con una copia delle espressioni MusicStrings.</returns>
    public override AudioAction DeepClone()
    {
        var clonedStrings = MusicStrings.Select(s => (Expr)s.DeepClone()).ToImmutableArray();
        return new ConcurrentPlayAction(clonedStrings);
    }
}

/// <summary>
/// Rappresenta l'azione di accodamento musicale.
/// </summary>
public sealed record QueuePlayAction(Expr MusicString) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di QueuePlayAction con una copia dell'espressione MusicString.</returns>
    public override AudioAction DeepClone()
    {
        return new QueuePlayAction((Expr)MusicString.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione di riproduzione su un canale nominato.
/// </summary>
public sealed record NamedPlayAction(Expr MusicString, Expr Channel) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di NamedPlayAction con una copia delle sue espressioni figlie.</returns>
    public override AudioAction DeepClone()
    {
        return new NamedPlayAction((Expr)MusicString.DeepClone(), (Expr)Channel.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione di riproduzione in loop.
/// </summary>
public sealed record LoopPlayAction(Expr MusicString, Expr Channel, Expr? Times) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di LoopPlayAction con una copia delle sue espressioni figlie.</returns>
    public override AudioAction DeepClone()
    {
        return new LoopPlayAction((Expr)MusicString.DeepClone(), (Expr)Channel.DeepClone(), (Expr?)Times?.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'azione di controllo generale sull'audio.
/// </summary>
public sealed record ControlAudioAction(Token Command, Expr? Argument) : AudioAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione.
    /// </summary>
    /// <returns>Una nuova istanza di ControlAudioAction con una copia delle sue espressioni figlie.</returns>
    public override AudioAction DeepClone()
    {
        return new ControlAudioAction((Token)Command.DeepClone(), (Expr?)Argument?.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione audio complessa (PLAY o SOUND con modificatori).
/// </summary>
[JsonDerivedType(typeof(PlayStmt), nameof(PlayStmt))]
public sealed record PlayStmt(AudioAction Action, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Action.GetType().Name.Replace("PlayAction", "").Replace("Action", "");

    /// <summary>
    /// Crea una copia profonda di questo nodo PlayStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PlayStmt con una copia della sua azione figlia.</returns>
    public override AstNode DeepClone()
    {
        var clonedAction = Action.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new PlayStmt(clonedAction, clonedToken);
    }
}

/// <summary>
/// Carica un file audio (WAV) e restituisce un handle numerico.
/// </summary>
[JsonDerivedType(typeof(SoundLoadStmt), nameof(SoundLoadStmt))]
public sealed record SoundLoadStmt(Expr FilePath, Target Handle, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SOUND LOAD {FilePath} AS #{Handle}";
    public override AstNode DeepClone() =>
        new SoundLoadStmt((Expr)FilePath.DeepClone(), (Target)Handle.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Riproduce un file audio caricato con SOUND LOAD.
/// </summary>
[JsonDerivedType(typeof(SoundPlayStmt), nameof(SoundPlayStmt))]
public sealed record SoundPlayStmt(Expr Handle, Expr? Channel, Expr? Loops, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SOUND PLAY #{Handle}";
    public override AstNode DeepClone() =>
        new SoundPlayStmt((Expr)Handle.DeepClone(), (Expr?)Channel?.DeepClone(), (Expr?)Loops?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Imposta il volume di un canale audio specifico o del master.
/// </summary>
[JsonDerivedType(typeof(SoundVolumeStmt), nameof(SoundVolumeStmt))]
public sealed record SoundVolumeStmt(Expr Volume, Expr? Channel, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SOUND VOLUME {Volume}{(Channel is not null ? $", {Channel}" : "")}";
    public override AstNode DeepClone() =>
        new SoundVolumeStmt((Expr)Volume.DeepClone(), (Expr?)Channel?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta un'istruzione di controllo audio come
/// SOUND/PLAY PAUSE, SOUND/PLAY RESUME, o SOUND/PLAY STOP [canale|"ALL"].
/// Questo nodo distingue l'origine del comando (SOUND o PLAY).
/// </summary>
/// <param name="Command">Il token originale del comando (SOUND o PLAY).</param>
/// <param name="Action">Il token dell'azione di controllo (STOP, PAUSE, RESUME).</param>
/// <param name="Argument">L'espressione opzionale per il canale o "ALL".</param>
[JsonDerivedType(typeof(AudioControlStmt), nameof(AudioControlStmt))]
public sealed record AudioControlStmt(Token Command, Token Action, Expr? Argument) 
    : Stmt(Command.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Command.Lexeme} {Action.Lexeme}{(Argument != null ? $" {Argument}" : "")}";
    
    /// <summary>
    /// Crea una copia profonda di questo nodo AudioControlStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AudioControlStmt con copie dei suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new AudioControlStmt(
            (Token)Command.DeepClone(),
            (Token)Action.DeepClone(),
            (Expr?)Argument?.DeepClone()
        );
    }
}
