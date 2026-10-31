using CommonGraphics.Consoles;
using CommonLib;
using Basic;
using CommonLib.Screens;
using CommonLib.Keyboard;
using CommonLib.Consoles;
// FILE: Interpreter.Graphics.Manager.cs

using System.Collections.Concurrent;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using MathNet.Numerics.LinearAlgebra;
using SDL2;

// ReSharper disable InconsistentNaming
// ReSharper disable UseUtf8StringLiteral

namespace CommonGraphics.Graphics;

/// <summary>
/// Gestisce la finestra grafica e le operazioni di disegno agendo come un proxy thread-safe.
/// L'Executor accoda i comandi da un thread in background. Il thread principale (REPL)
/// deve chiamare periodicamente ProcessQueuedCommands(), PollEvents(), e Present() per eseguire
/// tali comandi, garantendo che tutte le interazioni con SDL avvengano sul thread corretto.
/// </summary>
public sealed partial class GraphicsManager : IDisposable
{
    /// <summary>
    /// Rappresenta una richiesta generica di esecuzione di un'azione sul thread principale.
    /// Questo viene usato per operazioni che non sono comandi grafici ma che
    /// devono comunque essere eseguite in sicurezza (es. Console.Write).
    /// </summary>
    private sealed record MainThreadActionRequest(Action Action, TaskCompletionSource CompletionSource);

    #region Singleton

    private static readonly Lazy<GraphicsManager> _lazyInstance = new(() => new GraphicsManager());
    public static GraphicsManager Instance => _lazyInstance.Value;

    private GraphicsManager()
    {
        // Il costruttore ora è passivo e non ha effetti collaterali.
    }

    #endregion

    #region Private Fields

    private readonly BlockingCollection<Action> _commandQueue = new();

    /// <summary>
    /// Coda per le azioni generiche che devono essere eseguite in modo sincrono sul thread principale.
    /// </summary>
    private readonly ConcurrentQueue<MainThreadActionRequest> _mainThreadActionQueue = new();

    // Struttura per archiviare lo stato completo di uno schermo salvato.
    private sealed record SavedScreenState(ScreenConfig Config, IntPtr Texture);

    // Dizionario per mappare i nomi degli schermi salvati al loro stato.
    private readonly Dictionary<string, SavedScreenState> _savedScreens = new();
    // BSAVE/BLOAD (IMAGEGET/IMAGEPUT): area salvata internamente (buffer BASIC ignorato).
    private readonly Dictionary<string, uint[]> _savedAreas = new();

    /// <summary>
    /// Mappa gli handle delle texture (numeri interi visibili al programmatore BASIC)
    /// ai puntatori delle texture SDL in memoria video.
    /// </summary>
    private readonly Dictionary<int, IntPtr> _loadedTextures = new();
    /// <summary>Pixel ARGB delle texture caricate (per il blit via buffer: RenderCopy/RenderCopyEx
    /// non funziona col renderer software + target texture). Popolato in LoadTextureInternal.</summary>
    private readonly Dictionary<int, uint[]> _loadedTexturePixels = new();

    /// <summary>
    /// Contatore per generare handle univoci per le nuove texture caricate.
    /// </summary>
    private int _nextTextureId = 1;

    private SdlContext? _programContext;

    // --- Viewport state ---
    private int _viewX1, _viewY1, _viewX2, _viewY2;
    private bool _viewPortActive;

    // --- Window (logical coordinate) state ---
    private double _winX1, _winY1, _winX2, _winY2;
    private bool _windowActive;
    private bool _windowScreen;

    // Dizionari separati per le texture delle pagine
    private readonly Dictionary<int, IntPtr> _programPageTextures = new();
    private int _activePage;
    private int _visiblePage;
    private IntPtr _font;
    private IntPtr _font2x;              // font a 2x per SSAA del testo (rendering nitido a piccole dimensioni)
    private string _fontPath = "";       // path dell'ultimo font aperto (per il 2x lazy)
    private int _fontSize;               // dimensione dell'ultimo font aperto
    private int _screenWidth;
    private int _screenHeight;
    /// <summary>Scala HiDPI della finestra (outputPixels/windowPoints, es. 2 su Retina).
    /// Le pagine e i font vengono renderizzati a risoluzione NATIVA (x scale) e il
    /// Present copia 1:1 — come fa SDLConsole. Prima le pagine erano 640x480 scalate
    /// 2x dal driver → testo TTF a 14px aveva solo 14px di informazione per 28px fisici
    /// (sfocato/soft). Le coordinate logiche (BASIC) restano 0.._screenWidth-1; il
    /// rendering interno moltiplica per _renderScale.</summary>
    private float _renderScale = 1.0f;
    private int _backingWidth;
    private int _backingHeight;
    private byte[][]? _colorPalette;
    private string? _activePaletteName;
    private (int x, int y) _cursorPosition;
    // Cursore di testo grafico: visibilità/stile da SET CURSOR / CURSOR ON|OFF / CURSOR SHAPE
    // (default ON + BlinkingBlock, come ClipperState / SdlConsole). DrawTextCursor nel Present.
    private bool _cursorVisible = true;
    private CommonLib.Consoles.CursorStyle _cursorStyle = CommonLib.Consoles.CursorStyle.BlinkingBlock;
    private readonly System.Diagnostics.Stopwatch _cursorBlinkTimer = System.Diagnostics.Stopwatch.StartNew();
    private byte[] _currentColor = [255, 255, 255];
    private byte[] _backgroundColor = [0, 0, 0];
    private int _backgroundColorIndex;
    private int _charGridWidth;
    private int _charGridHeight;
    private string? _currentFontPath;
    private int _currentFontSize;
    private int _charCellWidth;
    private int _charCellHeight;
    private int _defaultForegroundColor;
    private int _defaultBackgroundColor;

    // --- GPU (Vulkan) acceleration ---
    private CommonGraphics.Vulkan.VulkanBackend? _vk;

    private ScreenMode _currentScreenMode = ScreenMode.Text;

    // --- Graphics Capture (per test headless) ---
    private volatile bool _captureMode;
    private readonly List<string> _capturedCommands = new();

    #endregion

    #region Matrix Providers (Callback System)

    /// <summary>
    /// Callback fornita dall'Executor per recuperare una matrice di algebra lineare (MathNet) tramite il suo handle.
    /// Questo è il "ponte" disaccoppiato tra il sistema grafico e quello di algebra lineare.
    /// </summary>
    private Func<long, Matrix<double>>? _getLinearMatrixProvider;

    /// <summary>
    /// Callback fornita dall'Executor per recuperare la matrice di MONDO (World) globale corrente.
    /// </summary>
    private Func<Matrix<double>>? _getWorldMatrixProvider;

    /// <summary>
    /// Callback fornita dall'Executor per recuperare la matrice di VISTA (View) globale corrente.
    /// </summary>
    private Func<Matrix<double>>? _getViewMatrixProvider;

    /// <summary>
    /// Callback fornita dall'Executor per recuperare la matrice di PROIEZIONE (Projection) globale corrente.
    /// </summary>
    private Func<Matrix<double>>? _getProjectionMatrixProvider;

    /// <summary>
    /// La matrice di MONDO (World) di rendering, usata nel frame corrente.
    /// È una cache aggiornata dal sistema LINALG tramite il comando LINALG.MATRIX.WORLD
    /// e la syscall UPDATE_WORLD_MATRIX_GPM.
    /// Tipo: System.Numerics.Matrix4x4.
    /// </summary>
    private Matrix4x4 _worldMatrix = Matrix4x4.Identity;

    /// <summary>
    /// La matrice di VISTA (View) di rendering, usata nel frame corrente.
    /// È una cache aggiornata dal sistema LINALG tramite il comando LINALG.MATRIX.VIEW
    /// e la syscall UPDATE_VIEW_MATRIX_GPM.
    /// Tipo: System.Numerics.Matrix4x4.
    /// </summary>
    private Matrix4x4 _viewMatrix = Matrix4x4.Identity;

    /// <summary>
    /// La matrice di PROIEZIONE (Projection) di rendering, usata nel frame corrente.
    /// È una cache aggiornata dal sistema LINALG tramite il comando LINALG.MATRIX.PROJECTION
    /// e la syscall UPDATE_PROJECTION_MATRIX_GPM.
    /// Tipo: System.Numerics.Matrix4x4.
    /// </summary>
    private Matrix4x4 _projectionMatrix = Matrix4x4.Identity;
    
    /// <summary>
    /// Imposta i provider (callback) che il GraphicsManager userà per recuperare le matrici
    /// dal sistema di algebra lineare (Executor). Questo metodo viene chiamato una sola volta
    /// all'avvio per "collegare" i due sistemi in modo disaccoppiato.
    /// </summary>
    /// <param name="getLinearMatrixProvider">Una funzione che accetta un handle (long) e restituisce una matrice MathNet.</param>
    /// <param name="getWorldMatrixProvider">Una funzione che restituisce la matrice di Mondo MathNet.</param>
    /// <param name="getViewMatrixProvider">Una funzione che restituisce la matrice di Vista MathNet.</param>
    /// <param name="getProjectionMatrixProvider">Una funzione che restituisce la matrice di Proiezione MathNet.</param>
    public void SetMatrixProviders(
        Func<long, Matrix<double>> getLinearMatrixProvider,
        Func<Matrix<double>> getWorldMatrixProvider,
        Func<Matrix<double>> getViewMatrixProvider,
        Func<Matrix<double>> getProjectionMatrixProvider)
    {
        _getLinearMatrixProvider =
            getLinearMatrixProvider ?? throw new ArgumentNullException(nameof(getLinearMatrixProvider));
        _getWorldMatrixProvider =
            getWorldMatrixProvider ?? throw new ArgumentNullException(nameof(getWorldMatrixProvider));
        _getViewMatrixProvider =
            getViewMatrixProvider ?? throw new ArgumentNullException(nameof(getViewMatrixProvider));
        _getProjectionMatrixProvider = getProjectionMatrixProvider ??
                                       throw new ArgumentNullException(nameof(getProjectionMatrixProvider));
    }

    #endregion

    #region Public Properties

    /// <summary>V1.2 — sink grafico intercettabile (SDL vs buffer WebSocket) per Canvas remoto.
    /// Se non null, i comandi PSET/LINE/RECT/CIRCLE/CLEAR vengono anche serializzati e inviati
    /// su WS (vedi IGraphicsSink, WebSocketGraphicsSink). Default null = solo SDL.</summary>
    public IGraphicsSink? CurrentSink { get; set; }

    /// <summary>
    /// Ottiene un valore che indica se SDL è stato inizializzato e non ancora rilasciato.
    /// Questo è il flag principale per determinare se l'applicazione è in modalità grafica.
    /// </summary>
    public bool IsInitialized { get; private set; }

    /// <summary>
    /// Ottiene un valore che indica se lo schermo è attualmente attivo.
    /// </summary>
    public bool IsScreenOn { get; private set; }

    /// <summary>
    /// Ottiene la posizione corrente del cursore.
    /// </summary>
    public (int X, int Y) CursorPosition => _cursorPosition;
    public void AdvanceTextCursor(int chars)
    {
        var c = _cursorPosition;
        _cursorPosition = (c.x + chars * (_charCellWidth > 0 ? _charCellWidth : 8), c.y);
    }
    public void NewTextLine()
    {
        var c = _cursorPosition;
        _cursorPosition = (0, c.y + (_charCellHeight > 0 ? _charCellHeight : 16));
    }
    public void SetTextCursor(int x, int y) => _cursorPosition = (x, y);
    public void SetTextCursorVisible(bool visible) => _cursorVisible = visible;
    public void SetTextCursorStyle(CommonLib.Consoles.CursorStyle style) => _cursorStyle = style;
    /// <summary>
    /// Ottiene la larghezza corrente dello schermo in pixel.
    /// </summary>
    public int ScreenWidth => _screenWidth;

    /// <summary>
    /// Ottiene l'altezza corrente dello schermo in pixel.
    /// </summary>
    public int ScreenHeight => _screenHeight;

    public int CharGridWidth => _charGridWidth;
    public int CharGridHeight => _charGridHeight;
    public string? CurrentFontPath => _currentFontPath;
    public int CurrentFontSize => _currentFontSize;

    public int DefaultForegroundColor => _defaultForegroundColor;
    public int DefaultBackgroundColor => _defaultBackgroundColor;

    public int CharCellWidth => _charCellWidth;
    public int CharCellHeight => _charCellHeight;

    public IntPtr RendererHandle => _programContext?.RendererHandle ?? IntPtr.Zero;

    public void DrawTextInternal(string text, int x, int y, double rotation, TextAlign align, uint color)
    {
        if (_font == IntPtr.Zero || string.IsNullOrEmpty(text)) return;
        var renderer = RendererHandle;
        if (renderer == IntPtr.Zero) return;

        var surface = SDL_ttf.TTF_RenderUTF8_Blended(_font, text, new SDL.SDL_Color
        {
            r = (byte)((color >> 24) & 0xFF),
            g = (byte)((color >> 16) & 0xFF),
            b = (byte)((color >> 8) & 0xFF),
            a = (byte)(color & 0xFF)
        });
        if (surface == IntPtr.Zero) return;

        var texture = SDL.SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == IntPtr.Zero) { SDL.SDL_FreeSurface(surface); return; }

        SDL.SDL_QueryTexture(texture, out _, out _, out var w, out var h);
        var destX = align switch
        {
            TextAlign.Center => x - w / 2,
            TextAlign.Right => x - w,
            _ => x
        };
        // Centra il bounding box REALE dei glifi (pixel non-trasparenti) sul punto y. Le
        // metriche del font (ascent/capHeight) e l'altezza texture NON corrispondono ai
        // pixel disegnati (la surface TTF ha un padding sopra: ttfPad = sur.h - FontHeight),
        // e il disallineamento cresce col font. Scansionando i pixel si trova il centro vero
        // dei glifi, indipendente da font, dimensione e padding.
        var sur = System.Runtime.InteropServices.Marshal.PtrToStructure<SDL.SDL_Surface>(surface);
        var glyTop = 0;
        var glyBot = Math.Max(0, sur.h - 1);
        if (sur.pitch > 0 && sur.h > 0)
        {
            var pixelPtr = sur.pixels;
            var bpp = 4;
            var topFound = false;
            var botFound = false;
            for (var yy = 0; yy < sur.h; yy++)
            {
                var rowPtr = pixelPtr + yy * sur.pitch;
                for (var xx = 0; xx < sur.w; xx++)
                {
                    var px = System.Runtime.InteropServices.Marshal.ReadInt32(rowPtr + xx * bpp);
                    var a = (byte)((px >> 24) & 0xFF);
                    if (a > 16)
                    {
                        if (!topFound) { glyTop = yy; topFound = true; }
                        glyBot = yy;
                        botFound = true;
                    }
                }
            }
            if (!topFound) glyTop = 0;
            if (!botFound) glyBot = Math.Max(0, sur.h - 1);
        }
        // centro glifi nella surface = (glyTop + glyBot) / 2; lo vogliamo su y.
        var destY = y - (glyTop + glyBot) / 2;

        var center = new SDL.SDL_Point { x = 0, y = 0 };
        var dest = new SDL.SDL_Rect { x = destX, y = destY, w = w, h = h };
        SDL.SDL_RenderCopyEx(renderer, texture, IntPtr.Zero, ref dest, rotation, ref center, SDL.SDL_RendererFlip.SDL_FLIP_NONE);
        SDL.SDL_DestroyTexture(texture);
        SDL.SDL_FreeSurface(surface);
    }

    public IntPtr GetTexturePtr(int textureId)
    {
        _loadedTextures.TryGetValue(textureId, out var ptr);
        return ptr;
    }

    public void UnloadTexture(int textureId)
    {
        if (_loadedTextures.TryGetValue(textureId, out var ptr))
        {
            if (ptr != IntPtr.Zero)
                SDL.SDL_DestroyTexture(ptr);
            _loadedTextures.Remove(textureId);
        }
    }

    public void UnloadFont()
    {
        if (_font != IntPtr.Zero)
        {
            SDL_ttf.TTF_CloseFont(_font);
            _font = IntPtr.Zero;
        }
        _currentFontPath = null;
        _currentFontSize = 0;
    }

    public string CurrentScreenModeName => _currentScreenMode.ToString();

    /// <summary>La modalità grafica corrente (ScreenMode), o ScreenMode.Text se la finestra è chiusa.</summary>
    public int CurrentScreenMode => IsScreenOn ? (int)_currentScreenMode : 0;

    /// <summary>
    /// Si attiva quando l'utente chiude la finestra del programma grafico
    /// tramite i controlli del sistema operativo (es. il pulsante 'X').
    /// </summary>
    public event Action? WindowCloseRequested;

    /// <summary>
    /// Si attiva quando la finestra del programma grafico viene ridimensionata
    /// dall'utente o dal sistema operativo. Usato da ON WINDOW RESIZE.
    /// </summary>
    public event Action? WindowResized;

    #endregion

    [GeneratedRegex("([A-Z])([^A-Z]*)")]
    private static partial Regex DrawCommandRegex();

    #region Main Thread API

    /// <summary>
    /// NUOVO METODO GPM: Accoda un'operazione grafica (comando o funzione) dal mondo GPM
    /// per l'esecuzione sul thread principale. Al termine, invoca un callback con il
    /// risultato o l'eccezione. Questo è il ponte GPM-compatibile che sostituisce l'uso
    /// diretto di TaskCompletionSource per l'Executor.
    /// </summary>
    /// <param name="gfxOperation">La funzione da eseguire sul thread principale. Deve restituire un risultato (o null per i comandi).</param>
    /// <param name="completionCallback">Il callback da invocare al termine, che riceve il risultato e un'eventuale eccezione.</param>
    public void EnqueueGpmOperation(Func<object?> gfxOperation, Action<object?, Exception?> completionCallback)
    {
        if (_captureMode)
        {
            try { gfxOperation(); completionCallback(null, null); } catch { completionCallback(null, null); }
            return;
        }
        // Headless (GMBASIC_HEADLESS=1, runner test_ilrun/test_vmrun/sweep): niente SDL —
        // l'operazione non va accodata (nessun pump la eseguirebbe: PollEvents/SDL non
        // disponibili) e il completamento arriva subito, come col NullGraphicsService del
        // backend IL. Senza questo, i park grafici della PCodeVM (SCREEN/CLS/PSET/SYNC...)
        // restano appesi per sempre — la coda non viene mai drenata (bug
        // GPM-Graphics-Park-SCREEN-MOUSE-Headless-Hang-CPU-Spin, gamba --engine=pcode).
        if (Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1")
        {
            completionCallback(null, null);
            return;
        }
        _commandQueue.Add(() =>
        {
            try
            {
                var result = gfxOperation();
                completionCallback(result, null);
            }
            catch (Exception ex)
            {
                completionCallback(null, ex);
            }
        });
    }

    /// <summary>Selezione modalita' console (TEXTMODE n): applica la palette della ScreenConfig al
    /// motore grafico. In headless/capture e' un'operazione sicura (EnqueueGpmOperation).</summary>
    public void TextModeGpm(int mode, Action<object?, Exception?> completionCallback)
    {
        EnqueueGpmOperation(() =>
        {
            var cfg = ScreenConfig.GetDefault((ScreenMode)mode);
            if (cfg.PaletteData != null) _colorPalette = cfg.PaletteData;
            return null;
        }, completionCallback);
    }

    /// <summary>
    /// Accoda un'azione generica da eseguire in modo sincrono sul thread principale e attende il suo completamento.
    /// Questo è il meccanismo di sicurezza per eseguire codice che potrebbe entrare in conflitto con SDL
    /// (come System.Console.Write) dal thread dell'Executor.
    /// </summary>
    /// <param name="action">L'azione da eseguire.</param>
    public Task InvokeOnMainThreadAsync(Action action)
    {
        var tcs = new TaskCompletionSource();
        _mainThreadActionQueue.Enqueue(new MainThreadActionRequest(action, tcs));
        // Aggiungiamo un log per tracciare la richiesta
        LogDebugGfx("[GFX_MAN] Accodata azione per il thread principale. In attesa del completamento.");
        //System.Diagnostics.Debug.WriteLine($"[DEADLOCK_TRACE] InvokeOnMainThreadAsync: Accodata azione. Se il thread principale è in attesa, questo è un deadlock.");
        return tcs.Task;
    }

    /// <summary>
    /// Esegue tutte le azioni generiche in attesa nella coda del thread principale.
    /// Deve essere chiamato dal render loop.
    /// </summary>
    public void ProcessMainThreadActions()
    {
        while (_mainThreadActionQueue.TryDequeue(out var request))
        {
            try
            {
                LogDebugGfx("[GFX_MAN] Esecuzione di un'azione richiesta dal thread principale.");
                request.Action.Invoke();
                LogDebugGfx("[GFX_MAN] Azione completata con successo.");
                request.CompletionSource.TrySetResult();
            }
            catch (Exception ex)
            {
                LogDebugGfx($"[GFX_MAN] Errore durante l'esecuzione di un'azione richiesta: {ex.Message}");
                request.CompletionSource.TrySetException(ex);
            }
        }
    }

    /// <summary>
    /// Imposta il titolo della finestra del programma in modo thread-safe.
    /// </summary>
    /// <param name="title">Il nuovo titolo da visualizzare.</param>
    public void SetWindowTitle(string title)
    {
        Capture($"TITLE \"{title}\"");
        _commandQueue.Add(() =>
        {
            // Agisce sul contesto del programma, se esiste.
            if (_programContext is not null)
            {
                SDL.SDL_SetWindowTitle(_programContext.WindowHandle, title);
            }
        });
    }

    /// <summary>
    /// Inizializza il gestore della grafica nel thread principale, creando la finestra per la REPL.
    /// </summary>
    /// <summary>
    /// Inizializza il gestore della grafica nel thread principale, creando la finestra per la REPL.
    /// </summary>
    /// <summary>
    /// Inizializza il GraphicsManager e i sottosistemi SDL.
    /// Si iscrive agli eventi di finestra necessari.
    /// </summary>
    public void InitializeOnMainThread()
    {
        if (IsInitialized) return;

        SdlManager.Instance.Initialize();
        SdlManager.Instance.WindowEvent += HandleWindowEvent;

        // Init Vulkan GPU acceleration if available
        try
        {
            _vk = new CommonGraphics.Vulkan.VulkanBackend();
            if (_vk.IsAvailable) { _vk.Initialize(); }
        }
        catch { }

        IsInitialized = true;
    }

    /// <summary>
    /// Elabora i comandi in coda. Questo metodo deve essere chiamato in un ciclo di gioco
    /// per garantire che i comandi grafici vengano eseguiti.
    /// </summary>
    public void ProcessQueuedCommands()
    {
        while (_commandQueue.TryTake(out var action))
        {
            try
            {
                //System.Diagnostics.Debug.WriteLine($"[MAIN_THREAD] Esecuzione di un'azione dalla coda grafica: {action.Method.Name}");
                action();
            }
            catch (Exception ex)
            {
                LogDebugGfx($"[GFX_MAN_ERROR] Exception in queued command: {ex.Message}");
            }
        }
    }

    /// <summary>
    /// Presenta la pagina visibile della finestra del programma a schermo.
    /// Non fa nulla se lo schermo del programma non è attivo.
    /// </summary>
    public void Present()
    {
        // Esegui sempre i comandi di disegno accodati prima di presentare.
        ProcessQueuedCommands();

        // Ora questo metodo si occupa solo della finestra del programma.
        if (IsScreenOn && _programContext is not null)
        {
            var flags = SDL.SDL_GetWindowFlags(_programContext.WindowHandle);
            if ((flags & (uint)SDL.SDL_WindowFlags.SDL_WINDOW_SHOWN) != 0)
            {
                SDL.SDL_SetRenderTarget(_programContext.RendererHandle, IntPtr.Zero);

                if (_programPageTextures.TryGetValue(_visiblePage, out var programTexture))
                {
                    SDL.SDL_RenderCopy(_programContext.RendererHandle, programTexture, IntPtr.Zero, IntPtr.Zero);
                }
                else
                {
                    // Fallback: pulisce con il colore di sfondo se la texture non esiste.
                    var bgRgb = _backgroundColor;
                    SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, bgRgb[0], bgRgb[1], bgRgb[2], 255);
                    SDL.SDL_RenderClear(_programContext.RendererHandle);
                }

                // Cursore di testo grafico (blink) sopra la pagina — stesse regole di SdlConsole.
                DrawTextCursor(_programContext.RendererHandle);

                SDL.SDL_RenderPresent(_programContext.RendererHandle);
            }
        }
    }

    /// <summary>Cursore di testo grafico: visibile se <see cref="_cursorVisible"/>, con blink
    /// ~500ms per gli stili Blinking* (parità SdlConsole.Internals). Disegnato sul renderer
    /// DOPO la pagina (il cursore NON viene salvato nello screenshot).</summary>
    private void DrawTextCursor(IntPtr renderer)
    {
        if (!_cursorVisible) return;
        if (_font == IntPtr.Zero) return; // niente font → niente misurazioni TTF sicure
        if (_cursorStyle is CommonLib.Consoles.CursorStyle.BlinkingBlock
            or CommonLib.Consoles.CursorStyle.BlinkingBar
            or CommonLib.Consoles.CursorStyle.BlinkingUnderline
            && _cursorBlinkTimer.ElapsedMilliseconds % 1000 > 500)
            return; // fase OFF del blink

        var sc = _renderScale > 0 ? _renderScale : 1.0f;
        var bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        // Clip del cursore al viewport nativo: coordinate folli (overflow da posizioni
        // fuori schermo durante le demo) non devono mai raggiungere SDL_RenderFillRect.
        var cx = Math.Max(0, Math.Min((int)Math.Round(_cursorPosition.x * sc), bw - 2));
        var cy = Math.Max(0, Math.Min((int)Math.Round(_cursorPosition.y * sc), bh - 2));
        // Dimensioni FISSE dalla cella carattere (niente TTF_SizeUTF8 qui: le misurazioni
        // TTF nel Present, mentre il thread VM accoda operazioni, causavano crash nativi
        // racem/timing — il cursore non deve chiamare SDL_ttf).
        var cw = Math.Max(2, Math.Min((int)Math.Round((_charCellWidth > 0 ? _charCellWidth : 8) * sc), bw));
        var ch = Math.Max(2, Math.Min((int)Math.Round((_charCellHeight > 0 ? _charCellHeight : 16) * sc), bh));
        var rect = new SDL.SDL_Rect { x = cx, y = cy, w = cw, h = ch };
        if (_cursorStyle is CommonLib.Consoles.CursorStyle.BlinkingBar)
            rect.w = Math.Max(2, rect.w / 4);
        else if (_cursorStyle is CommonLib.Consoles.CursorStyle.BlinkingUnderline)
        {
            rect.h = Math.Max(2, rect.h / 6);
            rect.y = cy + ch - rect.h;
        }
        SDL.SDL_SetRenderDrawColor(renderer, _currentColor[0], _currentColor[1], _currentColor[2], 255);
        SDL.SDL_RenderFillRect(renderer, ref rect);
    }

    /// <summary>
    /// Inserisce programmaticamente un evento SDL_QUIT nella coda degli eventi.
    /// Questo è il modo corretto e thread-safe per richiedere la chiusura dell'applicazione
    /// dall'esterno del loop di eventi principale (es. dal comando QUIT della REPL).
    /// </summary>
    public void RequestQuit()
    {
        _commandQueue.Add(() =>
        {
            var quitEvent = new SDL.SDL_Event { type = SDL.SDL_EventType.SDL_QUIT };
            SDL.SDL_PushEvent(ref quitEvent);
        });
    }

    /// <summary>
    /// Gestore di eventi che viene invocato da SdlEventManager quando si verifica
    /// un evento relativo a una finestra.
    /// </summary>
    private void HandleWindowEvent(SDL.SDL_Event e)
    {
        // Controlliamo specificamente se l'evento è la chiusura della finestra
        // del programma grafico.
        if (e.window.windowEvent == SDL.SDL_WindowEventID.SDL_WINDOWEVENT_CLOSE &&
            _programContext is not null &&
            e.window.windowID == SDL.SDL_GetWindowID(_programContext.WindowHandle))
        {
            // Notifica l'Executor che l'utente ha richiesto la chiusura del programma.
            // L'Executor fermerà i task e richiederà la transizione a REPL.
            WindowCloseRequested?.Invoke();
        }
        else if (e.window.windowEvent == SDL.SDL_WindowEventID.SDL_WINDOWEVENT_RESIZED &&
                 _programContext is not null &&
                 e.window.windowID == SDL.SDL_GetWindowID(_programContext.WindowHandle))
        {
            WindowResized?.Invoke();
        }
    }

    #endregion

    #region Internal Implementations

    /// <summary>
    /// Implementazione interna per l'impostazione dello schermo.
    /// Ora crea sempre un nuovo contesto per il programma per evitare contaminazioni di stato.
    /// </summary>
    /// <summary>
    /// Implementazione interna per l'impostazione dello schermo.
    /// Ora crea sempre un nuovo contesto per il programma e imposta lo stato del GraphicsManager.
    /// </summary>
    private void ScreenInternal(int mode, int activePage, int visiblePage, ScreenConfig? config = null,
        bool performImplicitCls = true, ScreenWindowMode windowMode = ScreenWindowMode.Windowed)
    {
        ResetViewportAndWindow();
        if (_captureMode) { Capture($"SCREEN {mode},{activePage},{visiblePage}"); return; }
        Capture($"SCREEN {mode},{activePage},{visiblePage}");
        // La modalita' PRECEDENTE va letta PRIMA dell'assegnazione: piu' sotto la condizione di
        // "cambio pagina" confrontava _currentScreenMode con se' STESSO, quindi era sempre vera:
        // un cambio di modalita' reale (es. SCREEN 13 -> SCREEN 12) non ricostruiva MAI il
        // contesto e la risoluzione logica restava quella precedente (CurrentScreenMode
        // riportava la nuova, ScreenWidth la vecchia). Il ramo serve solo per lo SCREEN di
        // pagina (SCREEN mode,ap,vp con la STESSA modalita').
        var previousScreenMode = _currentScreenMode;
        _currentScreenMode = (ScreenMode)mode;
        if (mode == 0)
        {
            CloseProgramWindow();
            return;
        }

        var activeConfig = config ?? ScreenConfig.GetDefault((ScreenMode)mode);

        // Stessa modalità e nessuna config custom: è un CAMBIO PAGINA (SCREEN mode, ap, vp) —
        // NON distruggere il contesto (il Dispose azzerava il contenuto di tutte le pagine:
        // dopo SCREEN 12,1,1 + SCREEN 12,0,0 la pagina 0 era vuota → PCOPY/resto = nero).
        if (_programContext != null && IsScreenOn && previousScreenMode == (ScreenMode)mode && config == null)
        {
            _activePage = activePage;
            _visiblePage = visiblePage;
            return;
        }

        // Se esiste già un contesto, distruggilo per crearne uno nuovo pulito.
        _programContext?.Dispose();

        // Se DESKTOP mode, usa la risoluzione del display
        int w, h;
        if (mode == -1)
        {
            SDL.SDL_DisplayMode displayMode;
            SDL.SDL_GetDesktopDisplayMode(0, out displayMode);
            w = displayMode.w;
            h = displayMode.h;
        }
        else
        {
            w = activeConfig.WindowWidth ?? activeConfig.LogicalWidth;
            h = activeConfig.WindowHeight ?? activeConfig.LogicalHeight;
        }

        // Determina i flag SDL in base alla modalità finestra
        var sdlFlags = SDL.SDL_WindowFlags.SDL_WINDOW_SHOWN | SDL.SDL_WindowFlags.SDL_WINDOW_ALLOW_HIGHDPI;
        sdlFlags |= windowMode switch
        {
            ScreenWindowMode.Fullscreen => SDL.SDL_WindowFlags.SDL_WINDOW_FULLSCREEN,
            ScreenWindowMode.FullscreenDesktop => SDL.SDL_WindowFlags.SDL_WINDOW_FULLSCREEN_DESKTOP,
            ScreenWindowMode.Borderless => SDL.SDL_WindowFlags.SDL_WINDOW_BORDERLESS,
            _ => 0
        };

        _programContext = new SdlContext("GMBASIC - Program",
            activeConfig.WindowX ?? SDL.SDL_WINDOWPOS_CENTERED,
            activeConfig.WindowY ?? SDL.SDL_WINDOWPOS_CENTERED,
            w, h, sdlFlags);

        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);

        _screenWidth = activeConfig.LogicalWidth;
        _screenHeight = activeConfig.LogicalHeight;
        // Scala HiDPI: output del renderer in pixel / dimensione finestra in punti.
        // Su Retina 640x480 logici → 1280x960 fisici → renderScale 2. Le pagine vengono
        // create a risoluzione nativa e il Present copia 1:1 (niente scala → niente
        // filtro → testo e sprite nitidi). Stesso pattern di SDLConsole.
        SDL.SDL_GetRendererOutputSize(_programContext.RendererHandle, out var _outW, out var _outH);
        SDL.SDL_GetWindowSize(_programContext.WindowHandle, out var _winW, out var _winH);
        _renderScale = (_outW > 0 && _winW > 0) ? (float)_outW / _winW : 1.0f;
        if (_renderScale < 1.0f) _renderScale = 1.0f;
        _backingWidth = (int)Math.Round(_screenWidth * _renderScale);
        _backingHeight = (int)Math.Round(_screenHeight * _renderScale);
        // PaletteData può essere null nelle config di default → _colorPalette restava null:
        // PALETTE era un no-op silenzioso (PaletteInternal faceva return) e FindColorIndex
        // (usato da IMAGEGET/BSAVE) restituiva sempre 0 → IMAGEPUT/BLOAD disegnavano nero.
        _colorPalette = activeConfig.PaletteData ?? ScreenConfig.Palettes.GetVGA16Palette();
        // Le pagine sono a risoluzione NATIVA (_backingWidth × _backingHeight). I comandi
        // SDL vengono chiamati con coordinate LOGICHE e scalati MANUALMENTE (× _renderScale)
        // in ogni punto di rendering: SDL_RenderSetScale NON funziona perché RenderGeometry
        // (DRAW TEXTURES) non la rispetta, creando incoerenza tra comandi. Vedi le chiamate
        // SDL in Internals.cs (LineInternal/PsetInternal/CircleInternal/DrawTexture*).

        // Assegniamo i valori ai campi di stato del GraphicsManager.
        _defaultForegroundColor = activeConfig.DefaultFgColor;
        _defaultBackgroundColor = activeConfig.DefaultBgColor;
        _charGridWidth = activeConfig.CharGridWidth ?? 0;
        _charGridHeight = activeConfig.CharGridHeight ?? 0;
        _currentFontPath = activeConfig.FontPath;
        _currentFontSize = activeConfig.FontSize ?? 0;

        _activePage = activePage;
        _visiblePage = visiblePage;
        _cursorPosition = (_screenWidth / 2, _screenHeight / 2);

        CreatePages(_programContext.RendererHandle, _programPageTextures);

        //System.Diagnostics.Debug.WriteLine($"[MAIN_THREAD] Dentro ScreenInternal. Imposto IsScreenOn = true.");
        IsScreenOn = true;

        ColorInternal(activeConfig.DefaultFgColor, activeConfig.DefaultBgColor);
        if (!string.IsNullOrEmpty(activeConfig.FontPath) && activeConfig.FontSize.HasValue)
            FontInternal(activeConfig.FontPath, activeConfig.FontSize.Value);

        // Esegue una CLS implicita per pulire la pagina attiva ogni volta
        // che viene impostata una nuova modalità grafica, garantendo una "tela bianca".
        if (performImplicitCls)
        {
            ClsInternal(null);
        }
    }

    /// <summary>
    /// Distrugge la finestra grafica del programma e le risorse associate.
    /// Azzerando le variabili, si forza la ricreazione completa alla prossima
    /// chiamata a SCREEN > 0.
    /// </summary>
    private void CloseProgramWindow()
    {
        LogDebugGfx("[GFX_MAN] Chiamata a CloseProgramWindow.");

        // Se il contesto del programma non esiste o lo schermo non è attivo, non c'è nulla da fare.
        if (_programContext is null || !IsScreenOn)
        {
            LogDebugGfx("[GFX_MAN] Nessuna finestra di programma da chiudere.");
            return;
        }

        // Pulisce le texture caricate dall'utente.
        foreach (var texturePtr in _loadedTextures.Values)
        {
            if (texturePtr != IntPtr.Zero) SDL.SDL_DestroyTexture(texturePtr);
        }

        _loadedTextures.Clear();
        _nextTextureId = 1;

        // Pulisce le texture delle pagine associate al programma.
        foreach (var texturePtr in _programPageTextures.Values)
        {
            if (texturePtr != IntPtr.Zero) SDL.SDL_DestroyTexture(texturePtr);
        }

        _programPageTextures.Clear();

        // Pulisce gli schermi salvati, che sono legati all'esecuzione del programma.
        foreach (var screen in _savedScreens.Values)
        {
            if (screen.Texture != IntPtr.Zero) SDL.SDL_DestroyTexture(screen.Texture);
        }

        _savedScreens.Clear();

        // Rilascia il font specifico del programma.
        if (_font != IntPtr.Zero)
        {
            SDL_ttf.TTF_CloseFont(_font);
            _font = IntPtr.Zero;
        }

        // La chiamata a Dispose() gestisce la distruzione sicura di renderer e finestra.
        _programContext.Dispose();
        _programContext = null;

        // Resetta lo stato del GraphicsManager per indicare che la modalità grafica non è più attiva.
        IsScreenOn = false;
        _currentScreenMode = ScreenMode.Text;

        LogDebugGfx("[GFX_MAN] Finestra del programma e risorse associate chiuse con successo.");
    }

    #endregion

    /// <summary>
    /// Libera le risorse gestite dal gestore della grafica.
    /// </summary>
    public void Dispose()
    {
        LogDebugGfx("[GFX_MAN.Dispose] Avvio del rilascio finale delle risorse grafiche.");

        // Se non è mai stato inizializzato, non c'è nulla da fare.
        if (!IsInitialized) return;

        // 1. Deregistra gli handler dagli eventi globali per prevenire chiamate su oggetti rilasciati.
        SdlManager.Instance.WindowEvent -= HandleWindowEvent;

        // 2. Accoda un'ultima azione per chiudere la finestra del programma, se ancora aperta.
        //    Questo assicura una pulizia ordinata anche se Dispose viene chiamato inaspettatamente.
        _commandQueue.Add(CloseProgramWindow);

        // 3. Completa l'aggiunta di comandi e processa la coda un'ultima volta.
        _commandQueue.CompleteAdding();
        ProcessQueuedCommands();

        _commandQueue.Dispose();

        // Gestisce la chiusura dell'interprete mentre una finestra è ancora aperta.
        foreach (var t in _loadedTextures.Values) SDL.SDL_DestroyTexture(t);
        _loadedTextures.Clear();

        // 4. Chiama Quit sul manager centralizzato. Se altri componenti lo usano,
        //    la logica idempotente impedirà una chiusura prematura. Se questo è
        //    l'ultimo componente a essere rilasciato, SDL verrà chiuso correttamente.
        SdlManager.Instance.Quit();

        // 5. Resetta i flag di stato.
        IsInitialized = false;
        IsScreenOn = false;
        _programContext = null; // Assicura che il riferimento sia nullo dopo il dispose.

        LogDebugGfx("[GFX_MAN.Dispose] Rilascio finale completato.");
    }

    /// <summary>
    /// Disattiva la modalità grafica. Nel nuovo modello a due finestre,
    /// questo metodo delega la chiusura alla funzione specifica per la finestra del programma.
    /// </summary>
    private void CloseGraphicsInternal()
    {
        // La logica è stata spostata in CloseProgramWindow per maggiore chiarezza.
        // Questa chiamata assicura che SCREEN 0 si comporti come previsto.
        CloseProgramWindow();
    }

    /// <summary>
    /// Crea le texture per le pagine grafiche sul renderer specificato,
    /// popolando il dizionario corrispondente.
    /// </summary>
    private void CreatePages(IntPtr renderer, Dictionary<int, IntPtr> pageDictionary)
    {
        foreach (var t in pageDictionary.Values) SDL.SDL_DestroyTexture(t);
        pageDictionary.Clear();
        for (var i = 0; i < 8; i++)
        {
            var tex = SDL.SDL_CreateTexture(renderer, SDL.SDL_PIXELFORMAT_ARGB8888,
                (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_TARGET, _backingWidth > 0 ? _backingWidth : _screenWidth,
                _backingHeight > 0 ? _backingHeight : _screenHeight);
            pageDictionary[i] = tex;
        }
    }


    /// <summary>
    /// Legge i pixel della pagina attiva in un buffer di array di interi senza segno.
    /// </summary>
    private uint[] ReadActivePageToBuffer(SDL.SDL_Rect? rect = null)
    {
        // La guardia ora controlla solo il contesto del programma. Se non esiste, ritorna un buffer vuoto.
        if (_programContext is null)
            return [];

        var bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        var r = rect ?? new SDL.SDL_Rect { x = 0, y = 0, w = bw, h = bh };
        if (r.w <= 0 || r.h <= 0)
            return [];

        var p = new uint[r.w * r.h];
        var h = GCHandle.Alloc(p, GCHandleType.Pinned);
        try
        {
            SetRenderTargetToActivePage();
            SDL.SDL_RenderReadPixels(_programContext.RendererHandle, ref r, SDL.SDL_PIXELFORMAT_ARGB8888,
                h.AddrOfPinnedObject(), r.w * sizeof(uint));
        }
        finally
        {
            h.Free();
        }

        // Il ReadPixels dalle texture target del renderer restituisce le righe CAPOVOLTE
        // (verificato: una linea disegnata a y=10 appariva a y=479-y nel buffer). Il
        // rendering top-down sulla texture è corretto (la finestra via Present lo mostra
        // bene); solo la LETTURA va invertita. UpdateActivePageFromBuffer inverte allo
        // stesso modo, così PAINT/GetPixel/screenshot restano coerenti con il disegno.
        var flipped = new uint[p.Length];
        for (var yy = 0; yy < r.h; yy++)
            Array.Copy(p, yy * r.w, flipped, (r.h - 1 - yy) * r.w, r.w);
        return flipped;
    }

    /// <summary>
    /// Salva la pagina attiva della finestra del programma come immagine BMP (usando
    /// SDL_SaveBMP). Ritorna false se la finestra grafica non è attiva o il salvataggio
    /// fallisce. È pensato per test/verifica automatica: chi lo chiama deve assicurarsi
    /// che il rendering sia già stato presentato (il game loop esterno chiama Present()).
    /// </summary>
    public bool SaveActivePageScreenshot(string filePath)
    {
        if (_programContext is null || !IsScreenOn) return false;
        try
        {
            var pixels = ReadActivePageToBuffer();
            var bw3 = _backingWidth > 0 ? _backingWidth : _screenWidth;
            var bh3 = _backingHeight > 0 ? _backingHeight : _screenHeight;
            if (pixels.Length == 0 || bw3 <= 0 || bh3 <= 0) return false;

            var h = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            try
            {
                var surface = SDL.SDL_CreateRGBSurfaceFrom(
                    h.AddrOfPinnedObject(), bw3, bh3, 32,
                    bw3 * sizeof(uint),
                    0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                if (surface == IntPtr.Zero) return false;
                try
                {
                    var result = SDL.SDL_SaveBMP(surface, filePath);
                    return result == 0;
                }
                finally
                {
                    SDL.SDL_FreeSurface(surface);
                }
            }
            finally
            {
                h.Free();
            }
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Aggiorna la pagina attiva con il contenuto di un buffer di pixel.
    /// Opera sempre sul set di pagine della finestra del programma.
    /// </summary>
    private void UpdateActivePageFromBuffer(uint[] p)
    {
        // Cerca la texture attiva nel dizionario del programma.
        if (!_programPageTextures.TryGetValue(_activePage, out var t)) return;

        // Il buffer p[] è in orientamento visivo (top-down, come lo produce
        // ReadActivePageToBuffer): la texture va aggiornata CAPOVOLTA (il ReadPixels
        // restituisce le righe invertite — vedi ReadActivePageToBuffer).
        var flipped = new uint[p.Length];
        var bw2 = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var bh2 = _backingHeight > 0 ? _backingHeight : _screenHeight;
        for (var yy = 0; yy < bh2; yy++)
            Array.Copy(p, yy * bw2, flipped, (bh2 - 1 - yy) * bw2, bw2);
        var h = GCHandle.Alloc(flipped, GCHandleType.Pinned);
        try
        {
            SDL.SDL_UpdateTexture(t, IntPtr.Zero, h.AddrOfPinnedObject(), bw2 * sizeof(uint));
        }
        finally
        {
            h.Free();
        }
    }

    /// <summary>
    /// Imposta la pagina attiva come target di rendering.
    /// Seleziona il renderer e il dizionario delle pagine corretti in base
    /// allo stato di IsScreenOn (modalità programma vs modalità REPL).
    /// </summary>
    private void SetRenderTargetToActivePage()
    {
        if (_programContext is null) return;
        SDL.SDL_SetRenderTarget(_programContext.RendererHandle,
            _programPageTextures.TryGetValue(_activePage, out var t) ? t : IntPtr.Zero);
    }

    /// <summary>
    /// PIXELBUFFER: disegna un buffer ARGB (row-major, 0x00RRGGBB) come texture a schermo
    /// intero. Fire-and-forget sulla coda GPM (come LINE/PSET/...): viene eseguito sul MAIN
    /// THREAD in ORDINE dopo le altre primitive (es. SCREEN), quindi il check screen-attivo
    /// avviene qui, non dal thread VM. Con OpenGL reale creare una texture dal thread VM
    /// produrrebbe texture nere, quindi il blit DEVE avvenire sul thread del renderer.
    /// </summary>
    public void EnqueuePixelBuffer(byte[] argb, int width, int height) =>
        EnqueueGpmOperation(() => { DrawPixelBufferInternal(argb, width, height); return null; }, (_, _) => { });

    /// <summary>Renderizza un buffer TUI (ScreenBuffer, griglia di celle con carattere + fg/bg
    /// true-color) sulla pagina grafica, cella per cella, accodato sul main thread (pattern
    /// GPM). Usato dal backend IL per disegnare le finestre TUI (WindowManager) anche quando
    /// lo schermo grafico è attivo — il TUI non è più solo ANSI console. I caratteri sono
    /// disegnati con il font grafico corrente; le celle con carattere '\0'/' ' riempiono lo
    /// sfondo. Il bg di ogni cella viene disegnato come un quadrato (DrawText di uno spazio
    /// col bg) per coprire l'area.</summary>
    public void RenderScreenBufferGpm(CommonLib.Consoles.Tui.ScreenBuffer buffer, Action<object?, Exception?>? completion = null)
    {
        if (buffer is null) { completion?.Invoke(null, null); return; }
        int w = buffer.Width, h = buffer.Height;
        var cells = new CommonLib.Screens.ScreenCell[h, w];
        for (var yy = 0; yy < h; yy++)
            for (var xx = 0; xx < w; xx++)
                cells[yy, xx] = buffer.Cells[yy, xx];
        EnqueueGpmOperation(() =>
        {
            RenderScreenBufferInternal(cells, w, h);
            return null;
        }, completion ?? ((_, _) => { }));
    }

    private void RenderScreenBufferInternal(CommonLib.Screens.ScreenCell[,] cells, int w, int h)
    {
        if (_programContext is null) return;
        var renderer = _programContext.RendererHandle;
        if (renderer == IntPtr.Zero) return;
        // Fallback: se nessun FONT è stato caricato dal programma, carica il font di
        // progetto (Resources/font.ttf) così il TUI renderizza anche senza FONT esplicito.
        if (_font == IntPtr.Zero)
        {
            var fontCandidates = new[]
            {
                Path.Combine(CommonLib.GmbiPaths.SharedResourcesDir, "font.ttf"),   // una sola Resources
                Path.Combine(AppContext.BaseDirectory, "Resources", "font.ttf"),
                Path.Combine(AppContext.BaseDirectory, "font.ttf"),
                Path.Combine(Directory.GetCurrentDirectory(), "Resources", "font.ttf"),
                Path.Combine("Resources", "font.ttf"),
            };
            foreach (var fc in fontCandidates)
            {
                if (File.Exists(fc)) { FontInternal(fc, 14); break; }
            }
        }
        if (_font == IntPtr.Zero) return; // nessun font disponibile: TUI non renderizzabile
        var cw = _charCellWidth > 0 ? _charCellWidth : 8;
        var ch = _charCellHeight > 0 ? _charCellHeight : 16;
        for (var row = 0; row < h; row++)
        {
            for (var col = 0; col < w; col++)
            {
                var cell = cells[row, col];
                var ch2 = cell.Character == '\0' ? ' ' : cell.Character;
                var fg = ToArgb(cell.ForegroundRgb);
                var bg = ToArgb(cell.BackgroundRgb);
                // Sfondo: QUADRATO PIENO col bg (SDL_RenderFillRect), non un glifo spazio —
                // il glifo non riempie l'intera cella e lascia trasparire ciò che c'è sotto
                // (finestre sovrapposte si mischiavano). Il fill copre sempre l'area.
                var bgRect = new SDL.SDL_Rect
                {
                    x = col * cw,
                    y = row * ch,
                    w = cw + 1,
                    h = ch + 1
                };
                SDL.SDL_SetRenderDrawColor(renderer, (byte)((bg >> 16) & 0xFF), (byte)((bg >> 8) & 0xFF), (byte)(bg & 0xFF), 255);
                SDL.SDL_RenderFillRect(renderer, ref bgRect);
                if (ch2 != ' ')
                    // DrawTextInternal centra la LINEA su y: si passa il centro della cella
                    // (row*ch + ch/2) così il carattere è centrato verticalmente nella cella.
                    DrawTextInternal(ch2.ToString(), col * cw, row * ch + ch / 2, 0, TextAlign.Left, fg);
            }
        }
    }

    private static uint ToArgb(CellColor? c)
    {
        if (c is null || c.Value.IsDefault) return 0xFF000000;
        return 0xFF000000u | ((uint)c.Value.R << 16) | ((uint)c.Value.G << 8) | c.Value.B;
    }

    private void DrawPixelBufferInternal(byte[] argb, int width, int height)
    {
        if (_programContext is null) return;
        var renderer = _programContext.RendererHandle;
        if (renderer == IntPtr.Zero || width <= 0 || height <= 0) return;
        var tex = SDL.SDL_CreateTexture(renderer, SDL.SDL_PIXELFORMAT_ARGB8888,
            (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_STATIC, width, height);
        if (tex == IntPtr.Zero) return;
        var handle = System.Runtime.InteropServices.GCHandle.Alloc(argb, System.Runtime.InteropServices.GCHandleType.Pinned);
        try { SDL.SDL_UpdateTexture(tex, IntPtr.Zero, handle.AddrOfPinnedObject(), width * 4); }
        finally { handle.Free(); }
        if (_programPageTextures.TryGetValue(_activePage, out var page))
        {
            SDL.SDL_SetRenderTarget(renderer, page);
            // Blit 1:1 (semantica storica motore C): il buffer NON viene stirato a
            // tutto schermo ma scritto alla sua dimensione in alto a sinistra.
            var dst = new SDL.SDL_Rect { x = 0, y = 0, w = width, h = height };
            SDL.SDL_RenderCopy(renderer, tex, IntPtr.Zero, ref dst);
        }
        SDL.SDL_DestroyTexture(tex);
    }

    /// <summary>
    /// Helper per l'algoritmo di riempimento. Controlla la linea successiva per i pixel da riempire.
    /// </summary>
    private void CheckNextLine(uint[] p, int ny, int wx, int ex, int s, uint tc, Stack<(int, int)> st)
    {
        for (var i = wx; i <= ex; i++)
        {
            if (p[(long)ny * s + i] == tc)
            {
                st.Push((i, ny));
                while (i <= ex && p[(long)ny * s + i] == tc) i++;
            }
        }
    }

    /// <summary>
    /// Ottiene i valori RGB per un dato indice di colore dalla palette corrente.
    /// </summary>
    private byte[] GetColor(int i)
    {
        // Colore RGB diretto (flag 0x40000000 da IGraphicsService.Color per gli RGBA):
        // supporta QUALSIASI colore, non solo la palette 0-15.
        if ((i & 0x40000000) != 0)
            return [(byte)(i >> 16), (byte)(i >> 8), (byte)i];
        var p = _colorPalette ?? ScreenConfig.Palettes.GetVGA16Palette();
        return i >= 0 && i < p.Length ? p[i] : p[0];
    }

    /// <summary>
    /// Accoda una richiesta per recuperare i componenti RGB di un colore dalla palette
    /// e restituisce il risultato in modo asincrono e thread-safe.
    /// </summary>
    /// <param name="index">L'indice del colore nella palette.</param>
    /// <returns>Un array di 3 byte [R, G, B] se l'indice è valido, altrimenti null.</returns>
    public Task<byte[]?> GetPaletteColorAsync(int index)
    {
        var tcs = new TaskCompletionSource<byte[]?>();
        // Accoda un'azione che verrà eseguita sul thread principale (quello del rendering)
        _commandQueue.Add(() =>
        {
            try
            {
                // Chiama il metodo interno 'GetColor' che può accedere in sicurezza a '_colorPalette'
                var result = GetColor(index);
                tcs.TrySetResult(result);
            }
            catch (Exception ex)
            {
                tcs.TrySetException(ex);
            }
        });
        return tcs.Task;
    }

    /// <summary>
    /// Trova l'indice più vicino nella palette di colori per un dato valore RGB.
    /// </summary>
    private int FindColorIndex(uint p)
    {
        // Pixel con alfa < 128 (trasparente, es. lo sfondo di un PNG) → indice -1: è la
        // convenzione del blit IMAGEPUT/IMAGEPUTS (ci < 0 = trasparente, salta il pixel).
        // Senza questo, IMAGEGET catturava lo sfondo trasparente di sprite.png come indice
        // 0 (nero opaco) → IMAGEPUTS ridisegnava il QUADRATO nero attorno allo sprite.
        var alpha = (byte)((p >> 24) & 0xFF);
        if (alpha < 128) return -1;

        if (_colorPalette == null) return 0; // non dovrebbe più accadere (inizializzata a SCREEN)
        byte r = (byte)((p >> 16) & 0xFF), g = (byte)((p >> 8) & 0xFF), b = (byte)(p & 0xFF);
        int bi = 0, md = int.MaxValue;
        for (var i = 0; i < _colorPalette.Length; i++)
        {
            var c = _colorPalette[i];
            var d = (r - c[0]) * (r - c[0]) + (g - c[1]) * (g - c[1]) + (b - c[2]) * (b - c[2]);
            if (d < md)
            {
                md = d;
                bi = i;
            }

            if (md == 0) break;
        }

        return bi;
    }

    private static uint RgbToUint(byte[] r) => GraphicsUtils.RgbToUint(r);

    /// <summary>
    /// Scrive un messaggio di debug sulla console, ma solo nelle build DEBUG.
    /// Questo metodo è condizionale e non viene compilato nelle build RELEASE.
    /// </summary>
    /// <param name="message">Il messaggio di debug da scrivere.</param>
    [System.Diagnostics.Conditional("DEBUG")]
    private static void LogDebugGfx(string message)
    {
        /* Console.WriteLine(message); */
    }

    // ====================================================================
    // VIEWPORT / WINDOW
    // ====================================================================

    /// <summary>Imposta il viewport (clipping rettangolo in pixel).</summary>
    public void SetViewport(int x1, int y1, int x2, int y2)
    {
        _viewX1 = Math.Max(0, Math.Min(x1, x2));
        _viewY1 = Math.Max(0, Math.Min(y1, y2));
        _viewX2 = Math.Min(_screenWidth - 1, Math.Max(x1, x2));
        _viewY2 = Math.Min(_screenHeight - 1, Math.Max(y1, y2));
        _viewPortActive = true;
    }

    /// <summary>Porta la finestra del programma in primo piano e le dà il focus di input
    /// (utile quando il processo parte in background — macOS può nascondere la finestra).</summary>
    public void RaiseToFront()
    {
        if (_programContext is null) return;
        try { SDL.SDL_RaiseWindow(_programContext.WindowHandle); } catch { }
        // SetWindowInputFocus da processo in background può BLOCCARE il main thread
        // (attende il WindowServer) → freeze totale dopo ~1s. Lo usiamo solo in foreground.
    }

    /// <summary>Imposta il sistema di coordinate logiche.</summary>
    public void SetWindow(double x1, double y1, double x2, double y2, bool screen)
    {
        _winX1 = x1; _winY1 = y1; _winX2 = x2; _winY2 = y2;
        _windowScreen = screen;
        _windowActive = true;
        if (!_viewPortActive)
            SetViewport(0, 0, _screenWidth - 1, _screenHeight - 1);
    }

    /// <summary>Resetta viewport e window.</summary>
    public void ResetViewportAndWindow()
    {
        _viewPortActive = false;
        _windowActive = false;
        _viewX1 = _viewY1 = 0;
        _viewX2 = _screenWidth - 1;
        _viewY2 = _screenHeight - 1;
    }

    /// <summary>Attiva la modalità capture. I comandi grafici vengono registrati ma non eseguiti.</summary>
    public void EnableCapture()
    {
        _captureMode = true;
        _capturedCommands.Clear();
    }

    /// <summary>Restituisce la lista dei comandi grafici catturati.</summary>
    public string[] GetCapturedCommands() => _capturedCommands.ToArray();

    /// <summary>Resetta la lista dei comandi catturati.</summary>
    public void ClearCapturedCommands() => _capturedCommands.Clear();

    private void Capture(string cmd)
    {
        if (_captureMode)
            _capturedCommands.Add(cmd);
    }

    /// <summary>Numero di comandi grafici catturati (per debugging).</summary>
    public int CapturedCommandCount => _capturedCommands.Count;

    /// <summary>Trasforma coordinate logiche → pixel.</summary>
    public (int px, int py) TransformPoint(double wx, double wy)
    {
        if (!_windowActive) return ((int)wx, (int)wy);
        var vw = _viewX2 - _viewX1;
        var vh = _viewY2 - _viewY1;
        var ww = _winX2 - _winX1;
        var wh = _winY2 - _winY1;
        if (ww == 0) ww = 1;
        if (wh == 0) wh = 1;
        var px = (int)((wx - _winX1) / ww * vw + _viewX1);
        var py = _windowScreen
            ? (int)((wy - _winY1) / wh * vh + _viewY1)
            : (int)(_viewY2 - (wy - _winY1) / wh * vh);
        return (px, py);
    }

    /// <summary>Applica trasformazione e clipping punto. Ritorna false se fuori viewport.</summary>
    public bool TransformAndClipPoint(ref int x, ref int y)
    {
        int vx1 = _viewPortActive ? _viewX1 : 0;
        int vy1 = _viewPortActive ? _viewY1 : 0;
        int vx2 = _viewPortActive ? _viewX2 : _screenWidth - 1;
        int vy2 = _viewPortActive ? _viewY2 : _screenHeight - 1;
        if (x < vx1 || x > vx2 || y < vy1 || y > vy2) return false;
        return true;
    }

    /// <summary>Applica trasformazione WINDOW + clipping VIEWPORT su un punto. Ritorna false se fuori.</summary>
    public bool WindowAndClipPoint(ref int x, ref int y)
    {
        int vx1 = _viewPortActive ? _viewX1 : 0;
        int vy1 = _viewPortActive ? _viewY1 : 0;
        int vx2 = _viewPortActive ? _viewX2 : _screenWidth - 1;
        int vy2 = _viewPortActive ? _viewY2 : _screenHeight - 1;
        if (x < vx1 || x > vx2 || y < vy1 || y > vy2) return false;
        return true;
    }

    /// <summary>Clippa un segmento (Liang-Barsky) al viewport. Ritorna false se completamente fuori.</summary>
    public bool ClipLine(ref int x1, ref int y1, ref int x2, ref int y2)
    {
        int vx1 = _viewPortActive ? _viewX1 : 0;
        int vy1 = _viewPortActive ? _viewY1 : 0;
        int vx2 = _viewPortActive ? _viewX2 : _screenWidth - 1;
        int vy2 = _viewPortActive ? _viewY2 : _screenHeight - 1;

        var dx = x2 - x1;
        var dy = y2 - y1;
        var p = new[] { -dx, dx, -dy, dy };
        var q = new[] { x1 - vx1, vx2 - x1, y1 - vy1, vy2 - y1 };
        var u1 = 0.0;
        var u2 = 1.0;

        for (var i = 0; i < 4; i++)
        {
            if (p[i] == 0)
            {
                if (q[i] < 0) return false;
            }
            else
            {
                var t = (double)q[i] / p[i];
                if (p[i] < 0) { if (t > u1) u1 = t; }
                else { if (t < u2) u2 = t; }
            }
        }

        if (u1 > u2) return false;
        if (u2 < 0 || u1 > 1) return false;

        if (u1 > 0)
        {
            x1 = x1 + (int)(u1 * dx);
            y1 = y1 + (int)(u1 * dy);
        }
        if (u2 < 1)
        {
            x2 = x1 + (int)(u2 * dx);
            y2 = y1 + (int)(u2 * dy);
        }
        return true;
    }
}