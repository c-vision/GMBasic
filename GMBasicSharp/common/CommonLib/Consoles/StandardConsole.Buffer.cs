using CommonLib.Screens;
using CommonLib.Consoles.Tui;
// FILE: Interpreter.Consoles.StandardConsole.Buffer.cs

using System.Text;

namespace CommonLib.Consoles;

/// <summary>
/// Contiene la logica del buffer di schermo, del rendering e del clipboard
/// per la classe parziale StandardConsole.
/// </summary>
public sealed partial class StandardConsole
{
    /// <summary>
    /// Implementazione interna di <see cref="TextWriter"/> che reindirizza le scritture
    /// al buffer di schermo interno e gestisce il rendering tramite un meccanismo di flush.
    /// </summary>
    private sealed class StandardConsoleWriter : TextWriter
    {
        /// <summary>
        /// Riferimento all'istanza della console proprietaria.
        /// </summary>
        private readonly StandardConsole _owner;

        /// <summary>
        /// Ottiene l'encoding dell'output.
        /// </summary>
        public override Encoding Encoding => Console.OutputEncoding;

        /// <summary>
        /// Inizializza una nuova istanza della classe <see cref="StandardConsoleWriter"/>.
        /// </summary>
        /// <param name="owner">L'istanza di <see cref="StandardConsole"/> a cui questo writer deve inviare l'output.</param>
        public StandardConsoleWriter(StandardConsole owner) { _owner = owner; }

        /// <summary>
        /// Scrive un singolo carattere nel buffer di memoria. Non innesca il rendering.
        /// </summary>
        /// <param name="value">Il carattere da scrivere.</param>
        public override void Write(char value)
        {
            _owner.WriteInternal(value);
        }
        
        /// <summary>
        /// Scrive una stringa di testo nel buffer di memoria, carattere per carattere.
        /// </summary>
        /// <param name="value">La stringa da scrivere.</param>
        public override void Write(string? value)
        {
            if (value != null)
            {
                foreach(var c in value)
                {
                    Write(c);
                }
            }
        }

        /// <summary>
        /// Scrive una stringa seguita da un terminatore di riga e, se AutoFlush è attivo,
        /// esegue il flush dell'output.
        /// </summary>
        /// <param name="value">La stringa da scrivere.</param>
        public override void WriteLine(string? value)
        {
            Write(value);
            WriteLine(); // Delega alla versione senza argomenti per il terminatore e il flush.
        }

        /// <summary>
        /// Scrive un terminatore di riga e, se AutoFlush è attivo, esegue il flush dell'output.
        /// </summary>
        public override void WriteLine()
        {
            Write(_owner.NewLine);
            if (_owner.AutoFlush)
            {
                Flush();
            }
        }

        /// <summary>
        /// Forza la scrittura di tutto l'output bufferizzato sulla console fisica.
        /// Questa operazione chiama il metodo di rendering del proprietario.
        /// </summary>
        public override void Flush()
        {
            _owner.RenderBuffer();
        }

        /// <summary>
        /// Forza in modo asincrono la scrittura di tutto l'output bufferizzato.
        /// </summary>
        /// <returns>Un task che rappresenta l'operazione di flush completata.</returns>
        public override Task FlushAsync()
        {
            _owner.RenderBuffer();
            return Task.CompletedTask;
        }
    }

    /// <summary>
    /// Alloca la memoria per tutti i buffer di schermo in base alle dimensioni della console.
    /// </summary>
    private void InitializeBuffers()
    {
        _buffer = new ScreenCell[_height, _width];
        _previousBuffer = new ScreenCell[_height, _width];
        _screen = new ScreenBuffer(_width, _height);
        //Debug.WriteLine($"[StandardConsole.Buffer] Unified buffers initialized to {_width}x{_height}.");
    }
    
    /// <summary>
    /// Il cuore della logica di scrittura. Processa un singolo carattere,
    /// aggiornando il buffer interno e gestendo i caratteri di controllo e
    /// il line wrapping in modo atomico e corretto.
    /// </summary>
    /// <param name="value">Il carattere da processare.</param>
    private void WriteInternal(char value)
    {
        if (_nativeScrollMode)
        {
            WriteInternalNative(value);
            return;
        }

        IsDirty = true;

        switch (value)
        {
            case '\n': // Newline: sposta il cursore all'inizio della riga successiva.
                CursorLeft = 0;
                CursorTop++;
                break;
            case '\r': // Carriage Return: sposta il cursore all'inizio della riga corrente.
                CursorLeft = 0;
                break;
            case '\b': // Backspace: sposta il cursore indietro di una posizione, se possibile.
                if (CursorLeft > 0) CursorLeft--;
                break;
            default: // Carattere stampabile
                // Se il cursore è già alla fine della riga o oltre,
                // vai a capo PRIMA di scrivere il nuovo carattere.
                if (CursorLeft >= _width)
                {
                    CursorLeft = 0;
                    CursorTop++;
                }

                // Controlla se il cursore è ancora fuori dai limiti verticali dopo un possibile a capo.
                if (CursorTop >= _height)
                {
                    ScrollBufferUp();
                    CursorTop = _height - 1;
                }
                
                // Ora scrivi il carattere nella posizione corretta e sicura.
                _buffer[CursorTop, CursorLeft] = new ScreenCell(value, ForegroundColor, BackgroundColor);

                // E solo dopo aver scritto, avanza il cursore per il carattere successivo.
                CursorLeft++;
                break;
        }

        // Dopo ogni carattere (inclusi \n, \r, \b), controlla se il cursore
        // è finito su una riga non valida (es. dopo un \n all'ultima riga).
        // Questo blocco gestisce lo scrolling.
        if (CursorTop >= _height)
        {
            ScrollBufferUp();
            CursorTop = _height - 1;
        }
    }

    /// <summary>
    /// Percorso di scrittura per la modalità "nativa" (CONSOLE.SCROLLMODE("NATIVE")):
    /// scrive direttamente su System.Console, bypassando il buffer a griglia, così che sia
    /// il terminale stesso a gestire avanzamento riga e scrolling — con il relativo
    /// scrollback reale, a differenza della modalità a buffer che perde per sempre le righe
    /// che escono dal buffer fisso.
    /// </summary>
    /// <param name="value">Il carattere da scrivere.</param>
    private void WriteInternalNative(char value)
    {
        try
        {
            WriteAnsiFg(ForegroundColor);
            WriteAnsiBg(BackgroundColor);
            _renderOut.Write(value);
            CursorLeft = Console.CursorLeft;
            CursorTop = Console.CursorTop;
        }
        catch (IOException)
        {
            // Ignora se lo stream è stato chiuso/rediretto in modo non interattivo.
        }
    }

    /// <summary>
    /// Ottiene la stringa del terminatore di riga per questa console.
    /// </summary>
    private string NewLine => Environment.NewLine;
    
    /// <summary>
    /// Esegue lo scrolling di tutto il contenuto del buffer di una riga verso l'alto.
    /// </summary>
    private void ScrollBufferUp()
    {
        var top = _scrollBottom >= 0 ? _scrollTop : 0;
        var bottom = _scrollBottom >= 0 ? _scrollBottom : _height - 1;

        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);

        for (var r = top; r < bottom; r++)
        for (var c = 0; c < _width; c++)
        {
            _buffer[r, c] = _buffer[r + 1, c];
        }

        for (var c = 0; c < _width; c++)
        {
            _buffer[bottom, c] = emptyCell;
        }
    }

    /// <summary>
    /// Renderizza il buffer interno sulla console fisica. Gestisce proattivamente il
    /// ridimensionamento tramite una sequenza di sincronizzazione autoritaria:
    /// 1. Ricalibra il buffer logico.
    /// 2. Imposta il colore di sfondo corretto.
    /// 3. Pulisce l'intera console fisica.
    /// 4. Forza un ridisegno completo del buffer logico invalidando quello precedente.
    /// </summary>
    private void RenderBuffer()
    {
        if (_nativeScrollMode)
        {
            // In modalità nativa le scritture sono già andate dirette al terminale
            // (WriteInternalNative): non c'è alcun buffer da ridisegnare, serve solo
            // sincronizzare la visibilità del cursore.
            try { Console.CursorVisible = CursorVisible; } catch (PlatformNotSupportedException) { }
            IsDirty = false;
            return;
        }

        try
        {
            var physicalWidth = Console.WindowWidth;
            var physicalHeight = Console.WindowHeight;

            // Vedi il commento nel costruttore (StandardConsole.cs) e in PollForResize: senza
            // un vero terminale (output rediretto, sandbox, processo in background),
            // Console.WindowWidth/Height restituiscono 0 su macOS/Linux. Senza questo guard,
            // QUALUNQUE Flush()/RenderBuffer() (chiamato ad ogni PRINT con AutoFlush attivo)
            // rilevava "0 != dimensioni logiche correnti" come un resize genuino e ricalibrava
            // immediatamente il buffer a 0x0 — vanificando il fallback del costruttore ancora
            // prima che il programma stampasse il primo carattere (bastava anche solo la
            // Clear() del costruttore stesso), riportando il crash IndexOutOfRangeException.
            if (physicalWidth <= 0 || physicalHeight <= 0) return;

            if (physicalWidth != _width || physicalHeight != _height)
            {
                // 1. Ricalibra il nostro buffer logico. Il suo contenuto ora è la verità.
                Recalibrate(physicalWidth, physicalHeight);

                // 2. Imposta il colore di sfondo che useremo per la pulizia.
                Console.BackgroundColor = MapToConsoleColor(BackgroundColor);

                // 3. Pulisce fisicamente la console, eliminando tutti gli artefatti.
                Console.Clear();

                // 4. Invalida il buffer 'previous' per forzare un ridisegno completo.
                // Poiché nessuna cella del buffer precedente corrisponderà a quella del
                // buffer corrente, ogni cella verrà ridisegnata sulla console pulita.
                _previousBuffer = new ScreenCell[_height, _width];
            }

            if (!IsDirty) return;

            try { Console.CursorVisible = false; } catch (PlatformNotSupportedException) { }
            var lastFg = -1;
            var lastBg = -1;

            var rowsToRender = Math.Min(_height, physicalHeight);
            var colsToRender = Math.Min(_width, physicalWidth);

            for (var r = 0; r < rowsToRender; r++)
            {
                for (var c = 0; c < colsToRender; c++)
                {
                    if (_buffer[r, c] != _previousBuffer[r, c])
                    {
                        Console.SetCursorPosition(c, r);
                        
                        var currentCell = _buffer[r, c];
                        var (fg, bg) = currentCell.Attributes.HasFlag(ScreenCellAttributes.Inverse)
                            ? (currentCell.BackgroundColor, currentCell.ForegroundColor)
                            : (currentCell.ForegroundColor, currentCell.BackgroundColor);
                        
                        if (fg != lastFg)
                        {
                            WriteAnsiFg(fg);
                            lastFg = fg;
                        }
                        if (bg != lastBg)
                        {
                            WriteAnsiBg(bg);
                            lastBg = bg;
                        }

                        _renderOut.Write(currentCell.Character);
                        _previousBuffer[r, c] = currentCell;
                    }
                }
            }

            var clampedLeft = Math.Clamp(CursorLeft, 0, physicalWidth - 1);
            var clampedTop = Math.Clamp(CursorTop, 0, physicalHeight - 1);
            Console.SetCursorPosition(clampedLeft, clampedTop);

            try { Console.CursorVisible = CursorVisible; } catch (PlatformNotSupportedException) { }
            
            // Sincronizza il buffer TUI (ScreenBuffer) con il buffer interno
            SyncToScreen();
        }
        catch (Exception)
        {
            //Debug.WriteLine($"[StandardConsole.Render] Error: {e.Message}");
        }
        finally
        {
            IsDirty = false;
        }
    }
    
    /// <summary>
    /// Copia il contenuto testuale del buffer negli appunti di sistema.
    /// </summary>
    public void CopyBufferToClipboard()
    {
        var sb = new StringBuilder();
        for (var r = 0; r < _height; r++)
        {
            var rowText = new StringBuilder();
            for (var c = 0; c < _width; c++)
            {
                rowText.Append(_buffer[r, c].Character);
            }
            sb.AppendLine(rowText.ToString().TrimEnd());
        }
        _clipboardService.SetText(sb.ToString().TrimEnd());
    }

    /// <summary>
    /// L'operazione di incolla è gestita dal KeyboardService.
    /// </summary>
    public void PasteFromClipboard()
    {
        // Metodo vuoto come da design.
    }
    
        #region Implementazione Metodi di Manipolazione del Buffer

    /// <summary>
    /// Legge un blocco rettangolare di celle dal buffer di schermo logico.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da leggere.</param>
    /// <returns>Una matrice di ScreenCell con i dati dell'area specificata.</returns>
    public ScreenCell[,] Get(ScreenRect rect)
    {
        var result = new ScreenCell[rect.h, rect.w];
        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);

        for (var r = 0; r < rect.h; r++)
        {
            for (var c = 0; c < rect.w; c++)
            {
                var sourceX = rect.x + c;
                var sourceY = rect.y + r;

                // Se le coordinate sorgente sono valide, copia la cella,
                // altrimenti inserisci una cella vuota.
                if (sourceX >= 0 && sourceX < _width && sourceY >= 0 && sourceY < _height)
                {
                    result[r, c] = _buffer[sourceY, sourceX];
                }
                else
                {
                    result[r, c] = emptyCell;
                }
            }
        }
        return result;
    }

    /// <summary>
    /// Scrive un blocco di celle nel buffer, eseguendo il clipping per evitare errori.
    /// </summary>
    /// <param name="x">La colonna di destinazione dove iniziare a scrivere.</param>
    /// <param name="y">La riga di destinazione dove iniziare a scrivere.</param>
    /// <param name="cells">La matrice di ScreenCell da scrivere.</param>
    public void Put(int x, int y, ScreenCell[,] cells)
    {
        IsDirty = true;
        var sourceRows = cells.GetLength(0);
        var sourceCols = cells.GetLength(1);

        for (var r = 0; r < sourceRows; r++)
        {
            for (var c = 0; c < sourceCols; c++)
            {
                var destX = x + c;
                var destY = y + r;

                // Esegui la scrittura solo se la destinazione è all'interno dei limiti del buffer.
                if (destX >= 0 && destX < _width && destY >= 0 && destY < _height)
                {
                    _buffer[destY, destX] = cells[r, c];
                }
            }
        }
    }

    /// <summary>
    /// Pulisce un'area rettangolare del buffer, riempiendola con una cella specificata.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da pulire.</param>
    /// <param name="fillCell">La cella da usare per il riempimento, o null per quella di default.</param>
    public void ClearRect(ScreenRect rect, ScreenCell? fillCell = null)
    {
        IsDirty = true;
        // Se non viene fornita una cella, crea quella di default.
        var cellToFill = fillCell ?? new ScreenCell(' ', ForegroundColor, BackgroundColor);

        // Calcola l'intersezione tra il rettangolo richiesto e i limiti del buffer
        // per agire solo sull'area valida (clipping).
        var startX = Math.Max(0, rect.x);
        var startY = Math.Max(0, rect.y);
        var endX = Math.Min(_width, rect.x + rect.w);
        var endY = Math.Min(_height, rect.y + rect.h);

        for (var r = startY; r < endY; r++)
        {
            for (var c = startX; c < endX; c++)
            {
                _buffer[r, c] = cellToFill;
            }
        }
    }

    #endregion

    /// <summary>
    /// Sincronizza il buffer TUI condiviso (<see cref="ScreenBuffer"/>) con lo stato
    /// corrente del buffer interno, permettendo ai renderer esterni (es. SdlRenderer)
    /// di accedere alle stesse celle.
    /// </summary>
    private void SyncToScreen()
    {
        for (var r = 0; r < _height; r++)
            for (var c = 0; c < _width; c++)
                _screen.Cells[r, c] = _buffer[r, c];
    }
}