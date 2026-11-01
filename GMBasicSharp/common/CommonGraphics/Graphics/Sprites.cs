using CommonLib.Screens;
// FILE: Interpreter.Graphics.Sprite.cs

using System.Numerics;
using MathNet.Numerics.LinearAlgebra;
using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

/// <summary>
/// Contiene le definizioni delle strutture dati e delle proprietà
/// relative alla gestione degli sprite 2D.
/// </summary>
public sealed partial class GraphicsManager
{
    /// <summary>
    /// Rappresenta un singolo sprite all'interno del motore grafico.
    /// Contiene tutte le proprietà necessarie per il rendering, le trasformazioni e la logica di gioco.
    /// È una 'class' invece di una 'struct' perché deve essere modificabile e gestita tramite riferimento.
    /// </summary>
    /// <summary>Collisioni rilevate nell'ultimo SYNC: (id1, id2, x, y). Consumate dalla VM
    /// per dispacciare gli eventi ON SPRITE COLLISION(S).</summary>
    private readonly List<(int id1, int id2, float x, float y)> _lastCollisions = new();

    /// <summary>Restituisce e azzera le collisioni dell'ultimo SYNC.</summary>
    public List<(int id1, int id2, float x, float y)> TakeLastCollisions()
    {
        var c = _lastCollisions.ToList();
        _lastCollisions.Clear();
        return c;
    }

    /// <summary>Rileva le collisioni AABB tra gli sprite visibili (O(n²)) dopo il draw.</summary>
    private int _collisionCheckCounter;

    /// <summary>Rileva le collisioni AABB tra gli sprite visibili (O(n²)) dopo il draw.
    /// Il check è THROTTLED (ogni 10 SYNC): con le collisioni continue, il dispatch delle
    /// callback a OGNI frame rallentava il loop di animazione (frame rate basso, lampeggio).</summary>
    private void DetectCollisions()
    {
        _lastCollisions.Clear();
        if (++_collisionCheckCounter % 5 != 0) return;
        var vis = _visibleSpritesSorted;
        if (vis.Count < 2) return;

        // SPATIAL GRID (griglia spaziale): invece di O(n^2) (124.750 coppie a 500 sprite),
        // dividi lo schermo in celle di dimensione ~sprite e controlla solo le coppie nella
        // stessa cella o celle adiacenti → O(n) medio. SDL2/Vulkan non hanno collision
        // detection (sono solo rendering): è un problema CPU, e la griglia è la soluzione
        // ottimale per sprite di dimensione uniforme.
        var cellSize = Math.Max(16, _screenWidth / 32); // celle ~20px per schermi 640x480
        var cols = Math.Max(1, _screenWidth / cellSize);
        var rows = Math.Max(1, _screenHeight / cellSize);
        // Bucket per cella (indice = row * cols + col)
        var buckets = new List<Sprite>[cols * rows];
        var cellOf = new int[vis.Count]; // cella di ogni sprite (evita ricalcolo)
        for (var i = 0; i < vis.Count; i++)
        {
            var s = vis[i];
            if (!s.IsVisible) { cellOf[i] = -1; continue; }
            var rb = s.BoundingBox;
            var cx = Math.Clamp((int)((rb.x + rb.w / 2f) / cellSize), 0, cols - 1);
            var cy = Math.Clamp((int)((rb.y + rb.h / 2f) / cellSize), 0, rows - 1);
            var ci = cy * cols + cx;
            cellOf[i] = ci;
            (buckets[ci] ??= new List<Sprite>()).Add(s);
        }

        // Confronta ogni sprite con gli altri nella STESSA cella o nelle 8 adiacenti.
        // HashSet evita i duplicati (la coppia a-b è vista da entrambe le celle).
        var seen = new System.Collections.Generic.HashSet<(int, int)>();
        for (var i = 0; i < vis.Count; i++)
        {
            var ci = cellOf[i];
            if (ci < 0) continue;
            var a = vis[i];
            var ra = a.BoundingBox;
            var idA = FindSpriteId(a);
            var row = ci / cols;
            var col = ci % cols;
            for (var dr = -1; dr <= 1; dr++)
            for (var dc = -1; dc <= 1; dc++)
            {
                var rr = row + dr; var cc = col + dc;
                if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                var bucket = buckets[rr * cols + cc];
                if (bucket == null) continue;
                foreach (var b in bucket)
                {
                    if (ReferenceEquals(b, a)) continue;
                    var idB = FindSpriteId(b);
                    var k1 = idA < idB ? (idA, idB) : (idB, idA);
                    if (!seen.Add(k1)) continue;
                    var rb = b.BoundingBox;
                    if (ra.x < rb.x + rb.w && ra.x + ra.w > rb.x &&
                        ra.y < rb.y + rb.h && ra.y + ra.h > rb.y)
                    {
                        _lastCollisions.Add((idA, idB, ra.x + ra.w / 2f, ra.y + ra.h / 2f));
                    }
                }
            }
        }
    }

    private sealed class Sprite
    {
        /// <summary>
        /// L'handle (puntatore) alla texture SDL in memoria video.
        /// </summary>
        public IntPtr Texture { get; }

        /// <summary>
        /// Le dimensioni originali (in pixel) della texture.
        /// Usate per calcolare i rettangoli e le trasformazioni.
        /// </summary>
        public (int Width, int Height) TextureSize { get; }

        /// <summary>Pixel ARGB della texture (per il blit 2D — RenderCopy non disegna col
        /// renderer software; le matrici 3D sono opzionali).</summary>
        public uint[]? Pixels { get; set; }

        /// <summary>Bounding box del CONTENUTO OPACO (pixel con alfa &gt; 0), in coordinate
        /// della texture. Le collisioni AABB usano QUESTO (le reali dimensioni visibili),
        /// non il rettangolo pieno della texture (che include i pixel trasparenti).</summary>
        public (int MinX, int MinY, int MaxX, int MaxY)? OpaqueBounds { get; set; }

        // --- Proprietà di Trasformazione ---

        /// <summary>
        /// La posizione 2D del centro dello sprite nel mondo di gioco.
        /// </summary>
        public Vector2 Position { get; set; } = Vector2.Zero;

        /// <summary>
        /// La rotazione dello sprite in gradi.
        /// </summary>
        public float Rotation { get; set; }

        /// <summary>
        /// Il fattore di scala dello sprite. (1.0f, 1.0f) significa nessuna scala.
        /// </summary>
        public Vector2 Scale { get; set; } = Vector2.One;

        // --- Proprietà di Stato e Rendering ---
        
        /// <summary>
        /// Se false, lo sprite non verrà disegnato.
        /// Corrisponde al comando BASIC `SPRITE OFF`.
        /// </summary>
        public bool IsVisible { get; set; } = true;
        
        /// <summary>
        /// L'ordine di disegno (z-index). Valori più alti vengono disegnati sopra.
        /// Corrisponde al comando BASIC `SPRITE PRIORITY`.
        /// </summary>
        public int Priority { get; set; }

        /// <summary>
        /// Determina se lo sprite deve essere trasformato dalle matrici
        /// globali (Mondo, Vista, Proiezione). Se false, viene disegnato
        /// direttamente in coordinate schermo (utile per la UI).
        /// </summary>
        public bool UseWorldTransform { get; set; } = true;
        
        // --- Proprietà per le Collisioni ---
        
        /// <summary>
        /// Il rettangolo di ingombro (bounding box) dello sprite, calcolato in coordinate del mondo.
        /// Viene ricalcolato a ogni frame se lo sprite si muove, ruota o scala.
        /// </summary>
        public SDL.SDL_Rect BoundingBox { get; set; }
        
        /// <summary>
        /// L'handle (ID) della matrice di trasformazione LINALG (MathNet) associata a questo sprite.
        /// Se 0, non viene usata nessuna matrice.
        /// Corrisponde al comando BASIC `SPRITE SET MATRIX`.
        /// </summary>
        public long TransformMatrixId { get; set; }
        
        /// <summary>
        /// Inizializza una nuova istanza della classe Sprite.
        /// </summary>
        /// <param name="texture">L'handle della texture SDL.</param>
        /// <param name="width">La larghezza della texture.</param>
        /// <param name="height">L'altezza della texture.</param>
        public Sprite(IntPtr texture, int width, int height)
        {
            Texture = texture;
            TextureSize = (width, height);
            // Inizializza la bounding box con le dimensioni iniziali
            BoundingBox = new SDL.SDL_Rect { x = 0, y = 0, w = width, h = height };
        }

        /// <summary>Id dello sprite (set dall'esterno alla registrazione in _sprites).
        /// Permette a FindSpriteId di essere O(1) invece della ricerca lineare su _sprites
        /// (che con centinaia di sprite in collisione = milioni di iterazioni per SYNC →
        /// frame rate crollato → lampeggio/stutter nel rilevamento collisioni).</summary>
        public int Id { get; set; }
    }
    
    // --- Strutture Dati Principali del GraphicsManager per gli Sprite ---

    /// <summary>
    /// Il dizionario principale che mappa l'ID di uno sprite (visibile al programmatore BASIC)
    /// all'oggetto Sprite interno che ne contiene lo stato.
    /// </summary>
    private readonly Dictionary<int, Sprite> _sprites = new();

    /// <summary>
    /// Un contatore per generare ID univoci per i nuovi sprite creati.
    /// </summary>
    private int _nextSpriteId = 1;
    
    /// <summary>
    /// Una lista che viene ricalcolata a ogni frame e contiene solo gli sprite visibili,
    /// già ordinati per priorità (z-index). Questo ottimizza il ciclo di disegno.
    /// </summary>
    private readonly List<Sprite> _visibleSpritesSorted = new();
    
    /// <summary>
    /// Carica una texture, crea un oggetto Sprite e lo aggiunge al dizionario di gestione.
    /// </summary>
    /// <param name="filePath">Il percorso del file immagine.</param>
    /// <returns>L'ID del nuovo sprite, o 0 se il caricamento fallisce.</returns>
    private int LoadSpriteInternal(string filePath)
    {
        if (_programContext is null) return 0;

        // Come LoadTextureInternal: i sample usano path relativi al CWD ma le risorse di
        // test stanno in bin/ — risolve il path se il file non esiste.
        var resolvedPath = filePath;
        if (!File.Exists(filePath))
        {
            var candidates = new[]
            {
                Path.Combine(AppContext.BaseDirectory, filePath),
                Path.Combine(Directory.GetCurrentDirectory(), "bin", filePath),
                Path.Combine(Directory.GetCurrentDirectory(), "Resources", filePath),
                Path.Combine("bin", filePath),
            };
            foreach (var cand in candidates)
            {
                if (File.Exists(cand)) { resolvedPath = cand; break; }
            }
        }

        // Carica con IMG_Load (surface) per conservare i pixel ARGB (blit 2D), poi crea la
        // texture. IMG_LoadTexture (solo texture) non dà accesso ai pixel.
        var surface = SDL_image.IMG_Load(resolvedPath);
        if (surface == IntPtr.Zero)
        {
            LogDebugGfx($"[SPRITE_ERROR] Impossibile caricare l'immagine per lo sprite: {filePath}. Errore: {SDL.SDL_GetError()}");
            return 0;
        }
        uint[]? spritePixels = null;
        var logicalW = 0; var logicalH = 0;
        try
        {
            var argb = SDL.SDL_ConvertSurfaceFormat(surface, SDL.SDL_PIXELFORMAT_ARGB8888, 0);
            if (argb != IntPtr.Zero)
            {
                var sur = System.Runtime.InteropServices.Marshal.PtrToStructure<SDL.SDL_Surface>(argb);
                if (sur.w > 0 && sur.h > 0 && sur.pitch >= sur.w * 4 && sur.pixels != IntPtr.Zero)
                {
                    logicalW = sur.w; logicalH = sur.h;
                    spritePixels = new uint[sur.w * sur.h];
                    for (var yy = 0; yy < sur.h; yy++)
                    {
                        var row = sur.pixels + yy * sur.pitch;
                        for (var xx = 0; xx < sur.w; xx++)
                            spritePixels[yy * sur.w + xx] = (uint)System.Runtime.InteropServices.Marshal.ReadInt32(row + xx * 4);
                    }
                }
                SDL.SDL_FreeSurface(argb);
            }
        }
        catch { spritePixels = null; }

        // Texture a risoluzione NATIVA (w×scale, h×scale) come il font: su Retina scale=2
        // uno sprite 32×32 logico viene disegnato a 64×64 nativi. Se la texture fosse
        // logica, RenderCopyEx la ingrandirebbe con filtro → SFOCATA. Si scala la texture
        // con nearest-neighbor (pixel-art: niente blur) e si mantiene TextureSize LOGICO
        // per posizionamento, bounding box e collisioni.
        IntPtr texture;
        var rs = _renderScale > 0 ? _renderScale : 1.0f;
        var nw = (int)Math.Round(logicalW * rs);
        var nh = (int)Math.Round(logicalH * rs);
        if (rs > 1.0f && spritePixels != null && nw > 0 && nh > 0)
        {
            var native = new uint[nw * nh];
            for (var yy = 0; yy < nh; yy++)
            {
                var sy = Math.Min(logicalH - 1, yy / (int)rs);
                for (var xx = 0; xx < nw; xx++)
                {
                    var sx = Math.Min(logicalW - 1, xx / (int)rs);
                    native[yy * nw + xx] = spritePixels[sy * logicalW + sx];
                }
            }
            texture = SDL.SDL_CreateTexture(_programContext.RendererHandle,
                SDL.SDL_PIXELFORMAT_ARGB8888, (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_STATIC,
                nw, nh);
            if (texture != IntPtr.Zero)
            {
                var handle = System.Runtime.InteropServices.GCHandle.Alloc(native,
                    System.Runtime.InteropServices.GCHandleType.Pinned);
                try { SDL.SDL_UpdateTexture(texture, IntPtr.Zero, handle.AddrOfPinnedObject(), nw * 4); }
                finally { handle.Free(); }
            }
        }
        else
        {
            texture = SDL.SDL_CreateTextureFromSurface(_programContext.RendererHandle, surface);
            nw = logicalW; nh = logicalH;
        }
        SDL.SDL_FreeSurface(surface);
        if (texture == IntPtr.Zero)
        {
            LogDebugGfx($"[SPRITE_ERROR] Impossibile creare la texture dello sprite: {filePath}. Errore: {SDL.SDL_GetError()}");
            return 0;
        }

        SDL.SDL_SetTextureBlendMode(texture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);

        // TextureSize LOGICO (dimensione del file) per posizionamento/collisioni; la
        // texture SDL è nativa (nw×nh). w/h qui sono i LOGICI.
        var w = logicalW; var h = logicalH;

        var sprite = new Sprite(texture, w, h) { Pixels = spritePixels };
        if (spritePixels != null && w > 0 && h > 0)
        {
            // Bounding box del contenuto opaco (alfa > 0): collisioni sulle dimensioni REALI.
            var minX = w; var minY = h; var maxX = -1; var maxY = -1;
            for (var yy = 0; yy < h; yy++)
            {
                var row = yy * w;
                for (var xx = 0; xx < w; xx++)
                {
                    if (((spritePixels[row + xx] >> 24) & 0xFF) > 0)
                    {
                        if (xx < minX) minX = xx;
                        if (xx > maxX) maxX = xx;
                        if (yy < minY) minY = yy;
                        if (yy > maxY) maxY = yy;
                    }
                }
            }
            if (maxX >= minX && maxY >= minY)
                sprite.OpaqueBounds = (minX, minY, maxX, maxY);
        }
        var newId = _nextSpriteId++;
        sprite.Id = newId;
        _sprites[newId] = sprite;

        // la bounding box iniziale è già impostata dal costruttore (0,0,w,h)
        return newId;
    }
    
    /// <summary>
    /// Modifica la posizione di uno sprite esistente e aggiorna la bounding box.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da modificare.</param>
    /// <param name="x">La nuova coordinata X del centro dello sprite.</param>
    /// <param name="y">La nuova coordinata Y del centro dello sprite.</param>
    private void SetSpritePositionInternal(int spriteId, float x, float y)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.Position = new Vector2(x, y);
            UpdateBoundingBox(sprite);
        }
    }

    /// <summary>
    /// Modifica la scala di uno sprite esistente (SPRITE SET SCALE id, sx, sy) e aggiorna
    /// la bounding box: la scala cambia l'area occupata a schermo.
    /// </summary>
    private void SetSpriteScaleInternal(int spriteId, float sx, float sy)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.Scale = new Vector2(sx, sy);
            UpdateBoundingBox(sprite);
        }
    }

    /// <summary>
    /// Modifica la rotazione di uno sprite esistente, in gradi (SPRITE SET ROTATION id, gradi).
    /// La bounding box si aggiorna perché l'area a schermo cambia con l'angolo.
    /// </summary>
    private void SetSpriteRotationInternal(int spriteId, float degrees)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.Rotation = degrees;
            UpdateBoundingBox(sprite);
        }
    }

    /// <summary>
    /// Modifica la visibilità di uno sprite esistente.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da modificare.</param>
    /// <param name="isVisible">True per rendere visibile, false per invisibile.</param>
    private void SetSpriteVisibilityInternal(int spriteId, bool isVisible)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.IsVisible = isVisible;
        }
    }
    
    /// <summary>
    /// Modifica la priorità di disegno di uno sprite esistente.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da modificare.</param>
    /// <param name="priority">Il nuovo valore di priorità (z-index).</param>
    private void SetSpritePriorityInternal(int spriteId, int priority)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.Priority = priority;
        }
    }
    
    /// <summary>
    /// Il metodo principale del game loop, chiamato da SYNC.
    /// Orchestra l'aggiornamento dello stato e il rendering di tutti gli sprite.
    /// Non accetta più parametri, ma utilizza le callback interne (_...MatrixProvider) per
    /// recuperare le matrici dall'Executor.
    /// </summary>
    private void UpdateAndDrawAllSpritesInternal()
    {
        if (_programContext is null || !IsScreenOn) return;

        // Il SYNC NON cancella la pagina. In un game loop professionale il CLS appartiene
        // al back buffer (o al dirty-rect): qui cancella l'intero schermo a ogni frame
        // (307200 px) ed è la causa del lampeggio se il disegno avviene in un'operazione
        // separata. Gli sprite ridisegnano SOLO le loro aree (vedi DrawVisibleSprites2D).
        
        // Se le callback non sono state impostate, non è possibile eseguire il rendering
        // che dipende dalle matrici. Si esce per evitare un NullReferenceException.
        if (_getLinearMatrixProvider == null || _getWorldMatrixProvider == null || _getViewMatrixProvider == null || _getProjectionMatrixProvider == null)
        {
            // SYNC senza matrici 3D: sprite 2D semplici — disegna col blit via buffer
            // (RenderCopy/RenderGeometry non funzionano col renderer software).
            UpdateSpriteLogic();
            PrepareVisibleSpriteList();
            DrawVisibleSprites2D();
            DetectCollisions();
            return;
        }
        
        UpdateSpriteLogic();
        PrepareVisibleSpriteList();
        DrawVisibleSprites();
        DetectCollisions();
    }
    
    /// <summary>
    /// Placeholder per la futura logica di gioco (movimento, animazioni, IA).
    /// </summary>
    private void UpdateSpriteLogic()
    {
        // In futuro, qui si aggiorneranno le posizioni, le animazioni, etc.
        // Esempio:
        // foreach (var sprite in _sprites.Values) {
        //     sprite.Position += sprite.Velocity * deltaTime;
        // }
    }

    /// <summary>
    /// Pulisce la lista degli sprite visibili, la ripopola con gli sprite
    /// attualmente attivi e la ordina in base alla priorità (z-index).
    /// </summary>
    private void PrepareVisibleSpriteList()
    {
        _visibleSpritesSorted.Clear();
        foreach (var sprite in _sprites.Values)
        {
            if (sprite.IsVisible)
            {
                _visibleSpritesSorted.Add(sprite);
            }
        }
        _visibleSpritesSorted.Sort((s1, s2) => s1.Priority.CompareTo(s2.Priority));
    }
    
    /// <summary>
    /// Associa un handle di matrice LINALG (MathNet) a uno sprite.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite.</param>
    /// <param name="matrixId">L'ID della matrice LINALG da associare (0 per rimuovere l'associazione).</param>
    private void SetSpriteMatrixInternal(int spriteId, long matrixId)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
        {
            sprite.TransformMatrixId = matrixId;
        }
    }

    /// <summary>
    /// Applica una tantum una matrice di trasformazione LINALG (MathNet) a uno sprite,
    /// decomponendola e modificando le proprietà Position, Rotation e Scale dello sprite.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da trasformare.</param>
    /// <param name="matrixId">L'ID della matrice LINALG da applicare.</param>
    private void TransformSpriteInternal(int spriteId, long matrixId)
    {
        if (_getLinearMatrixProvider == null) return;
        if (!_sprites.TryGetValue(spriteId, out var sprite)) return;
        if (matrixId == 0) return;

        var mathNetMatrix = _getLinearMatrixProvider(matrixId);

        // Converte la matrice MathNet in Matrix4x4 di System.Numerics
        var matrix = ToSystemNumericsMatrix(mathNetMatrix);

        if (Matrix4x4.Decompose(matrix, out var scale, out var rotation, out var translation))
        {
            sprite.Position = new Vector2(translation.X, translation.Y);
            sprite.Scale = new Vector2(scale.X, scale.Y);

            // Estrae la rotazione Z (yaw) in radianti e la converte in gradi
            var yaw = MathF.Atan2(2.0f * (rotation.Y * rotation.W + rotation.X * rotation.Z), 1.0f - 2.0f * (rotation.X * rotation.X + rotation.Y * rotation.Y));
            sprite.Rotation = yaw * (180.0f / MathF.PI);

            UpdateBoundingBox(sprite);
        }
    }
    
    /// <summary>
    /// Esegue il rendering di tutti gli sprite visibili usando un approccio ottimizzato
    /// di batch rendering con SDL_RenderGeometry. Applica selettivamente le matrici
    /// globali (Mondo, Vista, Proiezione) in base alla proprietà 'UseWorldTransform' di ogni sprite.
    /// Utilizza i campi interni del GraphicsManager per le matrici globali e la callback per le matrici degli sprite.
    /// </summary>
    private void DrawVisibleSprites()
    {
        if (_programContext is null || _visibleSpritesSorted.Count == 0) return;
        
        // FALLBACK 2D: senza i provider delle matrici (SYNC usato per sprite 2D semplici,
        // come i sample SpritesExamples), disegna con il BLIT via buffer — RenderCopy/
        // RenderGeometry non funzionano col renderer software. Le matrici 3D restano
        // il percorso completo quando i provider sono impostati.
        if (_getLinearMatrixProvider == null)
        {
            DrawVisibleSprites2D();
            return;
        }
        
        var renderer = _programContext.RendererHandle;
        SetRenderTargetToActivePage();

        // Usa i campi interni del GraphicsManager (che sono stati aggiornati dai comandi LINALG)
        // per le matrici globali.
        var worldViewProjectionMatrix = _worldMatrix * _viewMatrix * _projectionMatrix;
        
        var spritesByTexture = new Dictionary<IntPtr, List<Sprite>>();
        foreach (var sprite in _visibleSpritesSorted)
        {
            if (!spritesByTexture.TryGetValue(sprite.Texture, out var spriteList))
            {
                spriteList = new List<Sprite>();
                spritesByTexture[sprite.Texture] = spriteList;
            }
            spriteList.Add(sprite);
        }
        
        foreach (var (texturePtr, spriteGroup) in spritesByTexture)
        {
            var spriteCount = spriteGroup.Count;
            if (spriteCount == 0) continue;

            var vertices = new GraphicsManagerExtensions.SDL_Vertex[spriteCount * 4];
            var indices = new int[spriteCount * 6];

            for (var i = 0; i < spriteCount; i++)
            {
                var sprite = spriteGroup[i];

                var modelMatrix = Matrix4x4.CreateScale(sprite.Scale.X, sprite.Scale.Y, 1.0f) *
                                  Matrix4x4.CreateRotationZ(sprite.Rotation * (MathF.PI / 180.0f)) *
                                  Matrix4x4.CreateTranslation(sprite.Position.X, sprite.Position.Y, 0);

                // Applica la matrice LINALG associata allo sprite, se presente,
                // recuperandola tramite la callback.
                if (sprite.TransformMatrixId != 0)
                {
                    var attachedMathNetMatrix = _getLinearMatrixProvider(sprite.TransformMatrixId);
                    modelMatrix *= ToSystemNumericsMatrix(attachedMathNetMatrix);
                }

                // La matrice finale di trasformazione dipende da UseWorldTransform
                var finalMatrix = sprite.UseWorldTransform ? modelMatrix * worldViewProjectionMatrix : modelMatrix;
                
                var halfW = sprite.TextureSize.Width / 2.0f;
                var halfH = sprite.TextureSize.Height / 2.0f;

                var corners = new Vector2[]
                {
                    new(-halfW, -halfH), new(halfW, -halfH), new(halfW, halfH), new(-halfW, halfH)
                };
                
                var vertexBaseIndex = i * 4;
                var tl = Vector2.Transform(corners[0], finalMatrix);
                var tr = Vector2.Transform(corners[1], finalMatrix);
                var br = Vector2.Transform(corners[2], finalMatrix);
                var bl = Vector2.Transform(corners[3], finalMatrix);
                
                vertices[vertexBaseIndex + 0] = CreateVertex(tl.X, tl.Y, 0.0f, 0.0f);
                vertices[vertexBaseIndex + 1] = CreateVertex(tr.X, tr.Y, 1.0f, 0.0f);
                vertices[vertexBaseIndex + 2] = CreateVertex(br.X, br.Y, 1.0f, 1.0f);
                vertices[vertexBaseIndex + 3] = CreateVertex(bl.X, bl.Y, 0.0f, 1.0f);

                var indexBaseIndex = i * 6;
                indices[indexBaseIndex + 0] = vertexBaseIndex + 0;
                indices[indexBaseIndex + 1] = vertexBaseIndex + 1;
                indices[indexBaseIndex + 2] = vertexBaseIndex + 2;
                indices[indexBaseIndex + 3] = vertexBaseIndex + 0;
                indices[indexBaseIndex + 4] = vertexBaseIndex + 2;
                indices[indexBaseIndex + 5] = vertexBaseIndex + 3;
            }
            
            GraphicsManagerExtensions.SDL_RenderGeometry(
                renderer, texturePtr, vertices, vertices.Length, indices, indices.Length
            );
        }
    }

    /// <summary>
    /// Disegna gli sprite 2D semplici (senza matrici) con il BLIT via buffer: legge la
    /// pagina, incolla i pixel ARGB di ogni sprite visibile alla sua posizione, riscrive.
    /// La rotazione non è supportata dal blit (si disegna non ruotato).
    /// </summary>
    private void DrawVisibleSprites2D()
    {
        if (_programContext is null || _visibleSpritesSorted.Count == 0) return;
        if (_screenWidth <= 0 || _screenHeight <= 0) return;

        // Con il driver REALE (Metal) RenderCopy è accelerato (GPU): il blit via buffer
        // era il collo di bottiglia (legge+riscrive 307200 px per frame → movimenti non
        // fluidi). RenderCopy texture→pagina NON funziona SOLO col renderer software
        // (dummy); col Metal sì.
        if (Environment.GetEnvironmentVariable("SDL_VIDEODRIVER") != "dummy")
        {
            var renderer = _programContext.RendererHandle;
            SetRenderTargetToActivePage();
            SDL.SDL_SetRenderDrawBlendMode(renderer, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
            // Scala HiDPI: pagina nativa (backing × _renderScale). La texture sprite è NATIVA
            // (caricata × scale in LoadSpriteInternal), TextureSize resta LOGICO: il
            // destRect va scalato per _renderScale così la texture nativa mappa 1:1 (nitido)
            // e le coordinate logiche raggiungono i bordi. Senza, su Retina gli sprite del
            // SYNC finivano compressi nella metà sinistra (Position 0..640 su pagina 0..1280).
            var rs = _renderScale > 0 ? _renderScale : 1.0f;
            foreach (var sprite in _visibleSpritesSorted)
            {
                var tw = sprite.TextureSize.Width;
                var th = sprite.TextureSize.Height;
                var scaleX = sprite.Scale.X;
                var scaleY = sprite.Scale.Y;
                var dw = (int)(tw * scaleX);
                var dh = (int)(th * scaleY);
                if (dw <= 0 || dh <= 0) continue;
                var destRect = new SDL.SDL_Rect
                {
                    x = (int)((sprite.Position.X - dw / 2f) * rs),
                    y = (int)((sprite.Position.Y - dh / 2f) * rs),
                    w = (int)(dw * rs),
                    h = (int)(dh * rs)
                };
                SDL.SDL_RenderCopyEx(renderer, sprite.Texture, IntPtr.Zero, ref destRect,
                    sprite.Rotation, IntPtr.Zero, SDL.SDL_RendererFlip.SDL_FLIP_NONE);
            }
            return;
        }
        var sw = _screenWidth; var sh = _screenHeight;
        uint[] page;
        try { page = ReadActivePageToBuffer(); }
        catch { return; }
        if (page.Length == 0) return;

        foreach (var sprite in _visibleSpritesSorted)
        {
            if (sprite.Pixels == null) continue;
            var w = sprite.TextureSize.Width;
            var h = sprite.TextureSize.Height;
            if (w <= 0 || h <= 0 || sprite.Pixels.Length < w * h) continue;
            // Posizione = centro dello sprite (coordinate LOGICHE). Il buffer è NATIVO
            // (backing × _renderScale): ogni pixel logico diventa un blocco scale×scale,
            // come DrawTextureInternal — senza, gli sprite del SYNC apparivano compressi
            // a metà scala in headless.
            var rs = _renderScale > 0 ? _renderScale : 1.0f;
            var sxN = (int)((sprite.Position.X - w / 2.0f) * rs);
            var syN = (int)((sprite.Position.Y - h / 2.0f) * rs);
            var scX = rs;
            var scYv = rs;
            for (var yy = 0; yy < h; yy++)
            {
                var dstY0 = syN + (int)(yy * scYv);
                var dstY1 = syN + (int)((yy + 1) * scYv);
                if (dstY1 <= dstY0) dstY1 = dstY0 + 1;
                if (dstY0 < 0 || dstY0 >= sh) continue;
                for (var xx = 0; xx < w; xx++)
                {
                    var px = sprite.Pixels[yy * w + xx];
                    if (((px >> 24) & 0xFF) == 0) continue;
                    var dstX0 = sxN + (int)(xx * scX);
                    var dstX1 = sxN + (int)((xx + 1) * scX);
                    if (dstX1 <= dstX0) dstX1 = dstX0 + 1;
                    for (var dy = dstY0; dy < dstY1 && dy < sh; dy++)
                    {
                        var bufY = sh - 1 - dy; // buffer flippato (come il testo)
                        if (bufY < 0 || bufY >= sh) continue;
                        for (var dx = dstX0; dx < dstX1 && dx < sw; dx++)
                        {
                            if (dx < 0 || dx >= sw) continue;
                            page[bufY * sw + dx] = px;
                        }
                    }
                }
            }
        }
        UpdateActivePageFromBuffer(page);
    }

    /// <summary>
    /// Ricalcola la bounding box di uno sprite a partire da Position, Scale e TextureSize.
    /// La bounding box è un SDL_Rect che rappresenta l'axis-aligned bounding box (AABB)
    /// dello sprite, usato per il collision detection.
    /// </summary>
    /// <param name="sprite">Lo sprite di cui aggiornare la bounding box.</param>
    private static void UpdateBoundingBox(Sprite sprite)
    {
        // Collisioni sulle REALI dimensioni visibili (contenuto opaco), non sulla texture piena.
        if (sprite.OpaqueBounds is { } ob)
        {
            var tw = sprite.TextureSize.Width;
            var th = sprite.TextureSize.Height;
            var cw = (ob.MaxX - ob.MinX + 1) * sprite.Scale.X;
            var ch = (ob.MaxY - ob.MinY + 1) * sprite.Scale.Y;
            // offset del centro del contenuto opaco rispetto al centro della texture
            var ox = ((ob.MinX + ob.MaxX + 1) / 2f - tw / 2f) * sprite.Scale.X;
            var oy = ((ob.MinY + ob.MaxY + 1) / 2f - th / 2f) * sprite.Scale.Y;
            sprite.BoundingBox = new SDL.SDL_Rect
            {
                x = (int)(sprite.Position.X + ox - cw / 2f),
                y = (int)(sprite.Position.Y + oy - ch / 2f),
                w = (int)cw,
                h = (int)ch
            };
            return;
        }
        var halfW = sprite.TextureSize.Width * sprite.Scale.X / 2.0f;
        var halfH = sprite.TextureSize.Height * sprite.Scale.Y / 2.0f;
        sprite.BoundingBox = new SDL.SDL_Rect
        {
            x = (int)(sprite.Position.X - halfW),
            y = (int)(sprite.Position.Y - halfH),
            w = (int)(halfW * 2),
            h = (int)(halfH * 2)
        };
    }

    /// <summary>
    /// SPRITECOLLIDE(id1, id2): collisione AABB tra due sprite. -1 = collisione, 0 = no
    /// (o sprite inesistente/nascosto). La bounding box è aggiornata ad ogni SYNC.
    /// </summary>
    public int SpriteCollide(int id1, int id2)
    {
        if (!_sprites.TryGetValue(id1, out var a) || !_sprites.TryGetValue(id2, out var b)) return 0;
        if (!a.IsVisible || !b.IsVisible) return 0;
        var ra = a.BoundingBox;
        var rb = b.BoundingBox;
        var hit = ra.x < rb.x + rb.w && ra.x + ra.w > rb.x &&
                  ra.y < rb.y + rb.h && ra.y + ra.h > rb.y;
        return hit ? -1 : 0;
    }

    /// <summary>
    /// COLLIDEBATCH: broad+narrow phase AABB su ARRAY di posizioni (non sul sistema sprite).
    /// Spatial grid (celle ~dimSprite): ogni sprite confrontato solo con le celle adiacenti →
    /// O(n) medio invece di O(n^2). Zero allocazioni per frame: le coppie sono raccolte in un
    /// buffer riusato e copiate nell'array di ritorno. SDL2/Vulkan non hanno collision detection
    /// (sono solo rendering) — è un problema CPU e la griglia è la soluzione ottimale per sprite
    /// di dimensione uniforme. Ritorna int[] flatten: [i0,j0, i1,j1, ...].
    /// </summary>
    public int[] CollideBatch(float[] xs, float[] ys, float w, float h)
    {
        var n = xs.Length;
        if (n < 2) return Array.Empty<int>();
        var halfW = w / 2f; var halfH = h / 2f;
        var cellSize = Math.Max(16f, w + 4f); // cella leggermente più grande dello sprite
        var cols = Math.Max(1, (int)(_screenWidth / cellSize));
        var rows = Math.Max(1, (int)(_screenHeight / cellSize));
        if (cols * rows > 4096) { cols = 64; rows = 64; }
        var bucketCount = cols * rows;
        // Riutilizza i buffer per evitare allocazioni per frame
        if (_collideBuckets == null || _collideBuckets.Length != bucketCount)
            _collideBuckets = new List<int>[bucketCount];
        else
            for (var i = 0; i < bucketCount; i++) _collideBuckets[i] = null;
        var cellOf = _collideCellOf;
        if (cellOf == null || cellOf.Length < n) { cellOf = new int[n]; _collideCellOf = cellOf; }

        // Broad phase: assegna ogni sprite alla sua cella (solo se dentro lo schermo)
        for (var i = 0; i < n; i++)
        {
            var cx = (int)(xs[i] / cellSize);
            var cy = (int)(ys[i] / cellSize);
            if (cx < 0 || cx >= cols || cy < 0 || cy >= rows) { cellOf[i] = -1; continue; }
            var ci = cy * cols + cx;
            cellOf[i] = ci;
            (_collideBuckets[ci] ??= new List<int>()).Add(i);
        }

        // Narrow phase: per ogni sprite, confronta con le celle adiacenti
        var pairBuffer = _collidePairBuffer;
        if (pairBuffer == null) { pairBuffer = new List<int>(256); _collidePairBuffer = pairBuffer; }
        else pairBuffer.Clear();
        var seen = _collideSeen;
        if (seen == null) { seen = new System.Collections.Generic.HashSet<(int, int)>(); _collideSeen = seen; }
        else seen.Clear();

        for (var i = 0; i < n; i++)
        {
            var ci = cellOf[i];
            if (ci < 0) continue;
            var ax = xs[i]; var ay = ys[i];
            var row = ci / cols; var col = ci % cols;
            for (var dr = -1; dr <= 1; dr++)
            for (var dc = -1; dc <= 1; dc++)
            {
                var rr = row + dr; var cc = col + dc;
                if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                var bucket = _collideBuckets[rr * cols + cc];
                if (bucket == null) continue;
                foreach (var j in bucket)
                {
                    if (j <= i) continue; // ogni coppia una sola volta
                    var k1 = i < j ? (i, j) : (j, i);
                    if (!seen.Add(k1)) continue;
                    var bx = xs[j]; var by = ys[j];
                    // AABB (sprite centrato su x,y)
                    if (ax + halfW >= bx - halfW && ax - halfW <= bx + halfW &&
                        ay + halfH >= by - halfH && ay - halfH <= by + halfH)
                    {
                        pairBuffer.Add(i);
                        pairBuffer.Add(j);
                    }
                }
            }
        }
        return pairBuffer.ToArray();
    }

    private List<int>?[]? _collideBuckets;
    private int[]? _collideCellOf;
    private List<int>? _collidePairBuffer;
    private System.Collections.Generic.HashSet<(int, int)>? _collideSeen;

    /// <summary>
    /// Restituisce i pixel ARGB di uno sprite CARICATO (SPRITE LOAD) nel formato array IMAGEGET:
    /// [width, height, indexPalette...] con **-1 per i pixel trasparenti** (alfa &lt; 128).
    /// Questo PRESERVA la trasparenza vera del PNG (a differenza di IMAGEGET da schermo, dove il
    /// SYNC ha già blendato il PNG sul colore di sfondo e la trasparenza è persa — lo sprite
    /// catturato aveva il RIQUADRO (colore di sfondo) invece di essere trasparente, coprendo gli
    /// altri sprite sotto).
    /// </summary>
    public int[] GetSpriteImagePixels(int spriteId)
    {
        if (!_sprites.TryGetValue(spriteId, out var sprite) || sprite.Pixels == null)
            return new int[] { 0, 0 };
        var w = sprite.TextureSize.Width;
        var h = sprite.TextureSize.Height;
        var a = new int[2 + w * h];
        a[0] = w;
        a[1] = h;
        for (var yy = 0; yy < h; yy++)
        for (var xx = 0; xx < w; xx++)
        {
            var px = sprite.Pixels[yy * w + xx];
            if (((px >> 24) & 0xFF) < 128) { a[2 + yy * w + xx] = -1; continue; } // trasparente
            a[2 + yy * w + xx] = FindColorIndex(px);
        }
        return a;
    }

    private int FindSpriteId(Sprite sprite)
    {
        // O(1): l'Id è salvato nello sprite alla registrazione. Prima era una ricerca lineare
        // su _sprites per OGNI coppia in collisione — con centinaia di sprite densi diventava
        // milioni di iterazioni per SYNC (frame rate crollato → lampeggio nel detect).
        return sprite.Id;
    }

    /// <summary>
    /// Ricalcola la bounding box di uno sprite dato il suo ID.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite.</param>
    private void UpdateBoundingBoxInternal(int spriteId)
    {
        if (_sprites.TryGetValue(spriteId, out var sprite))
            UpdateBoundingBox(sprite);
    }

    /// <summary>
    /// Verifica la collisione AABB tra due sprite.
    /// Aggiorna le bounding box di entrambi prima del check.
    /// </summary>
    /// <param name="id1">L'ID del primo sprite.</param>
    /// <param name="id2">L'ID del secondo sprite.</param>
    /// <returns>True se i due sprite collidono (AABB), false altrimenti.</returns>
    /// <summary>Esposizione sincrona (non-GPM) per la VM: collisione AABB tra due sprite.
    /// Senza SCREEN attivo o sprite mancanti restituisce false (come il sample atteso).</summary>
    public bool Collide(int id1, int id2) => CheckCollisionInternal(id1, id2);

    /// <summary>Esposizione sincrona (non-GPM): ID degli sprite visibili in collisione con id.
    /// Restituisce un array vuoto se lo sprite non esiste o senza SCREEN attivo.</summary>
    public int[] CollideAll(int id) => CheckCollisionAllInternal(id);

    private bool CheckCollisionInternal(int id1, int id2)
    {
        if (!_sprites.TryGetValue(id1, out var s1) || !_sprites.TryGetValue(id2, out var s2))
            return false;
        UpdateBoundingBox(s1);
        UpdateBoundingBox(s2);

        var bb1 = s1.BoundingBox;
        var bb2 = s2.BoundingBox;
        return SDL.SDL_HasIntersection(ref bb1, ref bb2) == SDL.SDL_bool.SDL_TRUE;
    }

    /// <summary>
    /// Verifica la collisione AABB di uno sprite con tutti gli altri sprite visibili.
    /// </summary>
    /// <param name="id">L'ID dello sprite da testare.</param>
    /// <returns>Un array di ID degli sprite che collidono con quello dato.</returns>
    private int[] CheckCollisionAllInternal(int id)
    {
        if (!_sprites.TryGetValue(id, out var source))
            return Array.Empty<int>();

        UpdateBoundingBox(source);
        var sourceBB = source.BoundingBox;

        var hits = new List<int>();
        foreach (var (otherId, other) in _sprites)
        {
            if (otherId == id || !other.IsVisible) continue;
            UpdateBoundingBox(other);
            var otherBB = other.BoundingBox;
            if (SDL.SDL_HasIntersection(ref sourceBB, ref otherBB) == SDL.SDL_bool.SDL_TRUE)
                hits.Add(otherId);
        }

        return hits.ToArray();
    }

    /// <summary>
    /// Funzione helper per convertire una matrice MathNet.Numerics.LinearAlgebra.Matrix&lt;double&gt;
    /// in System.Numerics.Matrix4x4.
    /// </summary>
    /// <param name="m">La matrice MathNet da convertire.</param>
    /// <returns>La matrice System.Numerics.Matrix4x4 risultante.</returns>
    /// <exception cref="ArgumentException">Se la matrice non è 4x4.</exception>
    private static Matrix4x4 ToSystemNumericsMatrix(Matrix<double> m)
    {
        if (m.RowCount != 4 || m.ColumnCount != 4)
            throw new ArgumentException("La matrice deve essere 4x4 per la conversione.");
        
        return new Matrix4x4(
            (float)m[0,0], (float)m[0,1], (float)m[0,2], (float)m[0,3],
            (float)m[1,0], (float)m[1,1], (float)m[1,2], (float)m[1,3],
            (float)m[2,0], (float)m[2,1], (float)m[2,2], (float)m[2,3],
            (float)m[3,0], (float)m[3,1], (float)m[3,2], (float)m[3,3]
        );
    }
}