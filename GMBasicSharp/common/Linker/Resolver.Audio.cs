// FILE: Linker/Resolver.Audio.cs — Specializzazioni audio/SDL2 (AUDIO, ONKEY, CLIP)
// Separate dal core condiviso Resolver.cs. Isolate per un'unificazione futura
// con compilazione condizionale (#if GMBASIC_DATABASES / GMBASIC_AUDIO).

using System.Collections.Immutable;
using System.Collections.Generic;
using System.Linq;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Risolve ricorsivamente le espressioni all'interno di un'azione audio.
    /// Questo metodo gestisce i diversi tipi concreti di <see cref="AudioAction"/>,
    /// risolvendo i SymbolId per ogni espressione (es. stringa musicale, canale, volume).
    /// </summary>
    /// <param name="action">L'azione audio da risolvere (es. <see cref="StandardPlayAction"/>, <see cref="LoopPlayAction"/>).</param>
    /// <param name="program">L'istanza del programma contenente la SymbolTable per la risoluzione dei simboli.</param>
    /// <returns>
    /// Una nuova istanza di <see cref="AudioAction"/> con tutte le espressioni interne risolte (SymbolId validi).
    /// Se l'azione non contiene espressioni (o non è supportata), viene restituita l'istanza originale.
    /// </returns>
    private static AudioAction ResolveAudioAction(AudioAction action, BasicProgram program)
    {
        return action switch
        {
            StandardPlayAction a => new StandardPlayAction(
                ResolveExpression(a.MusicString, program)
            ),

            ConcurrentPlayAction a => new ConcurrentPlayAction(
                [..a.MusicStrings.Select(e => ResolveExpression(e, program))]
            ),

            QueuePlayAction a => new QueuePlayAction(
                ResolveExpression(a.MusicString, program)
            ),

            NamedPlayAction a => new NamedPlayAction(
                ResolveExpression(a.MusicString, program),
                ResolveExpression(a.Channel, program)
            ),

            LoopPlayAction a => new LoopPlayAction(
                ResolveExpression(a.MusicString, program),
                ResolveExpression(a.Channel, program),
                a.Times != null ? ResolveExpression(a.Times, program) : null
            ),

            ControlAudioAction a => new ControlAudioAction(
                a.Command, // Token, non expression
                a.Argument != null ? ResolveExpression(a.Argument, program) : null
            ),

            _ => action
        };
    }
    /// <summary>
    /// Risolve l'azione associata a un evento `ON KEY`.
    /// Gestisce specificamente l'azione `CallAction`, che contiene un'espressione di chiamata
    /// a una procedura (sub o function) che deve essere risolta semanticamente.
    /// </summary>
    /// <param name="action">L'azione da risolvere (es. <see cref="CallAction"/>, <see cref="GotoAction"/>).</param>
    /// <param name="program">L'istanza del programma contenente la SymbolTable per la risoluzione dei simboli.</param>
    /// <returns>
    /// Una nuova istanza di <see cref="OnKeyAction"/> con l'espressione di chiamata risolta (se applicabile).
    /// Per le azioni che non contengono espressioni (es. `GotoAction`, `StopAction`), restituisce l'istanza originale.
    /// </returns>
    private static OnKeyAction ResolveOnKeyAction(OnKeyAction action, BasicProgram program)
    {
        return action switch
        {
            // CallAction contiene un'espressione (la chiamata alla procedura) che deve essere risolta.
            // Utilizziamo il costruttore di copia per garantire la corretta inizializzazione.
            CallAction a => new CallAction(ResolveExpression(a.Call, program)),

            // GotoAction e GosubAction contengono solo un Token (label), che non richiede risoluzione in questa fase
            // (le label vengono validate dal Linker in un passaggio separato o a runtime).
            // StopAction non ha parametri.
            _ => action
        };
    }
}
