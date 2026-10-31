using CommonLib.Clipboard;
using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Clipboard.ClipboardServiceSdl.cs

using SDL2;

namespace CommonGraphics.Clipboard;

/// <summary>
/// Implementazione di <see cref="IClipboardService"/> che utilizza le API di SDL2
/// per interagire con gli appunti di sistema.
/// Questa implementazione è attiva quando l'interprete è in modalità grafica.
/// </summary>
public class ClipboardServiceSdl : IClipboardService
{
    /// <summary>
    /// Recupera il testo attualmente presente negli appunti di sistema utilizzando SDL.
    /// </summary>
    /// <returns>Il testo degli appunti, o una stringa vuota se SDL non può recuperare il testo.</returns>
    public string GetText()
    {
        if (SDL.SDL_HasClipboardText() != SDL.SDL_bool.SDL_TRUE)
            return string.Empty;

        return SDL.SDL_GetClipboardText() ?? string.Empty;
    }

    /// <summary>
    /// Imposta il testo specificato negli appunti di sistema utilizzando SDL.
    /// </summary>
    /// <param name="text">Il testo da inserire negli appunti.</param>
    public void SetText(string? text)
    {
        SDL.SDL_SetClipboardText(text);
    }

    /// <summary>
    /// Cancella il contenuto corrente degli appunti di sistema impostando una stringa vuota.
    /// </summary>
    public void Clear()
    {
        SDL.SDL_SetClipboardText(string.Empty);
    }
}
