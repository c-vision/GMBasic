// FILE: Basic.Runtime.Services.NullGraphicsService.cs

namespace Basic.Runtime;

public class NullGraphicsService : IGraphicsService
{
    public virtual bool IsAvailable => false;
    public void Screen(int mode, int width, int height, int bpp) { }
    public void TextMode(int mode) { }
    public void PSet(int x, int y, int color) { }
    public void Preset(int x, int y) { }
    public int Point(int x, int y) => 0;
    public void Line(int x1, int y1, int x2, int y2, int color, string? style) { }
    public void Circle(int x, int y, int radius, int color) { }
    public void Paint(int x, int y, int fillColor, int borderColor) { }
    public void Draw(string command) { }
    public void PCopy(int srcPage, int destPage) { }
    public void ScreenSet(int width, int height, int bpp) { }
    public void Font(string name, int size) { }
    public int[] ImageGet(int x1, int y1, int x2, int y2) => [0, 0];
    public void ImagePut(int x, int y, int[] img, int op) { }
    public void ImagePuts(int[] spriteData, int[] positions, int op) { }
    public void Palette(int index, int r, int g, int b) { }
    public void SaveScreen(string path) { }
    public void RestoreScreen(string path) { }
    public void ViewPort(int x1, int y1, int x2, int y2) { }
    public void Window(int x1, int y1, int x2, int y2) { }
    public void BSave(string path, int offset, int size) { }
    public void BLoad(string path, int offset) { }
    public void ClearRect(int x1, int y1, int x2, int y2) { }
    public void DrawTexture(int x, int y, int textureId) { }
    public void DrawTextureEx(int textureId, int x, int y, double rotation, double scaleX, double scaleY, int color) { }
    public int LoadTexture(string path, int textureId) => 0;
    public void DrawTextures(int x, int y, int textureId, int frame) { }
    public void Polygon(float[] points, int color) { }
    public void Sync() { NullDetectCollisions(); }
    public void SetAttribute(int attr) { }
    public void CursorState(bool visible) { }
    public void CursorShape(int shape) { }
    /* CPRINT = console-only: in headless/IL scrive su stdout (come PRINT, e come
     * SdlGraphicsService) — prima era no-op e la gamba IL perdeva il CPRINT (divergenza). */
    public void CPrint(string text) => System.Console.Write(text);
    public void CPrintNewLine() => System.Console.WriteLine();
    public void CLocate(int line, int col) { }
    public void CCls() { }
    public void CSaveScreen(string path) { }
    public void CRestoreScreen(string path) { }
    /* COLLIDE(id1,id2) / COLLIDE(id) / COLLIDEBATCH: NIENTE stub. Il modello software qui
     * sotto ha già rettangoli veri (dimensioni PNG × posizione × visibilità) e le collisioni
     * al SYNC le calcola da sempre; questi tre erano `=> 0` / `=> vuoto`, quindi su IL/PCode
     * headless `COLLIDE()` rispondeva SEMPRE 0 (mentre C99/VM-C collidevano) — bastava un
     * demo con due sprite sovrapposti per vederlo. Stessa AABB del SYNC e dello Sprites
     * gestito: tocco se a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y. */
    private bool NullOverlap((float X, float Y, int W, int H, bool Vis) a, (float X, float Y, int W, int H, bool Vis) b)
        => a.X < b.X + b.W && a.X + a.W > b.X && a.Y < b.Y + b.H && a.Y + a.H > b.Y;
    public double SpriteCollide(int id1, int id2)
    {
        if (!_nullSprites.TryGetValue(id1, out var a) || !_nullSprites.TryGetValue(id2, out var b)) return 0;
        if (!a.Vis || !b.Vis) return 0;          /* sprite spento (SPRITE OFF) non collide */
        return NullOverlap(a, b) ? -1 : 0;
    }
    public object[] SpriteCollideAll(int id)
    {
        var hits = new List<object>();
        if (!_nullSprites.TryGetValue(id, out var a) || !a.Vis) return hits.ToArray();
        foreach (var kv in _nullSprites)
        {
            if (kv.Key == id || !kv.Value.Vis) continue;
            if (NullOverlap(a, kv.Value)) hits.Add(kv.Key);   /* stessi elementi di Sdl/Sprites (id) */
        }
        return hits.ToArray();
    }
    public object[] SpriteCollideBatch(float[] xs, float[] ys, float w, float h)
    {
        var n = Math.Min(xs?.Length ?? 0, ys?.Length ?? 0);
        var pairs = new List<object>();
        if (n < 2) return pairs.ToArray();
        var halfW = w / 2f; var halfH = h / 2f;
        for (var i = 0; i < n; i++)
            for (var j = i + 1; j < n; j++)
            {
                float ax = xs![i], ay = ys![i], bx = xs[j], by = ys[j];
                if (ax + halfW >= bx - halfW && ax - halfW <= bx + halfW &&
                    ay + halfH >= by - halfH && ay - halfH <= by + halfH)
                {
                    pairs.Add(i); pairs.Add(j);   /* coppie piatte [i0,j0,i1,j1,…] come CollideBatch */
                }
            }
        return pairs.ToArray();
    }
    // Modello software minimo degli sprite (headless, senza SDL): id progressivi, rettangolo = dimensioni del PNG,
    // posizione e visibilita'. Serve solo a rilevare le collisioni al SYNC (ON SPRITE COLLISION), come la C runtime
    // headless (gmb_graphics_sprite.c) e GraphicsManager.DetectCollisions.
    private readonly System.Collections.Generic.SortedDictionary<int, (float X, float Y, int W, int H, bool Vis)> _nullSprites = new();
    private int _nullNextSprite = 1;
    private readonly System.Collections.Generic.List<SpriteCollisionHit> _nullHits = new();
    private int NullSpriteAdd(string filePath)
    {
        int w = 1, h = 1;
        try
        {
            using var fs = System.IO.File.OpenRead(filePath);
            var hdr = new byte[24];
            if (fs.Read(hdr, 0, 24) == 24 && hdr[1] == (byte)'P' && hdr[2] == (byte)'N' && hdr[3] == (byte)'G')
            {
                w = (hdr[16] << 24) | (hdr[17] << 16) | (hdr[18] << 8) | hdr[19];
                h = (hdr[20] << 24) | (hdr[21] << 16) | (hdr[22] << 8) | hdr[23];
            }
        }
        catch { }
        var id = _nullNextSprite++;
        _nullSprites[id] = (0, 0, w, h, true);
        return id;
    }
    private void NullDetectCollisions()
    {
        _nullHits.Clear();
        var ids = _nullSprites.Keys.ToList();
        for (var i = 0; i < ids.Count; i++)
        {
            var a = _nullSprites[ids[i]];
            if (!a.Vis) continue;
            for (var j = i + 1; j < ids.Count; j++)
            {
                var b = _nullSprites[ids[j]];
                if (!b.Vis) continue;
                /* Canone del motore (gmb_graphics.c / GraphicsManager.DetectCollisions):
                 * (X,Y) è il CENTRO dello sprite, non il vertice alto-sinistro. Prima qui si
                 * usava X come top-left → punto di collisione (X + W/2) = 12 con sprite 4x4
                 * a 10, mentre PCode/VM-C/C99 davano 10 (posizione-centro). */
                float ax = a.X - a.W / 2f, ay = a.Y - a.H / 2f;
                float bx = b.X - b.W / 2f, by = b.Y - b.H / 2f;
                if (ax < bx + b.W && ax + a.W > bx && ay < by + b.H && ay + a.H > by)
                    _nullHits.Add(new SpriteCollisionHit(ids[i], ids[j], (a.X + b.X) / 2f, (a.Y + b.Y) / 2f));
            }
        }
    }
    public int SpriteLoad(string filePath, int spriteId) => NullSpriteAdd(filePath);
    public void SpriteSetPosition(int spriteId, float x, float y)
    { if (_nullSprites.TryGetValue(spriteId, out var s)) _nullSprites[spriteId] = (x, y, s.W, s.H, s.Vis); }
    public void SpriteSetVisible(int spriteId, bool visible)
    { if (_nullSprites.TryGetValue(spriteId, out var s)) _nullSprites[spriteId] = (s.X, s.Y, s.W, s.H, visible); }
    public void SpriteSetPriority(int spriteId, int priority) { }
    public void SpriteSetScale(int spriteId, float sx, float sy) { }
    public void SpriteSetRotation(int spriteId, float degrees) { }
    public void SpriteLoadGpm(string filePath, Action<object?, Exception?> completion) => completion(NullSpriteAdd(filePath), null);
    public void LoadTextureGpm(string path, Action<object?, Exception?> completion) => completion(1, null);
    public void LoadTextureSheetGpm(string path, int cellW, int cellH, Action<object?, Exception?> completion)
        => completion(Array.Empty<int>(), null);
    public void SyncGpm(Action<object?, Exception?> completion) { NullDetectCollisions(); completion(null, null); }
    public void PointGpm(int x, int y, Action<object?, Exception?> completion) => completion(0, null);
    public void ScreenGpm(int mode, int width, int height, int bpp, Action<object?, Exception?> completion) => completion(null, null);
    // GETPIXELS headless: dummy VALIDO — [width, height, pixel...] di uno sprite 1x1 bianco
    // (px(1) = height = 1 > 0, UBOUND = 2). Con [0,0] i sample grafici GPM fallivano il check
    // `px(1) <= 0` (il Null non carica sprites reali ma deve permettere il percorso).
    public void SpriteGetPixelsGpm(int spriteId, Action<object?, Exception?> completion)
        => completion(new int[] { 1, 1, 0x00FFFFFF }, null);
    public void SpriteSetTransform(int spriteId, long matrixId) { }
    public void SpriteSetMatrix(int spriteId, long matrixId) { }
    public SpriteCollisionHit[] TakeLastCollisions() { var r = _nullHits.ToArray(); _nullHits.Clear(); return r; }
    public void Print(string? text) { }
    public void Locate(int col, int line) { }
    public void Color(object? foreground, object? background) { }
    public void DrawPixelBuffer(int width, int height, object? buffer) { }
    public void Clear(int? color) { }
    public void BSave(int x1, int y1, int x2, int y2) { }
    public void BLoad(int x, int y) { }
    public void SetPaletteColor(int index, int r, int g, int b) { }
    public void DrawText(string text, int x, int y, double align) { }
    public double TextWidth(string text) => (text?.Length ?? 0) * 8;
    public double TextHeight() => 16;
    public void GraphicsTab(int col) { }
    public void GraphicsTabZone() { }
    public void SpriteDef(int spriteId, int width, int height, int[] pixels) { }
    public int[] SpriteGetPixels(int spriteId) => [0, 0];
    public void TilemapLoad(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles) { }
    public void TilemapSet(int row, int col, int tileIndex) { }
    public void TilemapSetOffset(float x, float y) { }
    public void TilemapScroll(float dx, float dy) { }
    public void TilemapDraw() { }
    // Chart → runtime C (registry headless-safe): create/plot/config reali anche sotto
    // NullGraphicsService (parity/test), come i controlli TUI qui sotto.
    public long ChartCreate(string title, string xLabel, string yLabel) => Tui.ChartCreate(title, xLabel, yLabel);
    public void ChartPlot(long handle, double[] xs, double[] ys, string plotType, string legend) => Tui.ChartPlot(handle, xs, ys, plotType, legend);
    public void ChartShow(long handle, int x, int y) => Tui.ChartShow(handle, x, y);
    public void ChartClear(long handle) => Tui.ChartClear(handle);
    public void ChartClose(long handle) => Tui.ChartClose(handle);
    public void ChartSave(long handle, string path) => Tui.ChartSave(handle, path);
    public void RenderTui(TuiCell[] cells, int w, int h) { }

    // --- TUI → runtime C anche headless: il registry C e' headless-safe, cosi' ID/handle/eventi
    // dei controlli TUI sono reali anche sotto NullGraphicsService (parity/test). ---
    private static readonly CGfxGraphicsService Tui = new();
    public void UiFrame(int frameMs) => Tui.UiFrame(frameMs);
    /* WINDOWCLOSED/FORM CLOSED in headless: nessuna finestra → NON chiusa (0), canone del
     * runtime C (`gmb_form_closed()` = flag interno, falso senza form). Il default
     * dell'interfaccia è `true` e la gamba IL rispondeva 1 dove IL/PCode/VM-C/C99 danno 0
     * (windows/windows_all: wclosed= 1 vs 0). */
    public bool FormClosed() => false;
    public void UiAddCheckbox(int x, int y, int w, string label) => Tui.UiAddCheckbox(x, y, w, label);
    public void FormReset() => Tui.FormReset();
    /// <summary>Clausole del controllo creato (NAME/TAG/…/STYLE e LAYOUT EXPAND/ANCHOR/ALIGN):
    /// in headless il runtime C le applica al registry reale, così le gambe gestite hanno la
    /// stessa semantica di C99Native/VM-C (senza questo la spec andava persa nel default no-op).</summary>
    public void FormSetLastStyle(string spec) => Tui.FormSetLastStyle(spec);
    public void FormSetLastStyleForHandle(int handle, string spec) => Tui.FormSetLastStyleForHandle(handle, spec);
    public double UiLastHandle() => Tui.UiLastHandle();
    public void ChartFinancial(long handle, double[] ohlc, string plotType, string legend) => Tui.ChartFinancial(handle, ohlc, plotType, legend);
    public void ChartConfigSet(int handle, string prop, double num, string str) => Tui.ChartConfigSet(handle, prop, num, str);
    public double ChartConfigGet(int handle, string prop) => Tui.ChartConfigGet(handle, prop);
    public string ChartConfigGetStr(int handle, string prop) => Tui.ChartConfigGetStr(handle, prop);
    public double ChartDraw(int handle, string op, double[] args) => Tui.ChartDraw(handle, op, args);
    public double FormControlGet(string name, string prop) => Tui.FormControlGet(name, prop);
    public string FormControlGetStr(string name, string prop) => Tui.FormControlGetStr(name, prop);
    public void FormControlSet(string name, string prop, double num, string str) => Tui.FormControlSet(name, prop, num, str);
    public string Dtoc(double oa) => Tui.Dtoc(oa);
    public string PictureApply(object? value, string picture) => Tui.PictureApply(value, picture);
    public void ClipperPrinterWrite(string s, bool newLine) => Tui.ClipperPrinterWrite(s, newLine);
    public bool ClipperPrinterOnly() => Tui.ClipperPrinterOnly();
    public bool ClipperPrinterRouting() => Tui.ClipperPrinterRouting();
    public int ClipperPrinterProw() => Tui.ClipperPrinterProw();
    public int ClipperPrinterPcol() => Tui.ClipperPrinterPcol();
    public void ClipperPrinterEject() => Tui.ClipperPrinterEject();
    public string ClipperPrinterText() => Tui.ClipperPrinterText();
    public void ClipperSetPrinter(bool on) => Tui.ClipperSetPrinter(on);
    public void ClipperSetDevice(string device) => Tui.ClipperSetDevice(device);
    public void ClipperSetWrap(bool on) => Tui.ClipperSetWrap(on);
    public void ClipperSetEscape(bool on) => Tui.ClipperSetEscape(on);
    public void ClipperSetIntensity(bool on) => Tui.ClipperSetIntensity(on);
    public void ClipperSetEpoch(int year) => Tui.ClipperSetEpoch(year);
    public int ClipperEpoch() => Tui.ClipperEpoch();
    public void ClipperSetThemeName(string name) => Tui.ClipperSetThemeName(name);
    public string ClipperThemeName() => Tui.ClipperThemeName();
    public void ClipperSetFgBg(CommonLib.Screens.CellColor fg, CommonLib.Screens.CellColor bg) => Tui.ClipperSetFgBg(fg, bg);
    public CommonLib.Screens.CellColor ClipperFg() => Tui.ClipperFg();
    public CommonLib.Screens.CellColor ClipperBg() => Tui.ClipperBg();
    public void ClipperSetDecimals(int n) => Tui.ClipperSetDecimals(n);
    public void ClipperSetFixed(bool on) => Tui.ClipperSetFixed(on);
    public void ClipperSetTypeahead(int size) => Tui.ClipperSetTypeahead(size);
    public void ClipperSetDelimiters(string v) => Tui.ClipperSetDelimiters(v);
    public void ClipperSetDateFormat(string fmt) => Tui.ClipperSetDateFormat(fmt);
    public void ClipperSetMessage(int row) => Tui.ClipperSetMessage(row);
    public string ClipperFormatNumber(double d) => Tui.ClipperFormatNumber(d);
    public int ClipperMouseMRow() => Tui.ClipperMouseMRow();
    public int ClipperMouseMCol() => Tui.ClipperMouseMCol();
    public int ClipperMouseIsMouse() => Tui.ClipperMouseIsMouse();
    public int ClipperMouseMLeft() => Tui.ClipperMouseMLeft();
    public int ClipperMouseMRight() => Tui.ClipperMouseMRight();
    public int ClipperMouseMMiddle() => Tui.ClipperMouseMMiddle();
    public int ClipperMouseMLeftPress() => Tui.ClipperMouseMLeftPress();
    public int ClipperMouseMRightPress() => Tui.ClipperMouseMRightPress();
    public int ClipperMouseMMiddlePress() => Tui.ClipperMouseMMiddlePress();
    public int ClipperMouseMLeftLess() => Tui.ClipperMouseMLeftLess();
    public int ClipperMouseMRightLess() => Tui.ClipperMouseMRightLess();
    public int ClipperMouseMMiddleLess() => Tui.ClipperMouseMMiddleLess();
    public int ClipperMouseMLeftShift() => Tui.ClipperMouseMLeftShift();
    public int ClipperMouseMRightShift() => Tui.ClipperMouseMRightShift();
    public int ClipperMouseMMiddleShift() => Tui.ClipperMouseMMiddleShift();
    public int ClipperMouseMLeftControl() => Tui.ClipperMouseMLeftControl();
    public int ClipperMouseMRightControl() => Tui.ClipperMouseMRightControl();
    public int ClipperMouseMMiddleControl() => Tui.ClipperMouseMMiddleControl();
    public int ClipperMouseMLeftM() => Tui.ClipperMouseMLeftM();
    public int ClipperMouseMRightM() => Tui.ClipperMouseMRightM();
    public int ClipperMouseMMiddleM() => Tui.ClipperMouseMMiddleM();
    public int ClipperMouseX() => Tui.ClipperMouseX();
    public int ClipperMouseY() => Tui.ClipperMouseY();
    public int ClipperMouseButton(int hasId, int buttonId) => Tui.ClipperMouseButton(hasId, buttonId);
    public int ClipperMouseWheel() => Tui.ClipperMouseWheel();
    public int ClipperMouseClick() => Tui.ClipperMouseClick();
    public int ClipperMouseClickArea(int x1, int y1, int x2, int y2) => Tui.ClipperMouseClickArea(x1, y1, x2, y2);
    public int ClipperMouseDblClick() => Tui.ClipperMouseDblClick();
    public int ClipperMouseMove() => Tui.ClipperMouseMove();
    public int ClipperMousePresent() => Tui.ClipperMousePresent();
    public int ClipperMouseLeft() => Tui.ClipperMouseLeft();
    public int ClipperMouseRight() => Tui.ClipperMouseRight();
    public int ClipperMouseMiddle() => Tui.ClipperMouseMiddle();
    public int ClipperMouseMouStat(int button, out int row, out int col, out int mods) => Tui.ClipperMouseMouStat(button, out row, out col, out mods);
    public string ClipperMouseRelease() => Tui.ClipperMouseRelease();
    public void ClipperMouseUpdateEvent(int kind, int x, int y) => Tui.ClipperMouseUpdateEvent(kind, x, y);
    public void FormControlMove(string name, int x, int y, int w, int h) => Tui.FormControlMove(name, x, y, w, h);
    public void FormControlFocus(string name) => Tui.FormControlFocus(name);
    public void FormControlZorder(string name, int front) => Tui.FormControlZorder(name, front);
    public void FormControlInvalidate(string name) => Tui.FormControlInvalidate(name);
    public void FormControlRefresh(string name) => Tui.FormControlRefresh(name);
    // UserControl + eventi (§11.7): registry C headless-safe → ID/handle/eventi reali anche sotto Null
    // (parity IL/PCode ↔ gambe C). L'istanza è il .NET object; il dispatch eventi è C# (ControlEventHandlers2).
    public void UserControlAdd(object? instance, int x, int y, int w, int h, string container = "", string name = "") => Tui.UserControlAdd(instance, x, y, w, h, container, name);
    public void UserControlRemove(object? instance) => Tui.UserControlRemove(instance);
    public void UserControlDraw(int h, int op, double a, double b, double c, double d, string s) => Tui.UserControlDraw(h, op, a, b, c, d, s);
    public int UserControlLastHandle() => Tui.UserControlLastHandle();
    public double FormLastHandle() => Tui.FormLastHandle();
    public int FormEventIndex(string name) => Tui.FormEventIndex(name);
    public void FormRegisterEventThunk(int handle, int ev) => Tui.FormRegisterEventThunk(handle, ev);
    public void FormSetControlEventSink(Action<int, int> sink) => Tui.FormSetControlEventSink(sink);
    public void FormFireEvent(int handle, int ev) => Tui.FormFireEvent(handle, ev);
    public void FormSetLastEvent(int handle, int ev) => Tui.FormSetLastEvent(handle, ev);
    public void UiAddTbrowse(int row, int col, int w, int h, string colsCsv, string dataCsv) => Tui.UiAddTbrowse(row, col, w, h, colsCsv, dataCsv);
    public void UiAddTextInput(int x, int y, int w, string initial) => Tui.UiAddTextInput(x, y, w, initial);
    public int UiTbrowseSelRow(int i) => Tui.UiTbrowseSelRow(i);
    public int UiControlValue(int i) => Tui.UiControlValue(i);
    public void UiCodeAreaError(int i, int line, bool on) => Tui.UiCodeAreaError(i, line, on);
    public string UiControlText(int i) => Tui.UiControlText(i);
    public bool UiClosed() => Tui.UiClosed();
    public void UiResetClose() => Tui.UiResetClose();
    public int UiPollKey() => Tui.UiPollKey();
    public void UiControlsReset() => Tui.UiControlsReset();
    public bool UiPollEvent(out int ev, out int widx) => Tui.UiPollEvent(out ev, out widx);
    public void UiAddFiledialog(string title, string mask) => Tui.UiAddFiledialog(title, mask);
    public void UiAddPrinterdialog(string title) => Tui.UiAddPrinterdialog(title);
    public void UiAddTreeview(int x, int y, int w, int h, string csv) => Tui.UiAddTreeview(x, y, w, h, csv);
    public void UiAddGrid(int x,int y,int w,int h,int cols,string csv) => Tui.UiAddGrid(x, y, w, h, cols, csv);
    public void UiAddDatepicker(int x,int y,int w) => Tui.UiAddDatepicker(x, y, w);
    public void UiAddColorpicker(int x,int y,int w,int h) => Tui.UiAddColorpicker(x, y, w, h);
    public void UiAddListbox(int x,int y,int w,int h,string csv) => Tui.UiAddListbox(x, y, w, h, csv);
    public int UiAddListboxH(int x,int y,int w,int h,string csv) => Tui.UiAddListboxH(x, y, w, h, csv);
    public void UiAddCombobox(int x,int y,int w,string csv) => Tui.UiAddCombobox(x, y, w, csv);
    public void UiAddRadiogroup(int x,int y,int w,int h,string csv) => Tui.UiAddRadiogroup(x, y, w, h, csv);
    public void UiAddToggle(int x,int y,int w,string caption) => Tui.UiAddToggle(x, y, w, caption);
    public void UiAddSlider(int x,int y,int w,int mn,int mx,int val) => Tui.UiAddSlider(x, y, w, mn, mx, val);
    public void UiAddNumericUpDown(int x,int y,int w,int mn,int mx,int val) => Tui.UiAddNumericUpDown(x, y, w, mn, mx, val);
    public void UiAddStarRating(int x,int y,int w,int mx,int val) => Tui.UiAddStarRating(x, y, w, mx, val);
    public void UiAddScrollBar(int x,int y,int len,int mn,int mx,int val) => Tui.UiAddScrollBar(x, y, len, mn, mx, val);
    public void UiAddScrollView(int x,int y,int w,int h,string csv) => Tui.UiAddScrollView(x, y, w, h, csv);
    public void UiAddLabel(int x,int y,int w,string text) => Tui.UiAddLabel(x, y, w, text);
    public void UiAddDataList(int x,int y,int w,int h,string csv) => Tui.UiAddDataList(x, y, w, h, csv);
    public void UiAddPropertyGrid(int x,int y,int w,int h,string csv) => Tui.UiAddPropertyGrid(x, y, w, h, csv);
    public void UiAddSplitter(int x,int y,int w,int h,int orient) => Tui.UiAddSplitter(x, y, w, h, orient);
    public void UiAddContextMenu(int x,int y,int w,int h,string csv) => Tui.UiAddContextMenu(x, y, w, h, csv);
    public void UiAddLogView(int x,int y,int w,int h,string csv) => Tui.UiAddLogView(x, y, w, h, csv);
    public void UiAddHexView(int x,int y,int w,int h,string data) => Tui.UiAddHexView(x, y, w, h, data);
    public void UiAddBigText(int x,int y,string text) => Tui.UiAddBigText(x, y, text);
    public void UiAddCodeArea(int x,int y,int w,int h,string csv) => Tui.UiAddCodeArea(x, y, w, h, csv);
    public void UiAddReorderListBox(int x,int y,int w,int h,string csv) => Tui.UiAddReorderListBox(x, y, w, h, csv);
    public void UiAddAlertBox(int x,int y,int w,int h,string text) => Tui.UiAddAlertBox(x, y, w, h, text);
    public void UiAddConfirmationBox(int x,int y,int w,int h,string text) => Tui.UiAddConfirmationBox(x, y, w, h, text);
    public void UiAddFileExplorer(int x,int y,int w,int h,string path) => Tui.UiAddFileExplorer(x, y, w, h, path);
    public void UiAddDashboard(int x,int y,int w,int h,string csv) => Tui.UiAddDashboard(x, y, w, h, csv);
    public void UiAddSpinner(int x,int y,int w,string preset) => Tui.UiAddSpinner(x, y, w, preset);
    public void UiAddStatusbar(int x,int y,int w,string text) => Tui.UiAddStatusbar(x, y, w, text);
    public void UiAddTab(int x,int y,int w,string csv) => Tui.UiAddTab(x, y, w, csv);
    public void UiAddMenu(int x,int y,int w,string csv) => Tui.UiAddMenu(x, y, w, csv);
    // Controlli pixel (BUTTON/TEXTBOX/PROGRESS/FUNCTIONKEYBAR/...) + clausole/props: stessa
    // registry C, così NAME/TAG/CONTROL SET/GET sono reali anche headless (parity con le gambe C).
    public void FormAddControl(string kind, object[] args, string name) => Tui.FormAddControl(kind, args, name);
    public void FormDispatchMouse(int type, int x, int y, int button, int wheel) => Tui.FormDispatchMouse(type, x, y, button, wheel);
    public void FormDispatchKey(string key) => Tui.FormDispatchKey(key);
    public void WindowResize(int w, int h) => Tui.WindowResize(w, h);
    public void UiControlApplyProps(int handle, string? name, string? tag, string? tooltip, double? tabindex, double? visible, double? enabled, double? tabstop) => Tui.UiControlApplyProps(handle, name, tag, tooltip, tabindex, visible, enabled, tabstop);
    public void UiControlApplyFocus(int handle) => Tui.UiControlApplyFocus(handle);
    public void UiControlApplyStyle(int handle, string? spec) => Tui.UiControlApplyStyle(handle, spec);
}
