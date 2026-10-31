using Basic;
using CommonLib.Screens;
// FILE: Interpreter.Graphics.Commands.cs
// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

public sealed partial class GraphicsManager
{
    #region Public Graphics Commands
        
    /// <summary>
    /// Imposta la modalità dello schermo. Questo metodo ora è asincrono ("fire-and-forget").
    /// Accoda l'operazione e ritorna immediatamente.
    /// </summary>
    /// <param name="m">La modalità dello schermo. 0 per disattivare. -1 per usare una config custom.</param>
    /// <param name="ap">La pagina attiva (predefinita a 0).</param>
    /// <param name="vp">La pagina visibile (predefinita a 0).</param>
    /// <param name="config">Una configurazione personalizzata da usare se la modalità è -1.</param>
    /// <param name="windowMode">Modalità finestra (Windowed, Fullscreen, FullscreenDesktop, Borderless).</param>
    public Task SCREEN(int m, int ap = 0, int vp = 0, ScreenConfig? config = null, ScreenWindowMode windowMode = ScreenWindowMode.Windowed) => 
        InvokeOnMainThreadAsync(() => ScreenInternal(m, ap, vp, config, windowMode: windowMode));
        
    /// <summary>
    /// Cancella l'intera area dello schermo attivo con un colore specificato.
    /// </summary>
    /// <param name="c">
    /// L'indice del colore di sfondo da utilizzare. Se <c>null</c>, usa il colore di sfondo corrente.
    /// </param>
    public Task CLS(int? c = null) => InvokeOnMainThreadAsync(() => ClsInternal(c));
        
    /// <summary>
    /// Imposta i colori di primo piano e/o sfondo per le operazioni grafiche successive.
    /// Se un parametro è nullo, il colore corrispondente non viene modificato.
    /// Se entrambi sono nulli, potrebbe ripristinare i colori di default.
    /// </summary>
    /// <summary>
    /// Imposta i colori di primo piano e/o sfondo per le operazioni grafiche successive.
    /// Questo comando ha effetto solo se una modalità grafica è attiva.
    /// </summary>
    public Task COLOR(int? f = null, int? b = null) => InvokeOnMainThreadAsync(() => ColorInternal(f, b));

    /// <summary>
    /// Disegna una linea, un rettangolo vuoto o un rettangolo pieno.
    /// </summary>
    /// <param name="x1">Coordinata x di partenza.</param>
    /// <param name="y1">Coordinata y di partenza.</param>
    /// <param name="x2">Coordinata x di arrivo.</param>
    /// <param name="y2">Coordinata y di arrivo.</param>
    /// <param name="c">L'indice del colore. Se <c>null</c>, usa il colore di primo piano corrente.</param>
    /// <param name="s">Lo stile del disegno. "B" per un rettangolo vuoto, "BF" per un rettangolo pieno.
    /// Se <c>null</c>, disegna una linea.</param>
    public Task LINE(int x1, int y1, int x2, int y2, int? c = null, string? s = null) => 
        InvokeOnMainThreadAsync(() => LineInternal(x1, y1, x2, y2, c, s));
        
    /// <summary>
    /// Imposta un singolo punto a schermo.
    /// </summary>
    /// <param name="x">Coordinata x del punto.</param>
    /// <param name="y">Coordinata y del punto.</param>
    /// <param name="c">L'indice del colore. Se <c>null</c>, usa il colore di primo piano corrente.</param>
    public Task PSET(int x, int y, int? c = null) => InvokeOnMainThreadAsync(() => PsetInternal(x, y, c));
        
    /// <summary>
    /// Imposta un singolo punto a schermo utilizzando il colore di sfondo corrente.
    /// </summary>
    /// <param name="x">Coordinata x del punto.</param>
    /// <param name="y">Coordinata y del punto.</param>
    public Task PRESET(int x, int y) => PSET(x, y, _backgroundColorIndex);
        
    /// <summary>
    /// Disegna un cerchio o un arco.
    /// </summary>
    /// <param name="x">Coordinata x del centro.</param>
    /// <param name="y">Coordinata y del centro.</param>
    /// <param name="r">Il raggio del cerchio.</param>
    /// <param name="c">L'indice del colore. Se <c>null</c>, usa il colore di primo piano corrente.</param>
    /// <param name="sa">L'angolo di partenza in radianti (opzionale). Predefinito a 0.</param>
    /// <param name="ea">L'angolo di fine in radianti (opzionale). Predefinito a 2 * pi.</param>
    /// <param name="a">Il fattore di aspetto del cerchio (opzionale). Predefinito a 1.0.</param>
    public Task CIRCLE(int x, int y, int r, int? c = null, double? sa = null, double? ea = null, double? a = null) => 
        InvokeOnMainThreadAsync(() => CircleInternal(x, y, r, c, sa, ea, a));
        
    /// <summary>
    /// Riempie un'area chiusa a partire da un punto di partenza.
    /// </summary>
    /// <param name="x">Coordinata x di partenza.</param>
    /// <param name="y">Coordinata y di partenza.</param>
    /// <param name="fc">L'indice del colore di riempimento. Se <c>null</c>, usa il colore di primo piano corrente.</param>
    /// <param name="bc">L'indice del colore del bordo. Se <c>null</c>, viene ignorato.</param>
    public Task PAINT(int x, int y, int? fc = null, int? bc = null) => InvokeOnMainThreadAsync(() => PaintInternal(x, y, fc, bc));

    /// <summary>
    /// Imposta il font corrente per le operazioni di stampa successive.
    /// Questo comando ha effetto solo se una modalità grafica è attiva.
    /// Accoda su `_commandQueue` (processata dal main loop Present/PollEvents): il chiamante
    /// può attendere il Task con GetResult() senza deadlock, come LOAD_TEXTURE. Il vecchio
    /// InvokeOnMainThreadAsync usava `_mainThreadActionQueue`, MAI processata dai runner →
    /// deadlock se atteso; usato async-gpm da FONT_GPM.
    /// </summary>
    public Task FONT(string p, int s)
    {
        var tcs = new TaskCompletionSource();
        _commandQueue.Add(() =>
        {
            try { FontInternal(p, s); tcs.TrySetResult(); }
            catch (Exception ex) { tcs.TrySetException(ex); }
        });
        return tcs.Task;
    }

    /// <summary>
    /// Stampa una stringa di testo a schermo nella posizione corrente del cursore.
    /// </summary>
    /// <param name="t">Il testo da stampare.</param>
    public Task PRINT(string t) => InvokeOnMainThreadAsync(() => PrintInternal(t));

    /// <param name="x">La coordinata X in pixel.</param>
    /// <param name="y">La coordinata Y in pixel.</param>
    public Task LOCATE(int x, int y) => InvokeOnMainThreadAsync(() => LocateInternal(x, y));
    
    /// <summary>
    /// Mette un'immagine a schermo nella pagina attiva, supportando diverse modalità di disegno.
    /// </summary>
    /// <param name="x">Coordinata X dell'angolo in alto a sinistra dove iniziare a disegnare l'immagine.</param>
    /// <param name="y">Coordinata Y dell'angolo in alto a sinistra dove iniziare a disegnare l'immagine.</param>
    /// <param name="i">
    /// L'array di interi che rappresenta l'immagine. La struttura dell'array è:
    /// - i(0): Larghezza dell'immagine in pixel.
    /// - i(1): Altezza dell'immagine in pixel.
    /// - i(2) a i(n): Indici di colore dei pixel, letti da sinistra a destra, dall'alto in basso.
    /// Un indice di colore negativo viene trattato come trasparente e il pixel di destinazione non viene modificato.
    /// </param>
    /// <param name="m">
    /// La modalità di disegno (stringa, case-insensitive). Valori supportati:
    /// - "PSET" (Default): Disegna l'immagine sovrascrivendo i pixel di destinazione. È la modalità più veloce.
    /// - "AND": Esegue un'operazione bitwise AND tra l'indice di colore del pixel sorgente e quello di destinazione. Utile per creare maschere.
    /// - "OR": Esegue un'operazione bitwise OR tra l'indice di colore del pixel sorgente e quello di destinazione. Utile per combinare immagini.
    /// - "XOR" (o "NOT"): Esegue un'operazione bitwise XOR tra l'indice di colore del pixel sorgente e quello di destinazione. Utile per "invertire" aree dello schermo o per animazioni semplici, poiché una seconda applicazione ripristina lo stato originale.
    /// </param>
    /// <example>
    /// Esempio di utilizzo in BASIC:
    /// <code>
    /// ' 1. Prepara un array per contenere un'immagine 2x2
    /// DIM MySprite(3)
    /// 
    /// ' 2. Definisci le dimensioni
    /// MySprite(0) = 2  ' Larghezza
    /// MySprite(1) = 2  ' Altezza
    /// 
    /// ' 3. Definisci i pixel (colore 4=Rosso, 14=Giallo, -1=Trasparente)
    /// MySprite(2) = 4  ' Pixel (0,0)
    /// MySprite(3) = 14 ' Pixel (1,0)
    /// MySprite(4) = -1 ' Pixel (0,1) -> Trasparente
    /// MySprite(5) = 4  ' Pixel (1,1)
    /// 
    /// ' --- Utilizzo senza operatori ---
    /// ' Disegna lo sprite a (10, 10) sovrascrivendo ciò che c'è sotto
    /// IMAGEPUT 10, 10, MySprite()
    /// 
    /// ' --- Utilizzo con operatori logici ---
    /// ' Disegna prima un rettangolo blu (colore 1)
    /// LINE (50, 50)-(60, 60), 1, BF
    /// 
    /// ' Applica lo sprite con AND. Dove lo sprite è rosso (4, binario 100)
    /// ' e lo sfondo è blu (1, binario 001), il risultato sarà nero (0, binario 000).
    /// IMAGEPUT 50, 50, MySprite(), "AND"
    /// 
    /// ' Applica di nuovo lo sprite con XOR per un effetto di inversione
    /// IMAGEPUT 30, 30, MySprite(), "XOR"
    /// ' Se si esegue di nuovo lo stesso comando, l'immagine scompare ripristinando lo sfondo.
    /// IMAGEPUT 30, 30, MySprite(), "XOR"
    /// </code>
    /// </example>
    public Task IMAGEPUT(int x, int y, int[] i, string m = "PSET") => InvokeOnMainThreadAsync(() => ImagePutInternal(x, y, i, m));
        
    /// <summary>
    /// Disegna in blocco (batch) più istanze dello stesso sprite in posizioni diverse.
    /// Questo metodo è significativamente più veloce rispetto a chiamate multiple a IMAGEPUT,
    /// poiché il ciclo di disegno avviene interamente in codice C# compilato, riducendo
    /// l'overhead dell'interprete a una singola chiamata.
    /// </summary>
    /// <param name="spriteData">L'array di interi che rappresenta lo sprite da disegnare (stesso formato di IMAGEPUT).</param>
    /// <param name="positions">
    /// Un array di interi contenente le coordinate dove disegnare lo sprite.
    /// La struttura è [X1, Y1, X2, Y2, X3, Y3, ...]. La lunghezza dell'array deve essere un multiplo di 2.
    /// </param>
    /// <param name="mode">La modalità di disegno (es. "PSET", "XOR"), applicata a tutte le istanze.</param>
    /// <example>
    /// Esempio di utilizzo in BASIC:
    /// <code>
    /// ' Definisci uno sprite 2x2
    /// DIM MySprite(5)
    /// MySprite(0) = 2 : MySprite(1) = 2
    /// MySprite(2) = 4 : MySprite(3) = 14
    /// MySprite(4) = 14 : MySprite(5) = 4
    /// 
    /// ' Definisci un array per le posizioni di 3 sprite
    /// DIM Locations(5) ' Indici da 0 a 5 -> 3 coppie (X,Y)
    /// Locations(0) = 10 : Locations(1) = 20  ' Posizione 1
    /// Locations(2) = 50 : Locations(3) = 60  ' Posizione 2
    /// Locations(4) = 100 : Locations(5) = 120 ' Posizione 3
    /// 
    /// ' Disegna i 3 sprite in un'unica, velocissima operazione
    /// IMAGEPUTS MySprite(), Locations(), "PSET"
    /// </code>
    /// </example>
    public Task IMAGEPUTS(int[] spriteData, int[] positions, string mode = "PSET") => 
        InvokeOnMainThreadAsync(() => ImagePutsInternal(spriteData, positions, mode));
        
    /// <summary>
    /// Disegna usando una stringa di comandi compatibile con il comando DRAW di BASIC.
    /// </summary>
    /// <param name="cs">La stringa di comandi DRAW, come "C12 U5 R10".</param>
    public Task DRAW(string cs) => InvokeOnMainThreadAsync(() => DrawInternal(cs));
        
    /// <summary>
    /// Copia il contenuto di una pagina sorgente in una pagina di destinazione.
    /// </summary>
    /// <param name="sp">L'indice della pagina sorgente.</param>
    /// <param name="dp">L'indice della pagina di destinazione.</param>
    public Task PCOPY(int sp, int dp) => InvokeOnMainThreadAsync(() => PCopyInternal(sp, dp));
        
    /// <summary>
    /// Imposta la pagina visibile e/o attiva.
    /// </summary>
    /// <param name="v">L'indice della pagina visibile (opzionale).</param>
    /// <param name="a">L'indice della pagina attiva (opzionale).</param>
    public Task SCREENSET(int? v, int? a = null) => InvokeOnMainThreadAsync(() => ScreenSetInternal(v, a));
        
    /// <summary>
    /// Imposta la posizione del cursore. Questo metodo è asincrono.
    /// </summary>
    /// <param name="x">Coordinata x del cursore.</param>
    /// <param name="y">Coordinata y del cursore.</param>
    public Task SetCursorPosition(int x, int y) => InvokeOnMainThreadAsync(() => _cursorPosition = (x, y));
        
    /// <summary>
    /// Cattura un'area rettangolare dello schermo e la restituisce come array di interi.
    /// Questo metodo è ora thread-safe: accoda l'operazione e attende il risultato.
    /// </summary>
    /// <param name="x">Coordinata x di partenza.</param>
    /// <param name="y">Coordinata y di partenza.</param>
    /// <param name="w">La larghezza dell'area.</param>
    /// <param name="h">L'altezza dell'area.</param>
    /// <returns>Un array di interi che rappresenta l'immagine. Restituisce <c>null</c> se l'operazione fallisce.</returns>
    public Task<int[]?> IMAGEGET(int x, int y, int w, int h)
    {
        if (!IsScreenOn || w <= 0 || h <= 0) return Task.FromResult<int[]?>(null);
        var tcs = new TaskCompletionSource<int[]?>();
        _commandQueue.Add(() =>
        {
            try { tcs.TrySetResult(ImageGetInternal(x, y, w, h)); }
            catch (Exception ex) { tcs.TrySetException(ex); }
        });
        return tcs.Task;
    }

    /// <summary>
    /// Calcola la larghezza in pixel di una stringa di testo utilizzando il font corrente.
    /// Questo metodo è ora thread-safe: accoda l'operazione e attende il risultato.
    /// </summary>
    /// <param name="t">Il testo di cui calcolare la larghezza.</param>
    /// <returns>La larghezza in pixel del testo. Restituisce un valore predefinito se l'operazione fallisce.</returns>
    public Task<int> TEXTWIDTH(string t)
    {
        if (!IsScreenOn || _font == IntPtr.Zero) return Task.FromResult(t.Length * 8);
        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() =>
        {
            try { tcs.TrySetResult(TextWidthInternal(t)); }
            catch (Exception ex) { tcs.TrySetException(ex); }
        });
        return tcs.Task;
    }
        
    /// <summary>
    /// Calcola l'altezza in pixel del font corrente.
    /// Questo metodo è ora thread-safe: accoda l'operazione e attende il risultato.
    /// </summary>
    /// <returns>L'altezza in pixel del font. Restituisce un valore predefinito se l'operazione fallisce.</returns>
    public Task<int> TEXTHEIGHT()
    {
        if (!IsScreenOn || _font == IntPtr.Zero) return Task.FromResult(16);
        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() =>
        {
            try { tcs.TrySetResult(TextHeightInternal()); }
            catch (Exception ex) { tcs.TrySetException(ex); }
        });
        return tcs.Task;
    }
        
    /// <summary>
    /// Salva lo stato corrente dello schermo (pagina attiva, configurazione e contenuto)
    /// con un nome specificato.
    /// </summary>
    /// <param name="name">Il nome da associare allo stato dello schermo salvato.</param>
    public Task SAVE_SCREEN(string name) => InvokeOnMainThreadAsync(() => SaveScreenInternal(name));

    /// <summary>
    /// Ripristina uno stato dello schermo precedentemente salvato con il nome specificato.
    /// </summary>
    /// <param name="name">Il nome dello stato dello schermo da ripristinare.</param>
    public Task RESTORE_SCREEN(string name) => InvokeOnMainThreadAsync(() => RestoreScreenInternal(name));
        
    /// <summary>
    /// Legge il colore di un singolo pixel dallo schermo grafico e restituisce
    /// il suo indice nella palette di colori corrente.
    /// </summary>
    /// <param name="x">La coordinata x del pixel da leggere.</param>
    /// <param name="y">La coordinata y del pixel da leggere.</param>
    /// <returns>
    /// Un Task che si completa con l'indice del colore del pixel.
    /// Restituisce -1 se le coordinate sono fuori schermo o in caso di errore.
    /// </returns>
    public Task<int> POINT(int x, int y)
    {
        // Se lo schermo non è attivo, restituisci immediatamente -1 senza accodare nulla.
        if (!IsScreenOn) return Task.FromResult(-1);

        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() =>
        {
            try 
            {
                tcs.TrySetResult(PointInternal(x, y));
            }
            catch (Exception ex) 
            {
                // In caso di errore imprevisto, completa il task con un'eccezione.
                tcs.TrySetException(ex);
            }
        });
        return tcs.Task;
    }
        
    /// <summary>
    /// Modifica un colore nella palette corrente, sia in modalità grafica che
    /// per la console emulata (SdlConsole).
    /// </summary>
    /// <param name="index">L'indice del colore nella palette (0-255).</param>
    /// <param name="r">La componente rossa del nuovo colore (0-255).</param>
    /// <param name="g">La componente verde del nuovo colore (0-255).</param>
    /// <param name="b">La componente blu del nuovo colore (0-255).</param>
    public Task PALETTE(int index, int r, int g, int b) => 
        InvokeOnMainThreadAsync(() => PaletteInternal(index, r, g, b));
        
    /// <summary>
    /// Carica un'immagine dal disco in una texture accelerata e restituisce il suo handle.
    /// Questo metodo è thread-safe e attende il completamento.
    /// </summary>
    /// <param name="filePath">Il percorso del file immagine.</param>
    /// <returns>
    /// Un Task che si completa con l'handle (un numero intero > 0) della texture caricata.
    /// Restituisce 0 in caso di errore.
    /// </returns>
    public Task<int> LOAD_TEXTURE(string filePath)
    {
        if (!IsScreenOn) return Task.FromResult(0);
            
        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() =>
        {
            try { tcs.TrySetResult(LoadTextureInternal(filePath)); }
            catch (Exception ex) { tcs.TrySetException(ex); }
        });
        return tcs.Task;
    }

    /// <summary>
    /// Disegna una texture precedentemente caricata sullo schermo.
    /// Questo metodo è asincrono ("fire-and-forget").
    /// </summary>
    /// <param name="textureId">L'handle della texture da disegnare.</param>
    /// <param name="x">La coordinata X del centro della texture.</param>
    /// <param name="y">La coordinata Y del centro della texture.</param>
    /// <param name="rotation">L'angolo di rotazione in gradi (default 0).</param>
    /// <param name="scaleX">Il fattore di scala sull'asse X (default 1.0).</param>
    /// <param name="scaleY">Il fattore di scala sull'asse Y (default usa scaleX).</param>
    public Task DRAW_TEXTURE(int textureId, int x, int y, double rotation, double scaleX, double? scaleY) => 
        InvokeOnMainThreadAsync(() => DrawTextureInternal(textureId, x, y, rotation, scaleX, scaleY, 0));

    /// <summary>
    /// Disegna in batch (in un'unica chiamata) più istanze della stessa texture.
    /// Questo è il comando ad alte prestazioni che sostituisce i loop di disegno in BASIC.
    /// </summary>
    /// <param name="textureId">L'handle della texture da disegnare.</param>
    /// <param name="xPos">L'array delle coordinate X.</param>
    /// <param name="yPos">L'array delle coordinate Y.</param>
    /// <param name="rotations">L'array delle rotazioni in gradi.</param>
    /// <param name="scales">L'array dei fattori di scala.</param>
    public Task DRAW_TEXTURES(int textureId, Array xPos, Array yPos, Array rotations, Array scales) =>
        InvokeOnMainThreadAsync(() => DrawTexturesInternal(textureId, xPos, yPos, rotations, scales));
    
    #endregion

    #region Public Sprite Commands
    
    /// <summary>
    /// Carica un'immagine dal disco, la converte in uno sprite e ne restituisce l'handle.
    /// </summary>
    /// <param name="filePath">Il percorso del file immagine.</param>
    /// <returns>Un Task che si completa con l'handle (ID) del nuovo sprite, o 0 in caso di errore.</returns>
    public Task<int> SPRITE_LOAD(string filePath)
    {
        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() => tcs.TrySetResult(LoadSpriteInternal(filePath)));
        return tcs.Task;
    }
    
    /// <summary>
    /// Imposta la posizione del centro di uno sprite.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da modificare.</param>
    /// <param name="x">La nuova coordinata X del centro dello sprite.</param>
    /// <param name="y">La nuova coordinata Y del centro dello sprite.</param>
    public Task SPRITE_SET_POSITION(int spriteId, float x, float y) =>
        InvokeOnMainThreadAsync(() => SetSpritePositionInternal(spriteId, x, y));
        
    /// <summary>
    /// Rende uno sprite invisibile, senza rimuoverlo dalla memoria.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da nascondere.</param>
    public Task SPRITE_OFF(int spriteId) =>
        InvokeOnMainThreadAsync(() => SetSpriteVisibilityInternal(spriteId, false));
        
    /// <summary>
    /// Rende uno sprite nuovamente visibile.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da mostrare.</param>
    public Task SPRITE_ON(int spriteId) =>
        InvokeOnMainThreadAsync(() => SetSpriteVisibilityInternal(spriteId, true));

    /// <summary>
    /// Imposta la priorità di disegno (z-index) di uno sprite.
    /// Sprite con priorità più alta vengono disegnati sopra quelli con priorità più bassa.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da modificare.</param>
    /// <param name="priority">Il nuovo valore di priorità.</param>
    public Task SPRITE_PRIORITY(int spriteId, int priority) =>
        InvokeOnMainThreadAsync(() => SetSpritePriorityInternal(spriteId, priority));

    /// <summary>
    /// Sincronizza lo stato del gioco e disegna un singolo frame.
    /// Questa funzione esegue l'intero ciclo di update e draw:
    /// aggiorna le posizioni, controlla le collisioni (in futuro),
    /// e disegna tutti gli sprite visibili.
    /// </summary>
    public Task SYNC() => InvokeOnMainThreadAsync(UpdateAndDrawAllSpritesInternal);
    
    /// <summary>
    /// Associa permanentemente una matrice a uno sprite. Da questo momento in poi,
    /// la matrice verrà applicata automaticamente a ogni frame.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite.</param>
    /// <param name="matrixId">L'ID della matrice da associare (0 per rimuovere l'associazione).</param>
    public Task SPRITE_SET_MATRIX(int spriteId, int matrixId) =>
        InvokeOnMainThreadAsync(() => SetSpriteMatrixInternal(spriteId, matrixId));

    /// <summary>
    /// Applica una tantum una matrice di trasformazione a uno sprite.
    /// Questa operazione modifica permanentemente le proprietà Position, Rotation e Scale dello sprite.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da trasformare.</param>
    /// <param name="matrixId">L'ID della matrice da applicare.</param>
    public Task SPRITE_TRANSFORM(int spriteId, int matrixId) =>
        InvokeOnMainThreadAsync(() => TransformSpriteInternal(spriteId, matrixId));

    #endregion

    /*
    #region Public Matrix Commands

    public Task<int> CREATE_MATRIX()
    {
        var tcs = new TaskCompletionSource<int>();
        _commandQueue.Add(() => tcs.TrySetResult(CreateMatrixInternal()));
        return tcs.Task;
    }

    public Task IDENTITY_MATRIX(int matrixId) => 
        InvokeOnMainThreadAsync(() => IdentityMatrixInternal(matrixId));

    public Task TRANSLATE_MATRIX(int matrixId, float x, float y, float z) => 
        InvokeOnMainThreadAsync(() => TranslateMatrixInternal(matrixId, x, y, z));

    public Task ROTATE_MATRIX(int matrixId, float ax, float ay, float az) => 
        InvokeOnMainThreadAsync(() => RotateMatrixInternal(matrixId, ax, ay, az));

    public Task SCALE_MATRIX(int matrixId, float sx, float sy, float sz) => 
        InvokeOnMainThreadAsync(() => ScaleMatrixInternal(matrixId, sx, sy, sz));
        
    public Task MULTIPLY_MATRIX(int m1, int m2, int result) =>
        InvokeOnMainThreadAsync(() => MultiplyMatrixInternal(m1, m2, result));
    
    /// <summary>
    /// Richiede di salvare lo stato corrente di una matrice sullo stack.
    /// </summary>
    public Task MATRIX_PUSH(int matrixId) => 
        InvokeOnMainThreadAsync(() => MatrixPushInternal(matrixId));

    /// <summary>
    /// Richiede di ripristinare l'ultimo stato di una matrice salvato sullo stack.
    /// </summary>
    public Task MATRIX_POP(int matrixId) =>
        InvokeOnMainThreadAsync(() => MatrixPopInternal(matrixId));

    /// <summary>
    /// Imposta la matrice di VISTA (telecamera) globale.
    /// </summary>
    public Task VIEW_MATRIX(int matrixId) => 
        InvokeOnMainThreadAsync(() => SetViewMatrixInternal(matrixId));
        
    /// <summary>
    /// Imposta la matrice di PROIEZIONE globale.
    /// </summary>
    public Task PROJECTION_MATRIX(int matrixId) =>
        InvokeOnMainThreadAsync(() => SetProjectionMatrixInternal(matrixId));
        
    /// <summary>
    /// Imposta la matrice di MONDO globale.
    /// </summary>
    public Task WORLD_MATRIX(int matrixId) =>
        InvokeOnMainThreadAsync(() => SetWorldMatrixInternal(matrixId));

    #endregion
    */

    /// <summary>Imposta il viewport (area di clipping per il disegno).</summary>
    public Task VIEW(int x1, int y1, int x2, int y2) =>
        InvokeOnMainThreadAsync(() => SetViewport(x1, y1, x2, y2));

    /// <summary>Imposta il sistema di coordinate logiche (WINDOW).</summary>
    public Task WINDOW(double x1, double y1, double x2, double y2, bool screen) =>
        InvokeOnMainThreadAsync(() => SetWindow(x1, y1, x2, y2, screen));

}