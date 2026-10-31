using CommonLib.Screens;
// FILE: Interpreter.Graphics.Extensions.cs

using System.Runtime.InteropServices;
using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

/// <summary>
/// Fornisce l'accesso a funzioni avanzate o recenti della libreria SDL2
/// che potrebbero non essere esposte dal wrapper SDL2-CS.
/// Questa classe utilizza P/Invoke per chiamare direttamente le funzioni native.
/// </summary>
public static class GraphicsManagerExtensions
{
    // --- NOME DELLA LIBRERIA NATIVA ---
        
    /// <summary>
    /// Il nome della libreria SDL2 da importare.
    /// Grazie alla configurazione del progetto (.csproj), su macOS viene creata una copia
    /// di "libSDL2.dylib" chiamata "SDL2.dll". Questo permette di usare lo stesso
    /// nome "SDL2.dll" sia per Windows che per macOS in modo trasparente.
    /// </summary>
    private const string SdlNativeLibraryName = "SDL2.dll";

    // --- ENUMERAZIONI E STRUTTURE MANCANTI ---

    /// <summary>
    /// Definisce gli algoritmi di scaling per le texture.
    /// Questa enum è definita qui perché mancante nel wrapper SDL2-CS.
    /// </summary>
    public enum SDL_ScaleMode
    {
        /// <summary>Scaling "Nearest pixel", ideale per la pixel art.</summary>
        Nearest,
        /// <summary>Scaling lineare (sfumato).</summary>
        Linear,
        /// <summary>Scaling "Anisotropic", la migliore qualità.</summary>
        Best
    }

    /// <summary>
    /// Rappresenta un punto con coordinate a virgola mobile, usato da SDL_Vertex.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct SDL_FPoint
    {
        /// <summary>La coordinata X.</summary>
        public float x;
        /// <summary>La coordinata Y.</summary>
        public float y;
    }

    /// <summary>
    /// Rappresenta un singolo vertice per il rendering di geometrie, contenente
    /// posizione, colore e coordinate della texture.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct SDL_Vertex
    {
        /// <summary>La posizione del vertice nello spazio del renderer.</summary>
        public SDL_FPoint position;
        /// <summary>Il colore del vertice.</summary>
        public SDL.SDL_Color color;
        /// <summary>Le coordinate della texture (UV) per questo vertice.</summary>
        public SDL_FPoint tex_coord;
    }

    // --- P/INVOKE PER LE FUNZIONI SDL2 ---

    /// <summary>
    /// Esegue il rendering di una geometria definita da un set di vertici.
    /// Questa è la funzione principale per il rendering 2D accelerato in batch.
    /// </summary>
    /// <param name="renderer">Il puntatore al renderer SDL.</param>
    /// <param name="texture">Il puntatore alla texture da applicare (o IntPtr.Zero se non usata).</param>
    /// <param name="vertices">Un array di vertici che definiscono la geometria.</param>
    /// <param name="num_vertices">Il numero di vertici nell'array.</param>
    /// <param name="indices">Un array opzionale di indici per definire i triangoli.</param>
    /// <param name="num_indices">Il numero di indici nell'array.</param>
    /// <returns>0 in caso di successo, un codice di errore negativo in caso di fallimento.</returns>
    [DllImport(SdlNativeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SDL_RenderGeometry")]
    public static extern int SDL_RenderGeometry(
        IntPtr renderer,
        IntPtr texture,
        [In] SDL_Vertex[] vertices,
        int num_vertices,
        [In] int[]? indices,
        int num_indices
    );

    /// <summary>
    /// Imposta la modalità di scaling per una texture (nearest, linear, best).
    /// Utile per controllare l'aspetto della pixel art.
    /// </summary>
    /// <param name="texture">Il puntatore alla texture da modificare.</param>
    /// <param name="scaleMode">La modalità di scaling (0=nearest, 1=linear, 2=best).</param>
    /// <returns>0 in caso di successo, un codice di errore negativo in caso di fallimento.</returns>
    [DllImport(SdlNativeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SDL_SetTextureScaleMode")]
    public static extern int SDL_SetTextureScaleMode(IntPtr texture, SDL_ScaleMode scaleMode);
        
    /// <summary>
    /// Ottiene la modalità di scaling corrente di una texture.
    /// </summary>
    /// <param name="texture">Il puntatore alla texture.</param>
    /// <param name="scaleMode">La variabile dove verrà memorizzata la modalità di scaling.</param>
    /// <returns>0 in caso di successo, un codice di errore negativo in caso di fallimento.</returns>
    [DllImport(SdlNativeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SDL_GetTextureScaleMode")]
    public static extern int SDL_GetTextureScaleMode(IntPtr texture, out SDL_ScaleMode scaleMode);

    /// <summary>
    /// Abilita o disabilita il VSync per un renderer.
    /// </summary>
    /// <param name="renderer">Il puntatore al renderer.</param>
    /// <param name="vsync">1 per abilitare, 0 per disabilitare, -1 per il vsync adattivo.</param>
    /// <returns>0 in caso di successo, un codice di errore negativo in caso di fallimento.</returns>
    [DllImport(SdlNativeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SDL_RenderSetVSync")]
    public static extern int SDL_RenderSetVSync(IntPtr renderer, int vsync);

    /// <summary>
    /// Ottiene informazioni sul renderer corrente, inclusi i flag come VSync.
    /// </summary>
    /// <param name="renderer">Il puntatore al renderer.</param>
    /// <param name="info">La struttura dove verranno memorizzate le informazioni.</param>
    /// <returns>0 in caso di successo, un codice di errore negativo in caso di fallimento.</returns>
    [DllImport(SdlNativeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SDL_GetRendererInfo")]
    public static extern int SDL_GetRendererInfo(IntPtr renderer, out SDL.SDL_RendererInfo info);
}