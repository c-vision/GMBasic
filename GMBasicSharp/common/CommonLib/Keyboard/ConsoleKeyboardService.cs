using CommonLib.Screens;
using CommonLib.Consoles;
using CommonLib.Clipboard;
// FILE: Interpreter.Keyboard.KeyboardServiceConsole.cs

using System.Collections.Concurrent;
using System.Text;
using System.Threading.Channels;

// ReSharper disable MethodHasAsyncOverload

namespace CommonLib.Keyboard;


    /// <summary>
    /// Implementazione di IKeyboardService per la console di sistema standard.
    /// Utilizza un thread in background per ascoltare i tasti in modo non bloccante,
    /// consentendo il funzionamento di INKEY$ e ON KEY durante l'esecuzione del programma.
    /// </summary>
    public sealed class KeyboardServiceConsole : IKeyboardService
    {
        private CancellationTokenSource _cts = new();
        private readonly Channel<ConsoleKeyInfo> _keyBuffer = Channel.CreateUnbounded<ConsoleKeyInfo>();
        private readonly IClipboardService _clipboardService;
        private readonly ConcurrentDictionary<KeyCombination, Action> _hooks = new();
        private readonly ManualResetEventSlim _pauseEvent = new(true);
        private Task? _listenerTask;
        private volatile bool _disposed;

        /// <summary>
        /// Campo privato per tenere traccia della richiesta di input bloccante attiva.
        /// </summary>
        private IBlockingRequest? _activeRequest;

        /// <summary>
        /// Ottiene la richiesta di input bloccante attiva.
        /// </summary>
        public IBlockingRequest? ActiveBlockingRequest => _activeRequest;


        public event Action? BreakRequested;

        /// <summary>
        /// Inizializza una nuova istanza di KeyboardServiceConsole.
        /// </summary>
        /// <param name="clipboardService">Il servizio di clipboard da utilizzare per l'operazione di incolla.</param>
        public KeyboardServiceConsole(IClipboardService clipboardService)
        {
            _clipboardService = clipboardService;
            Console.CancelKeyPress += OnCancelKeyPress;
        }

        /// <summary>
        /// Avvia il task in background che ascolta la pressione dei tasti.
        /// </summary>
        public void Start()
        {
            // Ricrea il CTS se e' stato cancellato da un precedente BREAK/stop
            if (_cts.IsCancellationRequested)
            {
                var old = _cts;
                _cts = new CancellationTokenSource();
                try { old.Dispose(); } catch (ObjectDisposedException) { }
            }
            _listenerTask = Task.Run(() => ListenerLoopAsync(_cts.Token));
        }

        /// <summary>
        /// Ferma il task di ascolto in background e rilascia le risorse.
        /// </summary>
        public void Stop()
        {
            if (_disposed) return;
            _disposed = true;

            Console.CancelKeyPress -= OnCancelKeyPress;

            if (!_cts.IsCancellationRequested)
            {
                try
                {
                    _cts.Cancel();
                }
                catch (ObjectDisposedException)
                {
                    /* Ignora se già rilasciato */
                }
            }

            _pauseEvent.Set();
            _keyBuffer.Writer.TryComplete();

            try
            {
                if (_listenerTask is { IsCompleted: false })
                {
                    _listenerTask.Wait(250);
                }
            }
            catch (AggregateException ex)
            {
                // Gestisce l'eccezione attesa quando il task viene annullato.
                ex.Handle(e => e is OperationCanceledException);
            }
            catch (Exception)
            {
                // Logga qualsiasi altra eccezione imprevista durante lo spegnimento.
                //Debug.WriteLine($"[KeyboardServiceConsole] Unexpected error during Stop: {ex.Message}");
            }
            finally
            {
                // Usa un blocco catch specifico per ignorare solo l'eccezione attesa.
                try
                {
                    _cts.Dispose();
                }
                catch (ObjectDisposedException)
                {
                    /* Ignora */
                }

                try
                {
                    _pauseEvent.Dispose();
                }
                catch (ObjectDisposedException)
                {
                    /* Ignora */
                }
            }
        }

        /// <summary>
        /// Tenta di leggere un tasto dal buffer in modo istantaneo e non bloccante.
        /// </summary>
        public ConsoleKeyInfo? TryReadKey(bool convertToUppercase)
        {
            if (!_keyBuffer.Reader.TryRead(out var key)) return null;
            return ApplyConversion(key, convertToUppercase);
        }

        /// <summary>
        /// Inserisce una stringa nel buffer di input, come se fosse stata digitata dall'utente.
        /// Usato da CONSOLE.SENDKEY per simulare la pressione di tasti (test automatici).
        /// CHR(13)/CHR(10) diventano ConsoleKey.Enter, CHR(8) diventa ConsoleKey.Backspace;
        /// tutti gli altri caratteri vengono accodati come normali caratteri stampabili
        /// (il ciclo di lettura riga/tasto guarda solo KeyChar per questi, non Key).
        /// </summary>
        public void SendKeys(string keys)
        {
            foreach (var c in keys)
            {
                var key = c switch
                {
                    '\r' or '\n' => ConsoleKey.Enter,
                    '\b' => ConsoleKey.Backspace,
                    _ => default
                };
                _keyBuffer.Writer.TryWrite(new ConsoleKeyInfo(c, key, false, false, false));
            }
        }

        /// <summary>
        /// Attende in modo asincrono e restituisce il prossimo tasto.
        /// </summary>
        public async ValueTask<ConsoleKeyInfo> ReadKeyAsync(CancellationToken cancellationToken = default,
            bool convertToUppercase = false)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            using var linkedCts = CancellationTokenSource.CreateLinkedTokenSource(_cts.Token, cancellationToken);
            try
            {
                var keyRead = await _keyBuffer.Reader.ReadAsync(linkedCts.Token);
                return ApplyConversion(keyRead, convertToUppercase) ?? keyRead;
            }
            catch (ChannelClosedException ex)
            {
                throw new OperationCanceledException("KeyboardService has been stopped.", ex, linkedCts.Token);
            }
        }

        /// <summary>
        /// Gestisce l'intero processo di lettura di una riga di input dall'utente in modo asincrono.
        /// Utilizza una regola di conversione opzionale per trasformare o validare i caratteri in tempo reale.
        /// Aggiorna la console fisica dopo ogni modifica per fornire un feedback visivo immediato.
        /// </summary>
        /// <param name="console">La console attiva su cui visualizzare il prompt e l'input.</param>
        /// <param name="prompt">Il prompt da visualizzare all'utente.</param>
        /// <param name="charConversionRule">
        /// Una funzione opzionale che definisce una regola di conversione o validazione per ogni carattere digitato.
        /// </param>
        /// <param name="cancellationToken">Token per annullare l'attesa.</param>
        /// <returns>Un Task che contiene la stringa letta, o null se l'input è stato interrotto.</returns>
        public async Task<string?> ReadLineAsync(IConsole console, string prompt,
            Func<char, string, (bool process, char convertedChar)>? charConversionRule,
            CancellationToken cancellationToken = default)
        {
            var request = new ReadLineRequest(console, new StringBuilder(), prompt,
                new TaskCompletionSource<string?>(), charConversionRule);
            _activeRequest = request;

            var previousCursorState = console.CursorVisible;

            try
            {
                console.CursorVisible = true;
                console.Out.Write(prompt);

                // Forza il rendering iniziale del prompt prima di attendere i tasti.
                console.Present();

                using var linkedCts = CancellationTokenSource.CreateLinkedTokenSource(_cts.Token, cancellationToken);

                while (!linkedCts.IsCancellationRequested)
                {
                    var keyInfo = await _keyBuffer.Reader.ReadAsync(linkedCts.Token);

                    switch (keyInfo.Key)
                    {
                        case ConsoleKey.Enter:
                            console.Out.WriteLine();
                            // Non serve un Present() qui perché il ciclo principale lo farà.
                            return request.InputBuffer.ToString();

                        case ConsoleKey.Backspace:
                            if (request.CursorPosition > 0)
                            {
                                request.CursorPosition--;
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                                // Forza il rendering dopo la cancellazione.
                                console.Present();
                            }

                            break;

                        case ConsoleKey.Delete:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                                // Forza il rendering dopo la cancellazione.
                                console.Present();
                            }

                            break;

                        case ConsoleKey.LeftArrow:
                            if (request.CursorPosition > 0)
                            {
                                request.CursorPosition--;
                                console.CursorLeft = request.Prompt.Length + request.CursorPosition;
                                // Forza l'aggiornamento della posizione del cursore fisico.
                                console.Present();
                            }

                            break;

                        case ConsoleKey.RightArrow:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                request.CursorPosition++;
                                console.CursorLeft = request.Prompt.Length + request.CursorPosition;
                                // Forza l'aggiornamento della posizione del cursore fisico.
                                console.Present();
                            }

                            break;

                        case ConsoleKey.Home:
                            request.CursorPosition = 0;
                            console.CursorLeft = request.Prompt.Length;
                            // Forza l'aggiornamento della posizione del cursore fisico.
                            console.Present();
                            break;

                        case ConsoleKey.End:
                            request.CursorPosition = request.InputBuffer.Length;
                            console.CursorLeft = request.Prompt.Length + request.CursorPosition;
                            // Forza l'aggiornamento della posizione del cursore fisico.
                            console.Present();
                            break;

                        default:
                            if (!char.IsControl(keyInfo.KeyChar))
                            {
                                bool processChar;
                                char charToInsert;

                                if (charConversionRule != null)
                                {
                                    (processChar, charToInsert) = charConversionRule(keyInfo.KeyChar,
                                        request.InputBuffer.ToString());
                                }
                                else
                                {
                                    processChar = true;
                                    charToInsert = keyInfo.KeyChar;
                                }

                                if (processChar)
                                {
                                    request.InputBuffer.Insert(request.CursorPosition, charToInsert);
                                    request.CursorPosition++;
                                    RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                                    // Forza il rendering dopo l'inserimento del carattere.
                                    console.Present();
                                }
                            }

                            break;
                    }
                }

                throw new OperationCanceledException();
            }
            finally
            {
                console.CursorVisible = previousCursorState;
                _activeRequest = null;
            }
        }

        /// <summary>
        /// Gestisce la lettura di una riga di input in modo SINCRONO e BLOCCANTE,
        /// implementando la logica completa di editing della riga per la console di sistema.
        /// 
        /// IMPORTANTE: Questo metodo NON fa parte dell'interfaccia IKeyboardService.
        /// È un'implementazione specializzata per la modalità console, progettata per essere
        /// chiamata esclusivamente dal thread principale dell'applicazione tramite un cast
        /// sull'istanza di IKeyboardService.
        /// 
        /// Questa architettura permette di avere una gestione stateful e complessa dell'input
        /// (necessaria per console bufferizzate come StandardConsole) senza inquinare
        /// l'interfaccia pubblica con metodi specifici di una singola implementazione.
        /// 
        /// Dipendenze per il corretto funzionamento:
        /// - Il metodo `Pause()` e `Resume()` per sospendere il listener in background.
        /// - Il metodo helper `RenderLine()` per ridisegnare la riga di input.
        /// - Il campo `_activeRequest` per pubblicare lo stato dell'input corrente.
        /// </summary>
        public string? ReadLineBlocking(IConsole console, string prompt,
            Func<char, string, (bool process, char convertedChar)>? charConversionRule,
            CancellationToken cancellationToken)
        {
            // Mette in pausa il listener in background per ottenere l'accesso esclusivo a System.Console.
            Pause();

            // Crea la richiesta e la assegna a _activeRequest per permettere il rendering
            // da parte di console bufferizzate come StandardConsole.
            var request = new ReadLineRequest(console, new StringBuilder(), prompt,
                new TaskCompletionSource<string?>(), charConversionRule);
            _activeRequest = request;

            var previousCursorState = console.CursorVisible;

            try
            {
                console.CursorVisible = true;
                console.Out.Write(prompt);
                console.Out.Flush(); // Forza la scrittura del prompt.

                while (true)
                {
                    // Attende che un tasto sia disponibile, controllando periodicamente la cancellazione.
                    while (!Console.KeyAvailable)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        Thread.Sleep(25);
                    }

                    var keyInfo = Console.ReadKey(intercept: true);

                    switch (keyInfo.Key)
                    {
                        case ConsoleKey.Enter:
                            console.Out.WriteLine();
                            console.Out.Flush();
                            return request.InputBuffer.ToString();

                        case ConsoleKey.Backspace:
                            if (request.CursorPosition > 0)
                            {
                                request.CursorPosition--;
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        case ConsoleKey.Delete:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        case ConsoleKey.LeftArrow:
                            if (request.CursorPosition > 0)
                            {
                                request.CursorPosition--;
                                console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                    console.CursorTop);
                            }

                            break;

                        case ConsoleKey.RightArrow:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                request.CursorPosition++;
                                console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                    console.CursorTop);
                            }

                            break;

                        case ConsoleKey.Home:
                            request.CursorPosition = 0;
                            console.SetCursorPosition(request.Prompt.Length, console.CursorTop);
                            break;

                        case ConsoleKey.End:
                            request.CursorPosition = request.InputBuffer.Length;
                            console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                console.CursorTop);
                            break;

                        default:
                            if (!char.IsControl(keyInfo.KeyChar))
                            {
                                (var processChar, var charToInsert) = charConversionRule != null
                                    ? charConversionRule(keyInfo.KeyChar, request.InputBuffer.ToString())
                                    : (true, keyInfo.KeyChar);

                                if (processChar)
                                {
                                    request.InputBuffer.Insert(request.CursorPosition, charToInsert);
                                    request.CursorPosition++;
                                    RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                                }
                            }

                            break;
                    }

                    console.Out.Flush();
                }
            }
            finally
            {
                // Blocco CRITICO: assicura che lo stato venga sempre ripristinato.
                console.CursorVisible = previousCursorState;
                _activeRequest = null; // Pulisce lo stato.
                Resume(); // Riattiva il listener in background.
            }
        }

        /*
        /// <summary>
        /// Gestisce la lettura di una riga di input in modo SINCRONO e BLOCCANTE,
        /// implementando la logica completa di editing della riga, inclusa la navigazione nella cronologia dei comandi.
        /// 
        /// IMPORTANTE: Questo metodo NON fa parte dell'interfaccia IKeyboardService.
        /// È un'implementazione specializzata per la modalità console, progettata per essere
        /// chiamata esclusivamente dal thread principale dell'applicazione tramite un cast
        /// sull'istanza di IKeyboardService.
        /// 
        /// Questa architettura permette di avere una gestione stateful e complessa dell'input
        /// (necessaria per console bufferizzate come StandardConsole) senza inquinare
        /// l'interfaccia pubblica con metodi specifici di una singola implementazione.
        /// 
        /// Dipendenze per il corretto funzionamento:
        /// - Il metodo `Pause()` e `Resume()` per sospendere il listener in background.
        /// - Il metodo helper `RenderLine()` per ridisegnare la riga di input.
        /// - Il campo `_activeRequest` per pubblicare lo stato dell'input corrente.
        /// </summary>
        /// <param name="console">La console attiva su cui eseguire l'operazione di lettura.</param>
        /// <param name="prompt">Il prompt da visualizzare all'utente prima della riga di input.</param>
        /// <param name="commandHistory">Una lista in sola lettura contenente lo storico dei comandi per la navigazione.</param>
        /// <param name="charConversionRule">Una funzione opzionale per convertire o validare i caratteri durante la digitazione.</param>
        /// <param name="cancellationToken">Il token per annullare l'operazione di lettura.</param>
        /// <returns>La stringa inserita dall'utente, o `null` se l'operazione è stata annullata.</returns>
        public string? ReadLineBlocking(IConsole console, string prompt,
            IReadOnlyList<string> commandHistory,
            Func<char, string, (bool process, char convertedChar)>? charConversionRule,
            CancellationToken cancellationToken)
        {
            Pause();

            var request = new ReadLineRequest(console, new StringBuilder(), prompt,
                new TaskCompletionSource<string?>(), charConversionRule);
            _activeRequest = request;

            var historyIndex = commandHistory.Count;
            var previousCursorState = console.CursorVisible;

            try
            {
                console.CursorVisible = true;
                console.Out.Write(prompt);
                console.Out.Flush();

                while (true)
                {
                    while (!Console.KeyAvailable)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        Thread.Sleep(25);
                    }

                    var keyInfo = Console.ReadKey(intercept: true);

                    switch (keyInfo.Key)
                    {
                        case ConsoleKey.Enter:
                            console.Out.WriteLine();
                            console.Out.Flush();
                            return request.InputBuffer.ToString();

                        case ConsoleKey.Backspace:
                            if (request.CursorPosition > 0)
                            {
                                historyIndex = commandHistory.Count;
                                request.CursorPosition--;
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        case ConsoleKey.Delete:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                historyIndex = commandHistory.Count;
                                request.InputBuffer.Remove(request.CursorPosition, 1);
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        case ConsoleKey.LeftArrow:
                            if (request.CursorPosition > 0)
                            {
                                request.CursorPosition--;
                                console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                    console.CursorTop);
                            }

                            break;

                        case ConsoleKey.RightArrow:
                            if (request.CursorPosition < request.InputBuffer.Length)
                            {
                                request.CursorPosition++;
                                console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                    console.CursorTop);
                            }

                            break;

                        case ConsoleKey.Home:
                            request.CursorPosition = 0;
                            console.SetCursorPosition(request.Prompt.Length, console.CursorTop);
                            break;

                        case ConsoleKey.End:
                            request.CursorPosition = request.InputBuffer.Length;
                            console.SetCursorPosition(request.Prompt.Length + request.CursorPosition,
                                console.CursorTop);
                            break;

                        case ConsoleKey.UpArrow:
                            if (commandHistory.Count > 0)
                            {
                                historyIndex = Math.Max(0, historyIndex - 1);
                                request.InputBuffer.Clear().Append(commandHistory[historyIndex]);
                                request.CursorPosition = request.InputBuffer.Length;
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        case ConsoleKey.DownArrow:
                            if (commandHistory.Count > 0)
                            {
                                historyIndex++;
                                if (historyIndex >= commandHistory.Count)
                                {
                                    historyIndex = commandHistory.Count;
                                    request.InputBuffer.Clear();
                                }
                                else
                                {
                                    request.InputBuffer.Clear().Append(commandHistory[historyIndex]);
                                }

                                request.CursorPosition = request.InputBuffer.Length;
                                RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                            }

                            break;

                        default:
                            if (!char.IsControl(keyInfo.KeyChar))
                            {
                                (var processChar, var charToInsert) = charConversionRule != null
                                    ? charConversionRule(keyInfo.KeyChar, request.InputBuffer.ToString())
                                    : (true, keyInfo.KeyChar);

                                if (processChar)
                                {
                                    historyIndex = commandHistory.Count;
                                    request.InputBuffer.Insert(request.CursorPosition, charToInsert);
                                    request.CursorPosition++;
                                    RenderLine(console, request.Prompt, request.InputBuffer, request.CursorPosition);
                                }
                            }

                            break;
                    }

                    console.Out.Flush();
                }
            }
            finally
            {
                console.CursorVisible = previousCursorState;
                _activeRequest = null;
                Resume();
            }
        }
        */

        public void RegisterHook(KeyCombination keyCombination, Action action) => _hooks[keyCombination] = action;


        public void UnregisterHook(KeyCombination keyCombination) => _hooks.TryRemove(keyCombination, out _);


        public void UnregisterAllHooks() => _hooks.Clear();


        public bool IsHookRegistered(KeyCombination keyCombination) => _hooks.ContainsKey(keyCombination);


        public ValueTask ClearBufferAsync()
        {
            while (_keyBuffer.Reader.TryRead(out _))
            {
            }

            return ValueTask.CompletedTask;
        }


        public void Pause() => _pauseEvent.Reset();


        public void Resume() => _pauseEvent.Set();


        public Task ResetAsync()
        {
            // Svuota il buffer dei tasti residui (es. input accidentale prima del RUN)
            while (_keyBuffer.Reader.TryRead(out _)) { }
            return Task.CompletedTask;
        }

        /// <summary>
        /// Cancella le letture in sospeso (usato per CTRL-C/BREAK).
        /// </summary>
        public void CancelPendingReads()
        {
            if (!_cts.IsCancellationRequested)
            {
                try { _cts.Cancel(); } catch (ObjectDisposedException) { }
            }
            while (_keyBuffer.Reader.TryRead(out _)) { }
        }


        public void Dispose() => Stop();

        /// <summary>
        /// Loop in background che ascolta i tasti, gestisce l'incolla e gli hook.
        /// </summary>
        private async Task ListenerLoopAsync(CancellationToken token)
        {
            try
            {
                while (!token.IsCancellationRequested)
                {
                    _pauseEvent.Wait(token);

                    if (Console.KeyAvailable)
                    {
                        var keyInfo = Console.ReadKey(intercept: true);

                        // Gestione Incolla
                        if (keyInfo is
                            {
                                Modifiers: (ConsoleModifiers.Control | ConsoleModifiers.Shift), Key: ConsoleKey.V
                            })
                        {
                            // La chiamata ora usa l'istanza del servizio iniettato invece di una classe statica.
                            var pastedText = _clipboardService.GetText();
                            if (!string.IsNullOrEmpty(pastedText))
                            {
                                foreach (var c in pastedText)
                                {
                                    await _keyBuffer.Writer.WriteAsync(new ConsoleKeyInfo(c, 0, false, false, false),
                                        token);
                                }
                            }

                            continue;
                        }

                        // Gestione Hook (ON KEY)
                        var combination = new KeyCombination(keyInfo.Key, keyInfo.Modifiers);
                        if (_hooks.TryGetValue(combination, out var action))
                        {
                            action.Invoke();
                        }
                        else
                        {
                            await _keyBuffer.Writer.WriteAsync(keyInfo, token);
                        }
                    }
                    else
                    {
                        await Task.Delay(25, token);
                    }
                }
            }
            catch (OperationCanceledException)
            {
                /* Comportamento atteso durante lo spegnimento. */
            }
            catch (InvalidOperationException)
            {
                // Succede se la console non è disponibile (es. in un ambiente non interattivo).
                // Logghiamo l'errore per diagnostica, ma il loop deve terminare.
                //Debug.WriteLine($"[KeyboardServiceConsole] Console not available, listener stopping: {ex.Message}");
            }
            catch (Exception)
            {
                // Logga qualsiasi altra eccezione imprevista che potrebbe interrompere il listener.
                //Debug.WriteLine($"[KeyboardServiceConsole] Unexpected error in listener loop: {ex.Message}");
            }
            // NON completare _keyBuffer.Writer qui: il canale è condiviso e sopravvive al
            // singolo listener. CancelPendingReads()+Start() (break Ctrl+C) fermano e
            // riavviano questo loop nella stessa sessione — completare il writer qui lo
            // chiuderebbe per sempre, rendendo la tastiera sorda dopo il primo break.
            // La chiusura definitiva del canale spetta solo a Stop() (Dispose reale).
        }

        /// <summary>
        /// Metodo helper per ridisegnare la riga di input durante l'editing in modo ottimizzato.
        /// </summary>
        private static void RenderLine(IConsole console, string prompt, StringBuilder buffer, int cursorPosition)
        {
            // Salva la posizione verticale del cursore, perché le operazioni di scrittura potrebbero alterarla.
            var originalTop = console.CursorTop;

            // Sposta il cursore all'inizio della riga di input (dopo il prompt).
            console.SetCursorPosition(prompt.Length, originalTop);

            // Scrive il contenuto corrente del buffer. Questo sovrascrive il vecchio testo.
            console.Out.Write(buffer.ToString());

            // Scrive uno spazio extra per cancellare l'ultimo carattere nel caso in cui
            // la nuova riga sia più corta della precedente (es. dopo un backspace).
            console.Out.Write(' ');

            // Flush per renderizzare il buffer sullo schermo fisico.
            console.Out.Flush();

            // Riposiziona il cursore al punto di editing corretto.
            console.SetCursorPosition(prompt.Length + cursorPosition, originalTop);
        }

        /// <summary>
        /// Converte un tasto in maiuscolo, se richiesto.
        /// </summary>
        private static ConsoleKeyInfo? ApplyConversion(ConsoleKeyInfo? keyInfo, bool convertToUppercase)
        {
            if (!keyInfo.HasValue || !convertToUppercase) return keyInfo;
            var original = keyInfo.Value;
            var upperChar = char.ToUpper(original.KeyChar);
            return upperChar == original.KeyChar
                ? original
                : new ConsoleKeyInfo(upperChar, original.Key, original.Modifiers.HasFlag(ConsoleModifiers.Shift),
                    original.Modifiers.HasFlag(ConsoleModifiers.Alt),
                    original.Modifiers.HasFlag(ConsoleModifiers.Control));
        }

        /// <summary>
        /// Gestore per l'evento di sistema Ctrl+C.
        /// </summary>
        private void OnCancelKeyPress(object? sender, ConsoleCancelEventArgs e)
        {
            e.Cancel = true;

            var ctrlCCombination = new KeyCombination(ConsoleKey.C, ConsoleModifiers.Control);

            if (_hooks.TryGetValue(ctrlCCombination, out var hookAction))
            {
                hookAction.Invoke();
            }
            else
            {
                BreakRequested?.Invoke();
            }
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
    public void SendKey(char key) => _keyBuffer.Writer.TryWrite(new ConsoleKeyInfo(key, (ConsoleKey)key, false, false, false));

    }
