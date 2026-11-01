// FILE: Basic.Runtime.IServices.IGraphicsService.cs

namespace Basic.Runtime;

/// <summary>Una cella TUI per il rendering su grafica (carattere + colori ARGB 0xAARRGGBB).</summary>
public readonly record struct TuiCell(char Character, uint FgArgb, uint BgArgb)
{
    public static readonly TuiCell Empty = new(' ', 0xFFC8D0DC, 0xFF11141D);
}

/// <summary>Servizio grafico per la P-CODE VM. In headless, NullGraphicsService.</summary>
public interface IGraphicsService
{
    void Screen(int mode, int width, int height, int bpp);

    // ── Controlli pixel + form (gambe gestite; default no-op) ──
    void FormReset() { }
    void FormAddControl(string kind, object[] args, string name) { }
    void FormDispatchMouse(int type, int x, int y, int button, int wheel) { }
    void FormDispatchKey(string key) { }
    // ── Controlli utente (UserControl, stile VB6): istanza di classe + Paint ──
    // container: nome del contenitore (window/tab/panel/usercontrol); "" = contenitore corrente.
    void UserControlAdd(object? instance, int x, int y, int w, int h, string container = "", string name = "") { }
    void UserControlRemove(object? instance) { }
    void UserControlDraw(int h, int op, double a, double b, double c, double d, string s) { }
    void FormFrame(int frameMs) { }
    void UiFrame(int frameMs) { }
    void UiAddCheckbox(int x, int y, int w, string label) { }
    double UiLastHandle() => -1;
    void UiControlApplyProps(int handle, string? name, string? tag, string? tooltip, double? tabindex, double? visible, double? enabled, double? tabstop) { }
    void UiControlApplyStyle(int handle, string? spec) { }
    void UiControlApplyFocus(int handle) { }
    void UiAddTbrowse(int row, int col, int w, int h, string colsCsv, string dataCsv) { }
    void UiAddTextInput(int x, int y, int w, string initial) { }
    int UiTbrowseSelRow(int i) => -1;
    int UiControlValue(int i) => -1;
    void UiCodeAreaError(int i, int line, bool on) { }
    string UiControlText(int i) => "";
    bool UiClosed() => false;
    void UiResetClose() { }
    int UiPollKey() => 0;
    void UiControlsReset() { }
    bool UiPollEvent(out int ev, out int widx) { ev = 0; widx = 0; return false; }
    void UiAddFiledialog(string title, string mask) { }
    void UiAddPrinterdialog(string title) { }
    void UiAddTreeview(int x, int y, int w, int h, string csv) { }
    void UiAddGrid(int x,int y,int w,int h,int cols,string csv) { }
    void UiAddDatepicker(int x,int y,int w) { }
    void UiAddColorpicker(int x,int y,int w,int h) { }
    void UiAddListbox(int x,int y,int w,int h,string csv) { }
    void UiAddCombobox(int x,int y,int w,string csv) { }
    void UiAddRadiogroup(int x,int y,int w,int h,string csv) { }
    void UiAddToggle(int x,int y,int w,string caption) { }
    void UiAddSlider(int x,int y,int w,int mn,int mx,int val) { }
    void UiAddNumericUpDown(int x,int y,int w,int mn,int mx,int val) { }
    void UiAddStarRating(int x,int y,int w,int mx,int val) { }
    void UiAddScrollBar(int x,int y,int len,int mn,int mx,int val) { }
    void UiAddScrollView(int x,int y,int w,int h,string csv) { }
    void UiAddLabel(int x,int y,int w,string text) { }
    void UiAddDataList(int x,int y,int w,int h,string csv) { }
    void UiAddPropertyGrid(int x,int y,int w,int h,string csv) { }
    void UiAddSplitter(int x,int y,int w,int h,int orient) { }
    void UiAddContextMenu(int x,int y,int w,int h,string csv) { }
    void UiAddLogView(int x,int y,int w,int h,string csv) { }
    void UiAddHexView(int x,int y,int w,int h,string data) { }
    void UiAddBigText(int x,int y,string text) { }
    void UiAddCodeArea(int x,int y,int w,int h,string csv) { }
    void UiAddReorderListBox(int x,int y,int w,int h,string csv) { }
    void UiAddAlertBox(int x,int y,int w,int h,string text) { }
    void UiAddConfirmationBox(int x,int y,int w,int h,string text) { }
    void UiAddFileExplorer(int x,int y,int w,int h,string path) { }
    void UiAddDashboard(int x,int y,int w,int h,string csv) { }
    void UiAddSpinner(int x,int y,int w,string preset) { }
    void UiAddStatusbar(int x,int y,int w,string text) { }
    void UiAddTab(int x,int y,int w,string csv) { }
    void UiAddMenu(int x,int y,int w,string csv) { }
    bool FormClosed() => true;
    void FormClose() { }
    void FormSetLastStyle(string spec) { }
    double FormControlGet(string name, string prop) => 0;
    string FormControlGetStr(string name, string prop) => "";
    void FormControlSet(string name, string prop, double num, string str) { }
    /// <summary>DTOC(oa) via runtime C (bridge P/Invoke, canon IL/PCode/VM-C).</summary>
    string Dtoc(double oa) => "";
    /// <summary>Applica un PICTURE (format) a un valore via runtime C.</summary>
    string PictureApply(object? value, string picture) => value?.ToString() ?? "";
    /// <summary>Printer virtuale Clipper via runtime C (stato condiviso).</summary>
    void ClipperPrinterWrite(string s, bool newLine) { }
    bool ClipperPrinterOnly() => false;
    bool ClipperPrinterRouting() => false;
    int ClipperPrinterProw() => 0;
    int ClipperPrinterPcol() => 0;
    void ClipperPrinterEject() { }
    string ClipperPrinterText() => "";
    void ClipperSetPrinter(bool on) { }
    void ClipperSetDevice(string device) { }
    void ClipperSetWrap(bool on) { }
    void ClipperSetEscape(bool on) { }
    void ClipperSetIntensity(bool on) { }
    void ClipperSetEpoch(int year) { }
    int ClipperEpoch() => 1900;
    void ClipperSetThemeName(string name) { }
    string ClipperThemeName() => "Turbo";
    void ClipperSetFgBg(CommonLib.Screens.CellColor fg, CommonLib.Screens.CellColor bg) { }
    CommonLib.Screens.CellColor ClipperFg() => CommonLib.Screens.CellColor.White;
    CommonLib.Screens.CellColor ClipperBg() => CommonLib.Screens.CellColor.Black;
    void ClipperSetDecimals(int n) { }
    void ClipperSetFixed(bool on) { }
    void ClipperSetTypeahead(int size) { }
    void ClipperSetDelimiters(string v) { }
    void ClipperSetDateFormat(string fmt) { }
    void ClipperSetMessage(int row) { }
    string ClipperFormatNumber(double d) => d.ToString(System.Globalization.CultureInfo.InvariantCulture);
    // ---- ClipperMouse (stato C) — default no-op (headless) ----
    int ClipperMouseMRow() => 0;
    int ClipperMouseMCol() => 0;
    int ClipperMouseIsMouse() => 0;
    int ClipperMouseMLeft() => 0;
    int ClipperMouseMRight() => 0;
    int ClipperMouseMMiddle() => 0;
    int ClipperMouseMLeftPress() => 0;
    int ClipperMouseMRightPress() => 0;
    int ClipperMouseMMiddlePress() => 0;
    int ClipperMouseMLeftLess() => 0;
    int ClipperMouseMRightLess() => 0;
    int ClipperMouseMMiddleLess() => 0;
    int ClipperMouseMLeftShift() => 0;
    int ClipperMouseMRightShift() => 0;
    int ClipperMouseMMiddleShift() => 0;
    int ClipperMouseMLeftControl() => 0;
    int ClipperMouseMRightControl() => 0;
    int ClipperMouseMMiddleControl() => 0;
    int ClipperMouseMLeftM() => 0;
    int ClipperMouseMRightM() => 0;
    int ClipperMouseMMiddleM() => 0;
    int ClipperMouseX() => 0;
    int ClipperMouseY() => 0;
    int ClipperMouseButton(int hasId, int buttonId) => 0;
    int ClipperMouseWheel() => 0;
    int ClipperMouseClick() => 0;
    int ClipperMouseClickArea(int x1, int y1, int x2, int y2) => 0;
    int ClipperMouseDblClick() => 0;
    int ClipperMouseMove() => 0;
    int ClipperMousePresent() => 0;
    int ClipperMouseLeft() => 0;
    int ClipperMouseRight() => 0;
    int ClipperMouseMiddle() => 0;
    int ClipperMouseMouStat(int button, out int row, out int col, out int mods) { row = 0; col = 0; mods = 0; return 0; }
    string ClipperMouseRelease() => "";
    void ClipperMouseUpdateEvent(int kind, int x, int y) { }
    void FormControlMove(string name, int x, int y, int w, int h) { }
    void FormControlFocus(string name) { }
    void FormControlZorder(string name, int front) { }
    void FormControlInvalidate(string name) { }
    void FormControlRefresh(string name) { }
    int UserControlLastHandle() { return -1; }
    double FormLastHandle() => -1;
    double FormClicked(int handle) => 0;
    double FormDblClicked(int handle) => 0;
    double FormChanged(int handle) => 0;
    double FormGotFocused(int handle) => 0;
    double FormLostFocused(int handle) => 0;
    double MouseClicked() => 0;
    double MouseDblClicked() => 0;
    double MousePressed() => 0;
    double KeyPressed() => 0;
    int FormEventIndex(string name) => 0;
    void FormRegisterEventThunk(int handle, int ev) { }
    void FormSetControlEventSink(System.Action<int, int> sink) { }
    void FormUnregisterEventThunk(int handle, int ev) { }
    void FormFireEvent(int handle, int ev) { }
    void FormSetLastEvent(int handle, int ev) { }

    /// <summary>Seleziona la modalita' console (TEXTMODE n): 0=%SDL, 1=%C64, 2=%ANSI, 3=%SYSTEM.</summary>
    void TextMode(int mode);
    void PSet(int x, int y, int color);
    void Preset(int x, int y);
    int Point(int x, int y);
    void Line(int x1, int y1, int x2, int y2, int color, string? style);    void Circle(int x, int y, int radius, int color);
    void Paint(int x, int y, int fillColor, int borderColor);
    void Draw(string command);
    void PCopy(int srcPage, int destPage);
    void ScreenSet(int width, int height, int bpp);
    void Font(string name, int size);
    /// <summary>IMAGEGET (x1,y1)-(x2,y2) → int[] [w, h, indice_palette...] (schema PCodeVM
    /// IMAGEGET; prima byte[] vestigiale ignorato dal servizio).</summary>
    int[] ImageGet(int x1, int y1, int x2, int y2);
    /// <summary>IMAGEPUT (x, y), img ([w,h,indice_palette...]), op — indice &lt;0 trasparente.</summary>
    void ImagePut(int x, int y, int[] img, int op);
    void ImagePuts(int[] spriteData, int[] positions, int op);
    void Palette(int index, int r, int g, int b);
    void SaveScreen(string path);
    void RestoreScreen(string path);
    void ViewPort(int x1, int y1, int x2, int y2);
    void Window(int x1, int y1, int x2, int y2);
    void ClearRect(int x1, int y1, int x2, int y2);
    void DrawTexture(int x, int y, int textureId);
    /// <summary>DRAW TEXTURE con rotazione/scala (parità gmo DRAW_TEXTURE_GPM).</summary>
    void DrawTextureEx(int textureId, int x, int y, double rotation, double scaleX, double scaleY, int color);
    int LoadTexture(string path, int textureId);
    // GRAPHICS.BSAVE/BLOAD (IMAGEGET/IMAGEPUT): salva l'area (x1,y1)-(x2,y2) in un buffer
    // interno e la ripristina a (x,y). Il buffer BASIC passato dal campione è ignorato
    // (il byte[] come argomento object generava IL invalido nel backend).
    void BSave(int x1, int y1, int x2, int y2);
    void BLoad(int x, int y);
    void DrawTextures(int x, int y, int textureId, int frame);
    void Polygon(float[] points, int color);
    void Sync();
    bool IsAvailable { get; }
    void SetAttribute(int attr);
    void CursorState(bool visible);

    /// <summary>WINDOW SHOW/HIDE: mostra/nasconde la finestra dello screen grafico.
    /// Default no-op; lo implementa il servizio SDL (CGfx).</summary>
    void WindowVisible(bool visible) { }
    void CursorShape(int shape);
    void CPrint(string text);
    void CPrintNewLine();
    void CLocate(int line, int col);
    void CCls();
    void CSaveScreen(string path);
    void CRestoreScreen(string path);
    // PRINT BASIC in modalità grafica: disegna il testo sulla pagina alla posizione del
    // cursore grafico (text == null → newline grafico). CPrint, al contrario, FORZA la
    // console anche con schermo grafico attivo (semantica Clipper CPrint).
    void Print(string? text);
    // LOCATE BASIC con schermo grafico attivo: imposta il cursore del testo grafico
    // (LOCATE_GPM col, line senza -1, come la PCodeVM di gmo).
    void Locate(int col, int line);
    // COLOR BASIC con schermo grafico attivo: imposta colore primo piano/sfondo del testo
    // grafico. Numerico = indice palette; RGBA (IEnumerable double[]) = colore RGB completo.
    void Color(object? foreground, object? background);
    // PIXELBUFFER(w,h,array): crea una texture dal buffer di pixel (double[] packati
    // 0x00RRGGBB, row-major) e la blitta a schermo intero — gradiente in UN colpo solo.
    void DrawPixelBuffer(int width, int height, object? buffer);
    // PALETTE index, r, g, b (grafico): ridefinisce il colore della palette attiva.
    void SetPaletteColor(int index, int r, int g, int b);
    // CLS grafico: cancella la pagina attiva con il colore indicato (indice palette o null).
    void Clear(int? color);
    // DRAW TEXT "testo", x, y [, ALIGN_*]: testo grafico alla posizione data, col colore
    // corrente. align: 0=Left, 1=Center, 2=Right.
    void DrawText(string text, int x, int y, double align);
    // TEXTWIDTH(testo): larghezza in pixel LOGICI del testo col font corrente (per
    // centrare/impaginare, es. `(640 - TEXTWIDTH(t)) / 2`).
    double TextWidth(string text);
    // TEXTHEIGHT(): altezza in pixel del font corrente.
    double TextHeight();

    // TAB(col) e zona tab (',') nel PRINT grafico: spostano il cursore del testo grafico
    // (schema PCodeVM TAB_GPM/TABZONE). GraphicsTabZone avanza alla zona successiva.
    void GraphicsTab(int col);
    void GraphicsTabZone();

    // SPRITEDEF id, righe...: crea uno sprite da pixel (indici palette, 0 = trasparente).
    void SpriteDef(int spriteId, int width, int height, int[] pixels);
    // Collisioni sprite (AABB). Prima il backend IL le risolveva via riflessione su
    // GraphicsManager (Type.GetType("CommonGraphics...")) — mai reflection per la
    // grafica: sono metodi dell'interfaccia, iniettati dal runner (SdlGraphicsService).
    double SpriteCollide(int id1, int id2);
    object[] SpriteCollideAll(int id);
    object[] SpriteCollideBatch(float[] xs, float[] ys, float w, float h);
    // Comandi sprite (SPRITE LOAD / SPRITE SET POSITION / SPRITE ON / SPRITE OFF /
    // SPRITE PRIORITY): il SYNC (Sync) disegna gli sprite registrati dal motore. Il
    // backend IL li emette qui; in gmo i corrispondenti *_GPM sono su GraphicsManager.
    int SpriteLoad(string filePath, int spriteId);
    void SpriteSetPosition(int spriteId, float x, float y);
    void SpriteSetVisible(int spriteId, bool visible);
    void SpriteSetPriority(int spriteId, int priority);
    // SPRITE SET SCALE id, sx, sy / SPRITE SET ROTATION id, gradi (parità gmb_gfx_sprite_set_scale/rotation).
    void SpriteSetScale(int spriteId, float sx, float sy);
    void SpriteSetRotation(int spriteId, float degrees);
    // Enqueue GPM (coda SDL, callback sul main thread). Il backend IL parcheggia il
    // PCodeTask; VIETATO attendere con GetAwaiter/Wait/Thread.Sleep.
    void SpriteLoadGpm(string filePath, Action<object?, Exception?> completion);
    void LoadTextureGpm(string path, Action<object?, Exception?> completion);
    /// <summary>LOAD TEXTURE path INTO array CELL(w,h): callback con int[] di id (vuoto se fallisce).</summary>
    void LoadTextureSheetGpm(string path, int cellW, int cellH, Action<object?, Exception?> completion);
    void SyncGpm(Action<object?, Exception?> completion);
    void PointGpm(int x, int y, Action<object?, Exception?> completion);
    void ScreenGpm(int mode, int width, int height, int bpp, Action<object?, Exception?> completion);
    void SpriteGetPixelsGpm(int spriteId, Action<object?, Exception?> completion);
    void SpriteSetTransform(int spriteId, long matrixId);
    void SpriteSetMatrix(int spriteId, long matrixId);
    SpriteCollisionHit[] TakeLastCollisions();

    // ─── SPRITE GETPIXELS (sincrono, schema PCodeVM SPRITE_GETPIXELS) ───
    // Ritorna i pixel dello sprite come array [w, h, indice_palette...] (row-major).
    int[] SpriteGetPixels(int spriteId);

    // ─── TILEMAP (schema PCodeVM TILEMAP_* → CommonGraphics.TilemapManager) ───
    void TilemapLoad(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles);
    void TilemapSet(int row, int col, int tileIndex);
    void TilemapSetOffset(float x, float y);
    void TilemapScroll(float dx, float dy);
    void TilemapDraw();

    // ─── CHART (schema PCodeVM CHART_* → CommonGraphics.ChartPlotManager) ───
    long ChartCreate(string title, string xLabel, string yLabel);
    void ChartPlot(long handle, double[] xs, double[] ys, string plotType, string legend);
    /// <summary>CHART FINANCIAL: serie OHLC. `ohlc` è piatto, 5 valori per candela
    /// (timestamp, open, high, low, close). Default no-op (il percorso reale è CGfx).</summary>
    void ChartFinancial(long handle, double[] ohlc, string plotType, string legend) { }
    /// <summary>CHART SET/GET/GETS: configurazione del grafico da BASIC (rendering, view window
    /// per zoom/pan, colori, titolo). Default no-op / 0 / "".</summary>
    void ChartConfigSet(int handle, string prop, double num, string str) { }
    double ChartConfigGet(int handle, string prop) => 0;
    string ChartConfigGetStr(int handle, string prop) => "";
    /// <summary>CHART DRAW: oggetti di disegno (line/hline/vline/rect/fib/move/remove/clear/
    /// count/hit). `args` in coordinate dati. Ritorna l'id del nuovo oggetto (o count/hit).
    /// Default no-op (il percorso reale è CGfx).</summary>
    double ChartDraw(int handle, string op, double[] args) => 0;
    void ChartShow(long handle, int x, int y);
    void ChartClear(long handle);
    void ChartClose(long handle);
    void ChartSave(long handle, string path);

    // ─── TUI WINDOWS su grafica (Fase W3) ───
    // Renderizza un buffer TUI (griglia di celle con fg/bg ARGB) sulla pagina grafica.
    // Il WindowManager TUI (backend IL/PCode) produce le celle; il servizio le disegna
    // cella per cella col font corrente. `w`/`h` sono dimensioni in celle.
    void RenderTui(TuiCell[] cells, int w, int h);
}

/// <summary>Hit AABB dell'ultimo SYNC (dispatch nel worker, mai nella callback SDL).</summary>
public readonly record struct SpriteCollisionHit(int Id1, int Id2, float X, float Y);
