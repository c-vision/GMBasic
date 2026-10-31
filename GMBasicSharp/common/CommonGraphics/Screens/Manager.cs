using CommonGraphics.Consoles;
using CommonLib.Screens;
using CommonLib.Keyboard;
using CommonLib.Consoles;
// FILE: Interpreter.Screens.Manager.cs

using System.Collections.Immutable;
using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Screens;

/// <summary>
/// Fornisce un accesso centralizzato e cachato alle informazioni sui display
/// e sulle modalità video disponibili nel sistema tramite SDL.
/// </summary>
public static class ScreenManager
{
    /// <summary>
    /// Contiene la lista cachata di tutte le modalità video disponibili.
    /// È inizializzata in modo lazy (al primo accesso) per garantire che SDL sia pronto.
    /// </summary>
    private static readonly Lazy<ImmutableList<DisplayModeInfo>> _availableModes = new(FetchAllDisplayModes);

    /// <summary>
    /// Ottiene l'elenco completo e cachato di tutte le modalità di visualizzazione
    /// supportate dal sistema.
    /// </summary>
    public static ImmutableList<DisplayModeInfo> AvailableModes => _availableModes.Value;

    /// <summary>
    /// Metodo privato che esegue l'interrogazione SDL per recuperare tutte le modalità video.
    /// Questo metodo viene eseguito una sola volta.
    /// </summary>
    /// <returns>Una lista immutabile di oggetti DisplayModeInfo.</returns>
    private static ImmutableList<DisplayModeInfo> FetchAllDisplayModes()
    {
        var modes = new List<DisplayModeInfo>();

        try
        {
            // SdlManager si occupa di inizializzare SDL_INIT_VIDEO
            SdlManager.Instance.Initialize();

            var numDisplays = SDL.SDL_GetNumVideoDisplays();
            if (numDisplays < 1)
            {
                return ImmutableList<DisplayModeInfo>.Empty;
            }

            for (var i = 0; i < numDisplays; i++)
            {
                var numModes = SDL.SDL_GetNumDisplayModes(i);
                if (numModes < 1)
                {
                    continue;
                }

                for (var j = 0; j < numModes; j++)
                {
                    if (SDL.SDL_GetDisplayMode(i, j, out var mode) != 0) continue;
                    var modeInfo = new DisplayModeInfo
                    {
                        Width = mode.w,
                        Height = mode.h,
                        RefreshRate = mode.refresh_rate,
                        PixelFormatName = SDL.SDL_GetPixelFormatName(mode.format),
                        DisplayIndex = i
                    };
                    modes.Add(modeInfo);
                }
            }
        }
        catch (Exception ex)
        {
            // In caso di errore, logga (se disponibile un logger) e restituisce una lista vuota.
            Console.Error.WriteLine($"[ScreenManager] Failed to fetch display modes: {ex.Message}");
            return ImmutableList<DisplayModeInfo>.Empty;
        }

        // Rimuove i duplicati basati su tutti i campi e ordina la lista per una presentazione coerente.
        return modes
            .Distinct()
            .OrderBy(m => m.Width)
            .ThenBy(m => m.Height)
            .ThenByDescending(m => m.RefreshRate)
            .ToImmutableList();
    }
}