using CommonGraphics.Consoles;
using CommonLib.Keyboard;
using CommonLib.Consoles;
// FILE: Interpreter.Keyboard.KeyboardServiceSdl.cs

using System.Collections.Concurrent;
using System.Text;
using System.Threading.Channels;
using SDL2;

namespace CommonGraphics.Keyboard;

public static partial class Keyboard
{
    /// <summary>
    /// Implementazione di IKeyboardService per l'interfaccia grafica SDL2.
    /// Si integra con SdlManager per ricevere eventi in modo disaccoppiato
    /// e gestisce gli hook di tastiera registrati dall'utente (es. ON KEY).
    /// </summary>
    public sealed class KeyboardServiceSdl : IKeyboardService
    {
        /// <summary>
        /// La richiesta di input bloccante attualmente attiva (es. GET, INPUT).
        /// </summary>
        private IBlockingRequest? _activeRequest;

        /// <summary>
        /// Flag che indica se l'istanza è stata rilasciata.
        /// </summary>
        private volatile bool _disposed;

        /// <summary>
        /// Fornisce accesso all'unica istanza globale del servizio di tastiera SDL.
        /// </summary>
        private static readonly Lazy<KeyboardServiceSdl> LazyInstance = new(() => new KeyboardServiceSdl());

        /// <summary>
        /// Ottiene l'istanza Singleton del servizio.
        /// </summary>
        public static KeyboardServiceSdl Instance => LazyInstance.Value;

        /// <summary>
        /// Una coda per le richieste di input bloccanti in attesa di essere processate.
        /// </summary>
        private readonly ConcurrentQueue<IBlockingRequest> _blockingRequestQueue = new();

        /// <summary>
        /// Un canale thread-safe che funge da buffer per i tasti non bloccanti (usato da INKEY$).
        /// </summary>
        private readonly Channel<ConsoleKeyInfo> _nonBlockingKeyBuffer = Channel.CreateUnbounded<ConsoleKeyInfo>();

        /// <summary>
        /// Un dizionario per memorizzare gli hook di tastiera registrati (ON KEY).
        /// </summary>
        private readonly ConcurrentDictionary<KeyCombination, Action> _hooks = new();

        /// <summary>
        /// Un evento di reset manuale per mettere in pausa e riprendere il listener di eventi.
        /// </summary>
        private readonly ManualResetEventSlim _pauseEvent = new(true);

        /// <summary>
        /// Mappa statica per convertire i codici dei tasti SDL in ConsoleKey di .NET.
        /// </summary>
        private static readonly Dictionary<SDL.SDL_Keycode, ConsoleKey> SdlToConsoleKeyMap = BuildKeyMap();

        /// <summary>
        /// Ottiene la richiesta di input bloccante attualmente attiva, se presente.
        /// </summary>
        public IBlockingRequest? ActiveBlockingRequest => _activeRequest;

        /// <summary>
        /// Evento scatenato quando viene richiesta un'interruzione (es. Ctrl+C) non gestita da un hook.
        /// </summary>
        public event Action? BreakRequested;

        /// <summary>
        /// Inizializza una nuova istanza della classe KeyboardServiceSdl.
        /// Il costruttore è privato per forzare l'uso del pattern Singleton.
        /// Si iscrive agli eventi centralizzati di SdlManager.
        /// </summary>
        private KeyboardServiceSdl()
        {
            SdlManager.Instance.KeyboardEvent += ProcessEvent;
        }

        /// <summary>
        /// Questo metodo non è utilizzato attivamente nell'implementazione SDL.
        /// </summary>
        public void Start()
        {
        }

        /// <summary>
        /// Questo metodo non è utilizzato attivamente nell'implementazione SDL.
        /// </summary>
        public void Stop()
        {
        }

        /// <summary>
        /// Rilascia le risorse e si disiscrive dagli eventi SDL.
        /// </summary>
        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;

            SdlManager.Instance.KeyboardEvent -= ProcessEvent;

            _nonBlockingKeyBuffer.Writer.TryComplete();
            while (_blockingRequestQueue.TryDequeue(out var request))
            {
                if (request is ReadKeyRequest rkr) rkr.Tcs.TrySetCanceled();
                if (request is ReadLineRequest rlr) rlr.Tcs.TrySetCanceled();
            }

            _pauseEvent.Dispose();
        }

        /// <summary>
        /// Tenta di leggere un tasto dal buffer non bloccante in modo istantaneo.
        /// Usato dall'istruzione INKEY$ in modalità grafica e dalla REPL.
        /// </summary>
        /// <param name="convertToUppercase">Se true, converte il carattere in maiuscolo.</param>
        /// <returns>Un <see cref="ConsoleKeyInfo"/> se un tasto è disponibile, altrimenti null.</returns>
        public ConsoleKeyInfo? TryReadKey(bool convertToUppercase)
        {
            if (!_nonBlockingKeyBuffer.Reader.TryRead(out var key)) return null;

            if (!convertToUppercase) return key;

            var upperChar = char.ToUpper(key.KeyChar);
            return upperChar == key.KeyChar
                ? key
                : new ConsoleKeyInfo(upperChar, key.Key, key.Modifiers.HasFlag(ConsoleModifiers.Shift),
                    key.Modifiers.HasFlag(ConsoleModifiers.Alt), key.Modifiers.HasFlag(ConsoleModifiers.Control));
        }

        /// <summary>
        /// Richiede in modo asincrono la lettura del prossimo tasto premuto.
        /// Usato da GET e WAITKEY in modalità grafica.
        /// </summary>
        /// <param name="cancellationToken">Il token per annullare l'operazione.</param>
        /// <param name="convertToUppercase">Se true, converte il carattere associato al tasto in maiuscolo.</param>
        /// <returns>Un <see cref="ValueTask"/> che si completa con il <see cref="ConsoleKeyInfo"/> del tasto premuto.</returns>
        public ValueTask<ConsoleKeyInfo> ReadKeyAsync(CancellationToken cancellationToken = default,
            bool convertToUppercase = false)
        {
            var tcs = new TaskCompletionSource<ConsoleKeyInfo>(TaskCreationOptions.RunContinuationsAsynchronously);
            _blockingRequestQueue.Enqueue(new ReadKeyRequest(tcs, convertToUppercase));
            cancellationToken.Register(() => tcs.TrySetCanceled());
            return new ValueTask<ConsoleKeyInfo>(tcs.Task);
        }

        /// <summary>
        /// Richiede in modo asincrono la lettura di un'intera riga di input.
        /// Usato da INPUT e LINE INPUT in modalità grafica e dalla REPL.
        /// </summary>
        /// <param name="console">La console su cui verrà visualizzato l'input.</param>
        /// <param name="prompt">Il prompt da visualizzare (ora gestito dall'Executor).</param>
        /// <param name="charConversionRule">Una regola di conversione opzionale per i caratteri digitati.</param>
        /// <param name="cancellationToken">Il token per l'annullamento.</param>
        /// <returns>Un task che si completa con la stringa letta, o null se annullato.</returns>
        public Task<string?> ReadLineAsync(IConsole console, string prompt, Func<char, string,
            (bool process, char convertedChar)>? charConversionRule, CancellationToken cancellationToken = default)
        {
            // --- INIZIO MODIFICA ---
            // Scrivi il prompt sulla console immediatamente.
            // Il ciclo di rendering principale (RunMainLoop) lo visualizzerà al prossimo frame.
            console.Out.Write(prompt);
            // --- FINE MODIFICA ---

            var tcs = new TaskCompletionSource<string?>(TaskCreationOptions.RunContinuationsAsynchronously);
            var request = new ReadLineRequest(console, new StringBuilder(), prompt, tcs, charConversionRule);
            _blockingRequestQueue.Enqueue(request);
            cancellationToken.Register(() => tcs.TrySetCanceled());
            return tcs.Task;
        }

        /// <summary>
        /// Registra un'azione (un "hook") da eseguire quando viene premuta una specifica combinazione di tasti.
        /// </summary>
        /// <param name="key">La combinazione di tasti da intercettare.</param>
        /// <param name="action">L'azione da eseguire.</param>
        public void RegisterHook(KeyCombination key, Action action) => _hooks[key] = action;

        /// <summary>
        /// Rimuove un hook registrato per una specifica combinazione di tasti.
        /// </summary>
        /// <param name="key">La combinazione di tasti per cui rimuovere l'hook.</param>
        public void UnregisterHook(KeyCombination key) => _hooks.TryRemove(key, out _);

        /// <summary>
        /// Rimuove tutti gli hook di tastiera registrati.
        /// </summary>
        public void UnregisterAllHooks() => _hooks.Clear();

        /// <summary>
        /// Verifica se un hook è attualmente registrato per una data combinazione di tasti.
        /// </summary>
        /// <param name="keyCombination">La combinazione di tasti da verificare.</param>
        /// <returns>True se un hook è registrato, altrimenti false.</returns>
        public bool IsHookRegistered(KeyCombination keyCombination) => _hooks.ContainsKey(keyCombination);

        /// <summary>
        /// Svuota il buffer dei tasti non bloccanti e la coda di eventi di SDL.
        /// </summary>
        public ValueTask ClearBufferAsync()
        {
            while (_nonBlockingKeyBuffer.Reader.TryRead(out _))
            {
            }

            SDL.SDL_PumpEvents();
            SDL.SDL_FlushEvents(SDL.SDL_EventType.SDL_KEYDOWN, SDL.SDL_EventType.SDL_TEXTINPUT);
            return ValueTask.CompletedTask;
        }

        /// <summary>
        /// Mette in pausa il processamento degli eventi della tastiera.
        /// </summary>
        public void Pause() => _pauseEvent.Reset();

        /// <summary>
        /// Riprende il processamento degli eventi della tastiera.
        /// </summary>
        public void Resume() => _pauseEvent.Set();

        /// <summary>
        /// Metodo centrale che riceve tutti gli eventi di tastiera da SdlManager.
        /// Gestisce la priorità degli hook e smista l'evento all'handler corretto.
        /// </summary>
        /// <param name="e">L'evento SDL da processare.</param>
        public void ProcessEvent(SDL.SDL_Event e)
        {
            // 1. Ignora eventi non desiderati o se il servizio è in pausa/disposed.
            if (_disposed || !_pauseEvent.IsSet || e.type != SDL.SDL_EventType.SDL_KEYDOWN || e.key.repeat != 0) return;

            // 2. Converte l'evento SDL in una rappresentazione .NET.
            if (!SdlToConsoleKeyMap.TryGetValue(e.key.keysym.sym, out var consoleKey)) return;
            var modifiers = GetModifiers(e.key.keysym.mod);
            var keyCombination = new KeyCombination(consoleKey, modifiers);

            // 3. Gestione prioritaria degli hook.
            // (Questa parte rimane invariata...)
            if (keyCombination.Key == ConsoleKey.C && modifiers.HasFlag(ConsoleModifiers.Control))
            {
                if (_hooks.TryGetValue(keyCombination, out var userHookAction))
                {
                    userHookAction.Invoke();
                }
                else
                {
                    BreakRequested?.Invoke();
                }

                return; // Evento gestito.
            }

            if (_hooks.TryGetValue(keyCombination, out var hookAction))
            {
                hookAction.Invoke();
                return; // Hook eseguito, evento gestito.
            }

            // 4. Se nessun hook è stato attivato, processa l'evento come input normale.

            // Controlla se c'è una richiesta bloccante attiva o in coda.
            if (_activeRequest is not null || !_blockingRequestQueue.IsEmpty)
            {
                // Tenta di processare l'evento per una richiesta bloccante.
                // Il metodo interno restituirà 'true' se l'evento è stato consumato.
                if (ProcessBlockingRequests(e))
                {
                    // Se l'evento è stato usato per soddisfare una richiesta bloccante (es. WAITKEY),
                    // interrompiamo immediatamente il processamento qui. L'evento ha fatto il suo
                    // dovere e NON deve finire nel buffer non bloccante di INKEY$.
                    return;
                }
            }

            // 5. Se non c'era una richiesta bloccante, o se l'evento non era adatto
            // (es. un tasto non stampabile durante un INPUT), l'evento è disponibile
            // per l'input non bloccante.
            ProcessNonBlockingEvent(e);
        }

        /*
        /// <summary>
        /// Metodo centrale che riceve tutti gli eventi di tastiera da SdlManager.
        /// Gestisce la priorità degli hook e smista l'evento all'handler corretto.
        /// CORRETTO: Assicura che un evento che soddisfa una richiesta bloccante
        /// venga consumato completamente e non passi all'handler non bloccante.
        /// </summary>
        /// <param name="e">L'evento SDL da processare.</param>
        public void ProcessEvent(SDL.SDL_Event e)
        {
            // 1. Ignora eventi non desiderati o se il servizio è in pausa/disposed.
            if (_disposed || !_pauseEvent.IsSet || e.type != SDL.SDL_EventType.SDL_KEYDOWN || e.key.repeat != 0) return;

            // 2. Converte l'evento SDL in una rappresentazione .NET.
            if (!SdlToConsoleKeyMap.TryGetValue(e.key.keysym.sym, out var consoleKey)) return;
            var modifiers = GetModifiers(e.key.keysym.mod);
            var keyCombination = new KeyCombination(consoleKey, modifiers);

            // 3. Gestione prioritaria degli hook.
            if (keyCombination.Key == ConsoleKey.C && modifiers.HasFlag(ConsoleModifiers.Control))
            {
                if (_hooks.TryGetValue(keyCombination, out var userHookAction))
                {
                    userHookAction.Invoke();
                }
                else
                {
                    BreakRequested?.Invoke();
                }
                return; // Evento gestito.
            }

            if (_hooks.TryGetValue(keyCombination, out var hookAction))
            {
                hookAction.Invoke();
                return; // Hook eseguito, evento gestito.
            }

            // 4. Se nessun hook è stato attivato, processa l'evento come input normale.

            // Controlla se c'è una richiesta bloccante attiva o in coda.
            if (_activeRequest is not null || !_blockingRequestQueue.IsEmpty)
            {
                // Tenta di processare l'evento per una richiesta bloccante.
                // Il metodo interno restituirà 'true' se l'evento è stato consumato.
                if (ProcessBlockingRequests(e))
                {
                    // Se l'evento è stato usato per soddisfare una richiesta bloccante (es. WAITKEY),
                    // interrompiamo immediatamente il processamento qui. L'evento ha fatto il suo
                    // dovere e NON deve finire nel buffer non bloccante di INKEY$.
                    return;
                }
            }

            // Se non c'era una richiesta bloccante, o se l'evento non era adatto
            // (es. un tasto non stampabile durante un INPUT), l'evento è disponibile
            // per l'input non bloccante.
            ProcessNonBlockingEvent(e);
        }
        */

        /// <summary>
        /// Processa un evento SDL per l'input non bloccante (usato da INKEY$ o dalla REPL).
        /// </summary>
        /// <param name="e">L'evento SDL da processare.</param>
        private void ProcessNonBlockingEvent(SDL.SDL_Event e)
        {
            if (!SdlToConsoleKeyMap.TryGetValue(e.key.keysym.sym, out var consoleKey)) return;

            var modifiers = GetModifiers(e.key.keysym.mod);
            var keyChar = GetCharFromSdlEvent(e.key.keysym, modifiers);
            var keyInfo = new ConsoleKeyInfo(keyChar, consoleKey, modifiers.HasFlag(ConsoleModifiers.Shift),
                modifiers.HasFlag(ConsoleModifiers.Alt), modifiers.HasFlag(ConsoleModifiers.Control));

            _nonBlockingKeyBuffer.Writer.TryWrite(keyInfo);
        }

        /// <summary>
        /// Processa un evento SDL per le richieste di input bloccanti (GET, INPUT, LINE INPUT).
        /// </summary>
        /// <param name="e">L'evento SDL da processare.</param>
        private bool ProcessBlockingRequests(SDL.SDL_Event e)
        {
            if (_activeRequest is null && !_blockingRequestQueue.TryDequeue(out _activeRequest))
            {
                // Non c'era nessuna richiesta attiva, quindi l'evento non è stato consumato.
                return false;
            }

            if (_activeRequest is ReadKeyRequest rkr)
            {
                if (!SdlToConsoleKeyMap.TryGetValue(e.key.keysym.sym, out var consoleKey))
                    return false; // Evento non adatto
                var modifiers = GetModifiers(e.key.keysym.mod);
                var keyChar = GetCharFromSdlEvent(e.key.keysym, modifiers);

                var finalChar = rkr.ConvertToUppercase ? char.ToUpper(keyChar) : keyChar;
                var keyInfo = new ConsoleKeyInfo(finalChar, consoleKey, modifiers.HasFlag(ConsoleModifiers.Shift),
                    modifiers.HasFlag(ConsoleModifiers.Alt), modifiers.HasFlag(ConsoleModifiers.Control));

                _activeRequest = null;
                rkr.Tcs.TrySetResult(keyInfo);
                return true; // Evento consumato da ReadKeyRequest.
            }
            else if (_activeRequest is ReadLineRequest rlr)
            {
                var keysym = e.key.keysym;

                if (keysym.sym is SDL.SDL_Keycode.SDLK_RETURN or SDL.SDL_Keycode.SDLK_KP_ENTER)
                {
                    _activeRequest = null;
                    rlr.Console.Out.WriteLine();
                    rlr.Tcs.TrySetResult(rlr.InputBuffer.ToString());
                    return true; // Evento consumato.
                }

                if (keysym.sym == SDL.SDL_Keycode.SDLK_BACKSPACE && rlr.InputBuffer.Length > 0)
                {
                    rlr.InputBuffer.Length--;
                    rlr.Console.Out.Write("\b \b");
                    return true; // Evento consumato.
                }
                else
                {
                    var modifiers = GetModifiers(keysym.mod);
                    var keyChar = GetCharFromSdlEvent(keysym, modifiers);

                    if (keyChar != '\0' && !char.IsControl(keyChar))
                    {
                        // (La logica interna per applicare la conversione rimane la stessa)
                        // ...
                        rlr.InputBuffer.Append(keyChar);
                        rlr.Console.Out.Write(keyChar.ToString());
                        return true; // Evento consumato.
                    }
                }
            }

            // Se siamo arrivati qui, l'evento non era adatto per la richiesta di ReadLine (es. tasto di controllo)
            // o c'era un tipo di richiesta sconosciuto.
            return false;
        }

        /*
        /// <summary>
        /// Processa un evento SDL per le richieste di input bloccanti (GET, INPUT, LINE INPUT).
        /// </summary>
        /// <param name="e">L'evento SDL da processare.</param>
        /// <returns>True se l'evento è stato consumato da una richiesta bloccante, altrimenti false.</returns>
        private bool ProcessBlockingRequests(SDL.SDL_Event e)
        {
            if (_activeRequest is null && !_blockingRequestQueue.TryDequeue(out _activeRequest))
            {
                // Non c'è nessuna richiesta attiva, quindi l'evento non è stato consumato.
                return false;
            }

            if (_activeRequest is ReadKeyRequest rkr)
            {
                if (!SdlToConsoleKeyMap.TryGetValue(e.key.keysym.sym, out var consoleKey)) return false;
                var modifiers = GetModifiers(e.key.keysym.mod);
                var keyChar = GetCharFromSdlEvent(e.key.keysym, modifiers);

                var finalChar = rkr.ConvertToUppercase ? char.ToUpper(keyChar) : keyChar;
                var keyInfo = new ConsoleKeyInfo(finalChar, consoleKey, modifiers.HasFlag(ConsoleModifiers.Shift), modifiers.HasFlag(ConsoleModifiers.Alt), modifiers.HasFlag(ConsoleModifiers.Control));

                _activeRequest = null;
                rkr.Tcs.TrySetResult(keyInfo);
                return true; // Evento consumato da ReadKeyRequest.
            }

            if (_activeRequest is ReadLineRequest rlr)
            {
                var keysym = e.key.keysym;

                if (keysym.sym == SDL.SDL_Keycode.SDLK_RETURN || keysym.sym == SDL.SDL_Keycode.SDLK_KP_ENTER)
                {
                    _activeRequest = null;
                    rlr.Console.Out.WriteLine();
                    rlr.Tcs.TrySetResult(rlr.InputBuffer.ToString());
                    return true; // Evento consumato da ReadLineRequest.
                }

                if (keysym.sym == SDL.SDL_Keycode.SDLK_BACKSPACE && rlr.InputBuffer.Length > 0 && rlr.CursorPosition > 0)
                {
                    rlr.CursorPosition--;
                    rlr.InputBuffer.Remove(rlr.CursorPosition, 1);
                    // Ridisegna l'intera riga per riflettere la cancellazione.
                    var fullLine = rlr.Prompt + rlr.InputBuffer;
                    rlr.Console.SetCursorPosition(0, rlr.Console.CursorTop);
                    rlr.Console.Out.Write(new string(' ', rlr.Console.BufferWidth - 1));
                    rlr.Console.SetCursorPosition(0, rlr.Console.CursorTop);
                    rlr.Console.Out.Write(fullLine);
                    rlr.Console.SetCursorPosition(rlr.Prompt.Length + rlr.CursorPosition, rlr.Console.CursorTop);
                    return true;
                }

                var modifiers = GetModifiers(keysym.mod);
                var keyChar = GetCharFromSdlEvent(keysym, modifiers);

                if (keyChar != '\0' && !char.IsControl(keyChar))
                {
                    var charToInsert = keyChar;

                    if (rlr.CharConversionRule != null)
                    {
                        var (process, converted) = rlr.CharConversionRule(keyChar, rlr.InputBuffer.ToString());
                        if (!process) return false;
                        charToInsert = converted;
                    }

                    rlr.InputBuffer.Insert(rlr.CursorPosition, charToInsert);
                    rlr.CursorPosition++;

                    // Ridisegna la riga con il carattere inserito.
                    var fullLine = rlr.Prompt + rlr.InputBuffer;
                    rlr.Console.SetCursorPosition(0, rlr.Console.CursorTop);
                    rlr.Console.Out.Write(fullLine);
                    rlr.Console.SetCursorPosition(rlr.Prompt.Length + rlr.CursorPosition, rlr.Console.CursorTop);

                    return true; // Evento consumato.
                }

                return false; // L'evento non era un carattere valido per ReadLine.
            }

            return false;
        }
        */

        /// <summary>
        /// Resetta lo stato del servizio in modo asincrono, cancellando tutte le richieste e i buffer.
        /// </summary>
        public async Task ResetAsync()
        {
            _blockingRequestQueue.Clear();

            if (_activeRequest is ReadKeyRequest rkr) rkr.Tcs.TrySetCanceled();
            if (_activeRequest is ReadLineRequest rlr) rlr.Tcs.TrySetCanceled();
            _activeRequest = null;

            while (_nonBlockingKeyBuffer.Reader.TryRead(out _))
            {
            }

            await ClearBufferAsync();
        }

        /// <summary>
        /// Converte un evento SDL_Keysym in un carattere, tenendo conto dei modificatori di tastiera.
        /// </summary>
        /// <param name="keysym">La struttura SDL che descrive il tasto.</param>
        /// <param name="mod">I modificatori (Shift, Ctrl, Alt) attivi.</param>
        /// <returns>Il carattere corrispondente, o '\0' se non è un carattere stampabile.</returns>
        private static char GetCharFromSdlEvent(SDL.SDL_Keysym keysym, ConsoleModifiers mod)
        {
            var sym = keysym.sym;
            var shift = mod.HasFlag(ConsoleModifiers.Shift);

            if (sym is >= SDL.SDL_Keycode.SDLK_a and <= SDL.SDL_Keycode.SDLK_z)
                return shift ? char.ToUpper((char)sym) : (char)sym;

            switch (sym)
            {
                case SDL.SDL_Keycode.SDLK_0: return shift ? ')' : '0';
                case SDL.SDL_Keycode.SDLK_1: return shift ? '!' : '1';
                case SDL.SDL_Keycode.SDLK_2: return shift ? '@' : '2';
                case SDL.SDL_Keycode.SDLK_3: return shift ? '#' : '3';
                case SDL.SDL_Keycode.SDLK_4: return shift ? '$' : '4';
                case SDL.SDL_Keycode.SDLK_5: return shift ? '%' : '5';
                case SDL.SDL_Keycode.SDLK_6: return shift ? '^' : '6';
                case SDL.SDL_Keycode.SDLK_7: return shift ? '&' : '7';
                case SDL.SDL_Keycode.SDLK_8: return shift ? '*' : '8';
                case SDL.SDL_Keycode.SDLK_9: return shift ? '(' : '9';
                case SDL.SDL_Keycode.SDLK_SPACE: return ' ';
                case SDL.SDL_Keycode.SDLK_MINUS: return shift ? '_' : '-';
                case SDL.SDL_Keycode.SDLK_EQUALS: return shift ? '+' : '=';
                case SDL.SDL_Keycode.SDLK_LEFTBRACKET: return shift ? '{' : '[';
                case SDL.SDL_Keycode.SDLK_RIGHTBRACKET: return shift ? '}' : ']';
                case SDL.SDL_Keycode.SDLK_BACKSLASH: return shift ? '|' : '\\';
                case SDL.SDL_Keycode.SDLK_SEMICOLON: return shift ? ':' : ';';
                case SDL.SDL_Keycode.SDLK_QUOTE: return shift ? '"' : '\'';
                case SDL.SDL_Keycode.SDLK_COMMA: return shift ? '<' : ',';
                case SDL.SDL_Keycode.SDLK_PERIOD: return shift ? '>' : '.';
                case SDL.SDL_Keycode.SDLK_SLASH: return shift ? '?' : '/';
                case SDL.SDL_Keycode.SDLK_BACKQUOTE: return shift ? '~' : '`';
                case SDL.SDL_Keycode.SDLK_KP_0: return '0';
                case SDL.SDL_Keycode.SDLK_KP_1: return '1';
                case SDL.SDL_Keycode.SDLK_KP_2: return '2';
                case SDL.SDL_Keycode.SDLK_KP_3: return '3';
                case SDL.SDL_Keycode.SDLK_KP_4: return '4';
                case SDL.SDL_Keycode.SDLK_KP_5: return '5';
                case SDL.SDL_Keycode.SDLK_KP_6: return '6';
                case SDL.SDL_Keycode.SDLK_KP_7: return '7';
                case SDL.SDL_Keycode.SDLK_KP_8: return '8';
                case SDL.SDL_Keycode.SDLK_KP_9: return '9';
                case SDL.SDL_Keycode.SDLK_KP_PERIOD: return '.';
                case SDL.SDL_Keycode.SDLK_KP_DIVIDE: return '/';
                case SDL.SDL_Keycode.SDLK_KP_MULTIPLY: return '*';
                case SDL.SDL_Keycode.SDLK_KP_MINUS: return '-';
                case SDL.SDL_Keycode.SDLK_KP_PLUS: return '+';
                case SDL.SDL_Keycode.SDLK_RETURN: return '\r';
                case SDL.SDL_Keycode.SDLK_KP_ENTER: return '\r';
                case SDL.SDL_Keycode.SDLK_BACKSPACE: return '\b';
                case SDL.SDL_Keycode.SDLK_TAB: return '\t';
            }

            return '\0';
        }

        /// <summary>
        /// Converte il bitmask dei modificatori di SDL nell'enum ConsoleModifiers di .NET.
        /// </summary>
        /// <param name="mod">Il valore SDL_Keymod da convertire.</param>
        /// <returns>L'equivalente <see cref="ConsoleModifiers"/>.</returns>
        private static ConsoleModifiers GetModifiers(SDL.SDL_Keymod mod)
        {
            var m = default(ConsoleModifiers);
            if ((mod & (SDL.SDL_Keymod.KMOD_LSHIFT | SDL.SDL_Keymod.KMOD_RSHIFT)) != 0) m |= ConsoleModifiers.Shift;
            if ((mod & (SDL.SDL_Keymod.KMOD_LCTRL | SDL.SDL_Keymod.KMOD_RCTRL | SDL.SDL_Keymod.KMOD_GUI)) != 0)
                m |= ConsoleModifiers.Control;
            if ((mod & (SDL.SDL_Keymod.KMOD_LALT | SDL.SDL_Keymod.KMOD_RALT)) != 0) m |= ConsoleModifiers.Alt;
            return m;
        }

        /// <summary>
        /// Costruisce e restituisce un dizionario per mappare i codici dei tasti SDL all'enum ConsoleKey di .NET.
        /// </summary>
        /// <returns>Un dizionario per la mappatura dei tasti.</returns>
        private static Dictionary<SDL.SDL_Keycode, ConsoleKey> BuildKeyMap()
        {
            var map = new Dictionary<SDL.SDL_Keycode, ConsoleKey>();
            for (var i = 0; i < 26; i++) map[(SDL.SDL_Keycode)('a' + i)] = (ConsoleKey)('A' + i);
            for (var i = 0; i < 10; i++) map[(SDL.SDL_Keycode)('0' + i)] = (ConsoleKey)('0' + i);
            for (var i = 0; i < 12; i++) map[SDL.SDL_Keycode.SDLK_F1 + i] = ConsoleKey.F1 + i;
            map[SDL.SDL_Keycode.SDLK_RETURN] = ConsoleKey.Enter;
            map[SDL.SDL_Keycode.SDLK_ESCAPE] = ConsoleKey.Escape;
            map[SDL.SDL_Keycode.SDLK_BACKSPACE] = ConsoleKey.Backspace;
            map[SDL.SDL_Keycode.SDLK_TAB] = ConsoleKey.Tab;
            map[SDL.SDL_Keycode.SDLK_SPACE] = ConsoleKey.Spacebar;
            map[SDL.SDL_Keycode.SDLK_DELETE] = ConsoleKey.Delete;
            map[SDL.SDL_Keycode.SDLK_UP] = ConsoleKey.UpArrow;
            map[SDL.SDL_Keycode.SDLK_DOWN] = ConsoleKey.DownArrow;
            map[SDL.SDL_Keycode.SDLK_LEFT] = ConsoleKey.LeftArrow;
            map[SDL.SDL_Keycode.SDLK_RIGHT] = ConsoleKey.RightArrow;
            map[SDL.SDL_Keycode.SDLK_HOME] = ConsoleKey.Home;
            map[SDL.SDL_Keycode.SDLK_END] = ConsoleKey.End;
            map[SDL.SDL_Keycode.SDLK_PAGEUP] = ConsoleKey.PageUp;
            map[SDL.SDL_Keycode.SDLK_PAGEDOWN] = ConsoleKey.PageDown;
            map[SDL.SDL_Keycode.SDLK_INSERT] = ConsoleKey.Insert;
            map[SDL.SDL_Keycode.SDLK_KP_0] = ConsoleKey.NumPad0;
            map[SDL.SDL_Keycode.SDLK_KP_1] = ConsoleKey.NumPad1;
            map[SDL.SDL_Keycode.SDLK_KP_2] = ConsoleKey.NumPad2;
            map[SDL.SDL_Keycode.SDLK_KP_3] = ConsoleKey.NumPad3;
            map[SDL.SDL_Keycode.SDLK_KP_4] = ConsoleKey.NumPad4;
            map[SDL.SDL_Keycode.SDLK_KP_5] = ConsoleKey.NumPad5;
            map[SDL.SDL_Keycode.SDLK_KP_6] = ConsoleKey.NumPad6;
            map[SDL.SDL_Keycode.SDLK_KP_7] = ConsoleKey.NumPad7;
            map[SDL.SDL_Keycode.SDLK_KP_8] = ConsoleKey.NumPad8;
            map[SDL.SDL_Keycode.SDLK_KP_9] = ConsoleKey.NumPad9;
            map[SDL.SDL_Keycode.SDLK_KP_MULTIPLY] = ConsoleKey.Multiply;
            map[SDL.SDL_Keycode.SDLK_KP_PLUS] = ConsoleKey.Add;
            map[SDL.SDL_Keycode.SDLK_KP_ENTER] = ConsoleKey.Enter;
            map[SDL.SDL_Keycode.SDLK_KP_MINUS] = ConsoleKey.Subtract;
            map[SDL.SDL_Keycode.SDLK_KP_PERIOD] = ConsoleKey.Decimal;
            map[SDL.SDL_Keycode.SDLK_KP_DIVIDE] = ConsoleKey.Divide;
            map[SDL.SDL_Keycode.SDLK_MINUS] = ConsoleKey.OemMinus;
            map[SDL.SDL_Keycode.SDLK_EQUALS] = ConsoleKey.OemPlus;
            map[SDL.SDL_Keycode.SDLK_COMMA] = ConsoleKey.OemComma;
            map[SDL.SDL_Keycode.SDLK_PERIOD] = ConsoleKey.OemPeriod;
            map[SDL.SDL_Keycode.SDLK_SEMICOLON] = ConsoleKey.Oem1;
            map[SDL.SDL_Keycode.SDLK_SLASH] = ConsoleKey.Oem2;
            map[SDL.SDL_Keycode.SDLK_BACKQUOTE] = ConsoleKey.Oem3;
            map[SDL.SDL_Keycode.SDLK_LEFTBRACKET] = ConsoleKey.Oem4;
            map[SDL.SDL_Keycode.SDLK_BACKSLASH] = ConsoleKey.Oem5;
            map[SDL.SDL_Keycode.SDLK_RIGHTBRACKET] = ConsoleKey.Oem6;
            map[SDL.SDL_Keycode.SDLK_QUOTE] = ConsoleKey.Oem7;
            return map;
        }
    public TerminalKeyInfo ReadKey()
    {
        var key = ReadKeyAsync().GetAwaiter().GetResult();
        return new TerminalKeyInfo(key.KeyChar, (int)key.Key, key.Modifiers.HasFlag(ConsoleModifiers.Shift), key.Modifiers.HasFlag(ConsoleModifiers.Alt), key.Modifiers.HasFlag(ConsoleModifiers.Control));
    }
    public TerminalKeyInfo? ReadKey(int timeoutMs)
    {
        using var cts = new CancellationTokenSource(timeoutMs);
        try { return ReadKey(); } catch { return null; }
    }
    public string? ReadLine(string? prompt) => ReadLineAsync(null!, prompt ?? "", null).GetAwaiter().GetResult();
    public TerminalKeyInfo? TryReadKey() => TryReadKey(false).HasValue ? ReadKey() : null;
    public void SendKey(char key) { }
    }
}