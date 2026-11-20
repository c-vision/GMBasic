// FILE: Basic.IlBackend.PCode.PCodeProcessor.Syscalls.cs
// P-CODE VM — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.Ledger;
using Compiler;
using System.Globalization;
using System.Linq;
using System.Net;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private static readonly System.Collections.Generic.List<(int idx, System.Action<string> apply)> _nativeReadApply = new();

    /// <summary>Azzerra lo stato READ del PROCESSO (registro dei campi `@GET` del runtime
    /// condiviso + gli apply del PCode). Va chiamato all'AVVIO di ogni programma: i campi
    /// `@GET` si accumulano fino al `READ`, e un programma che ne registra alcuni SENZA
    /// leggere li lasciava nel registro statico per il programma successivo — che al suo
    /// `READ` partiva da un campo altrui, con il `when_fn` legato a un task ormai morto
    /// (in headless: attesa di un tasto che non arriva → timeout, come in
    /// `console/get_when_clauses_parity` → `console/get_when_skip_parity`, 2026-11-17).
    /// Gli apply, allo stesso modo, riscrivevano variabili del programma precedente.</summary>
    internal static void ResetNativeReadState()
    {
        _nativeReadApply.Clear();
        Basic.Runtime.Services.NativeTui.ReadReset();
    }

    private static object? DynLoad(PCodeTask t, string n)
    {
        for (var fr = t.Frame; fr is not null; fr = fr.Previous)
            if (fr.DynamicVars is { } dv && dv.TryGetValue(n, out var v)) return v;
        return t.BaseDynamicVars.TryGetValue(n, out var b) ? b : null;
    }

    private static void DynStore(PCodeTask t, string n, object? v)
    {
        System.Collections.Generic.Dictionary<string, object?>? target = null;
        for (var fr = t.Frame; fr is not null; fr = fr.Previous)
            if (fr.DynamicVars is { } dv && dv.ContainsKey(n)) { target = dv; break; }
        if (target is null && t.BaseDynamicVars.ContainsKey(n)) target = t.BaseDynamicVars;
        if (target is null)
        {
            if (t.Frame is null) target = t.BaseDynamicVars;
            else { t.Frame.DynamicVars ??= new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase); target = t.Frame.DynamicVars; }
        }
        target[n] = v;
    }
    private static readonly SemaphoreSlim BlockingNativeSlots = new(
        ResolveBlockingNativeParallelism(), ResolveBlockingNativeParallelism());

    private static Basic.DBF.DbfFieldType DbfFieldTypeOf(Basic.DataType dt) => dt switch
    {
        Basic.StringType => Basic.DBF.DbfFieldType.Character,
        Basic.BitType => Basic.DBF.DbfFieldType.Logical,
        Basic.DateType or Basic.DateTimeType => Basic.DBF.DbfFieldType.Date,
        _ => Basic.DBF.DbfFieldType.Numeric,
    };

    private static int ResolveBlockingNativeParallelism()
    {
        var configured = Environment.GetEnvironmentVariable("GMBASIC_PCODE_BLOCKING_THREADS");
        return int.TryParse(configured, out var value) && value > 0
            ? Math.Min(value, 64)
            : Math.Max(2, Math.Min(Environment.ProcessorCount, 8));
    }

    private static void ParkGraphics(
        PCodeTask task,
        Action<Action<object?, Exception?>> begin,
        Action<object?>? applyResult = null)
    {
        var wait = task.BeginWait(PCodeWaitKind.Graphics);
        try
        {
            begin((result, error) => wait.TryComplete(() =>
            {
                if (error != null)
                    task.PendingException = error;
                else
                    applyResult?.Invoke(result);
            }));
        }
        catch (Exception ex)
        {
            wait.TryComplete(() => task.PendingException = ex);
        }
    }

    private static void ParkAsync<T>(
        PCodeTask task,
        PCodeWaitKind kind,
        Func<CancellationToken, ValueTask<T>> begin,
        Action<T> applyResult,
        int timeoutMs = 0,
        Action? applyTimeout = null)
    {
        var cancellation = new CancellationTokenSource();
        if (timeoutMs > 0)
            cancellation.CancelAfter(timeoutMs);
        var wait = task.BeginWait(kind, cancellation.Cancel);
        _ = CompleteAsync();

        async Task CompleteAsync()
        {
            try
            {
                var result = await begin(cancellation.Token).ConfigureAwait(false);
                wait.TryComplete(() => applyResult(result));
            }
            catch (OperationCanceledException)
            {
                wait.TryComplete(() => applyTimeout?.Invoke());
            }
            catch (Exception ex)
            {
                wait.TryComplete(() => task.PendingException = ex);
            }
            finally
            {
                cancellation.Dispose();
            }
        }
    }

    private static void ParkBlocking<T>(PCodeTask task, Func<T> operation, Action<T> applyResult)
    {
        var cancellation = new CancellationTokenSource();
        var wait = task.BeginWait(PCodeWaitKind.External, cancellation.Cancel);
        _ = CompleteBlockingAsync();

        async Task CompleteBlockingAsync()
        {
            var entered = false;
            try
            {
                await BlockingNativeSlots.WaitAsync(cancellation.Token).ConfigureAwait(false);
                entered = true;
                var result = await Task.Run(operation).ConfigureAwait(false);
                wait.TryComplete(() => applyResult(result));
            }
            catch (OperationCanceledException)
            {
                // La registration è già terminale quando la cancellazione proviene dal PCB.
            }
            catch (Exception ex)
            {
                wait.TryComplete(() => task.PendingException = ex);
            }
            finally
            {
                if (entered) BlockingNativeSlots.Release();
                cancellation.Dispose();
            }
        }
    }

    private void ExecuteFileCmd(PCodeTask task, byte subCode, int argCount)
    {
        // Raccogli gli argomenti dallo stack
        var args = new object?[argCount];
        for (var i = argCount - 1; i >= 0; i--)
            args[i] = task.Sp > 0 ? task.PopRef() : null;

        switch ((FileSubOpCode)subCode)
        {
            case FileSubOpCode.OPEN:
            {
                // args: [0]=FileName, [1]=Mode, [2]=LockMode, [3]=FileNumber,
                //       [4]=RecordLength, [5]=ContentType, [6]=KeyCount,
                //       [7]=IndexCount, [8]=Unsafe
                // FileNumber puo' essere NIL invece di 0.0 quando la variabile passata (es.
                // JSON.OPEN(file$, modo$, handle) con 'handle' mai inizializzata prima) non e'
                // mai stata scritta: una variabile implicita non dichiarata vale NIL a runtime
                // in questa VM (a differenza di GMBasicAI, dove il default e' 0.0) — trattare NIL
                // come file number 0, stesso comportamento gia' verificato in GMBasicAI.
                if (args.Length >= 4 && (args[3] is double || args[3] is null))
                {
                    var fileNum = args[3] is double fn ? fn : 0.0;
                    var fileName = args[0]?.ToString() ?? "";
                    var mode = args[1] is double m ? (int)m : 0;
                    var modeStr = mode switch
                    {
                        0 => "INPUT",    // FileOpenMode.Input
                        1 => "OUTPUT",    // FileOpenMode.Output (FOR OUTPUT / FOR WRITE)
                        2 => "APPEND",    // FileOpenMode.Append
                        3 => "RANDOM",    // FileOpenMode.Random
                        4 => "BINARY",    // FileOpenMode.Binary
                        _ => "INPUT"
                    };
                    try
                    {
                        // Per i modi di scrittura crea la cartella del file se manca
                        // (FileMode.Create non crea le directory; OPEN su 'dir/x.txt'
                        // con dir inesistente falliva in silenzio).
                        if (modeStr is "OUTPUT" or "APPEND" or "RANDOM" or "BINARY")
                        {
                            var openDir = Path.GetDirectoryName(Path.GetFullPath(fileName));
                            if (!string.IsNullOrEmpty(openDir) && !Directory.Exists(openDir))
                                Directory.CreateDirectory(openDir);
                        }
                        var handle = _runtime.File.Open(fileName, modeStr);
                        task.FileHandles[(int)fileNum] = handle;
                        // OPEN … FOR RANDOM [LEN = n]: abilita il record I/O a layout canonico (PUT/GET su UDT).
                        if (modeStr == "RANDOM" && _runtime.File.GetStream(handle) is { } randStream)
                            CommonLib.RandomRecordCodec.MarkRandom(randStream, args.Length > 4 && args[4] is double rl ? (long)rl : 0);
                        var contentType = args.Length > 5 && args[5] is double ct ? (byte)ct : (byte)0;
                        task.FileContentTypes[(int)fileNum] = contentType;
                    }
                    catch (Exception)
                    {
                        // File non trovato — ignora (come fa l'AST Executor)
                    }
                }
                break;
            }
            default:
                throw new InvalidOperationException(
                    $"FILE_CMD sub-opcode {subCode} non gestito dalla PCodeVM. "
                    + "Aggiungere un case reale, mai un fallback silenzioso.");
        }
    }

    /// <summary>Risolve il lock del sottosistema database per un dato SysCall, se applicabile —
    /// null per tutto il resto (PRINT/CLS/grafica/ecc., già gestiti dove serve dal proprio lock
    /// specifico, es. _consoleLock). Basato su intervalli dell'enum SysCall (ISAM_*/DBF_*/
    /// SPEED_*/LEDGER_* sono blocchi contigui — verificato via grep su Compiler/SysCalls.cs).
    /// DBF_CLOSE_DATABASES è un caso speciale gestito a parte in ExecuteSyscall (tocca tutti e
    /// 4 i sottosistemi, non solo DBF).</summary>
    private object? ResolveDbLock(SysCall call)
    {
        // RDD_* spostati da _isamLock a _sqlLock (BUG 23): sono i syscall dell'accesso RDD
        // late-binding (usati anche da Clipper-style DBF.*), che nella VM contendono sullo stesso
        // _dataSession usato da SQL, non su ISAM.
        if (call == SysCall.RDD_SET_DEFAULT || call == SysCall.RDD_USE || call == SysCall.RDD_RLOCK || call == SysCall.RDD_FLOCK || call == SysCall.RDD_UNLOCK || call == SysCall.RDD_SETLOCKTIMEOUT || call == SysCall.RDD_APPENDFROM || call == SysCall.RDD_GET_FIELD || call == SysCall.RDD_SET_FIELD || call == SysCall.RDD_ONEVENT || call == SysCall.RDD_OFFEVENT || call == SysCall.RDD_TODAY || call == SysCall.RDD_CTOD || call == SysCall.RDD_DTOC || call == SysCall.RDD_CURVAL || call == SysCall.RDD_BUFFER || call == SysCall.RDD_TABLEUPDATE || call == SysCall.RDD_TABLEREVERT || call == SysCall.RDD_OLDVAL || call == SysCall.RDD_GETFLDSTATE || call == SysCall.RDD_SETREPROCESS || call == SysCall.RDD_SETORDER || call == SysCall.RDD_CREATEINDEX || call == SysCall.RDD_CLOSEINDEX || call == SysCall.RDD_COPYTO || call == SysCall.RDD_SORTTO || call == SysCall.RDD_TOTALON || call == SysCall.RDD_SETDELETED || call == SysCall.RDD_VALTYPE || call == SysCall.RDD_FLOAT || call == SysCall.RDD_EVAL || call == SysCall.RDD_DELETE_SCOPED || call == SysCall.RDD_RECALL_SCOPED || call == SysCall.RDD_REPLACE_SCOPED) return _sqlLock;
        if (call >= SysCall.ISAM_USE && call <= SysCall.ISAM_APPENDMANY) return _isamLock;
        if (call >= SysCall.DBF_USE && call <= SysCall.DBF_FOLLOW_REPLICATION) return _dbfLock;
        if (call >= SysCall.SPEED_OPEN && call <= SysCall.SPEED_SETMANY) return _speedLock;
        if (call >= SysCall.LEDGER_OPEN && call <= SysCall.LEDGER_IMPORT) return _ledgerLock;
        if (call >= SysCall.SQL_OPEN && call <= SysCall.SQL_BACKUP) return _sqlLock;
        return null;
    }

    private void ExecuteSyscall(PCodeTask task, ushort code, int argCount)
    {
        var call = (SysCall)code;
        // DBF_CLOSE_DATABASES tocca tutti e 4 i sottosistemi: acquisisce tutti e 4 i lock in un
        // ordine FISSO (isam -> dbf -> speed -> ledger) per prevenire deadlock con qualunque
        // altro path multi-lock che dovesse in futuro toccare più di un sottosistema — nessun
        // altro punto del codice acquisisce più di un lock db alla volta oggi, ma se mai
        // succedesse dovrebbe seguire lo stesso ordine.
        if (call == SysCall.DBF_CLOSE_DATABASES)
        {
            lock (_isamLock)
            lock (_dbfLock)
            lock (_speedLock)
            lock (_ledgerLock)
            lock (_sqlLock)
                ExecuteSyscallDispatch(task, code, argCount);
            return;
        }
        var dbLock = ResolveDbLock(call);
        if (dbLock != null)
        {
            lock (dbLock) ExecuteSyscallDispatch(task, code, argCount);
            return;
        }
        ExecuteSyscallDispatch(task, code, argCount);
    }

    private void ExecuteSyscallDispatch(PCodeTask task, ushort code, int argCount)
    {
        switch ((SysCall)code)
        {
            case SysCall.CLS:
            {
                var color = task.Sp > 0 ? task.PopRef() : null;
                // Finestra attiva (parità backend IL, IlRuntime.ClsDispatch): svuota il
                // buffer della finestra invece di cancellare la pagina/console base. Prima
                // CLS ignorava sempre WINDOW SELECT e cancellava lo schermo intero.
                if (ActivePixelWindowPCode() is { } clsPw)
                {
                    clsPw.Lines.Clear();
                    clsPw.ScrollOffset = 0;
                    clsPw.CursorLine = 0;
                    clsPw.CursorCol = 0;
                    RenderPixelWindowsForPCode();
                    break;
                }
                if (ActiveWindowObjPCode() is { } clsAw)
                {
                    clsAw.Lines.Clear();
                    clsAw.ScrollOffset = 0;
                    clsAw.CursorLine = 0;
                    clsAw.CursorCol = 0;
                    RenderTuiWindowsForPCode();
                    break;
                }
                var resolved = ResolveColor(color);
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    // ClsInternal/GetColor si aspetta l'INDICE palette, non l'RGB: passare
                    // resolved.R (0 per il blu) puliva con il colore 0 (nero) — bug con
                    // `CLS 1` (sfondo blu) nel loop degli sprite. CLS senza argomento → null
                    // (lo sfondo CORRENTE, impostato da COLOR) — mai 0.
                    int? clsIdx = color is double cd ? (int)cd : resolved?.R;
                    PCodeGraphics.Instance.CLS_GPM(clsIdx, (_r, _e) => { });
                }
                else lock (_consoleLock)
                {
                    if (resolved.HasValue)
                        _runtime.Console.SetBackgroundColor(resolved.Value.R);
                    _runtime.Console.Clear();
                }
                break;
            }
            case SysCall.COLOR:
            {
                var bgRaw = task.PopRef();
                var fgRaw = task.PopRef();
                // Finestra pixel attiva (parità backend IL, IlRuntime.ColorDispatch): COLOR
                // fg,bg imposta testo/titolo (fg) e sfondo (bg) della finestra invece della
                // console base. Con finestra TUI attiva non c'è nulla da fare (il colore
                // segue il tema), stesso comportamento del backend IL.
                if (ActivePixelWindowPCode() is { } colorPw)
                {
                    try
                    {
                        var gmColor = PCodeGraphics.Instance;
                        if (fgRaw != null) colorPw.FgColor = colorPw.TitleColor = gmColor.GfxResolveColor(fgRaw, colorPw.FgColor);
                        if (bgRaw != null) colorPw.BodyColor = gmColor.GfxResolveColor(bgRaw, colorPw.BodyColor);
                        RenderPixelWindowsForPCode();
                    }
                    catch { }
                    break;
                }
                if (ActiveWindowObjPCode() is not null) break;
                var fg = ResolveColor(fgRaw);
                var bg = ResolveColor(bgRaw);
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    // COLOR_GPM/ColorInternal si aspetta l'INDICE palette (GetColor(index)),
                    // non l'RGB: passare fg.R (es. 255 per il bianco) faceva GetColor(255) →
                    // fuori palette → colore 0 (tutto nero, PRINT invisibile — bug verificato
                    // col sample SCREEN+FONT+COLOR 15,0+PRINT). Per un valore numerico si passa
                    // l'indice raw (15); per stringhe/hex si passa l'RGB (il ColorInternal li
                    // gestisce come indice: coperto dalla palette solo se < 16).
                    var fgIdx = fgRaw is double fd ? (int)fd : (int)(fg?.R ?? 255);
                    var bgIdx = bgRaw is double bd ? (int)bd : (int)(bg?.R ?? 0);
                    ParkGraphics(task, cb => PCodeGraphics.Instance.COLOR_GPM(fgIdx, bgIdx, cb));
                    return;
                }
                lock (_consoleLock)
                {
                    _runtime.Console.SetForegroundColor(fg?.R ?? 255);
                    _runtime.Console.SetBackgroundColor(bg?.R ?? 0);
                }
                if (fg.HasValue || bg.HasValue)
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetFgBg(
                        fg ?? Basic.IlBackend.IlRuntime.Current.Graphics.ClipperFg(),
                        bg ?? Basic.IlBackend.IlRuntime.Current.Graphics.ClipperBg());
                break;
            }
            case SysCall.LOCATE:
            {
                var col = PopInt(task);
                var line = PopInt(task);
                // Finestra attiva (parità backend IL, IlRuntime.LocateDispatch): posiziona
                // il cursore nel buffer, riga/colonna relativi alla viewport, invece che
                // sulla console/pagina base.
                if (ActivePixelWindowPCode() is { } locPw)
                {
                    var locCr = locPw.ClientRect;
                    locPw.CursorCol = Math.Max(0, col - 1);
                    var locTargetLine = Math.Max(0, line - 1);
                    if (locPw.Lines.Count == 0) locPw.Lines.Add("");
                    locPw.CursorLine = Math.Min(locTargetLine, locPw.Lines.Count - 1);
                    if (locPw.CursorLine < locPw.ScrollOffset) locPw.ScrollOffset = locPw.CursorLine;
                    var locRows = Math.Max(1, locCr.h / Math.Max(1, CommonGraphics.Graphics.GfxWindowManager.CharHeight()));
                    if (locPw.CursorLine >= locPw.ScrollOffset + locRows) locPw.ScrollOffset = locPw.CursorLine - locRows + 1;
                    RenderPixelWindowsForPCode();
                    break;
                }
                if (ActiveWindowObjPCode() is { } locAw)
                {
                    var locAcr = locAw.ContentRect;
                    locAw.CursorCol = Math.Max(0, col - 1);
                    var locAwTargetLine = Math.Max(0, line - 1);
                    if (locAw.Lines.Count == 0) locAw.Lines.Add("");
                    locAw.CursorLine = Math.Min(locAwTargetLine, locAw.Lines.Count - 1);
                    if (locAw.CursorLine < locAw.ScrollOffset) locAw.ScrollOffset = locAw.CursorLine;
                    if (locAw.CursorLine >= locAw.ScrollOffset + Math.Max(1, locAcr.H)) locAw.ScrollOffset = locAw.CursorLine - Math.Max(1, locAcr.H) + 1;
                    RenderTuiWindowsForPCode();
                    break;
                }
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance.LOCATE_GPM(col, line, cb));
                    return;
                }
                lock (_consoleLock) _runtime.Console.SetCursorPosition(col - 1, line - 1);
                break;
            }
            case SysCall.PRINT:
            {
                // Bufferizza come le altre PRINT-family (vedi PendingPrints): senza, goroutine
                // diverse che stampano contemporaneamente sotto vero parallelismo interlacciano i
                // frammenti di testo a metà parola sullo stesso Console/StringWriter condiviso
                // (osservato in GoRoutinesWorkers.bas).
                var val = task.Sp > 0 ? task.PopRef() : null;
                var printVal = FormatPrintValue(val);
                (task.PendingPrints ??= new()).Add((0, printVal,
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                break;
            }
            case SysCall.INPUT:
            {
                var prompt = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                ParkBlocking(task,
                    () => _runtime.Keyboard.ReadLine(prompt),
                    result => task.PushRef(result ?? ""));
                return;
            }
            case SysCall.WAITKEY:
            {
                ParkAsync(task, PCodeWaitKind.Input,
                    ct => _runtime.Keyboard.ReadKeyAsync(ct),
                    _ => { });
                return;
            }
            case SysCall.GET_KEY:
            {
                ParkAsync(task, PCodeWaitKind.Input,
                    ct => _runtime.Keyboard.ReadKeyAsync(ct),
                    key =>
                    {
                        DispatchKeyEvent(new CommonLib.Consoles.TerminalKeyInfo(
                            key.KeyChar, (int)key.Key,
                            key.Modifiers.HasFlag(ConsoleModifiers.Shift),
                            key.Modifiers.HasFlag(ConsoleModifiers.Alt),
                            key.Modifiers.HasFlag(ConsoleModifiers.Control)));
                        task.PushRef(key.KeyChar.ToString());
                    });
                return;
            }
            case SysCall.RANDOMIZE:
                {
                    if (task.Sp > 0) IlRuntime.RandomizeSeed(PopNum(task));
                    else IlRuntime.Randomize();
                    break;
                }
            case SysCall.SWAP:
            {
                if (argCount >= 4)
                {
                    var rightIdx = (int)PopNum(task);
                    var rightIsLocal = PopNum(task) != 0;
                    var leftIdx = (int)PopNum(task);
                    var leftIsLocal = PopNum(task) != 0;

                    object? GetVal(bool isLocal, int idx) =>
                        isLocal
                            ? (task.Frame != null && idx >= 0 && idx < task.Frame.LocalVariables.Length ? task.Frame.LocalVariables[idx] : null)
                            : task.GetGlobal(idx);

                    void SetVal(bool isLocal, int idx, object? val)
                    {
                        if (isLocal) { if (task.Frame != null && idx >= 0 && idx < task.Frame.LocalVariables.Length) task.Frame.LocalVariables[idx] = val; }
                        else { task.EnsureGlobals(idx + 1); task.Globals![idx] = val; }
                    }

                    var leftVal = GetVal(leftIsLocal, leftIdx);
                    var rightVal = GetVal(rightIsLocal, rightIdx);
                    SetVal(leftIsLocal, leftIdx, rightVal);
                    SetVal(rightIsLocal, rightIdx, leftVal);
                }
                break;
            }

            // === FILE SYSCALLS ===
            case SysCall.CLOSE_FILE:
            {
                // args: [0]=FileNumber (o null per chiudi tutti)
                var fileNum = PopOptInt(task);
                if (fileNum.HasValue)
                {
                    if (task.FileHandles.TryGetValue(fileNum.Value, out var handle))
                    {
                        _runtime.File.Close(handle);
                        task.FileHandles.Remove(fileNum.Value);
                        task.FileContentTypes.Remove(fileNum.Value);
                    }
                    // Un handle Memory Mapped File con lo stesso numero (IO.CLOSE(handleMMap)):
                    // UNICA implementazione condivisa (stesso registry delle funzioni).
                    try { Basic.IlBackend.IlRuntime.MemoryMapClose(fileNum.Value); }
                    catch { /* non è una mappa */ }
                }
                else
                {
                    foreach (var h in task.FileHandles.Values)
                        _runtime.File.Close(h);
                    task.FileHandles.Clear();
                    task.FileContentTypes.Clear();
                    try { Basic.IlBackend.IlRuntime.MemoryMapCloseAll(); }
                    catch { /* nessuna mappa */ }
                }
                break;
            }
            case SysCall.MEMORYMAP_OPEN:
            {
                // (path) → handle LONG. UNICA implementazione: IlRuntime.MemoryMap
                // (stessa policy capacità 128). Prima ogni mondo aveva il proprio
                // dictionary: le mappe aperte da IO.MEMORYMAP() (funzione) erano
                // invisibili ai comandi MMAPPOKE/MMWRITEBYTES (no-op silenziosi).
                var path = PopString(task) ?? "";
                if (string.IsNullOrEmpty(path)) { task.PushNum(0); break; }
                try { task.PushNum(Basic.IlBackend.IlRuntime.MemoryMap(path)); }
                catch { task.PushNum(0); }
                break;
            }
            case SysCall.MEMORYMAP_PEEK:
            {
                // (handle, offset) → BYTE. UNICA implementazione condivisa.
                var offset = (long)PopNum(task);
                var handle = (long)PopNum(task);
                byte result = 0;
                try { result = Basic.IlBackend.IlRuntime.MemoryPeek(handle, offset); }
                catch { /* oltre la mappa: 0 */ }
                task.PushNum(result);
                break;
            }
            case SysCall.MEMORYMAP_POKE:
            {
                // (handle, offset, value). UNICA implementazione condivisa.
                var value = (byte)PopNum(task);
                var offset = (long)PopNum(task);
                var handle = (long)PopNum(task);
                try { Basic.IlBackend.IlRuntime.MemoryPoke(handle, offset, value); }
                catch { /* oltre la mappa: ignora */ }
                break;
            }
            case SysCall.MEMORYMAP_READBYTES:
            {
                // (handle, offset, count) → BYTE()
                // Il VM materializza gli array numerici come double[] (gli elementi byte
                // in byte[] non si confrontano con gli altri valori numerici: byte == double
                // fallisce — visto nel sample FilesMemoryMap sezione 6).
                double[] result = Array.Empty<double>();
                var count = (int)PopNum(task);
                var offset = (long)PopNum(task);
                var handle = (long)PopNum(task);
                if (count > 0)
                {
                    try
                    {
                        var raw = Basic.IlBackend.IlRuntime.MemoryReadBytes(handle, offset, count);
                        result = raw.Select(b => (double)b).ToArray();
                    }
                    catch { /* oltre la mappa: zero-fill */ }
                }
                task.PushRef(result);
                break;
            }
            case SysCall.MEMORYMAP_WRITEBYTES:
            {
                // (handle, offset, byteArray, count) — count=-1 = intero array
                var count = (int)PopNum(task);
                var source = PopF(task);
                var offset = (long)PopNum(task);
                var handle = (long)PopNum(task);
                var bytes = ToBytes(source);
                if (count >= 0 && count < bytes.Length)
                    bytes = bytes.AsSpan(0, count).ToArray();
                // UNICA implementazione condivisa (stesso registry delle funzioni).
                try { Basic.IlBackend.IlRuntime.MemoryWriteBytes(handle, offset, bytes, count); }
                catch { /* oltre la mappa: ignora */ }
                break;
            }
            case SysCall.CONSOLE_CAPTION:
            {
                // (title) → ConsoleService.Title (titolo finestra/console)
                var title = PopString(task) ?? "";
                lock (_consoleLock) { try { _runtime.Console.Title = title; } catch { } }
                break;
            }
            case SysCall.CONSOLE_SENDKEY:
            {
                // (keys) → KeyboardService.SendKey (inietta un tasto nel buffer di input)
                var keys = PopString(task) ?? "";
                if (keys.Length > 0)
                    _runtime.Keyboard.SendKey(keys[0]);
                break;
            }
            case SysCall.CONSOLE_SCROLLMODE:
            {
                // (mode) → SetScrollRegion(0, 25) se abilitato, altrimenti (0, 0) — come l'IL
                var mode = PopInt(task);
                lock (_consoleLock) { _runtime.Console.SetScrollRegion(0, mode != 0 ? 25 : 0); }
                break;
            }
            case SysCall.SOUND_LOAD:
            {
                // (filePath) → handle (AudioManager.LoadAudioFile); assegna al target.
                // Caricare un file audio da disco può essere lento → blocking pool (mai sul worker).
                var path = PopString(task) ?? "";
                var audio = _audioManager ??= new CommonGraphics.AudioManager();
                ParkBlocking(task,
                    () => { try { return (object?)(double)audio.LoadAudioFile(path); } catch { return (object?)-1.0; } },
                    result => task.PushRef(result));
                break;
            }
            case SysCall.SOUND_PLAY_FILE:
            {
                // (handle, channel?, loops?) → AudioManager.PlayAudioFile
                var loops = PopOptInt(task);
                var channel = PopOptInt(task);
                var handle = PopInt(task);
                try
                {
                    var audio = _audioManager ??= new CommonGraphics.AudioManager();
                    _ = Task.Run(() => audio.PlayAudioFile(handle)); // non bloccare il task VM
                }
                catch { }
                break;
            }
            case SysCall.SOUND_VOLUME:
            {
                // (volume, channel?) → AudioManager.SetMasterVolume
                var channel = PopOptInt(task);
                var volume = PopInt(task);
                try
                {
                    var audio = _audioManager ??= new CommonGraphics.AudioManager();
                    audio.SetMasterVolume(Math.Clamp(volume, 0, 100));
                }
                catch { }
                break;
            }
            case SysCall.MOUSE_ENABLE:
            {
                // MOUSE ON/OFF — polimorfico: MouseInput (ANSI reporting in TUI, stato in grafica)
                if (PopInt(task) != 0) CommonLib.MouseInput.Enable();
                else CommonLib.MouseInput.Disable();
                break;
            }
            case SysCall.MOUSE_SHOW:
            {
                if (PopInt(task) != 0) CommonLib.MouseInput.Show();
                else CommonLib.MouseInput.Hide();
                break;
            }
            case SysCall.MOUSE_SETPOS:
            {
                var y = PopInt(task); var x = PopInt(task);
                CommonLib.MouseInput.SetPosition(x, y);
                break;
            }
            case SysCall.MOUSE_CURSOR:
            {
                CommonLib.MouseInput.SetCursor(PopInt(task));
                break;
            }
            case SysCall.MOUSE_LIMIT:
            {
                var y2 = PopOptIntValue(task); var x2 = PopOptIntValue(task);
                var y1 = PopOptIntValue(task); var x1 = PopOptIntValue(task);
                if (x1.HasValue && y1.HasValue && x2.HasValue && y2.HasValue)
                    CommonLib.MouseInput.Limit(x1.Value, y1.Value, x2.Value, y2.Value);
                else
                    CommonLib.MouseInput.Unlimit();
                break;
            }
            case SysCall.MOUSE_WAIT:
            {
                // Sincronizza gli eventi mouse in coda (come il deprecated: non blocca)
                CommonLib.MouseInput.PumpEvents();
                break;
            }
            case SysCall.CHART_CREATE:
            {
                // (title, xLabel?, yLabel?) → handle LONG (OxyPlot in memoria)
                var yLabel = task.Sp > 0 ? PopString(task) : null;
                var xLabel = task.Sp > 0 ? PopString(task) : null;
                var title = PopString(task) ?? "Grafico";
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.ChartCreate_GPM(
                        title, xLabel ?? "Asse X", yLabel ?? "Asse Y", cb),
                    result => { if (result != null) task.PushNum((long)result); });
                return;
            }
            case SysCall.CHART_PLOT:
            {
                // (handle, x[], y[], plotType, legend?)
                var raw = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--) raw[i] = task.Sp > 0 ? task.PopRef() : null;
                var handle = raw.Length > 0 ? (long)SafeDouble(raw[0]) : 0;
                var x = raw.Length > 1 ? ToDoubleArray(raw[1]) : Array.Empty<double>();
                var y = raw.Length > 2 ? ToDoubleArray(raw[2]) : Array.Empty<double>();
                var plotType = raw.Length > 3 ? raw[3]?.ToString() ?? "LINE" : "LINE";
                var legend = raw.Length > 4 ? raw[4]?.ToString() ?? "" : "";
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .ChartPlot_GPM(handle, x, y, plotType, legend, cb));
                return;
            }
            case SysCall.CHART_FINANCIAL:
            {
                var raw = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--) raw[i] = task.Sp > 0 ? task.PopRef() : null;
                var handle = raw.Length > 0 ? (long)SafeDouble(raw[0]) : 0;
                var ohlc = raw.Length > 1 && raw[1] is object?[] oa ? oa.Where(x => x != null).Cast<object>().ToArray()
                    : raw.Length > 1 && raw[1] is System.Collections.IEnumerable en ? en.Cast<object?>().Where(x => x != null).Cast<object>().ToArray()
                    : Array.Empty<object>();
                var plotType = raw.Length > 2 ? raw[2]?.ToString() ?? "CANDLE" : "CANDLE";
                var legend = raw.Length > 3 ? raw[3]?.ToString() ?? "" : "";
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .ChartFinancial_GPM(handle, ohlc, plotType, legend, cb));
                return;
            }
            case SysCall.CHART_CLEAR:
            {
                var handle = (long)PopNum(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance.ChartClear_GPM(handle, cb));
                return;
            }
            case SysCall.CHART_CLOSE:
            {
                var handle = (long)PopNum(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance.ChartClose_GPM(handle, cb));
                return;
            }
            case SysCall.CHART_RENDER:
            {
                // (isSave, handle, [filename], w, h)
                var isSave = PopInt(task) != 0;
                var raw = new object?[argCount - 1];
                for (var i = raw.Length - 1; i >= 0; i--) raw[i] = task.Sp > 0 ? task.PopRef() : null;
                var handle = raw.Length > 0 ? (long)SafeDouble(raw[0]) : 0;
                var w = raw.Length > (isSave ? 2 : 1) ? (int)SafeDouble(raw[isSave ? 2 : 1]) : 400;
                var h = raw.Length > (isSave ? 3 : 2) ? (int)SafeDouble(raw[isSave ? 3 : 2]) : 300;
                if (!isSave)
                {
                    // SHOW: gmb_gfx_chart_show (parity IL → ChartShow). Il render PNG (byte[])
                    // è solo backend C#; SAVE ha il suo path.
                    ParkGraphics(task, cb => PCodeGraphics.Instance.ChartShow_GPM(handle, w, h, cb));
                    return;
                }
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.ChartRender_GPM(handle, w, h, cb),
                    result =>
                    {
                        if (isSave && result is byte[] png && raw.Length > 1 && raw[1] is not null)
                        {
                            try { System.IO.File.WriteAllBytes(raw[1]?.ToString() ?? "chart.png", png); }
                            catch { }
                        }
                    });
                return;
            }
            case SysCall.ADD_HANDLER:
            {
                var handlerName = PopString(task) ?? "";
                var eventName = PopString(task) ?? "";
                                if (eventName.Length > 0 && handlerName.Length > 0 && _procedureAddresses.ContainsKey(handlerName))
                {
                    _eventHandlers ??= new System.Collections.Generic.Dictionary<string, System.Collections.Generic.List<string>>(StringComparer.OrdinalIgnoreCase);
                    if (!_eventHandlers.TryGetValue(eventName, out var list))
                        _eventHandlers[eventName] = list = new System.Collections.Generic.List<string>();
                    if (!list.Contains(handlerName)) list.Add(handlerName);
                }
                break;
            }
            case SysCall.REMOVE_HANDLER:
            {
                var handlerName = PopString(task) ?? "";
                var eventName = PopString(task) ?? "";
                if (_eventHandlers != null && _eventHandlers.TryGetValue(eventName, out var list))
                {
                    list.Remove(handlerName);
                    if (list.Count == 0) _eventHandlers.Remove(eventName);
                }
                break;
            }
            case SysCall.RAISE_EVENT:
            {
                // (eventName, args...): chiama tutti gli handler registrati in sequenza
                var raw = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--) raw[i] = task.Sp > 0 ? task.PopRef() : null;
                var eventName = raw.Length > 0 ? raw[0]?.ToString() ?? "" : "";
                var args = raw.Length > 1 ? raw[1..] : Array.Empty<object?>();
                if (_eventHandlers != null && _eventHandlers.TryGetValue(eventName, out var handlers) && handlers.Count > 0)
                {
                    _pendingEventHandlers = handlers.Select(h => (h, args)).ToArray();
                    _pendingEventIndex = 0;
                    DispatchEventHandler(task, _pendingEventHandlers[0].Handler, args);
                    return; // il controllo va all'handler; al RETURN → successivo → poi main
                }
                break;
            }
            case SysCall.PALETTE_SET:
            {
                var b = PopInt(task); var g = PopInt(task); var r = PopInt(task); var index = PopInt(task);
                lock (_consoleLock) { _runtime.Console.SetPaletteColor(index, r, g, b); }
                // Anche la palette GRAFICA (SCREEN attivo): il compiler emette PALETTE_SET per
                // PALETTE idx,r,g,b ma il case grafico PALETTE era orfano (nessun emittente)
                // → PALETTE era un no-op sui disegni (PSET/LINE usavano i colori di default).
                // BUG FIX: PALETTE_GPM + Blocked SENZA grafica attiva (sample console puro)
                // bloccava il task per sempre (il wake dal GraphicsManager non arriva mai).
                // Solo se IsScreenOn la grafica processa la palette; altrimenti basta la console.
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance.PALETTE_GPM(index, r, g, b, cb));
                    return;
                }
                break;
            }
            // --- MOTORE 3D / WORLD (2026-11-19, ponte C#): comandi. Lo stato vive SOLO nel
            // runtime C: qui si inoltra alla facciata gmb_world_* (CommonLib.World3DNative).
            // Gli argomenti sono sullo stack nell'ordine della sorgente → si estraggono al
            // contrario. Il rc della facciata non è leggibile da un comando (§1).
            case SysCall.WORLD_LOAD:
            {
                var path = PopString(task) ?? "";
                CommonLib.World3DNative.Load(path);
                break;
            }
            case SysCall.WORLD_CLOSE:
                CommonLib.World3DNative.Free();
                break;
            case SysCall.WORLD_TICK:
            {
                var seconds = PopNum(task);
                CommonLib.World3DNative.Tick(seconds);
                break;
            }
            case SysCall.WORLD_SECMOVE:
            {
                var floorZ = PopNum(task); var speed = PopNum(task); var sector = PopInt(task);
                CommonLib.World3DNative.SecMove(sector, speed, floorZ);
                break;
            }
            case SysCall.WORLD_SECMOVE_CEIL:
            {
                var ceilZ = PopNum(task); var floorZ = PopNum(task); var speed = PopNum(task); var sector = PopInt(task);
                CommonLib.World3DNative.SecMoveCeil(sector, speed, floorZ, ceilZ);
                break;
            }
            case SysCall.WORLD_HEIGHT:
            {
                var sector = PopInt(task);
                CommonLib.World3DNative.Height(sector);
                break;
            }
            case SysCall.WORLD_ANIM:
            {
                var fps = PopNum(task); var count = PopInt(task); var first = PopInt(task); var sector = PopInt(task);
                CommonLib.World3DNative.Anim(sector, first, count, fps);
                break;
            }
            case SysCall.WORLD_SCROLL:
            {
                var vPerS = PopNum(task); var uPerS = PopNum(task); var what = PopInt(task); var sector = PopInt(task);
                CommonLib.World3DNative.Scroll(sector, what, uPerS, vPerS);
                break;
            }
            case SysCall.WORLD_LIGHTFX:
            {
                var rate = PopNum(task); var amount = PopNum(task); var mode = PopInt(task); var sector = PopInt(task);
                CommonLib.World3DNative.LightFx(sector, mode, amount, rate);
                break;
            }
            case SysCall.WORLD_LAMP:
            {
                var level = PopNum(task); var radius = PopNum(task); var z = PopNum(task);
                var y = PopNum(task); var x = PopNum(task);
                CommonLib.World3DNative.Lamp(x, y, z, radius, level);
                break;
            }
            // --- TEMPO A PASSO FISSO / CLOCK (2026-11-20): comandi. L'orologio vive SOLO nel
            // runtime C: qui si inoltra alla facciata gmb_clock_frame_* (CommonLib.ClockNative).
            case SysCall.CLOCK_PAUSE:
                CommonLib.ClockNative.Pause(1);
                break;
            case SysCall.CLOCK_RESUME:
                CommonLib.ClockNative.Pause(0);
                break;
            case SysCall.CLOCK_SCALE:
            {
                var scale = PopNum(task);
                CommonLib.ClockNative.SetScale(scale);
                break;
            }
            case SysCall.CLOCK_FPS:
            {
                var fps = PopNum(task);
                CommonLib.ClockNative.SetFps(fps);
                break;
            }
            case SysCall.PALETTE_USE:
            {
                var name = PopString(task) ?? "";
                if (CommonLib.Screens.PaletteManager.TryGetPalette(name, out var pal) && pal != null)
                    lock (_consoleLock) { _runtime.Console.SetActivePalette(pal); }
                break;
            }
            case SysCall.PALETTE_DEFINE:
            case SysCall.PALETTE_REPLACE:
            {
                var colors = PopF(task);
                var name = PopString(task) ?? "";
                var pal = colors is System.Collections.IEnumerable en
                    ? en.Cast<object?>().Select(v => v is byte[] rgb && rgb.Length >= 3
                        ? new[] { rgb[0], rgb[1], rgb[2] } : v is System.Collections.IList l && l.Count >= 3
                        ? new[] { (byte)SafeDouble(l[0]), (byte)SafeDouble(l[1]), (byte)SafeDouble(l[2]) } : new byte[] { 0, 0, 0 }).ToArray()
                    : Array.Empty<byte[]>();
                CommonLib.Screens.PaletteManager.RegisterPalette(name, pal);
                lock (_consoleLock) { _runtime.Console.SetActivePalette(pal); }
                break;
            }
            case SysCall.PALETTE_SAVE:
            {
                var file = PopString(task) ?? "";
                try
                {
                    var pal = _runtime.Console.GetActivePalette();
                    System.IO.File.WriteAllText(file,
                        System.Text.Json.JsonSerializer.Serialize(pal));
                }
                catch { }
                break;
            }
            case SysCall.PALETTE_LOAD:
            {
                var file = PopString(task) ?? "";
                try
                {
                    var pal = System.Text.Json.JsonSerializer.Deserialize<byte[][]>(System.IO.File.ReadAllText(file));
                    if (pal != null)
                    {
                        CommonLib.Screens.PaletteManager.RegisterPalette("LOADED", pal);
                        lock (_consoleLock) { _runtime.Console.SetActivePalette(pal); }
                    }
                }
                catch { }
                break;
            }
            case SysCall.FILE_LOCK:
            case SysCall.FILE_UNLOCK:
            {
                var isLock = code == (ushort)SysCall.FILE_LOCK;
                var recordNumber = PopOptInt(task);
                var fileNum = PopInt(task);
                // LOCK #n [, record]: lock del record (offset = record * 1 byte, length 1)
                // o dell'intero file (senza record).
                var offset = (recordNumber ?? 0) * 1L;
                try
                {
                    if (isLock) _runtime.File.Lock(fileNum, offset, 1);
                    else _runtime.File.Unlock(fileNum, offset, 1);
                }
                catch { /* lock non disponibile (es. stream non lockabile) */ }
                break;
            }
            case SysCall.IO_WATCH:
            {
                // push order [Path, Mask, Callback] (Callback in cima) — il compilatore passa
                // il nome della SUB callback (prima veniva scartato e il watcher era inerte).
                var callback = PopString(task) ?? "";
                var mask = PopInt(task);
                var path = PopString(task) ?? "";
                if (path.Length > 0)
                {
                    _fileWatchers ??= new System.Collections.Generic.Dictionary<string, System.IO.FileSystemWatcher>(StringComparer.OrdinalIgnoreCase);
                    _fileWatchCallbacks ??= new System.Collections.Generic.Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    var dir = System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path)) ?? ".";
                    var file = System.IO.Path.GetFileName(path);
                    var watcher = new System.IO.FileSystemWatcher(dir, file)
                    {
                        NotifyFilter = System.IO.NotifyFilters.LastWrite | System.IO.NotifyFilters.FileName | System.IO.NotifyFilters.Size
                    };
                    watcher.EnableRaisingEvents = true;
                    _fileWatchCallbacks[path] = callback;
                    // Event handler → dispatch della SUB come task GPM (parità VM/C):
                    // mai invocare la SUB sul thread del watcher (§0ter) — si accoda.
                    watcher.Changed += (_, _) => DispatchIoWatchCallback(path);
                    _fileWatchers[path] = watcher;
                }
                break;
            }
            case SysCall.IO_UNWATCH:
            {
                var path = PopString(task) ?? "";
                if (_fileWatchers != null && _fileWatchers.TryGetValue(path, out var watcher))
                {
                    watcher.Dispose();
                    _fileWatchers.Remove(path);
                }
                _fileWatchCallbacks?.Remove(path);
                break;
            }
            case SysCall.PRINT_FILE:
            {
                var vals = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    vals[i] = task.Sp > 0 ? task.PopRef() : null;
                // FileNumber puo' essere NIL invece di 0.0 quando la variabile passata non e'
                // mai stata scritta prima (variabile implicita non dichiarata) — stesso caso di
                // FileSubOpCode.OPEN sopra, stesso fix (NIL trattato come file number 0).
                if (vals.Length >= 2 && (vals[0] is double || vals[0] is null))
                {
                    var fileNum = vals[0] is double fnPf ? fnPf : 0.0;
                    var isJson = task.FileContentTypes.TryGetValue((int)fileNum, out var ct) && ct == 1;
                    string text;
                    if (isJson && vals.Length >= 2)
                    {
                        var jsonNode = ObjectToJsonNode(vals[1]);
                        text = jsonNode?.ToJsonString() ?? "{}";
                    }
                    else
                    {
                        // Coerente col PRINT normale: FormatPrintValue formatta le liste come
                        // [a, 10] (PRINT #F usava ToString() → List<object?> diventava
                        // 'System.Collections.Generic.List`1[...]').
                        text = string.Join("", vals.Skip(1).Select(v => FormatPrintValue(v))) + "\n";
                    }
                    if (task.FileHandles.TryGetValue((int)fileNum, out var handle))
                        _runtime.File.Write(handle, text);
                }
                break;
            }
            case SysCall.INPUT_FILE:
            {
                // Stack (compilatore): fileNumber, poi N hint (1=numerico, 0=stringa).
                var ifTargets = Math.Max(0, argCount - 1);
                var ifHints = new int[ifTargets];
                for (var hi = ifTargets - 1; hi >= 0; hi--)
                    ifHints[hi] = PopOptInt(task) ?? 0;
                var ifFileNum = PopOptInt(task);
                if (ifFileNum.HasValue && task.FileHandles.TryGetValue(ifFileNum.Value, out var ifh))
                {
                    var isJson = task.FileContentTypes.TryGetValue(ifFileNum.Value, out var ct) && ct == 1;
                    if (isJson)
                    {
                        var jsonText = _runtime.File.ReadAll(ifh);
                        object? parsed;
                        try
                        {
                            using var doc = System.Text.Json.JsonDocument.Parse(string.IsNullOrEmpty(jsonText) ? "{}" : jsonText);
                            parsed = JsonElementToObject(doc.RootElement);
                        }
                        catch
                        {
                            parsed = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                        }
                        for (var ip = 0; ip < ifTargets; ip++) task.PushRef(parsed);
                    }
                    else
                    {
                        // INPUT # canonico: UNA riga → campi CSV (quote-aware, TRUE/FALSE),
                        // campo i-esimo convertito per tipo (parità IL/VM-C/C99*).
                        var ifLine = _runtime.File.ReadLine(ifh);
                        var ifFields = ifLine == null ? null : IlRuntime.ParseCsvFields(ifLine);
                        for (var ip = 0; ip < ifTargets; ip++)
                        {
                            if (ifHints[ip] == 1) task.PushRef(IlRuntime.CsvFieldToDouble(ifFields, ip));
                            else task.PushRef(IlRuntime.CsvFieldToString(ifFields, ip));
                        }
                    }
                }
                else
                {
                    for (int ip = 0; ip < ifTargets; ip++)
                    {
                        if (ifHints[ip] == 1) task.PushRef(0.0);
                        else task.PushRef("");
                    }
                }
                break;
            }
            case SysCall.KILL_FILE:
            {
                var path = PopString(task);
                if (!string.IsNullOrEmpty(path))
                {
                    try { System.IO.File.Delete(path); } catch { }
                }
                break;
            }
            case SysCall.MKDIR:
            {
                var path = PopString(task);
                if (!string.IsNullOrEmpty(path))
                    _runtime.File.CreateDirectory(path);
                break;
            }
            case SysCall.RMDIR:
            {
                // Ordine di POP: l'emitter PCode pusha PRIMA il path e POI il flag ricorsivo
                // (Compiler.FileSystem.VisitRmDirStatement) → il flag è in cima allo stack.
                // Prima si poppava la stringa per prima (prendeva "0"/"-1") e si chiamava
                // `File.Delete(path)` su una DIRECTORY (no-op): `RMDIR "dir", TRUE` non
                // cancellava NULLA sulla gamba PCode (misurato: IL/VM-C rimuovono, PCode no).
                var recursive = task.Sp > 0 ? PopNum(task) != 0 : false;
                var path = PopString(task);
                // Stessa semantica di IlRuntime.RemoveDirectory (parità IL/PCode).
                if (!string.IsNullOrEmpty(path) && Directory.Exists(path))
                    Directory.Delete(path, recursive);
                break;
            }
            case SysCall.CHDIR:
            {
                var path = PopString(task);
                if (!string.IsNullOrEmpty(path))
                    _runtime.File.SetCurrentDirectory(path);
                break;
            }
            case SysCall.COPY_FILE:
            {
                var dest = PopString(task);
                var src = PopString(task);
                if (!string.IsNullOrEmpty(src) && !string.IsNullOrEmpty(dest))
                    _runtime.File.Copy(src, dest);
                break;
            }
            case SysCall.MOVE_FILE:
            {
                var dest = PopString(task);
                var src = PopString(task);
                if (!string.IsNullOrEmpty(src) && !string.IsNullOrEmpty(dest))
                    _runtime.File.Move(src, dest);
                break;
            }
            case SysCall.RENAME_FILE:
            {
                var newName = PopString(task);
                var oldName = PopString(task);
                if (!string.IsNullOrEmpty(oldName) && !string.IsNullOrEmpty(newName))
                    _runtime.File.Move(oldName, newName);
                break;
            }
            case SysCall.SEEK_FILE:
            {
                var skPos = (long)PopNum(task);
                var skFileNum = PopOptInt(task);
                // SEEK n, pos — COMANDO legacy, posizione 1-BASED (`SEEK 1, 1` = inizio, vedi
                // FilesIO.bas e il C99 C# che emette (pos - 1)). Prima qui si passava la
                // posizione così com'era (0-based): dopo `SEEK 1, 3` il PCode leggeva 3 mentre
                // IL/VMC/C99 leggevano 2 (§ MiniTests/io/io_misc.bas).
                if (skFileNum.HasValue && task.FileHandles.TryGetValue(skFileNum.Value, out var skh))
                    _runtime.File.Seek(skh, skPos > 1 ? skPos - 1 : 0, 0);
                break;
            }

            // ============================================================
            // MISSING CONSOLE I/O
            // ============================================================
            case SysCall.LINE_INPUT:
            {
                var prompt = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                ParkBlocking(task,
                    () => _runtime.Keyboard.ReadLine(prompt),
                    result => task.PushRef(result ?? ""));
                return;
            }
            case SysCall.INKEY:
            {
                // Timeout in millisecondi (INKEY k$, 4000 = 4s; 0/NIL = bloccante).
                var timeoutMs = task.Sp > 0 ? (int)SafeDouble(PopNum(task)) : 0;
                ParkAsync(task, PCodeWaitKind.Input,
                    ct => _runtime.Keyboard.ReadKeyAsync(ct),
                    key =>
                    {
                        DispatchKeyEvent(new CommonLib.Consoles.TerminalKeyInfo(
                            key.KeyChar, (int)key.Key,
                            key.Modifiers.HasFlag(ConsoleModifiers.Shift),
                            key.Modifiers.HasFlag(ConsoleModifiers.Alt),
                            key.Modifiers.HasFlag(ConsoleModifiers.Control)));
                        task.PushRef(key.KeyChar.ToString());
                    },
                    timeoutMs,
                    () => task.PushRef(""));
                return;
            }
            case SysCall.SET_ATTRIBUTE:
            {
                var attrVal = task.Sp > 0 ? task.PopRef()?.ToString()?.ToUpperInvariant() : "";
                var attrOn = task.Sp > 0 ? PopNum(task) != 0 : true;
                lock (_consoleLock) {
                    if (attrVal == "BOLD") _runtime.Console.Print(attrOn ? "\x1b[1m" : "\x1b[22m");
                    else if (attrVal == "UNDERLINE") _runtime.Console.Print(attrOn ? "\x1b[4m" : "\x1b[24m");
                    else if (attrVal == "INVERSE") _runtime.Console.Print(attrOn ? "\x1b[7m" : "\x1b[27m");
                }
                break;
            }
            case SysCall.CURSOR_STATE:
            {
                var csOn = task.Sp > 0 ? PopNum(task) != 0 : true;
                // Parita' IL: il cursore passa dal servizio grafico (NullGraphicsService in
                // headless -> nessun ANSI nello stdout catturato dagli oracoli).
                try { _runtime.Graphics.CursorState(csOn); } catch { }
                break;
            }
            case SysCall.CURSOR_SHAPE:
            {
                var csh = task.Sp > 0 ? task.PopRef()?.ToString()?.ToUpperInvariant() : "";
                int cshShape = 0;
                if (csh == "BLOCK") cshShape = 2;
                else if (csh == "UNDERLINE") cshShape = 4;
                else if (csh == "BAR") cshShape = 6;
                else if (int.TryParse(csh, out var cshN) && cshN >= 0 && cshN <= 6) cshShape = cshN;
                try { _runtime.Graphics.CursorShape(cshShape); } catch { }
                break;
            }

            // ============================================================
            // CLIPPER CONSOLE EXTENDED
            // ============================================================
            case SysCall.CCLS:
            {
                var color = task.Sp > 0 ? task.PopRef() : null;
                var resolved = ResolveColor(color);
                lock (_consoleLock)
                {
                    if (resolved.HasValue)
                        _runtime.Console.SetBackgroundColor(resolved.Value.R);
                    _runtime.Console.Clear();
                }
                break;
            }
            case SysCall.CLOCATE:
            {
                var bg = (int)PopNum(task);
                var fg = (int)PopNum(task);
                var col = (int)PopNum(task);
                var row = (int)PopNum(task);
                lock (_consoleLock)
                {
                    if (row >= 0 && col >= 0) _runtime.Console.SetCursorPosition(col - 1, row - 1);
                    if (fg >= 0) _runtime.Console.SetForegroundColor(fg);
                    if (bg >= 0) _runtime.Console.SetBackgroundColor(bg);
                }
                break;
            }
            case SysCall.CPRINT:
            {
                var cbg = (int)PopNum(task);
                var cfg = (int)PopNum(task);
                var text = task.PopRef()?.ToString() ?? "";
                var ccol = (int)PopNum(task);
                var crow = (int)PopNum(task);
                lock (_consoleLock)
                {
                    if (crow >= 0 && ccol >= 0) _runtime.Console.SetCursorPosition(ccol - 1, crow - 1);
                    if (cfg >= 0) _runtime.Console.SetForegroundColor(cfg);
                    if (cbg >= 0) _runtime.Console.SetBackgroundColor(cbg);
                    _runtime.Console.Print(text);
                }
                break;
            }
            case SysCall.CLEAR_RECT:
            {
                var cbg2 = (int)PopNum(task);
                var cfg2 = (int)PopNum(task);
                var cr = (int)PopNum(task);
                var cb = (int)PopNum(task);
                var cl = (int)PopNum(task);
                var ct = (int)PopNum(task);
                lock (_consoleLock)
                {
                    for (var y = ct; y <= cb; y++)
                        for (var x = cl; x <= cr; x++)
                            _runtime.Console.Print($"\x1b[{y};{x}H ");
                }
                break;
            }
            case SysCall.CSAVE_SCREEN:
            {
                if (task.Sp > 0) task.PopRef();
                lock (_consoleLock) _runtime.Console.Print("\x1b[?47h");
                task.PushRef(null);
                break;
            }
            case SysCall.CRESTORE_SCREEN:
            {
                if (task.Sp > 0) task.PopRef();
                lock (_consoleLock) _runtime.Console.Print("\x1b[?47l");
                task.PushRef(null);
                break;
            }

            // ============================================================
            // MISSING FILE I/O
            // ============================================================
            case SysCall.WRITE_FILE:
            {
                // Compiler: pusha FileNumber PRIMA, poi gli argomenti. Pop in ordine inverso.
                var wfValues = new System.Collections.Generic.List<object?>();
                for (int i = 0; i < argCount - 1; i++)
                    wfValues.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                var wfFileNum = PopOptInt(task);
                if (wfFileNum.HasValue && task.FileHandles.TryGetValue(wfFileNum.Value, out var wfH))
                {
                    // Formato WRITE # (stile BASIC): numeri nudi, stringhe tra virgolette
                    // (doppie " escape come ""), BIT TRUE/FALSE, campi separati da virgola.
                    var sb = new System.Text.StringBuilder();
                    for (int i = 0; i < wfValues.Count; i++)
                    {
                        if (i > 0) sb.Append(',');
                        var v = wfValues[i];
                        if (v is string s) sb.Append('"').Append(s.Replace("\"", "\"\"")).Append('"');
                        else if (v is bool b) sb.Append(b ? "TRUE" : "FALSE");
                        else if (v is double dv) { if (dv == -1.0) sb.Append("TRUE"); else if (dv == 0.0) sb.Append("FALSE"); else sb.Append(dv.ToString(System.Globalization.CultureInfo.InvariantCulture)); }
                        else if (v is null) sb.Append("NULL");
                        else sb.Append(Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture));
                    }
                    _runtime.File.WriteLine(wfH, sb.ToString());
                }
                break;
            }
            case SysCall.GET_FILE:
            {
                if (argCount >= 4)
                {
                    // GET #n, rec, <UDT>: push file, rec, descrittore, istanza UDT corrente → riempie
                    // l'istanza IN PLACE (campi per nome) e la rimette sullo stack.
                    var curUdt = task.Sp > 0 ? task.PopRef() : null;
                    var getDesc = task.Sp > 0 ? task.PopRef() as string : null;
                    var getRec = task.Sp > 0 ? task.PopRef() : null;
                    var getFile = task.Sp > 0 ? task.PopRef() : null;
                    var getFn = getFile is double gfd ? (int)gfd : getFile is int gfi ? gfi : 0;
                    if (getDesc != null && getDesc.StartsWith('@') && getFn > 0
                        && task.FileHandles.TryGetValue(getFn, out var bh) && _runtime.File.GetStream(bh) is { } bStream)
                    {
                        // GET #n, pos, <array di BYTE>: legge Length byte (EOF → 0) e riempie l'array in place.
                        if (getRec is double bpos && bpos > 0) bStream.Seek((long)bpos - 1, SeekOrigin.Begin);
                        var bufLen = curUdt switch { Array ba => ba.Length, System.Collections.IList bl => bl.Count, _ => 0 };
                        var bbuf = new byte[bufLen];
                        var bgot = 0;
                        while (bgot < bufLen) { var r = bStream.Read(bbuf, bgot, bufLen - bgot); if (r <= 0) break; bgot += r; }
                        switch (curUdt)
                        {
                            case byte[] bArr: Array.Copy(bbuf, bArr, bufLen); break;
                            case double[] dArr: for (var bi = 0; bi < bufLen; bi++) dArr[bi] = bbuf[bi]; break;
                            case object?[] oArr: for (var bi = 0; bi < bufLen; bi++) oArr[bi] = (double)bbuf[bi]; break;
                            case System.Collections.IList lArr: for (var bi = 0; bi < bufLen; bi++) lArr[bi] = (double)bbuf[bi]; break;
                        }
                        task.PushRef(curUdt);
                        break;
                    }
                    if (getDesc != null && getFn > 0 && task.FileHandles.TryGetValue(getFn, out var gh)
                        && _runtime.File.GetStream(gh) is { } gStream && CommonLib.RandomRecordCodec.IsRandom(gStream))
                    {
                        var (gNames, gTokens) = CommonLib.RandomRecordCodec.ParseDescriptor(getDesc);
                        var rc = CommonLib.RandomRecordCodec.Get(gStream, getRec is double gr ? (long)gr : 0, gTokens, out var gVals);
                        if (rc == -2) { RaiseBasicError(task, CommonLib.RandomRecordCodec.BadRecordLengthError, CommonLib.RandomRecordCodec.BadRecordLengthMessage); return; }
                        var inst = curUdt as Dictionary<string, object?> ?? new Dictionary<string, object?>();
                        for (var gi = 0; gi < gNames.Length && gi < gVals.Length; gi++)
                            inst[gNames[gi]] = gVals[gi] is long gl ? (double)gl : gVals[gi];
                        task.PushRef(inst);
                    }
                    else
                    {
                        RaiseBasicError(task, 5, "GET: destinazione UDT supportata solo su file RANDOM");
                        return;
                    }
                    break;
                }
                var recNum = task.Sp > 0 ? task.PopRef() : null;
                var fileNum = task.Sp > 0 ? task.PopRef() : null;
                var fn = fileNum is double d ? (int)d : fileNum is int i ? i : 0;
                // Check ISAM first
                var wa = ResolveIsamArea(task, fileNum);
                if (wa?.Engine != null)
                {
                    try
                    {
                        var record = wa.Engine.GetCurrentRecord();
                        if (record != null && wa.FieldSymbolIds != null)
                        {
                            // FieldNames keyed by COMPILER SymbolIds (what LOAD_MEMBER uses for lookup).
                            // wa.Udt riflette l'ORDINE FISICO su disco (letto da ReadSnapshot/BuildMinimalUdt),
                            // mentre wa.FieldSymbolIds è l'ORDINE DI DICHIARAZIONE del TYPE lato compilatore
                            // (pushato da VisitIsamUseStatement) — i due ordini NON coincidono per un file
                            // scritto da ISAM CONVERT DBF TO ISAM (lo schema ISAM viene costruito seguendo
                            // l'ordine fisico dei campi nel DBF sorgente, non l'ordine di dichiarazione del
                            // TYPE). Abbinare per POSIZIONE (wa.Udt.Fields[fi] con wa.FieldSymbolIds[fi])
                            // associava quindi il SymbolId sbagliato al valore sbagliato (es. Codice riceveva
                            // il valore di Valore e viceversa) ogni volta che i due ordini divergevano —
                            // scoperto testando DbIsamFullTests.bas sezione 11, dove ISAM GET dopo CONVERT
                            // DBF TO ISAM restituiva Codice e Valore scambiati. Fix: abbina per NOME campo
                            // (stesso confronto tronca-e-ignora-maiuscole già usato per i nomi campo DBF
                            // oltre i 10 caratteri), non per posizione.
                            var fieldNames = new Dictionary<int, string>();
                            var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                            dict["__class__"] = new Dictionary<string, object?> { ["FieldNames"] = fieldNames };
                            var declaredNames = wa.FieldNames;
                            for (int fi = 0; fi < wa.Udt!.Fields.Length; fi++)
                            {
                                var tf = wa.Udt.Fields[fi];
                                var engineSid = tf.SymbolId;
                                var fname = tf.Name.Lexeme;

                                var declaredIdx = -1;
                                if (declaredNames != null)
                                {
                                    declaredIdx = declaredNames.FindIndex(n => string.Equals(n, fname, StringComparison.OrdinalIgnoreCase));
                                    if (declaredIdx < 0)
                                    {
                                        declaredIdx = declaredNames.FindIndex(n =>
                                        {
                                            var truncated = n.Length > 10 ? n[..10] : n;
                                            return string.Equals(truncated, fname, StringComparison.OrdinalIgnoreCase);
                                        });
                                    }
                                }
                                var compilerSid = declaredIdx >= 0 && declaredIdx < wa.FieldSymbolIds!.Count
                                    ? wa.FieldSymbolIds[declaredIdx]
                                    : (fi < wa.FieldSymbolIds!.Count ? wa.FieldSymbolIds[fi] : engineSid);

                                var outName = declaredIdx >= 0 && declaredNames != null ? declaredNames[declaredIdx] : fname;
                                fieldNames[compilerSid] = outName;
                                dict[outName] = record.Fields.GetValueOrDefault(engineSid);
                            }
                            task.PushRef(dict);
                        }
                        else task.PushRef(null);
                    }
                    catch { task.PushRef(null); }
                }
                else
                {
                    var rn = recNum is double rd ? (int)rd : -1;
                    if (fn > 0 && task.FileHandles.TryGetValue(fn, out var h))
                        task.PushRef(_runtime.File.ReadAll(h));
                    else
                        task.PushRef(null);
                }
                break;
            }
            case SysCall.PUT_FILE:
            {
                // Ordine di push (VisitPutFileStatement): FileNumber, RecordNumber-o-nil, Source.
                // Pop in ordine LIFO inverso.
                var putDesc = argCount >= 4 && task.Sp > 0 ? task.PopRef() as string : null;
                var data = task.Sp > 0 ? task.PopRef() : null;
                var recNumRaw = task.Sp > 0 ? task.PopRef() : null;
                var fileNumRaw = task.Sp > 0 ? task.PopRef() : null;

                // ISAM.PUT(alias, record) e ISAM PUT alias, record condividono questo syscall
                // con il PUT # classico su file binari — controlla prima ISAM (come GET_FILE).
                var wa = ResolveIsamArea(task, fileNumRaw);
                if (wa?.Engine != null && data is System.Collections.IDictionary srcDict)
                {
                    try
                    {
                        var fields = new Dictionary<int, object?>();
                        foreach (var tf in wa.Udt!.Fields)
                        {
                            var fn = tf.Name.Lexeme;
                            object? val = null;
                            if (srcDict.Contains(fn)) val = srcDict[fn];
                            else foreach (var k in srcDict.Keys)
                                if (string.Equals(k!.ToString(), fn, StringComparison.OrdinalIgnoreCase))
                                { val = srcDict[k!]; break; }
                            fields[tf.SymbolId] = val;
                        }
                        wa.Engine!.Put(new Basic.UdtInstance(wa.Udt, fields), wa.Engine.Evaluator);
                    }
                    catch { }
                }
                else
                {
                    var fileNum = fileNumRaw is double fd ? (int)fd : fileNumRaw is int fi ? fi : (int?)null;
                    if (fileNum.HasValue && task.FileHandles.TryGetValue(fileNum.Value, out var h))
                    {
                        // PUT di un array di BYTE (descriptor "@y"): un byte grezzo per elemento.
                        if (putDesc != null && putDesc.StartsWith('@') && data is not null)
                        {
                            byte[] pbytes;
                            switch (data)
                            {
                                case byte[] pb: pbytes = pb; break;
                                case Array pa: pbytes = new byte[pa.Length]; for (var bi = 0; bi < pbytes.Length; bi++) pbytes[bi] = (byte)((long)Convert.ToDouble(pa.GetValue(bi) ?? 0.0) & 0xFF); break;
                                case System.Collections.IList pl: pbytes = new byte[pl.Count]; for (var bi = 0; bi < pbytes.Length; bi++) pbytes[bi] = (byte)((long)Convert.ToDouble(pl[bi] ?? 0.0) & 0xFF); break;
                                default: pbytes = Array.Empty<byte>(); break;
                            }
                            if (recNumRaw is double ppos && ppos > 0) _runtime.File.Seek(h, (long)ppos - 1, 0);
                            _runtime.File.WriteBytes(h, pbytes);
                            break;
                        }
                        // PUT di un UDT su file RANDOM: record a layout canonico (CommonLib.RandomRecordCodec).
                        if (putDesc != null && data is System.Collections.IDictionary putDict
                            && _runtime.File.GetStream(h) is { } pStream && CommonLib.RandomRecordCodec.IsRandom(pStream))
                        {
                            var (pNames, pTokens) = CommonLib.RandomRecordCodec.ParseDescriptor(putDesc);
                            var pVals = new object?[pNames.Length];
                            for (var pi = 0; pi < pNames.Length; pi++)
                            {
                                if (putDict.Contains(pNames[pi])) pVals[pi] = putDict[pNames[pi]];
                                else foreach (var pk in putDict.Keys)
                                    if (string.Equals(pk?.ToString(), pNames[pi], StringComparison.OrdinalIgnoreCase)) { pVals[pi] = putDict[pk!]; break; }
                            }
                            var prc = CommonLib.RandomRecordCodec.Put(pStream, recNumRaw is double prd ? (long)prd : 0, pTokens, pVals);
                            if (prc == -2) { RaiseBasicError(task, CommonLib.RandomRecordCodec.BadRecordLengthError, CommonLib.RandomRecordCodec.BadRecordLengthMessage); return; }
                            break;
                        }
                        // byte[] (es. FROMBASE64) → scrittura RAW dei byte; altrimenti stringa.
                        if (data is byte[] bytes)
                            _runtime.File.WriteBytes(h, bytes);
                        else
                            _runtime.File.Write(h, data?.ToString() ?? "");
                    }
                }
                break;
            }
            case SysCall.LINE_INPUT_FILE:
            {
                var fileNum = PopOptInt(task);
                if (fileNum.HasValue && task.FileHandles.TryGetValue(fileNum.Value, out var h))
                    task.PushRef(_runtime.File.ReadLine(h) ?? "");
                else
                    task.PushRef("");
                break;
            }
            case SysCall.XCOPY_DIR:
            {
                var xcOverwrite = task.Sp > 0 && PopNum(task) != 0;
                var xcRecursive = task.Sp > 0 ? PopNum(task) != 0 : false;
                var xcDest = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var xcSrc = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try {
                    if (Directory.Exists(xcSrc)) {
                        if (!Directory.Exists(xcDest)) Directory.CreateDirectory(xcDest);
                        foreach (var f in Directory.GetFiles(xcSrc)) File.Copy(f, Path.Combine(xcDest, Path.GetFileName(f)), xcOverwrite);
                        if (xcRecursive) foreach (var d in Directory.GetDirectories(xcSrc))
                            XCopyDirRec(d, Path.Combine(xcDest, Path.GetFileName(d)));
                    }
                } catch { }
                break;
            }

            // ============================================================
            // SQL DATABASE
            // ============================================================
            case SysCall.SQL_OPEN:
            {
                var alias = PopString(task);
                var path = PopString(task);
                try
                {
                    // SQLite non crea le directory: per path file ('Work/x.db') crea la
                    // cartella se manca (il runner pulisce 'Work/' ma non la ricrea).
                    if (!string.IsNullOrEmpty(path) && !path.Equals(":memory:", StringComparison.OrdinalIgnoreCase))
                    {
                        var sqlDir = Path.GetDirectoryName(Path.GetFullPath(path));
                        if (!string.IsNullOrEmpty(sqlDir) && !Directory.Exists(sqlDir))
                            Directory.CreateDirectory(sqlDir);
                    }
                    var connStr = (path ?? ":memory:").Equals(":memory:", StringComparison.OrdinalIgnoreCase)
                        ? "Data Source=:memory:"
                        : (path != null && path.Contains('=') ? path : $"Data Source={path}");
                    var driver = new Basic.Databases.Rdd.SqlDriver();
                    driver.Open(new Basic.Databases.Rdd.DbOpenInfo
                    {
                        FileName = connStr,
                        // L'alias NON è il nome tabella: SQL OPEN "<db>" ALIAS x apre il
                        // DATABASE; la tabella/query arriva da EXEC/QUERY. Con Alias = alias,
                        // SqlDriver.LoadTableData farebbe "SELECT * FROM x" → "no such table:
                        // x" (stesso fix di IlRuntime.SqlOpen). L'area viene registrata sotto
                        // l'alias reale qui sotto.
                        Alias = "",
                        Driver = "SQLite"
                    });
                    _dataSession.RegisterDirect(_dataSession.AllocateArea(), driver, alias ?? "");
                    task.PushRef(12345.0);
                }
                catch { task.PushRef((double)_runtime.Database.SqlOpen(path ?? "")); }
                break;
            }
            case SysCall.SQL_CLOSE:
            {
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").Close(); _dataSession.Unregister(_dataSession.FindAreaByAlias(alias ?? "")); } catch { }
                break;
            }
            case SysCall.SQL_EXEC:
            {
                // Both statement and expression paths now push paramCount at the end.
                // Stack bottom→top: [alias, sql, param1, ..., paramCount]
                int extraItems = argCount - 2;
                var allExtra = new System.Collections.Generic.List<object?>();
                for (int i = 0; i < extraItems; i++)
                    allExtra.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                // First item is paramCount (pushed last, on top)
                int pc = allExtra.Count > 0 ? (int)Convert.ToDouble(allExtra[0]) : 0;
                var paramList = new System.Collections.Generic.List<object?>();
                for (int i = 1; i < allExtra.Count; i++)
                    paramList.Add(allExtra[i]);
                var sql = PopString(task);
                var alias = PopString(task);
                // Coarse-park (regola GPM, bug 2026-10-01): la forma COMANDO `SQL EXEC`
                // compila a OP_SYSCALL (non a OP_CALL_BUILTIN SQLEXEC), quindi il park del
                // builtin non scattava mai qui: riscritture csv/parquet bloccavano il worker.
                // Il driver va risolto QUI, sincrono, sul thread che esegue correttamente lo
                // statement (via ResolveSqlDriver / _dataSession, un campo catturato
                // direttamente da _runtime) — MAI dentro la lambda di ParkBlocking tramite
                // IlRuntime.Current (o IlRuntime.SqlExec/SqlIsHeavyWrite, che lo usano
                // internamente): _current è [ThreadStatic] e un thread del ThreadPool .NET
                // riciclato da un'esecuzione PRECEDENTE in questo stesso processo (altro file
                // .bas, altro host) può restare con un valore stale quando viene riusato come
                // thread di continuazione di Task.Run dentro ParkBlocking — SqlExec risolveva
                // l'alias sul DataSession SBAGLIATO (quello host precedente), eseguendo la
                // scrittura su una connessione estranea e lasciando quella vera (risolta più
                // avanti via ResolveSqlDriver, che non passa da IlRuntime.Current) senza mai
                // la tabella appena "creata" — bug SQL-Parquet-CoarsePark-Race-Cross-
                // Test ("no such table" sul SELECT dopo scritture apparentemente riuscite).
                // Regola GPM (Knowledge/GPM-Implementazione-Attuale-5-Backend.md §header):
                // "mai [ThreadStatic] ri-applicato al resume" — il contesto va catturato e
                // passato esplicitamente al lavoro parcheggiato.
                Basic.Databases.Rdd.SqlDriver? resolvedDriver = null;
                try { resolvedDriver = ResolveSqlDriver(alias ?? ""); } catch { /* "database non aperto": gestito sotto come 0.0 */ }
                if (resolvedDriver != null && resolvedDriver.IsHeavyWrite(sql ?? ""))
                {
                    var heavyDriver = resolvedDriver;
                    var heavyParams = paramList.ToArray();
                    var heavySql = sql ?? "";
                    ParkBlocking(task,
                        () => heavyDriver.ExecRaw(heavySql, heavyParams, null),
                        r => task.PushRef((double)r));
                    return;
                }
                try { var rows = resolvedDriver!.ExecRaw(sql ?? "", paramList, null); task.PushRef((double)rows); }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.SQL_QUERY:
            {
                // argCount == 2: expression path (SQL.QUERY(alias, sql))
                // argCount >= 3: statement path (SQL QUERY alias, rs, sql [, params...])
                string? sql = null, alias = null;
                if (argCount >= 3)
                {
                    // Pop params first (they're on top)
                    int pcFromArg = argCount - 3;
                    var paramList = new System.Collections.Generic.List<object?>();
                    for (int pi = 0; pi < pcFromArg; pi++)
                        paramList.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                    var pc = (int)PopNum(task);
                    sql = PopString(task);
                    alias = PopString(task);
                    try { var rsId = ResolveSqlDriver(alias ?? "").QueryRaw(sql ?? "", paramList, null); var rsDict = new System.Collections.Generic.Dictionary<string, object?> { ["__rsid__"] = (double)rsId }; task.PushRef(rsDict); }
                    catch { task.PushRef(null); }
                }
                else
                {
                    sql = PopString(task);
                    alias = PopString(task);
                    try { var rsId = ResolveSqlDriver(alias ?? "").QueryRaw(sql ?? "", null, null); var rsDict = new System.Collections.Generic.Dictionary<string, object?> { ["__rsid__"] = (double)rsId }; task.PushRef(rsDict); }
                    catch { task.PushRef(null); }
                }
                break;
            }
            case SysCall.SQL_FETCH:
            {
                var rsDict = task.Sp > 0 ? task.PopRef() as System.Collections.Generic.Dictionary<string, object?> : null;
                if (rsDict == null || !rsDict.TryGetValue("__rsid__", out var rsIdObj))
                {
                    task.PushRef(false);
                    break;
                }
                var rsId = (int)(double)rsIdObj!;
                try
                {
                    bool hasRow = FindAnySqlDriver()?.Fetch(rsId) ?? false;
                    if (hasRow)
                    {
                        var row = FindAnySqlDriver()?.GetCurrentRow(rsId);
                        if (row != null)
                        {
                            var keysToRemove = new System.Collections.Generic.List<string>();
                            foreach (var k in rsDict.Keys)
                                if (k != "__rsid__") keysToRemove.Add(k);
                            foreach (var k in keysToRemove) rsDict.Remove(k);
                            foreach (var kv in row)
                                rsDict[kv.Key] = kv.Value;
                        }
                    }
                    task.PushRef(hasRow ? -1.0 : 0.0);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.SQL_CLOSE_RESULT_SET:
            {
                var rsDict = task.Sp > 0 ? task.PopRef() as System.Collections.Generic.Dictionary<string, object?> : null;
                if (rsDict != null && rsDict.TryGetValue("__rsid__", out var rsIdObj))
                {
                    try { FindAnySqlDriver()?.CloseResultSet((int)(double)rsIdObj!); } catch { }
                }
                break;
            }
            case SysCall.SQL_ATTACH:
            {
                var schema = PopString(task);
                var path = PopString(task);
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").Attach(path ?? "", schema ?? ""); }
                catch { }
                break;
            }
            case SysCall.SQL_DETACH:
            {
                var schema = PopString(task);
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").Detach(schema ?? ""); }
                catch { }
                break;
            }
            case SysCall.SQL_BEGIN_TRANSACTION:
            {
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").BeginTransaction(); } catch { }
                break;
            }
            case SysCall.SQL_COMMIT:
            {
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").Commit(); } catch { }
                break;
            }
            case SysCall.SQL_ROLLBACK:
            {
                var alias = PopString(task);
                try { ResolveSqlDriver(alias ?? "").Rollback(); } catch { }
                break;
            }
            case SysCall.SQL_SAVEPOINT:
            {
                var spName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { ResolveSqlDriver(alias ?? "").Savepoint(spName); } catch { }
                break;
            }
            case SysCall.SQL_RELEASE_SAVEPOINT:
            {
                var spName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { ResolveSqlDriver(alias ?? "").ReleaseSavepoint(spName); } catch { }
                break;
            }
            case SysCall.SQL_ROLLBACK_TO_SAVEPOINT:
            {
                var spName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { ResolveSqlDriver(alias ?? "").RollbackToSavepoint(spName); } catch { }
                break;
            }
            case SysCall.SQL_BACKUP:
            {
                var bkFile = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var bkAlias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { ResolveSqlDriver(bkAlias ?? "").ExecRaw("VACUUM INTO '" + (bkFile ?? "backup.db").Replace("'", "''") + "'", null, null); } catch { }
                break;
            }

            // ============================================================
            // ERROR HANDLING
            // ============================================================
            case SysCall.THROW_ERROR:
            {
                var msg = PopString(task);
                var errCode = task.Sp > 0 ? task.PopRef() : 0.0;
                RaiseBasicError(task, Convert.ToDouble(errCode ?? 0.0, System.Globalization.CultureInfo.InvariantCulture), msg!);
                return;
            }

            // ============================================================
            // JSON
            // ============================================================
            case SysCall.JSON_TO_OBJECT:
            {
                var jsonStr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "{}" : "{}";
                try
                {
                    using var doc = System.Text.Json.JsonDocument.Parse(jsonStr);
                    task.PushRef(JsonElementToObject(doc.RootElement));
                }
                catch
                {
                    task.PushRef(new Dictionary<string, object?>());
                }
                break;
            }
            case SysCall.JSON_SAVE:
            {
                var savePath = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var saveObj = task.Sp > 0 ? task.PopRef() : null;
                if (!string.IsNullOrEmpty(savePath))
                {
                    try
                    {
                        var json = System.Text.Json.JsonSerializer.Serialize(saveObj);
                        File.WriteAllText(savePath, json);
                    }
                    catch { }
                }
                break;
            }
            case SysCall.JSON_WRITE_VALUE:
            {
                // Il compilatore pushea: key, value, section (sezione può essere NIL).
                var jwSection = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var jwVal = task.Sp > 0 ? task.PopRef() : null;
                var jwKey = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var file = Path.Combine(Directory.GetCurrentDirectory(), "app.settings.json");
                CommonLib.FileReadWriteUtils.WriteValue(file, jwSection, jwKey, Convert.ToString(jwVal, System.Globalization.CultureInfo.InvariantCulture) ?? "");
                break;
            }
            case SysCall.JSON_WRITE_SECTION:
            {
                // Il compilatore pushea: source, section.
                var jsSec = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var jsSource = task.Sp > 0 ? task.PopRef() : null;
                var jsVals = new System.Collections.Generic.Dictionary<string, object?>();
                if (jsSource is System.Collections.IDictionary jsd)
                {
                    foreach (System.Collections.DictionaryEntry e in jsd)
                        jsVals[Convert.ToString(e.Key, System.Globalization.CultureInfo.InvariantCulture) ?? ""] = e.Value;
                }
                var file2 = Path.Combine(Directory.GetCurrentDirectory(), "app.settings.json");
                CommonLib.FileReadWriteUtils.WriteSection(file2, jsSec, jsVals);
                break;
            }

            case SysCall.ISAM_USE:
            {
                // Pop fieldCount first (top), then fields (sid, len, name)* in reverse of push order
                var fieldCount = (int)PopNum(task);
                var fieldNames = new List<string>();
                var fieldLengths = new List<int>();
                var fieldSymbolIds = new List<int>();
                var fieldTypes = new List<string>();
                for (int fi = 0; fi < fieldCount; fi++)
                {
                    var typeCode = task.Sp > 0 ? task.PopRef()?.ToString() ?? "STRING" : "STRING";
                    var ssid = (int)PopNum(task);
                    var flen = (int)PopNum(task);
                    var sname = task.Sp > 0 ? task.PopRef()?.ToString() ?? "F" + fi : "F" + fi;
                    fieldNames.Add(sname);
                    fieldLengths.Add(flen);
                    fieldSymbolIds.Add(ssid);
                    fieldTypes.Add(typeCode);
                }
                // Pushed in definition order, popped in reverse → reverse back
                fieldNames.Reverse();
                fieldLengths.Reverse();
                fieldSymbolIds.Reverse();
                fieldTypes.Reverse();
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                
                var idxCount = (int)PopNum(task);
                // TUTTI i campi di INDEXED BY (in ordine). Il primo è la chiave primaria, i
                // successivi sono indici SECONDARI (INDEXED BY a, b, c → PK=a, secondary=b, c).
                // Prima si teneva solo ki==0 e si scartavano gli altri: lo schema registrava una
                // COPIA del PK come unico "secondario" → SET ORDER su un campo secondario era un
                // no-op e SEEK su indice secondario non trovava mai (divergenza IL vs PCode).
                var keyFieldNames = new List<string>();
                for (int ki = 0; ki < idxCount; ki++)
                {
                    var desc = PopNum(task) != 0;
                    var unsafeFlag = PopNum(task) != 0;
                    var keyExpr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    var hashIdx = keyExpr.IndexOf('#');
                    keyFieldNames.Add(hashIdx >= 0 ? keyExpr[..hashIdx] : keyExpr);
                }
                string pkFieldName = keyFieldNames.Count > 0 ? keyFieldNames[0] : "ID";
                // while (task.Sp > 4) { task.PopRef(); }
                var isUnsafe = PopNum(task) != 0;
                var locking = (int)PopNum(task);
                var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                fileName = Basic.IlBackend.IlRuntime.ResolveDbPath(fileName);
                try
                {
                    // ISAM OPEN crea il file: crea anche la cartella se manca
                    // (il runner pulisce 'Work/' ma non la ricrea; un path 'Work/x.isdb'
                    // su cartella inesistente faceva fallire CreateNew in silenzio).
                    var isamDir = Path.GetDirectoryName(Path.GetFullPath(fileName));
                    if (!string.IsNullOrEmpty(isamDir) && !Directory.Exists(isamDir))
                        Directory.CreateDirectory(isamDir);
                    var lockMode = locking == 1 ? Basic.Databases.LockingStrategy.CrossProcess : Basic.Databases.LockingStrategy.IntraProcess;
                    var fields = new List<Basic.ISAM.Config.IsamFieldSchema>();
                    string pkName = idxCount > 0 ? (pkFieldName.Length > 0 ? pkFieldName : "ID") : "ID";
                    for (int gi = 0; gi < fieldNames.Count; gi++)
                    {
                        var flen = gi < fieldLengths.Count ? fieldLengths[gi] : 0;
                        var typeStr = gi < fieldTypes.Count ? fieldTypes[gi] : "STRING";
                        // Decoder gemello dei codici emessi da Compiler.Isam.cs
                        // (VisitIsamUseStatement) — mappava SOLO LONG/INTEGER/DOUBLE, qualunque
                        // altro codice (CURRENCY, BIT, DATE, ecc., mai emesso PRIMA del fix
                        // lato compiler ma qui comunque completato per lo stesso motivo)
                        // cadeva sempre su StringType, salvando il campo come testo nello
                        // schema ISAM su disco.
                        Basic.DataType dt = typeStr switch
                        {
                            "LONG" => new Basic.LongType(),
                            "INTEGER" => new Basic.IntegerType(),
                            "DOUBLE" => new Basic.DoubleType(),
                            "CURRENCY" => new Basic.CurrencyType(),
                            "BIT" => new Basic.BitType(),
                            "BYTE" => new Basic.ByteType(),
                            "SBYTE" => new Basic.SByteType(),
                            "SINGLE" => new Basic.SingleType(),
                            "ULONG" => new Basic.ULongType(),
                            "UINTEGER" => new Basic.UIntegerType(),
                            "QUAD" => new Basic.QuadType(),
                            "UQUAD" => new Basic.UQuadType(),
                            "DATE" => new Basic.DateType(),
                            "TIME" => new Basic.TimeType(),
                            "DATETIME" => new Basic.DateTimeType(),
                            "TIMESPAN" => new Basic.TimeSpanType(),
                            "JSON" => new Basic.JsonType(),
                            _ => new Basic.StringType()
                        };
                        fields.Add(new Basic.ISAM.Config.IsamFieldSchema(fieldNames[gi], dt, flen > 0 ? flen : (dt is Basic.StringType ? 50 : 0)));
                    }
                    var pkIdx = fieldNames.IndexOf(pkName);
                    var pkLen = pkIdx >= 0 && pkIdx < fieldLengths.Count && fieldLengths[pkIdx] > 0 ? fieldLengths[pkIdx] : 50;
                    // Parità engine C#/C99Native: con INDEXED BY il PK E il secondary sono il campo
                    // indicizzato; senza INDEXED BY, PK vuoto e 0 secondary.
                    Basic.ISAM.Config.IndexDefinition pkDef;
                    var secondary = new List<Basic.ISAM.Config.IndexDefinition>();
                    if (idxCount > 0)
                    {
                        pkDef = new Basic.ISAM.Config.IndexDefinition(pkName, pkName, pkLen, true, false);
                        // TUTTE le chiavi di INDEXED BY sono anche indici secondari (la prima
                        // INCLUSA: parità col riferimento IL BuildIsamSchemaJson, che mette tutti
                        // i KeyFields in "Indexes"). Escluderla faceva divergere lo schema
                        // (ISAM.INFO: secondary_indexes/schema_length/size_bytes).
                        for (int si = 0; si < keyFieldNames.Count; si++)
                        {
                            var sname = keyFieldNames[si];
                            if (string.IsNullOrEmpty(sname)) continue;
                            var sidx = fieldNames.IndexOf(sname);
                            var slen = sidx >= 0 && sidx < fieldLengths.Count && fieldLengths[sidx] > 0 ? fieldLengths[sidx] : 50;
                            secondary.Add(new Basic.ISAM.Config.IndexDefinition(sname, sname, slen, false, false));
                        }
                    }
                    else
                    {
                        pkDef = new Basic.ISAM.Config.IndexDefinition("", "", 20, true, false);
                    }
                    var schema = new Basic.ISAM.Config.IsamSchema(typeName, fields, pkDef, secondary);
                    var isamAlreadyExists = Basic.ISAM.IsamMemPath.IsMemPath(fileName)
                        ? Basic.ISAM.IsamMemStore.Exists(Basic.ISAM.IsamMemPath.NormalizeKey(fileName))
                        : File.Exists(fileName);
                    // ISAM USE/OPEN è sia "apri esistente" che "crea al primo uso" (semantica xBase):
                    // CreateNew usa FileMode.Create, che SOVRASCRIVE incondizionatamente un file già
                    // presente. Un file scritto in precedenza nella STESSA esecuzione (es. ISAM CONVERT
                    // DBF TO ISAM seguito da ISAM OPEN sullo stesso path) veniva quindi azzerato appena
                    // riaperto, perdendo tutti i record appena convertiti — scoperto testando
                    // DbIsamFullTests.bas sezione 11 ("Verifica dati convertiti:" restituiva sempre
                    // EOF immediato nonostante ISAM CONVERT avesse scritto correttamente 2 record).
                    if (!isamAlreadyExists)
                        Basic.ISAM.IsamFileHandle.CreateNew(fileName, schema, System.Text.Encoding.UTF8);
                    // Per mem:// ReadSnapshot non puo' aprire il path (non e' un file): lo schema
                    // e' gia' quello appena scritto, lo riuso direttamente. Per i file reali rileggi.
                    Basic.ISAM.Config.IsamSchema effSchema = schema;
                    if (!Basic.ISAM.IsamMemPath.IsMemPath(fileName))
                        effSchema = Basic.Tools.Isam.IsamOfflineToolkit.ReadSnapshot(fileName).Schema;
                    var udt = Basic.Tools.Isam.IsamOfflineToolkit.BuildMinimalUdt(effSchema);
                    var handle = new Basic.ISAM.IsamFileHandle(fileName, effSchema, udt, lockMode);
                    var engine = new Basic.ISAM.Engine(handle);
                    engine.Evaluator = (expr, ctx, tt) => { if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber(); if (ctx != null && ctx.TryGetValue(expr, out var val)) return val; return expr; };
                    var areaNum = _dataSession.AllocateArea();
                    _dataSession.RegisterDirect(areaNum, new Basic.Databases.Rdd.IsamDriver(engine, udt, alias), alias);
                    var wa = _dataSession.GetWorkArea(areaNum); if (wa != null) { wa.Meta["Udt"] = udt; wa.Meta["Schema"] = effSchema; wa.Meta["FieldSymbolIds"] = fieldSymbolIds; wa.Meta["FieldNames"] = fieldNames; wa.Meta["SchemaBuffer"] = Basic.Databases.Rdd.SchemaBuffer.FromUdt(udt); }
                    task.CurrentIsamArea = areaNum;
                    task.PushRef((double)areaNum);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("ISAM_USE ERROR: " + ex.Message); task.PushRef(0.0); }
                break;
            }
            
            case SysCall.ISAM_SELECT:
            {
                var area = task.Sp > 0 ? task.PopRef() : null;
                SetCurrentIsamArea(task, area);
                task.PushRef(null);
                break;
            }
            
            case SysCall.ISAM_RECORD:
            {
                var location = (int)PopNum(task);
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var engine = ResolveIsamEngine(task, fileRef);
                if (engine != null)
                {
                    try
                    {
                        engine.Go(location == (int)TokenKind.Top ? Basic.ISAM.Config.CursorMoveOperation.Top : Basic.ISAM.Config.CursorMoveOperation.Bottom);
                        // ISAM SET RELATION vive su IsamDriver (Databases/Rdd), ma questa
                        // syscall si muove direttamente sull'ISAM.Engine sottostante,
                        // bypassando IsamDriver.GoTop/GoBottom — senza questa chiamata
                        // esplicita nessun figlio in relazione si riposiziona mai (vedi
                        // DbIsamFullTests-Relazione-Cursore-TODO.md).
                        SyncChildRelations(ResolveIsamDriver(task, fileRef));
                    }
                    catch { }
                }
                break;
            }

            case SysCall.ISAM_CLOSE:
            {
                // ISAM CLOSE [alias]: libera la work area (Driver.Close -> Engine.Dispose + alias rimosso).
                // arg = alias stringa, numero d'area, o nil = area ISAM corrente.
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var closeArea = ResolveAreaNum(task, fileRef, task.CurrentIsamArea);
                if (closeArea > 0 && _dataSession.GetWorkArea(closeArea)?.Driver is Basic.Databases.Rdd.IsamDriver)
                {
                    try { _dataSession.Unregister(closeArea); } catch { }
                    if (task.CurrentIsamArea == closeArea) task.CurrentIsamArea = 0;
                }
                break;
            }

            case SysCall.ISAM_SKIP:
            {
                var count = task.Sp > 0 ? task.PopRef() : null;
                var skipCount = count is double dc ? (int)dc : count is int si ? si : 1;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var engine = ResolveIsamEngine(task, fileRef);
                if (engine != null)
                {
                    try
                    {
                        engine.Skip(skipCount);
                        // Vedi il commento gemello in ISAM_RECORD.
                        SyncChildRelations(ResolveIsamDriver(task, fileRef));
                    }
                    catch { }
                }
                break;
            }
            
            case SysCall.ISAM_APPEND:
            {
                var source = task.Sp > 0 ? task.PopRef() : null;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa != null && source is System.Collections.IDictionary srcDict)
                {
                    try
                    {
                        var fields = new Dictionary<int, object?>();
                        foreach (var tf in wa.Udt!.Fields)
                        {
                            var fn = tf.Name.Lexeme;
                            object? val = null;
                            if (srcDict.Contains(fn)) val = srcDict[fn];
                            else foreach (var k in srcDict.Keys)
                                if (string.Equals(k!.ToString(), fn, StringComparison.OrdinalIgnoreCase))
                                { val = srcDict[k!]; break; }
                            fields[tf.SymbolId] = val;
                        }
                        var udtInst = new Basic.UdtInstance(wa.Udt, fields);
                        wa.Engine!.Append(udtInst, wa.Engine.Evaluator);
                        // Bug ISAM-Comandi-Parita-5-Backend (LASTREC=0 dopo APPEND senza
                        // navigazione): Engine.Append bypassa IsamDriver.Append, quindi
                        // _recCount del driver non si aggiorna da solo.
                        wa.Driver?.SyncCursorState();
                        task.PushRef(12345.0);
                    }
                    catch (Exception ex) { System.Console.Error.WriteLine("ISAM_APPEND ERROR: " + ex.Message); task.PushRef(0.0); }
                }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.ISAM_SEEK:
            {
                var seekVal = task.Sp > 0 ? task.PopRef() : null;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null && seekVal != null)
                {
                    try
                    {
                        // Usa l'indice ATTIVO (SET ORDER), non sempre la PK: `ISAM SEEK … USING "X"`
                        // (e SEEK dopo SET ORDER TO "X") deve cercare nell'indice X. Il compiler PCode
                        // scarta la clausola USING, ma l'ordine attivo è già stato commutato da SET ORDER
                        // — stesso comportamento dell'IL (IsamSeek → IsamDriver.Seek usa l'ordine attivo).
                        var seekDef = wa.Schema!.PrimaryKey;
                        var actName = wa.Engine.ActiveIndexName();
                        if (!string.IsNullOrEmpty(actName))
                        {
                            foreach (var sk in wa.Schema.SecondaryKeys)
                                if (string.Equals(sk.Name, actName, StringComparison.OrdinalIgnoreCase)) { seekDef = sk; break; }
                        }
                        // NON convertire seekVal in stringa qui: SchemaManager.FormatKeyString ha rami di
                        // formattazione diversi per numeri (zero-padded, es. G17 per double) e stringhe
                        // (solo uppercase, nessun padding) — passare gia' una stringa forzava SEMPRE il
                        // ramo stringa, producendo per le chiavi numeriche una rappresentazione binaria
                        // diversa da quella scritta da ISAM_APPEND (che passa il valore nativo dal campo
                        // UDT), quindi SEEK non trovava mai un record appena scritto con chiave numerica.
                        var keyBytes = Basic.ISAM.SchemaManager.SerializeKeyValue(seekVal, seekDef);
                        wa.Engine.Seek(keyBytes);
                        task.LastIsamFound = wa.Engine.WasFound();
                        // Vedi il commento gemello in ISAM_RECORD/ISAM_SKIP.
                        SyncChildRelations(ResolveIsamDriver(task, fileRef));
                    }
                    catch { task.LastIsamFound = false; }
                }
                else task.LastIsamFound = false;
                task.PushRef(null);
                break;
            }

            case SysCall.ISAM_EOF:
            {
                var fileRef = argCount >= 1 && task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                task.PushRef(wa?.Engine != null && wa.Engine.IsEof() ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_CREATE:
            {
                var name = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.Create(name) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_ADD_AGENT:
            {
                var teamName = PopString(task) ?? "";
                var agentName = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.AddAgent(teamName, agentName) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_REMOVE_AGENT:
            {
                var teamName = PopString(task) ?? "";
                var agentName = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.RemoveAgent(teamName, agentName) ? -1.0 : 0.0);
                break;
            }
            case SysCall.MESSAGE_OP: HandleMessageOp(task); break;
            case SysCall.ON_MESSAGE: HandleOnMessage(task); break;
            case SysCall.SIGNAL_SUBSCRIBE: HandleSignalSubscribe(task); break;
            case SysCall.ON_SIGNAL: HandleOnSignal(task); break;
            case SysCall.SIGNAL_EMIT: HandleSignalEmit(task); break;
            case SysCall.SIGNAL_UNSUBSCRIBE: HandleSignalUnsubscribe(task); break;
            case SysCall.SIGNAL_PING: break;
            case SysCall.TEAM_MESSAGE:
            {
                var teamName = PopString(task) ?? "";
                var msg = task.PopRef()?.ToString() ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.PostMessage(teamName, "user", msg) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_RUN:
            {
                var teamName = PopString(task) ?? "";
                var taskText = task.PopRef()?.ToString() ?? "";
                var team = Basic.Agents.TeamServer.Instance.Get(teamName);
                if (team is null) { task.PushRef(""); break; }
                var result = Basic.Agents.TeamOrchestrator.Run(team, taskText);
                task.PushRef(result);
                break;
            }
            case SysCall.TEAM_SET_ROLE:
            {
                // push: RoleName, AgentName, TeamName → pop LIFO: TeamName, AgentName, RoleName
                var teamName = PopString(task) ?? "";
                var agentName = PopString(task) ?? "";
                var roleName = PopString(task) ?? "";
                var role = roleName.ToUpperInvariant() switch
                {
                    "COORDINATOR" => Basic.Agents.TeamRole.Coordinator,
                    "CRITIC" => Basic.Agents.TeamRole.Critic,
                    "OBSERVER" => Basic.Agents.TeamRole.Observer,
                    _ => Basic.Agents.TeamRole.Worker
                };
                task.PushRef(Basic.Agents.TeamServer.Instance.SetRole(teamName, agentName, role) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_STRATEGY:
            {
                // push: StrategyName, TeamName → pop LIFO: TeamName, StrategyName
                var teamName = PopString(task) ?? "";
                var stratName = PopString(task) ?? "";
                var strat = stratName.ToUpperInvariant() switch
                {
                    "ROUNDROBIN" or "ROUND_ROBIN" => Basic.Agents.TeamStrategy.RoundRobin,
                    "COORDINATOR" => Basic.Agents.TeamStrategy.Coordinator,
                    "MIXTURE" => Basic.Agents.TeamStrategy.Mixture,
                    "DISTILLATION" => Basic.Agents.TeamStrategy.Distillation,
                    "DELIBERATION" => Basic.Agents.TeamStrategy.Deliberation,
                    "DAG" => Basic.Agents.TeamStrategy.Dag,
                    _ => Basic.Agents.TeamStrategy.Sequential
                };
                task.PushRef(Basic.Agents.TeamServer.Instance.SetStrategy(teamName, strat) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_COORDINATOR:
            {
                // push: AgentName, TeamName → pop LIFO: TeamName, AgentName
                var teamName = PopString(task) ?? "";
                var agentName = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.SetCoordinator(teamName, agentName) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_RECURSION:
            {
                // push: Rounds, TeamName → pop LIFO: TeamName, Rounds
                var teamName = PopString(task) ?? "";
                var rounds = (int)PopNum(task);
                task.PushRef(Basic.Agents.TeamServer.Instance.SetRecursion(teamName, rounds) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_LATENT:
            {
                // push: Mode, TeamName → pop LIFO: TeamName, Mode
                var teamName = PopString(task) ?? "";
                var mode = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.SetLatent(teamName, mode) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_LINK:
            {
                // push: Config, Which, TeamName → pop LIFO: TeamName, Which, Config
                var teamName = PopString(task) ?? "";
                var which = PopString(task) ?? "";
                var cfg = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.SetLink(teamName, which, cfg) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_THOUGHTS:
            {
                // push: Count, TeamName → pop LIFO: TeamName, Count
                var teamName = PopString(task) ?? "";
                var m = (int)PopNum(task);
                task.PushRef(Basic.Agents.TeamServer.Instance.SetThoughts(teamName, m) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_TRAIN_INNER:
            {
                // push: Agent, Dataset, TeamName → pop LIFO: TeamName, Dataset, Agent
                var teamName = PopString(task) ?? "";
                var dataset = PopString(task) ?? "";
                var agent = PopString(task) ?? "";
                Basic.Agents.Recursive.RecursiveTrainer.TrainInner(teamName, agent, dataset);
                task.PushRef(0.0);
                break;
            }
            case SysCall.TEAM_TRAIN_OUTER:
            {
                // push: Dataset, TeamName → pop LIFO: TeamName, Dataset
                var teamName = PopString(task) ?? "";
                var dataset = PopString(task) ?? "";
                Basic.Agents.Recursive.RecursiveTrainer.TrainOuter(teamName, dataset);
                task.PushRef(0.0);
                break;
            }
            case SysCall.TEAM_SET_DAG:
            {
                // push: NMax, LMax, DMax, TeamName → pop LIFO: TeamName, DMax, LMax, NMax
                var teamName = PopString(task) ?? "";
                var d = (int)PopNum(task);
                var l = (int)PopNum(task);
                var n = (int)PopNum(task);
                task.PushRef(Basic.Agents.TeamServer.Instance.SetDag(teamName, d, l, n) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_COMPLEXITY:
            {
                var teamName = PopString(task) ?? "";
                var th = PopNum(task);
                task.PushRef(Basic.Agents.TeamServer.Instance.SetComplexity(teamName, th) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_REFLECTION:
            {
                var teamName = PopString(task) ?? "";
                var mode = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.SetReflection(teamName, mode) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_CRITIC:
            {
                // push: Judge, Personas, TeamName → pop LIFO: TeamName, Personas, Judge
                var teamName = PopString(task) ?? "";
                var personas = PopString(task) ?? "";
                var judge = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.SetCritic(teamName, personas, judge) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_SET_PRUNE:
            {
                var teamName = PopString(task) ?? "";
                var k = (int)PopNum(task);
                task.PushRef(Basic.Agents.TeamServer.Instance.SetPrune(teamName, k) ? -1.0 : 0.0);
                break;
            }
            case SysCall.TEAM_TRAIN_THINKING:
            {
                var teamName = PopString(task) ?? "";
                var dataset = PopString(task) ?? "";
                Basic.Agents.Recursive.RecursiveTrainer.TrainThinking(teamName, dataset);
                task.PushRef(0.0);
                break;
            }
            case SysCall.TEAM_TRAIN_ACTOR_CRITIC:
            {
                var teamName = PopString(task) ?? "";
                var dataset = PopString(task) ?? "";
                Basic.Agents.Recursive.DpsdpTrainer.TrainTeam(teamName, dataset);
                task.PushRef(0.0);
                break;
            }
            case SysCall.TEAM_DISTILL_THINKING:
            {
                var teamName = PopString(task) ?? "";
                var dataset = PopString(task) ?? "";
                Basic.Agents.Recursive.RecursiveTrainer.DistillThinking(teamName, dataset);
                task.PushRef(0.0);
                break;
            }
            case SysCall.TEAM_BROADCAST:
            {
                var teamName = PopString(task) ?? "";
                var message = PopString(task) ?? "";
                var count = Basic.Agents.TeamOrchestrator.Broadcast(teamName, message);
                task.PushRef((double)count);
                break;
            }
            case SysCall.TEAM_REDEFINE:
            {
                var teamName = PopString(task) ?? "";
                var def = PopString(task) ?? "";
                var count = Basic.Agents.TeamOrchestrator.Redefine(teamName, def);
                task.PushRef((double)count);
                break;
            }
            case SysCall.TEAM_SAVE:
            {
                var name = PopString(task) ?? "";
                try { Basic.Agents.TeamServer.Instance.Save(name); task.PushRef(-1.0); }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.TEAM_LOAD:
            {
                var name = PopString(task) ?? "";
                task.PushRef(Basic.Agents.TeamServer.Instance.Load(name) ? -1.0 : 0.0);
                break;
            }
            case SysCall.WORKFLOW_DEF:
            {
                var name = PopString(task) ?? "";
                task.PushRef(Basic.Workflow.WorkflowServer.Instance.BeginDefinition(name) ? -1.0 : 0.0);
                break;
            }
            case SysCall.WORKFLOW_STEP:
            {
                var stepName = PopString(task) ?? "";
                var action = task.PopRef()?.ToString() ?? "";
                var retryObj = task.PopRef();
                var timeoutObj = task.PopRef();
                var retry = retryObj is double rd ? (int)rd : retryObj is int ri ? ri : 0;
                var timeout = timeoutObj is double td ? (int)td : timeoutObj is int ti ? ti : 0;
                // stack order: timeout(bottom), retry, action, name(top) → pops name, action, retry, timeout
                task.PushRef(Basic.Workflow.WorkflowServer.Instance.AddStep(stepName, action, retry, timeout) ? -1.0 : 0.0);
                break;
            }
            case SysCall.WORKFLOW_END:
            {
                task.PushRef(Basic.Workflow.WorkflowServer.Instance.EndDefinition() ? -1.0 : 0.0);
                break;
            }
            case SysCall.WORKFLOW_EXECUTE:
            {
                var name = PopString(task) ?? "";
                task.PushRef(Basic.Workflow.WorkflowServer.Instance.Execute(name) ? -1.0 : 0.0);
                break;
            }
            case SysCall.ISAM_BOF:
            {
                var fileRef = argCount >= 1 && task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                task.PushRef(wa?.Engine != null && wa.Engine.IsBof() ? -1.0 : 0.0);
                break;
            }
            case SysCall.ISAM_RECNO:
            {
                /* argCount=0 (ISAM.RECNO() sull'area attiva): NON poppare — `task.Sp > 0` era
                 * vero anche con 0 argomenti (lo stack contiene gli operandi del PRINT/concat)
                 * → rubava uno slot e l'intera riga veniva scartata dalla staging. */
                var fileRef = argCount >= 1 && task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                task.PushRef(wa?.Engine != null ? (double)wa.Engine.GetCurrentRecordNumber() : 0.0);
                break;
            }
            case SysCall.ISAM_PUT:
            {
                var source = task.Sp > 0 ? task.PopRef() : null;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null && source is System.Collections.IDictionary srcDict)
                {
                    try
                    {
                        var fields = new Dictionary<int, object?>();
                        foreach (var tf in wa.Udt!.Fields)
                        {
                            var fn = tf.Name.Lexeme;
                            object? val = null;
                            if (srcDict.Contains(fn)) val = srcDict[fn];
                            else foreach (var k in srcDict.Keys)
                                if (string.Equals(k!.ToString(), fn, StringComparison.OrdinalIgnoreCase))
                                { val = srcDict[k!]; break; }
                            fields[tf.SymbolId] = val;
                        }
                        wa.Engine!.Put(new Basic.UdtInstance(wa.Udt, fields), wa.Engine.Evaluator);
                    }
                    catch { }
                }
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_DELETE:
            {
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null) try { wa.Engine.DeleteCurrent(wa.Engine.Evaluator); } catch { }
                wa?.Driver?.SyncCursorState();
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_PACK:
            {
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null)
                {
                    var engine = wa.Engine;
                    var packDriver = wa.Driver;
                    ParkBlocking(task,
                        () => { try { engine.Pack(engine.Evaluator); } catch { } return (object?)null; },
                        _ => { packDriver?.SyncCursorState(); task.PushRef(null); });
                }
                else
                    task.PushRef(null);
                break;
            }
            case SysCall.ISAM_REINDEX:
            {
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null)
                {
                    var engine = wa.Engine;
                    var reindexDriver = wa.Driver;
                    ParkBlocking(task,
                        () => { try { engine.Reindex(engine.Evaluator); } catch { } return (object?)null; },
                        _ => { reindexDriver?.SyncCursorState(); task.PushRef(null); });
                }
                else
                    task.PushRef(null);
                break;
            }

            case SysCall.ISAM_SET_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null) { var p = wa.Engine.SetReplication(string.IsNullOrEmpty(path) ? null : path); task.PushRef(p ?? ""); }
                else task.PushRef("");
                break;
            }
            case SysCall.ISAM_APPLY_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null)
                {
                    var engine = wa.Engine;
                    ParkBlocking(task,
                        () => { try { return (object?)(double)engine.ApplyReplication(path); } catch { return (object?)0.0; } },
                        result => task.PushRef(result));
                }
                else
                    task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_FOLLOW_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null) { var n = wa.Engine.FollowReplication(path); task.PushRef((double)n); }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.ISAM_APPENDMANY:
            {
                var batch = task.Sp > 0 ? task.PopRef() : null;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa != null && batch is System.Collections.IList list)
                {
                    int inserted = 0;
                    foreach (var item in list)
                    {
                        if (item is System.Collections.IDictionary srcDict)
                        {
                            try
                            {
                                var fields = new Dictionary<int, object?>();
                                foreach (var tf in wa.Udt!.Fields)
                                {
                                    var fn = tf.Name.Lexeme;
                                    object? val = null;
                                    if (srcDict.Contains(fn)) val = srcDict[fn];
                                    else foreach (var k in srcDict.Keys)
                                        if (string.Equals(k!.ToString(), fn, StringComparison.OrdinalIgnoreCase))
                                        { val = srcDict[k!]; break; }
                                    fields[tf.SymbolId] = val;
                                }
                                var inst = new Basic.UdtInstance(wa.Udt, fields);
                                wa.Engine!.Append(inst, wa.Engine.Evaluator);
                                inserted++;
                            }
                            catch { /* duplicate key: skip */ }
                        }
                    }
                    wa.Driver?.SyncCursorState();
                    task.PushRef((double)inserted);
                }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.ISAM_SET_FILTER:
            {
                // Arg 3: ClosureData per lambda compilata (FN(rec) => ...), oppure stringa per filtro runtime
                var conditionArg = task.Sp > 0 ? task.PopRef() : null;
                var hasCondition = PopNum(task) != 0;
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null && hasCondition)
                {
                    if (conditionArg is string filterExpr && !string.IsNullOrEmpty(filterExpr))
                    {
                        // Runtime filter da stringa (macro &(expr))
                        var evaluator = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(filterExpr);
                        wa.Engine.SetFilter((_, ctx) => evaluator?.Invoke(null, ctx) ?? false, null);
                    }
                    else if (conditionArg is ClosureData filterClosure)
                    {
                        // Lambda compilata: richiama la closure BASIC per ogni record durante la
                        // navigazione, passando il record come Dictionary<string,object?> (stessa
                        // convenzione di AFILTER — il compilatore risolve rec.Campo via OP_LOAD_FIELD
                        // per nome, non serve un tipo forte).
                        wa.Engine.SetFilter((_, ctx) =>
                        {
                            var recDict = new Dictionary<string, object?>(ctx, StringComparer.OrdinalIgnoreCase);
                            var result = CallClosureSync(task, filterClosure, recDict);
                            return SafeDouble(result) != 0.0;
                        }, null);
                    }
                    else
                    {
                        wa.Engine.SetFilter(null, null); // stringa vuota o valore non riconosciuto: nessun filtro
                    }
                }
                else if (wa?.Engine != null)
                {
                    wa.Engine.SetFilter(null, null); // Clear filter
                }
                break;
            }
            case SysCall.ISAM_INTEGRITY:
            {
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                var wa = ResolveIsamArea(task, fileRef);
                if (wa?.Engine != null)
                {
                    var (isValid, message) = wa.Engine.ValidateIntegrity();
                    var json = "{\"valid\":" + (isValid ? "true" : "false") + ",\"message\":\"" + (message ?? "").Replace("\"", "\\\"") + "\"}";
                    task.PushRef(json);
                }
                else task.PushRef("{}");
                break;
            }
            case SysCall.ISAM_INFO:
            {
                var fileRef = task.Sp > 0 ? task.PopRef() : null;
                // Runtime comune (IlRuntime.IsamInfo): JSON header+schema completo — prima
                // ritornava solo {"active_records":N} (divergente da IL/C99/VM-C).
                task.PushRef(Basic.IlBackend.IlRuntime.IsamInfo(fileRef?.ToString()));
                break;
            }
            
            case SysCall.DBF_USE:
            {
                // Statement path: 7+ args (typeName, typeSymId, alias, idxCount, idxPaths..., isUnsafe, locking, fileName)
                // Expression path: 5 args (typeName, alias, idxDict, exclusive, path)
                if (argCount <= 6)
                {
                    var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    var idxDict = task.Sp > 0 ? task.PopRef() : null;
                    var exclusive = PopNum(task) != 0;
                    var fileName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    fileName = Basic.IlBackend.IlRuntime.ResolveDbPath(fileName);
                    try
                    {
                        // Dispose engine(s) on the same file (from CREATE or prior USE)
                        // Close existing work areas for same file
                        for (int a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
                        {
                            var wa = _dataSession.GetWorkArea(a);
                            if (wa?.Driver is Basic.Databases.Rdd.DbfDriver d && string.Equals(d._engine?.FilePath ?? "", fileName, StringComparison.OrdinalIgnoreCase))
                            { try { d._engine?.Dispose(); } catch { } _dataSession.Unregister(a); }
                        }
                        var engine = new Basic.DBF.DbfEngine(fileName, Array.Empty<string>(), exclusive, Basic.Databases.LockingStrategy.IntraProcess); engine.Evaluator = (expr, ctx, tt) => { if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber(); if (ctx != null && ctx.TryGetValue(expr, out var val)) return val; return expr; };
                        var areaNum = _dataSession.AllocateArea();
                        _dataSession.RegisterDirect(areaNum, new Basic.Databases.Rdd.DbfDriver(engine, alias), alias);
                        task.CurrentDbfArea = areaNum;
                        task.PushRef((double)areaNum);
                    }
                    catch (Exception ex) { System.Console.Error.WriteLine("DBF_USE ERROR: " + ex.Message); task.PushRef(0.0); }
                }
                else
                {
                var dbfFieldCount = (int)PopNum(task);
                var dbfFieldSymbols = new List<(string Name, int SymbolId)>();
                var dbfFieldDefs = new List<(string Name, int Len, bool IsString, string TypeCode)>();
                // Tuple per campo (push order): nome, symbolId, len, isString, typeCode
                // (symbolId serve al wrapper __class__/FieldNames di DBF GET <var>; len/type
                // alla CREAZIONE del file quando `USE ... AS Tipo` lo trova assente).
                for (int fi = 0; fi < dbfFieldCount; fi++)
                {
                    var typeCode = task.Sp > 0 ? task.PopRef()?.ToString() ?? "STRING" : "STRING";
                    var isString = PopNum(task) != 0;
                    var flen = (int)PopNum(task);
                    var symId = (int)PopNum(task);
                    var fname = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    dbfFieldSymbols.Add((fname, symId));
                    dbfFieldDefs.Add((fname, flen, isString, typeCode));
                }
                dbfFieldSymbols.Reverse();
                dbfFieldDefs.Reverse();
                var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var typeSymId = (int)PopNum(task);
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var idxCount = (int)PopNum(task);
                var indexPaths = new List<string>();
                for (int ii = 0; ii < idxCount; ii++)
                    indexPaths.Add(task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "");
                var isUnsafe = PopNum(task) != 0;
                var locking = (int)PopNum(task);
                var fileName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    /* `USE ... AS Tipo` con file assente: CREA il file dalla struttura del TYPE
                     * (semantica documentata, parità IL DbfCreateIfMissing / VM-C/C99). Senza,
                     * lEngine ctor falliva e CurrentDbfArea restava 0 → SELECTED/LASTREC/SEEK
                     * vuoti (gamba PCode di ClipperDbfLateBinding). */
                    if (!File.Exists(fileName) && dbfFieldDefs.Count > 0)
                    {
                        var newFields = new List<Basic.DBF.DbfField>();
                        foreach (var fd in dbfFieldDefs)
                        {
                            var ftype = fd.TypeCode switch
                            {
                                "CURRENCY" => Basic.DBF.DbfFieldType.Currency,
                                "DOUBLE" => Basic.DBF.DbfFieldType.Double,
                                "DATE" => Basic.DBF.DbfFieldType.Date,
                                "BIT" => Basic.DBF.DbfFieldType.Logical,
                                "STRING" => Basic.DBF.DbfFieldType.Character,
                                _ => Basic.DBF.DbfFieldType.Numeric,
                            };
                            byte dec = fd.TypeCode is "DOUBLE" or "SINGLE" ? (byte)6 : (byte)0;
                            var flen2 = (byte)(fd.TypeCode switch
                            {
                                "CURRENCY" or "DOUBLE" or "DATE" => 8,
                                "BIT" => 1,
                                _ => fd.Len > 0 ? fd.Len : (dec > 0 ? 15 : 10),
                            });
                            newFields.Add(new Basic.DBF.DbfField(fd.Name, ftype, flen2, dec, 0u));
                        }
                        if (newFields.Count > 0)
                        {
                            var ndir = Path.GetDirectoryName(Path.GetFullPath(fileName));
                            if (!string.IsNullOrEmpty(ndir) && !Directory.Exists(ndir)) Directory.CreateDirectory(ndir);
                            Basic.DBF.DbfFile.CreateNew(fileName, System.Text.Encoding.UTF8, newFields);
                        }
                    }
                    var exclusive = locking == 1;
                    // Filter out non-existent index files to avoid DbfEngine constructor crash
                    var validIndexPaths = new List<string>();
                    foreach (var p in indexPaths)
                    {
                        var rp = p?.ToString() ?? "";
                        if (File.Exists(rp)) validIndexPaths.Add(rp);
                        else if (!string.IsNullOrEmpty(_dbfDefaultPath))
                        {
                            var fp = Path.Combine(_dbfDefaultPath, rp);
                            if (File.Exists(fp)) validIndexPaths.Add(fp);
                        }
                    }
                    var engine = new Basic.DBF.DbfEngine(fileName, validIndexPaths, exclusive, Basic.Databases.LockingStrategy.IntraProcess); engine.Evaluator = (expr, ctx, tt) => { if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber(); if (ctx != null && ctx.TryGetValue(expr, out var val)) return val; return expr; };
                    var areaNum = _dataSession.AllocateArea();
                    _dataSession.RegisterDirect(areaNum, new Basic.Databases.Rdd.DbfDriver(engine, alias), alias);
                    if (dbfFieldSymbols.Count > 0)
                    {
                        var waNew = _dataSession.GetWorkArea(areaNum);
                        if (waNew != null) waNew.Meta["DbfFieldSymbols"] = dbfFieldSymbols;
                    }
                    task.CurrentDbfArea = areaNum;
                    task.PushRef((double)areaNum);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("DBF_USE ERROR: " + ex.Message); task.PushRef(0.0); }
                }
                break;
            }
            
            case SysCall.DBF_SELECT:
            {
                var area = task.Sp > 0 ? task.PopRef() : null;
                if (area is string s) { var drv = _dataSession.FindAlias(s); if (drv != null) { for (int ai = 1; ai <= Basic.Databases.Rdd.DataSession.MaxWorkareas; ai++) { var swa = _dataSession.GetWorkArea(ai); if (swa?.Driver == drv) { task.CurrentDbfArea = ai; _dataSession.CurrentArea = ai; break; } } } }
                else if (area is double d && d > 0) { task.CurrentDbfArea = (int)d; _dataSession.CurrentArea = (int)d; }
                else if (area is int i && i > 0) { task.CurrentDbfArea = i; _dataSession.CurrentArea = i; }
                task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_APPEND_BLANK:
            {
                var engine = ResolveDbfEngine(task, null);
                if (engine != null)
                {
                    try { engine.AppendBlankRecord(null!); task.PushRef(12345.0); }
                    catch { task.PushRef(0.0); }
                }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.DBF_GO:
            {
                var dest = task.Sp > 0 ? task.PopRef() : null;
                var engine = ResolveDbfEngine(task, null);
                var drv = ResolveDbfDriver(task, null);
                if (engine != null && dest != null)
                {
                    var ds = dest.ToString()!.ToUpperInvariant();
                    if (ds == "TOP" || ds == "TOP ") engine.GoTop();
                    else if (ds == "BOTTOM" || ds == "BOTTOM ") engine.GoBottom();
                    else { var n = (int)SafeDouble(dest); if (n > 0) { engine.GoTop(); engine.Skip(n - 1); } }
                    SyncChildRelations(drv);
                }
                else if (drv != null && dest != null)
                {
                    // Area RDD NON-DBF (CSV/DELIM/SDF/ARRAY): il driver espone GoTop/GoBottom/Skip
                    // via IDatabaseDriver (come l'IL, che naviga l'area RDD via _dbfHandles).
                    var ds = dest.ToString()!.ToUpperInvariant();
                    if (ds == "TOP" || ds == "TOP ") { try { drv.GoTop(); } catch { } task.CurrentDbfArea = _dataSession.CurrentArea; }
                    else if (ds == "BOTTOM" || ds == "BOTTOM ") { try { drv.GoBottom(); } catch { } task.CurrentDbfArea = _dataSession.CurrentArea; }
                    else { var n = (int)SafeDouble(dest); if (n > 0) { try { drv.GoTop(); if (n > 1) drv.Skip(n - 1); } catch { } task.CurrentDbfArea = _dataSession.CurrentArea; } }
                }
                task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_SKIP:
            {
                var count = task.Sp > 0 ? task.PopRef() : null;
                var engine = ResolveDbfEngine(task, null);
                var drv = ResolveDbfDriver(task, null);
                var skipN = count is double d ? (int)d : count is int i ? i : 1;
                if (engine != null)
                {
                    try { engine.Skip(skipN); } catch { }
                    SyncChildRelations(drv);
                }
                else if (drv != null)
                {
                    try { drv.Skip(skipN); } catch { }
                }
                task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_SEEK:
            {
                var seekVal = task.Sp > 0 ? task.PopRef() : null;
                var engine = ResolveDbfEngine(task, null);
                var drv = ResolveDbfDriver(task, null);
                if (engine != null && seekVal != null)
                {
                    try { engine.Seek(seekVal); task.LastIsamFound = engine.Found; }
                    catch { task.LastIsamFound = false; }
                    SyncChildRelations(drv);
                }
                else task.LastIsamFound = false;
                task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_REPLACE:
            {
                // Stack (dal basso): [pairs..., count, scope...] — lo scope in CIMA (pattern
                // DBF_DELETE/PopScopeClause). PopScopeClause consuma i 4 arg scope (o i 4 nil
                // quando assente → ScopeInfoData.Current).
                var area = ResolveAreaNum(task, null, task.CurrentDbfArea);
                var (scope, condIp) = PopScopeClause(task);
                var repCount = (int)PopNum(task);
                var fields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                for (int ri = 0; ri < repCount; ri++)
                {
                    var val = task.Sp > 0 ? task.PopRef() : null;
                    var fname = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    if (!string.IsNullOrEmpty(fname)) fields[fname] = val;
                }
                var driver = area > 0 ? _dataSession.GetWorkArea(area)?.Driver : null;
                if (driver == null || !driver.IsOpen || fields.Count == 0)
                {
                    task.PushRef(null);
                    break;
                }
                try
                {
                    if (scope == null || scope.Type == Basic.Databases.Rdd.ScopeType.Current)
                    {
                        // Parità IL (DbfReplaceByName): aggiornamento PER CAMPO via driver, MAI
                        // PutCurrentRecord con dict parziale (riscrive l'intero record azzerando
                        // i campi non inclusi — bug corruzione verificato: REPLACE BALANCE perdeva NAME).
                        foreach (var (fname, val) in fields)
                        {
                            var fi = driver.GetFieldIndex(fname);
                            if (fi >= 0) driver.SetFieldValue(fi, val);
                            // Campo ignoto: skip silenzioso (parità IL DbfReplaceByName).
                        }
                    }
                    else
                    {
                        // Scoped (ALL/REST/NEXT/RECORD/FOR/WHILE): merge per-record del record
                        // CORRENTE completo + campi target → PutCurrentRecord(full) — mai
                        // PutCurrentRecord parziale (corruzione). Fix 2026-09-09: i codeblock
                        // condizionali ora sono bounded (OP_RETURN) → il loop termina su file.
                        ApplyScopedReplaceWithCondition(task, area, scope, condIp, fields);
                    }
                }
                catch { }
                task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_DELETE:
            {
                // Era solo DeleteCurrent: lo SCOPE (ALL/REST/NEXT/FOR/...) era ignorato →
                // DBF DELETE ALL cancellava un solo record. Ora applica lo scope via CoreDb.
                var area = ResolveAreaNum(task, null, task.CurrentDbfArea);
                var (scope, condIp) = PopScopeClause(task);
                if (scope != null)
                {
                    var count = ApplyScopedWithCondition(task, area, scope, condIp, d => { d.Delete(); return true; });
                    task.PushRef((double)count);
                }
                else task.PushRef(0.0);
                break;
            }

            case SysCall.DBF_RECALL:
            {
                var area = ResolveAreaNum(task, null, task.CurrentDbfArea);
                var (scope, condIp) = PopScopeClause(task);
                if (scope != null)
                {
                    var count = ApplyScopedWithCondition(task, area, scope, condIp, d => { d.Recall(); return true; });
                    task.PushRef((double)count);
                }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.DBF_GET_RECORD:
            {
                var (engine, fieldSymbols) = ResolveDbfArea(task, null);
                if (engine != null)
                {
                    try
                    {
                        var raw = engine.GetCurrentRecordValues();
                        if (raw != null && fieldSymbols != null && fieldSymbols.Count > 0)
                        {
                            // raw è chiavizzato sui nomi campo TRUNCATI su disco (max 10 caratteri,
                            // vedi DbfLazyRecord/WriteValuesToBuffer): stesso confronto
                            // tronca-e-ignora-maiuscole usato da DbfSchemaManager.IsHeaderCompatibleWithSchema
                            // e da LookupFieldValue in DbfEngine, altrimenti un nome campo BASIC oltre i
                            // 10 caratteri non troverebbe mai il proprio valore.
                            var fieldNames = new Dictionary<int, string>();
                            var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                            dict["__class__"] = new Dictionary<string, object?> { ["FieldNames"] = fieldNames };
                            foreach (var (name, symId) in fieldSymbols)
                            {
                                fieldNames[symId] = name;
                                object? val = null;
                                if (raw.TryGetValue(name, out var exact)) val = exact;
                                else
                                {
                                    var truncated = name.Length > 10 ? name[..10] : name;
                                    foreach (var (rk, rv) in raw)
                                    {
                                        if (string.Equals(rk, truncated, StringComparison.OrdinalIgnoreCase)) { val = rv; break; }
                                    }
                                }
                                dict[name] = val;
                            }
                            task.PushRef(dict);
                        }
                        else task.PushRef(raw);
                    }
                    catch { task.PushRef(null); }
                }
                else task.PushRef(null);
                break;
            }
            
            case SysCall.DBF_PACK: { var e = ResolveDbfEngine(task, null); if (e != null) try { e.Pack(); } catch { } break; }
            case SysCall.DBF_ZAP: { var e = ResolveDbfEngine(task, null); if (e != null) try { e.Zap(); } catch { } break; }
            case SysCall.DBF_REINDEX: { var e = ResolveDbfEngine(task, null); if (e != null) try { e.Reindex(); } catch { } break; }
            case SysCall.DBF_SET_DELETED: { var on = PopNum(task) != 0; var e = ResolveDbfEngine(task, null); if (e != null) e.SetDeleted(on); break; }
            case SysCall.DBF_SET_EXACT: { var on = PopNum(task) != 0; var e = ResolveDbfEngine(task, null); if (e != null) e.SetExact(on); break; }
            case SysCall.RDD_SET_DEFAULT: { _dataSession.DefaultDriver = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : ""; break; }
            case SysCall.RDD_USE:
            {
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var driverName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    // Auto-detect driver da estensione file o DefaultDriver
                    if (string.IsNullOrEmpty(driverName) || driverName == "NIL")
                    {
                        driverName = Basic.Databases.Rdd.DriverRegistry.FindForExtension(fileName) ?? _dataSession.DefaultDriver;
                    }
                    var info = new Basic.Databases.Rdd.DbOpenInfo { FileName = fileName, Alias = alias, Driver = driverName };
                    var driver = Basic.Databases.Rdd.DriverRegistry.Create(driverName);
                    if (driver != null)
                    {
                        driver.Open(info);
                        var area = _dataSession.AllocateArea();
                        _dataSession.RegisterDirect(area, driver, alias);
                        if (driver is Basic.Databases.Rdd.SqlDriver sqld && sqld._schemaBuf != null)
                            _dataSession.GetWorkArea(area)!.Meta["SchemaBuffer"] = sqld._schemaBuf;
                        task.PushRef((double)area);
                    }
                    else task.PushRef(0.0);
                }
                catch (Exception ex) { System.Console.Error.WriteLine($"RDD_USE ERROR: {ex.Message}"); task.PushRef(0.0); }
                break;
            }
            case SysCall.RDD_RLOCK:
            {
                var areaRef = task.Sp > 0 ? task.PopRef() : null;
                var area = areaRef is double d ? (int)d : areaRef is int i ? i : task.CurrentIsamArea;
                var ok = _dataSession.Locks.RLock(_dataSession, area);
                task.PushRef(ok ? -1.0 : 0.0);
                break;
            }
            case SysCall.RDD_FLOCK:
            {
                var areaRef = task.Sp > 0 ? task.PopRef() : null;
                var area = areaRef is double d ? (int)d : areaRef is int i ? i : task.CurrentIsamArea;
                var ok = _dataSession.Locks.FLock(_dataSession, area);
                task.PushRef(ok ? -1.0 : 0.0);
                break;
            }
            case SysCall.RDD_UNLOCK:
            {
                var areaRef = task.Sp > 0 ? task.PopRef() : null;
                var area = areaRef is double d ? (int)d : areaRef is int i ? i : task.CurrentIsamArea;
                _dataSession.Locks.UnLockAll(_dataSession, area);
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_SETLOCKTIMEOUT:
            {
                var ms = (int)PopNum(task);
                _dataSession.Locks.LockTimeoutMs = ms > 0 ? ms : 0;
                _dataSession.Locks.RetryBaseMs = Math.Max(10, ms / 60);
                break;
            }
            case SysCall.RDD_APPENDFROM:
            {
                var sourceArea = (int)PopNum(task);
                var targetArea = (int)PopNum(task);
                var count = Basic.Databases.Rdd.CoreDb.AppendFrom(_dataSession, targetArea, sourceArea);
                task.PushRef((double)count);
                break;
            }
            case SysCall.RDD_GET_FIELD:
            {
                var fieldObj = task.PopRef();
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                // Buffer attivo? Leggi da lì (chiave = SymbolId reale: nome o 1-based, ADR §1).
                if (_dataSession._buffers.TryGetValue(area, out var buf) && buf.IsActive)
                {
                    var gk2 = RddResolveFieldSymbolId(wa, fieldObj);
                    task.PushRef(Basic.IlBackend.IlRuntime.RddTypedValuePub(wa?.Driver, gk2, buf.GetField(gk2)));
                    break;
                }
                if (wa?.Driver is { IsOpen: true } gd)
                {
                    int gp = fieldObj is string gsn && gsn.Length > 0 ? gd.GetFieldIndex(gsn)
                         : (int)(fieldObj is double gd2 ? gd2 : 0) - 1;
                    var gv = (gp >= 0 && gp < gd.Fields.Length) ? gd.GetFieldValue(gp) : null;
                    task.PushRef(Basic.IlBackend.IlRuntime.RddTypedValuePub(gd, (gp >= 0 && gp < gd.Fields.Length) ? gd.Fields[gp].SymbolId : -1, gv));
                    break;
                }
                var sb = wa?.Meta.GetValueOrDefault("SchemaBuffer") as Basic.Databases.Rdd.SchemaBuffer;
                task.PushRef(sb?.ReadValue((int)(fieldObj is double sd0 ? sd0 : 0) - 1));
                break;
            }
            case SysCall.RDD_SET_FIELD:
            {
                var value = task.PopRef();
                var fieldObj2 = task.PopRef();
                var area = (int)PopNum(task);
                var wa2 = _dataSession.GetWorkArea(area);
                // Buffer attivo? Scrivi lì (chiave = SymbolId reale — vedi RddResolveFieldSymbolId:
                // senza la conversione, TABLEUPDATE scriveva il record invariato perché il buffer
                // usava chiavi "1"/"2"/"3" mai lette da GetCurrentRecord/PutCurrentRecord).
                if (_dataSession._buffers.TryGetValue(area, out var buf2) && buf2.IsActive)
                    // Chiave = STESSO resolver dei getter (RddGetField/OLDVAL/GETFLDSTATE, in
                    // IlRuntime): con RddResolveFieldSymbolId il campo 0 dava chiave 0 mentre il
                    // GET ne calcolava -1 → RDD.GETFIELD vuoto (bug PCode RDD_Memory_CRUD).
                    { buf2.SetField(Basic.IlBackend.IlRuntime.RddResolveFieldSymbolPub(wa2, fieldObj2), value); task.PushRef(null); break; }
                if (wa2?.Driver is { IsOpen: true } sd2)
                {
                    int sp = fieldObj2 is string ssn && ssn.Length > 0 ? sd2.GetFieldIndex(ssn)
                         : (int)(fieldObj2 is double sd3 ? sd3 : 0) - 1;
                    sd2.SetFieldValue(sp, value); task.PushRef(null); break;
                }
                var sb2 = wa2?.Meta.GetValueOrDefault("SchemaBuffer") as Basic.Databases.Rdd.SchemaBuffer;
                sb2?.WriteValue((int)(fieldObj2 is double sd4 ? sd4 : 0) - 1, value);
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_ONEVENT:
            {
                var callbackExpr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var eventName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (Enum.TryParse<Basic.Databases.Rdd.DbEventType>(eventName, true, out var evt))
                {
                    if (string.IsNullOrWhiteSpace(callbackExpr))
                    {
                        // Default callback: stampa evento
                        _dataSession.Events.On(evt, e =>
                            _runtime.Console.PrintLine($"[DB] {e.Type} area={e.Area} recno={e.RecNo}"));
                    }
                    else
                    {
                        // Callback personalizzato: valuta espressione quando l'evento scatta
                        _dataSession.Events.On(evt, e =>
                        {
                            var result = EvaluateSimpleExpression(callbackExpr);
                            if (result != null)
                                _runtime.Console.PrintLine(result.ToString()!);
                        });
                    }
                }
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_OFFEVENT:
            {
                var eventName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (Enum.TryParse<Basic.Databases.Rdd.DbEventType>(eventName, true, out var evt))
                    _dataSession.Events.Off(evt);
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_TODAY:
            {
                task.PushRef(new Basic.Databases.Rdd.GbDate(DateTime.Today));
                break;
            }
            case SysCall.RDD_CTOD:
            {
                var s = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                task.PushRef(Basic.Databases.Rdd.GbDate.FromLiteral(s));
                break;
            }
            case SysCall.RDD_DTOC:
            {
                var d = task.Sp > 0 ? task.PopRef() : null;
                if (d is Basic.Databases.Rdd.GbDate gbd)
                    task.PushRef(gbd.ToString());
                else if (d is DateTime dt)
                    task.PushRef(dt.ToString("yyyy-MM-dd"));
                else
                    task.PushRef("");
                break;
            }
            case SysCall.RDD_CURVAL:
            {
                var v = task.Sp > 0 ? task.PopRef() : null;
                if (v is double dv)
                    task.PushRef(new Basic.Databases.Rdd.GbCurrency(dv));
                else if (v is string sv)
                    task.PushRef(Basic.Databases.Rdd.GbCurrency.FromString(sv));
                else
                    task.PushRef(new Basic.Databases.Rdd.GbCurrency(0m));
                break;
            }
            case SysCall.RDD_BUFFER:
            {
                var modeInt = (int)PopNum(task);
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is { IsOpen: true } driver)
                {
                    // SQL driver: usa SchemaBuffer
                    if (driver is Basic.Databases.Rdd.SqlDriver sd)
                    {
                        var rec = driver.GetCurrentRecord();
                        if (rec != null)
                        {
                            var sb = wa.Meta.GetValueOrDefault("SchemaBuffer") as Basic.Databases.Rdd.SchemaBuffer;
                            if (sb == null)
                            {
                                int maxId = 0; foreach (var k in rec.Keys) maxId = Math.Max(maxId, k);
                                var fields = new Basic.Databases.Rdd.FieldOffsetInfo[maxId + 1];
                                int off = 0;
                                for (int i = 0; i <= maxId; i++)
                                {
                                    int sz = rec.TryGetValue(i, out var v) && v is string ? 64 : 8;
                                    fields[i] = new Basic.Databases.Rdd.FieldOffsetInfo($"f{i}", off, sz, i, typeof(double));
                                    off += sz;
                                }
                                sb = new Basic.Databases.Rdd.SchemaBuffer(fields);
                                wa.Meta["SchemaBuffer"] = sb;
                            }
                            sb.FromDictionary(rec);
                            var orig = new Dictionary<int, object?>();
                            for (int i = 0; i < sb.FieldCount; i++) orig[i] = sb.ReadValue(i);
                            wa.Meta["_originalValues"] = orig;
                            wa.Meta["_fieldStates"] = new Dictionary<int, int>();
                            wa.Meta["_bufferMode"] = modeInt;
                            task.PushRef(-1.0);
                        }
                        else task.PushRef(0.0);
                        break;
                    }
                    var buf = new Basic.Databases.Rdd.RowsBuffer();
                    var mode = modeInt switch { 2 => Basic.Databases.Rdd.BufferMode.Pessimistic, 3 => Basic.Databases.Rdd.BufferMode.Optimistic, _ => Basic.Databases.Rdd.BufferMode.None };
                    if (mode != Basic.Databases.Rdd.BufferMode.None)
                    {
                        buf.Activate(driver, mode);
                        _dataSession._buffers[area] = buf;
                            task.PushRef(-1.0);
                    }
                    else
                    {
                        if (_dataSession._buffers.TryGetValue(area, out var obuf2))
                        {
                            try { obuf2.TableRevert(driver); } catch { }
                            _dataSession._buffers.Remove(area);
                        }
                        task.PushRef(0.0);
                    }
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_TABLEUPDATE:
            {
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is { IsOpen: true } driver)
                {
                    if (driver is Basic.Databases.Rdd.SqlDriver sd)
                    {
                        var sb = wa.Meta.GetValueOrDefault("SchemaBuffer") as Basic.Databases.Rdd.SchemaBuffer;
                        if (sb != null)
                        {
                            try { driver.PutCurrentRecord(sb.ToDictionary()); wa.Meta.Remove("_fieldStates"); wa.Meta.Remove("_originalValues"); task.PushRef(-1.0); }
                            catch { task.PushRef(0.0); }
                        }
                        else task.PushRef(0.0);
                        break;
                    }
                    if (_dataSession._buffers.TryGetValue(area, out var buf))
                    {
                        var upt = buf.TableUpdate(driver);
                        _dataSession._buffers.Remove(area);
                        task.PushRef(upt ? -1.0 : 0.0);
                    }
                    else task.PushRef(0.0);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_TABLEREVERT:
            {
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is { IsOpen: true } driver)
                {
                    if (driver is Basic.Databases.Rdd.SqlDriver)
                    {
                        try { var rec = driver.GetCurrentRecord(); if (rec != null) { var sb = wa.Meta.GetValueOrDefault("SchemaBuffer") as Basic.Databases.Rdd.SchemaBuffer; if (sb != null) sb.FromDictionary(rec); } } catch { }
                        wa.Meta.Remove("_fieldStates"); wa.Meta.Remove("_originalValues");
                        task.PushRef(-1.0);
                    }
                    else if (_dataSession._buffers.TryGetValue(area, out var buf))
                    {
                        buf.TableRevert(driver);
                        _dataSession._buffers.Remove(area);
                        task.PushRef(-1.0);
                    }
                    else task.PushRef(0.0);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_OLDVAL:
            {
                var fieldObj3 = task.PopRef();
                var fieldIdx = (int)(fieldObj3 is double fdv ? fdv : 0);
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa != null && wa.Meta.GetValueOrDefault("_originalValues") is Dictionary<int, object?> orig && orig.TryGetValue(fieldIdx, out var oval))
                    task.PushRef(oval);
                else if (_dataSession._buffers.TryGetValue(area, out var buf2) && buf2.IsActive)
                {
                    var ok3 = RddResolveFieldSymbolId(wa, fieldObj3);
                    task.PushRef(Basic.IlBackend.IlRuntime.RddTypedValuePub(wa?.Driver, ok3, buf2.GetOldValue(ok3)));
                }
                else task.PushRef(null);
                break;
            }
            case SysCall.RDD_GETFLDSTATE:
            {
                var fieldObj4 = task.PopRef();
                var fieldIdx = (int)(fieldObj4 is double fds ? fds : 0);
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa != null && wa.Meta.GetValueOrDefault("_fieldStates") is Dictionary<int, int> states && states.TryGetValue(fieldIdx, out var st))
                    task.PushRef((double)st);
                else if (_dataSession._buffers.TryGetValue(area, out var buf3) && buf3.IsActive)
                    task.PushRef(buf3.GetFieldState(RddResolveFieldSymbolId(wa, fieldObj4)) switch
                    {
                        Basic.Databases.Rdd.FieldState.Deleted => 2.0,
                        Basic.Databases.Rdd.FieldState.Modified or Basic.Databases.Rdd.FieldState.Appended => 1.0,
                        _ => 0.0
                    });
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_SETREPROCESS:
            {
                var ms = (int)PopNum(task);
                _dataSession.Locks.LockTimeoutMs = ms > 0 ? ms : 0;
                _dataSession.Locks.RetryBaseMs = Math.Max(10, ms / 60);
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_SETORDER:
            {
                var orderName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                Basic.Databases.Rdd.CoreDb.Execute(_dataSession, area, d => d.SetOrder(orderName));
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_CREATEINDEX:
            {
                var keyExpr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var indexName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                var info = new Basic.Databases.Rdd.DbOrderInfo { Name = indexName, KeyExpression = keyExpr, KeyLength = 50, Unique = false };
                Basic.Databases.Rdd.CoreDb.Execute(_dataSession, area, d => d.CreateIndex(info));
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_CLOSEINDEX:
            {
                var indexName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                Basic.Databases.Rdd.CoreDb.Execute(_dataSession, area, d => d.CloseIndex(indexName));
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_COPYTO:
            {
                var outDriver = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var file = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is Basic.Databases.Rdd.SqlDriver sd && !string.IsNullOrWhiteSpace(file))
                {
                    try { sd.ExecRaw("VACUUM INTO '" + file.Replace("'", "''") + "'", null, null); task.PushRef(1.0); }
                    catch { task.PushRef(0.0); }
                }
                else
                {
                    var count = Basic.Databases.Rdd.CoreDb.CopyTo(_dataSession, area, file, outDriver);
                    task.PushRef((double)count);
                }
                break;
            }
            case SysCall.RDD_SORTTO:
            {
                var desc = PopNum(task) != 0;
                var sortField = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var outDriver = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var file = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is Basic.Databases.Rdd.SqlDriver sd && !string.IsNullOrWhiteSpace(file) && !string.IsNullOrWhiteSpace(sortField))
                {
                    var dir = desc ? "DESC" : "ASC";
                    try { sd.ExecRaw("CREATE TABLE \"" + file + "\" AS SELECT * FROM " + (wa.Alias ?? "t") + " ORDER BY \"" + sortField + "\" " + dir, null, null); task.PushRef(1.0); }
                    catch { task.PushRef(0.0); }
                }
                else
                {
                    var count = Basic.Databases.Rdd.CoreDb.SortTo(_dataSession, area, file, outDriver, sortField, desc);
                    task.PushRef((double)count);
                }
                break;
            }
            case SysCall.RDD_TOTALON:
            {
                var sumFields = task.Sp > 0 ? task.PopRef()?.ToString()?.Split(',') ?? Array.Empty<string>() : Array.Empty<string>();
                var groupField = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var outDriver = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var file = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area = (int)PopNum(task);
                var wa = _dataSession.GetWorkArea(area);
                if (wa?.Driver is Basic.Databases.Rdd.SqlDriver sd && !string.IsNullOrWhiteSpace(file) && !string.IsNullOrWhiteSpace(groupField))
                {
                    var sums = string.Join(", ", sumFields.Select(f => "SUM(\"" + f.Trim() + "\") AS \"" + f.Trim() + "\""));
                    try { sd.ExecRaw("CREATE TABLE \"" + file + "\" AS SELECT \"" + groupField + "\", " + sums + " FROM " + (wa.Alias ?? "t") + " GROUP BY \"" + groupField + "\"", null, null); task.PushRef(1.0); }
                    catch { task.PushRef(0.0); }
                }
                else
                {
                    var count = Basic.Databases.Rdd.CoreDb.TotalOn(_dataSession, area, file, outDriver, groupField, sumFields);
                    task.PushRef((double)count);
                }
                break;
            }
            case SysCall.RDD_SETDELETED:
            {
                // Sia la funzione RDD.SETDELETED(area, on) sia lo statement RDD SET DELETED
                // pushano [area, on]: il POP prende on PRIMA (in cima), poi area.
                var on = PopNum(task) != 0;
                var area = (int)PopNum(task);
                if (area == 0) area = task.CurrentIsamArea > 0 ? task.CurrentIsamArea : task.CurrentDbfArea > 0 ? task.CurrentDbfArea : _dataSession.CurrentArea;
                Basic.Databases.Rdd.CoreDb.Execute(_dataSession, area, d => d.SetDeleted(on));
                task.PushRef(null);
                break;
            }
            case SysCall.RDD_VALTYPE:
            {
                var v = task.Sp > 0 ? task.PopRef() : null;
                // Inspect BasicType from stack entry (already popped, check value type)
                var typeChar = v switch
                {
                    null => "U",
                    string => "C",
                    bool => "L",
                    double or int or long or float or decimal or short or byte => "N",
                    DateTime or DateOnly or Basic.Databases.Rdd.GbDate => "D",
                    Basic.Databases.Rdd.GbCurrency => "Y",
                    System.Collections.IDictionary or System.Collections.IList => "A",
                    _ => v.GetType().Name == "DateValue" ? "D" : "O"
                };
                task.PushRef(typeChar);
                break;
            }
            case SysCall.RDD_FLOAT:
            {
                var decimals = (int)PopNum(task);
                var value = PopNum(task);
                // Formatta come Clipper FLOAT: stringa con N decimali
                var fmt = $"F{decimals}";
                task.PushRef(value.ToString(fmt, System.Globalization.CultureInfo.InvariantCulture));
                break;
            }
            case SysCall.RDD_EVAL:
            {
                var expr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (!string.IsNullOrWhiteSpace(expr))
                {
                    // Valuta espressione aritmetica semplice
                    var result = EvaluateSimpleExpression(expr);
                    task.PushRef(result);
                }
                else task.PushRef(null);
                break;
            }
            case SysCall.RDD_DELETE_SCOPED:
            {
                var area = _dataSession.CurrentArea;
                var (scope, condIp) = PopScopeClause(task);
                if (scope != null)
                {
                    var count = ApplyScopedWithCondition(task, area, scope, condIp,
                        d => { d.Delete(); return true; });
                    task.PushRef((double)count);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_RECALL_SCOPED:
            {
                var area = _dataSession.CurrentArea;
                var (scope, condIp) = PopScopeClause(task);
                if (scope != null)
                {
                    var count = ApplyScopedWithCondition(task, area, scope, condIp,
                        d => { d.Recall(); return true; });
                    task.PushRef((double)count);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.RDD_REPLACE_SCOPED:
            {
                var area = _dataSession.CurrentArea;
                var (scope, condIp) = PopScopeClause(task);
                // cima stack: numero di replacement (field/value pairs già sotto lo scope)
                var replaceCount = (int)PopNum(task);
                var fields = new Dictionary<string, object?>();
                for (int ri = 0; ri < replaceCount; ri++)
                {
                    var val = task.Sp > 0 ? task.PopRef() : null;
                    var field = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    if (!string.IsNullOrEmpty(field)) fields[field] = val;
                }
                // Row-buffer ATTIVO + scope Current: le sostituzioni sono DEVIATE nel
                // buffer (ADR-RowBuffer §6, come RDD_SET_FIELD). Chiavi = SymbolId reali
                // del record (driver.Fields), non indici ordinali.
                if (_dataSession._buffers.TryGetValue(area, out var rbuf) && rbuf.IsActive
                    && fields.Count > 0 && scope?.Type == Basic.Databases.Rdd.ScopeType.Current)
                {
                    var rdrv = _dataSession.GetWorkArea(area)?.Driver;
                    long rn = 0;
                    if (rdrv != null)
                        foreach (var kvf in fields)
                        {
                            var symf = rdrv.Fields.FirstOrDefault(ff => string.Equals(ff.Name, kvf.Key, StringComparison.OrdinalIgnoreCase))?.SymbolId
                                       ?? rdrv.GetFieldIndex(kvf.Key);
                            rbuf.SetField(symf, kvf.Value);
                            rn++;
                        }
                    task.PushRef((double)rn);
                }
                else if (scope != null && fields.Count > 0)
                {
                    var count = ApplyScopedReplaceWithCondition(task, area, scope, condIp, fields);
                    task.PushRef((double)count);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.DBF_SET_DEFAULT: { var p = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : ""; _dbfDefaultPath = Basic.IlBackend.IlRuntime.ResolveProgramRelative(p); Basic.IlBackend.IlRuntime.DbfSetDefaultPath(p); break; }
            case SysCall.DBF_SET_PATH: { var p = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : ""; Basic.IlBackend.IlRuntime.DbfSetSearchPath(p); break; }
            case SysCall.DBF_SET_EXCLUSIVE: { var on = PopNum(task) != 0; Basic.IlBackend.IlRuntime.DbfSetExclusive(on); break; }
            case SysCall.DBF_SET_ORDER:
            {
                // SET ORDER TO <n> oppure TO "<nome indice>" (es. "idx_nome" o "idx_nome.ntx").
                // Prima si faceva SEMPRE Convert.ToDouble: con un NOME scattava l'eccezione
                // (catch silenzioso) → nessun ordine attivo → SEEK e liste in ordine naturale
                // (gamba PCode di ClipperDbfLateBinding §5).
                var ov = task.Sp > 0 ? task.PopRef() : null;
                var oeng = ResolveDbfEngine(task, null);
                if (oeng != null)
                {
                    if (ov is string os && os.Length > 0 && !double.TryParse(os, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out _))
                    { try { oeng.SetActiveOrder(os); } catch { } }
                    else
                    { try { oeng.SetActiveOrder((int)System.Convert.ToDouble(ov, System.Globalization.CultureInfo.InvariantCulture)); } catch { } }
                }
                break;
            }
            case SysCall.DBF_SET_RELATION:
            {
                var childAlias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var key = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var parent = ResolveDbfDriver(task, null);
                if (parent != null)
                {
                    if (string.IsNullOrWhiteSpace(key))
                    {
                        for (var a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
                        {
                            var child = _dataSession.GetWorkArea(a)?.Driver;
                            if (child != null && child.HasActiveRelation && ReferenceEquals(child.ParentRelationDriver, parent))
                                child.ClearRelation();
                        }
                    }
                    else
                    {
                        var child = _dataSession.FindAlias(childAlias);
                        if (child != null)
                            try { child.SetRelation(parent, key); child.SyncRelation(); } catch { }
                    }
                }
                break;
            }
            case SysCall.DBF_SET_FILTER:
            {
                // La condizione è serializzata a STRINGA (ExprPlain.From) dal compilatore e
                // valutata per-record da RuntimeExpressionEvaluator (risolve i campi per nome sul
                // dict del record) — come il filtro ISAM stringa e il backend IL. Un code-block
                // (CompileExpressionToCodeBlock) NON funziona: i nomi campo nudi (es.
                // `Citta = "Roma"`) vengono compilati come variabili globali, non come letture
                // dal record corrente → valutazione sempre falsa/null.
                var conditionArg = task.Sp > 0 ? task.PopRef() : null;
                var hasCondition = PopNum(task) != 0;
                var engine = ResolveDbfEngine(task, null);
                if (engine != null)
                {
                    /* ExprPlain.From serializza il letterale CON le virgolette: `SET FILTER TO ""`
                     * arriva come `"\"\""` → con il testo grezzo si creava un evaluator VUOTO che
                     * escludeva OGNI record (cursore a EOF → LOOKUP vuoto). Qui si tolgono le
                     * virgolette esterne e il testo vuoto vale "nessun filtro". */
                    var rawCond = conditionArg as string ?? "";
                    var filterExpr = (rawCond.Length >= 2 && rawCond.StartsWith("\"") && rawCond.EndsWith("\""))
                                     ? rawCond.Substring(1, rawCond.Length - 2) : rawCond;
                    if (hasCondition && !string.IsNullOrWhiteSpace(filterExpr))
                    {
                        var evaluator = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(filterExpr);
                        engine.SetFilter(ctx => evaluator?.Invoke(null, ctx) ?? false, null);
                        /* Il TESTO del filtro va memorizzato anche sul DRIVER: DBFILTER() legge
                         * FilterExpression (stringa) dal driver, mentre l'engine conserva solo
                         * l'oggetto espressione → senza questo DBFILTER() restava vuoto
                         * (§ MiniTests/dbf/dbf_lock_filter.bas). */
                        /* ExprPlain.From(StringExpr) serializza il letterale CON le virgolette:
                         * DBFILTER() deve restituire il testo, non il letterale (parità IL). */
                        (_dataSession.GetWorkArea(_dataSession.CurrentArea)?.Driver as Basic.Databases.Rdd.DbfDriver)?.SetFilter(filterExpr);
                    }
                    else
                    {
                        engine.SetFilter(null, null); // NIL / vuoto → nessun filtro
                        (_dataSession.GetWorkArea(_dataSession.CurrentArea)?.Driver as Basic.Databases.Rdd.DbfDriver)?.SetFilter("");
                    }
                }
                break;
            }
            case SysCall.DBF_COPY_TO: {
                var sc = task.Sp > 0 ? task.PopRef() : null;
                PopNum(task);
                var d = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                Basic.Databases.Rdd.RuntimeExpressionEvaluator.ExpressionEvaluator? cce = null;
                var cct = sc?.ToString();
                if (!string.IsNullOrWhiteSpace(cct)) cce = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(cct);
                try { var cdrv = ResolveDbfDriver(task, null); if (cdrv is Basic.Databases.Rdd.DbfDriver cdd) cdd.CopyTo(d, cce is null ? null : dd => cce(null, dd)); else if (e != null) e.CopyTo(d, cce is null ? null : dd => cce(null, dd), null!); } catch { }
                break; }
            case SysCall.DBF_APPEND_RECORD: { var src = task.Sp > 0 ? task.PopRef() : null; var e = ResolveDbfEngine(task, null); if (e != null && src is System.Collections.IDictionary sd) try { var d = new Dictionary<string, object?>(); foreach (System.Collections.DictionaryEntry kv in sd) { var k = kv.Key?.ToString() ?? ""; if (!k.StartsWith("__")) d[k] = kv.Value; } e.AppendRecord(d, null!); } catch (Exception ex) { System.Console.Error.WriteLine("APPEND_RECORD ERR: " + ex.Message); } break; }
            case SysCall.DBF_APPEND_FROM: {
                // Expression path: 1 arg (fileName)
                // Statement path: 3 args (fileName, hasScope, cond-text) — leggiamo il testo
                // e lo applichiamo al SORGENTE (prima veniva scartato: FOR ignorato).
                object? afSc = null;
                if (argCount >= 3) { afSc = task.Sp > 0 ? task.PopRef() : null; PopNum(task); }
                var sf = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                Basic.Databases.Rdd.RuntimeExpressionEvaluator.ExpressionEvaluator? afe = null;
                var aft = afSc?.ToString();
                if (!string.IsNullOrWhiteSpace(aft)) afe = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(aft);
                if (e != null) try { e.AppendFrom(sf, afe is null ? null : dd => afe(null, dd)); } catch { }
                break;
            }
            case SysCall.DBF_COPY_STRUCTURE: {
                var d = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                if (e != null && !string.IsNullOrEmpty(d))
                {
                    try
                    {
                        // Use engine's CloneStructure to copy header only (0 records)
                        e.CloneStructure(d, new Basic.DBF.DbfSchemaInfo("", new List<Basic.DBF.DbfFieldSchema>()));
                    }
                    catch (Exception ex) { System.Console.Error.WriteLine("COPY_STRUCTURE ERR: " + ex.Message); }
                }
                break;
            }
            case SysCall.DBF_INDEX_ON: {
                var fn = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var keyExprStr = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                task.PopRef(); // code ref
                var e = ResolveDbfEngine(task, null);
                if (e != null && !string.IsNullOrEmpty(fn)) try {
                    if (!string.IsNullOrEmpty(_dbfDefaultPath) && !fn.Contains('/') && !fn.Contains('\\'))
                        fn = Path.Combine(_dbfDefaultPath, fn);
                    // Ensure fn has a directory part for ClipperNtxBuilder.Write
                    if (!fn.Contains('/') && !fn.Contains('\\'))
                        fn = "./" + fn;
                    // keyExprStr è la forma DEBUG dell'AST (`(Citta#-1 + Nome#-1)`): per il
                    // hook VM (KeyExpressionPCodeHook → P-Code nel PCodeVm) serve il TESTO
                    // BASIC. Si tolgono i marcatori `#<symId>` e le parentesi esterne →
                    // "Citta + Nome": prima il testo troncato a `#` dava "(Citta" → chiave
                    // solo-Nome / primo campo (§5 Citta+Nome non ordinato).
                    var keyText = System.Text.RegularExpressions.Regex.Replace(keyExprStr, "#-?[0-9]+", "").Trim();
                    if (keyText.StartsWith("(", StringComparison.Ordinal) && keyText.EndsWith(")", StringComparison.Ordinal) && keyText.Length > 2)
                        keyText = keyText.Substring(1, keyText.Length - 2).Trim();
                    var plainName = keyText;
                    var hashIdx = plainName.IndexOf('#'); if (hashIdx > 0) plainName = plainName.Substring(0, hashIdx);
                    bool useRecno = keyExprStr.Contains("RECNO()") || keyExprStr.Contains("Recno");
                    var enc = e.GetEncoding();
                    // For RECNO-based indexes, store '__recno__' as expression so Evaluator can find it
                    var exprName = useRecno ? "__recno__" : plainName;
                    var dummyKey = new VariableExpr(-1,
                        new Token(TokenKind.Identifier, exprName, null, SourcePosition.Unknown),
                        new Token(TokenKind.Identifier, exprName, null, SourcePosition.Unknown));
                    int recNo = 1;
                    // Chiave COMPOSTA (`Citta + Nome`, `STR(RECNO())`, …): la si valuta con la
                    // STESSA pipeline usata da ISAM/filtri — KeyExpressionPCodeHook compila
                    // l'espressione in P-Code e la esegue nella VM (DbExprPCodeEvaluator riusa
                    // il PCodeVm), NON un evaluator ad hoc né il lookup per nome. Il vecchio
                    // fallback su `record.Values.FirstOrDefault()` indicizzava il PRIMO campo
                    // (ID) → ordine naturale invece di Nome (§5 ClipperDbfLateBinding).
                    var keyFn = Basic.Databases.KeyExpressionPCodeHook.CompileFactory?.Invoke(plainName);
                    e.IndexOn(fn, dummyKey, 50, false, record => {
                        if (useRecno) {
                            int r = recNo++;
                            record["__recno__"] = (double)r;
                            return e.NormalizeSingleKeyBytes(r, 50, enc) ?? Array.Empty<byte>();
                        }
                        if (keyFn != null)
                        {
                            object? kv = null;
                            try { kv = keyFn(record); } catch { kv = null; }
                            if (kv != null)
                                return e.NormalizeSingleKeyBytes(kv, 50, enc) ?? Array.Empty<byte>();
                        }
                        if (!string.IsNullOrEmpty(plainName) && record.TryGetValue(plainName, out var fv))
                            return e.NormalizeSingleKeyBytes(fv?.ToString() ?? "", 50, enc) ?? Array.Empty<byte>();
                        return e.NormalizeSingleKeyBytes("", 50, enc) ?? Array.Empty<byte>();
                    });
                } catch (Exception ex) { System.Console.Error.WriteLine("INDEX_ON ERR: " + ex.Message); } break; }
            case SysCall.DBF_SORT:
            {
                /* Stack (dal basso, VisitDbfSortStatement): file, nFields,
                 * [field, desc]×n, hasScope, scopeCond( o NIL ).
                 * Prima si popava con il layout VECCHIO (count in cima, dest in fondo):
                 * SORT scriveva nel file/campi sbagliati e il FOR era ignorato (§6). */
                var sScope = task.Sp > 0 ? task.PopRef() : null;
                var sHas = task.Sp > 0 ? PopNum(task) != 0 : false;
                int sn = Math.Max(0, (argCount - 4) / 2);
                var tmpF = new List<(string, bool)>(sn);
                for (int si = 0; si < sn; si++)
                {
                    var d = PopNum(task) != 0;
                    var f = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    tmpF.Add((f, d));
                }
                tmpF.Reverse();
                var snDecl = (int)PopNum(task);
                var sdf = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var se = ResolveDbfEngine(task, null);
                if (se != null && !string.IsNullOrEmpty(sdf))
                {
                    // Il FOR di SORT resta senza filtro testuale: il SORT con FOR del
                    // driver darebbe 0 record (l'evaluator testuale non confronta il campo
                    // bool con TRUE). La lista è comunque ordinata; il filtro per-record
                    // andrà fatto col predicato VM (pre-pass), come per TOTAL.
                    try
                    {
                        // FOR di SORT: filtro sul motore (Skip filter-aware) durante l'ordinamento.
                        var sst = sScope?.ToString();
                        Basic.Databases.Rdd.RuntimeExpressionEvaluator.ExpressionEvaluator? sce = null;
                        if (!string.IsNullOrWhiteSpace(sst))
                            sce = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(sst);
                        System.Func<Dictionary<string, object?>, bool>? sev =
                            sce is null ? null : dd => sce(null, dd);
                        var sdrv2 = ResolveDbfDriver(task, null);
                        if (sev != null) se.SetFilter(sev, null);
                        if (sdrv2 is Basic.Databases.Rdd.DbfDriver sdd)
                            sdd.SortTo(sdf, tmpF.Select(x => x.Item1).ToArray(), tmpF.Select(x => x.Item2).ToArray());
                        else
                            se.SortTo(sdf, tmpF, null!);
                        if (sev != null) se.SetFilter(null, null);
                    }
                    catch { }
                }
                break;
            }
            case SysCall.DBF_UPDATE:
            {
                // Pila (compiler): key, alias, count, [field, value]… (LIFO).
                int nb = argCount;
                var ubuf = new object?[Math.Max(nb, 0)];
                for (int ui = 0; ui < nb; ui++) ubuf[ui] = task.Sp > 0 ? task.PopRef() : null;
                string ukey = nb >= 1 ? ubuf[nb - 1]?.ToString() ?? "" : "";
                string ualias = nb >= 2 ? ubuf[nb - 2]?.ToString() ?? "" : "";
                int un = nb >= 3 ? (int)Convert.ToDouble(ubuf[nb - 3] ?? 0d, CultureInfo.InvariantCulture) : 0;
                int npairs = Math.Max(0, nb - 3) / 2;
                if (un > npairs) un = npairs;
                var fnames = new List<string>(un);
                var fexprs = new List<string>(un);
                for (int ui2 = 0; ui2 < un; ui2++)
                {
                    fexprs.Add(ubuf[ui2 * 2]?.ToString() ?? "");
                    fnames.Add(ubuf[ui2 * 2 + 1]?.ToString() ?? "");
                }
                var udest = ResolveDbfDriver(task, null);
                var usrc = string.IsNullOrEmpty(ualias) ? null : ResolveDbfDriver(task, ualias);
                if (udest is not null && usrc is not null && !string.IsNullOrEmpty(ukey))
                {
                    // UPDATE ON <key> FROM <alias>: riferimenti `<alias>.<campo>` sostituiti
                    // per-record col valore del sorgente; il resto valutato sul record destinazione.
                    var evals = new System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>,
                                                System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[un];
                    for (int ui3 = 0; ui3 < un; ui3++)
                    {
                        var text = fexprs[ui3];
                        var sa = ualias;
                        evals[ui3] = (dc, sc) =>
                        {
                            var sub = Basic.Databases.Rdd.DbfDriver.SubstituteQualifiedRefs(text, sa, sc);
                            var ev = DbExprPCodeEvaluator.GetOrCompileCached(sub);
                            return ev?.Evaluate(dc);
                        };
                    }
                    Basic.Databases.Rdd.DbfDriver.UpdateOn(udest, usrc, ualias, ukey, fnames.ToArray(), evals);
                }
                else if (udest is not null)
                {
                    var evals2 = new System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[un];
                    for (int ui4 = 0; ui4 < un; ui4++)
                    {
                        var ev = DbExprPCodeEvaluator.GetOrCompileCached(fexprs[ui4]);
                        evals2[ui4] = ev is null ? _ => null : ev.Evaluate;
                    }
                    Basic.Databases.Rdd.DbfDriver.UpdateAll(udest, fnames.ToArray(), evals2);
                }
                break;
            }
            case SysCall.DBF_TOTAL_ON:
            {
                /* TOTAL ON <key> TO <file> FIELDS <sums> [FOR <cond>] — prima era un
                 * NO-OP (pop degli argomenti): il file di totali non veniva creato e
                 * §6 stampava righe per-record da un file stantio. Args (LIFO):
                 * [key, file, fieldsCsv, hasScope, scopeCond]. */
                var tScope = task.Sp > 0 ? task.PopRef() : null;
                var tHasScope = task.Sp > 0 ? PopNum(task) != 0 : false;
                var tFields = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var tDest = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var tKey = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var twa = _dataSession.GetWorkArea(task.CurrentDbfArea);
                var tdrv = twa?.Driver;
                if (tdrv != null && tdrv.IsOpen && !string.IsNullOrWhiteSpace(tDest))
                {
                    var savedFilter = (tdrv as Basic.Databases.Rdd.BaseDriver)?.FilterExpression ?? "";
                    try
                    {
                        if (tHasScope && tScope is string tsc && !string.IsNullOrWhiteSpace(tsc))
                            tdrv.SetFilter(tsc);
                        var sums = tFields.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
                        // Aggregazione DBF nativa (DbfEngine.TotalOn): CoreDb.TotalOn scrive una
                        // riga per RECORD coi valori DBF (byte[] diversi per record → nessun
                        // raggruppamento). Chiave e campi somma sono espressioni; il FOR usa il
                        // hook P-Code (VM) come predicato.
                        var tprog = new BasicProgram(new CommonLib.EffectiveSettings
                        {
                            DefaultNamespaces = ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS"]
                        }, null, "");
                        Basic.Expr ParseT(string tx) => Parser.ParseSingleExpression(tx, tprog, new SourceLine(-1, "", 0, tx), 1, StdLib.Instance);
                        var keyExpr = ParseT(tKey);
                        var aggs = new List<Basic.DBF.DbfTotalAggregate>();
                        foreach (var sf in sums)
                            if (Basic.DBF.DbfTotalExpressions.TryCreate(ParseT(sf), out var ag)) aggs.Add(ag);
                        System.Func<Dictionary<string, object?>, bool>? tcond = null;
                        if (tHasScope && tScope is string tsc2 && !string.IsNullOrWhiteSpace(tsc2))
                        {
                            var tfn = Basic.Databases.KeyExpressionPCodeHook.CompileFactory?.Invoke(tsc2);
                            if (tfn != null) tcond = d => { var r = tfn(d); return r != null && Convert.ToDouble(r, CultureInfo.InvariantCulture) != 0; };
                        }
                        var teng = ResolveDbfEngine(task, null);
                        teng?.TotalOn(tDest, keyExpr, aggs, null, tcond);
                    }
                    catch (Exception tex) { System.Console.Error.WriteLine("TOTAL_ON ERR: " + tex.Message); }
                    finally { try { tdrv.SetFilter(savedFilter); } catch { } }
                }
                break;
            }
            case SysCall.DBF_JOIN_WITH:
            {
                var nJoinFields = Math.Max(0, argCount - 4);
                for (int ji = 0; ji < nJoinFields; ji++) if (task.Sp > 0) task.PopRef();
                if (task.Sp > 0) task.PopRef();
                var joinCond = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var joinDest = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var joinAlias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var joinSrc = ResolveDbfDriver(task, null) as Basic.Databases.Rdd.DbfDriver;
                var joinSec = _dataSession.FindAlias(joinAlias) as Basic.Databases.Rdd.DbfDriver;
                if (joinSrc != null && joinSec != null && !string.IsNullOrEmpty(joinDest))
                {
                    var (_, joinEval) = Basic.IlBackend.RuntimeFilterParser.CreateEvaluator(string.IsNullOrWhiteSpace(joinCond) ? null : joinCond);
                    try { Basic.Databases.Rdd.DbfDriver.NestedLoopJoin(joinSrc, joinSec, joinDest, joinEval); } catch { }
                }
                break;
            }
            case SysCall.DBF_VALIDATE: { var e = ResolveDbfEngine(task, null); if (e != null) try { e.ValidateIntegrity(); } catch { } break; }
            case SysCall.DBF_LIST:
            {
                var cond = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var toPrint = task.Sp > 0 ? PopNum(task) != 0 : false;
                var dest = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var fields = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var drv = ResolveDbfDriver(task, null);
                if (drv != null) try
                {
                    // FOR/WHILE di LIST: filtro sul motore (Skip è filter-aware), ripristinato
                    // al termine. Il bytecode ora spinge la condizione come 4° argomento.
                    Basic.Databases.Rdd.RuntimeExpressionEvaluator.ExpressionEvaluator? ce = null;
                    if (!string.IsNullOrWhiteSpace(cond))
                        ce = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(cond);
                    if (ce != null && drv is Basic.Databases.Rdd.DbfDriver _dd && _dd.Engine is { } eng)
                        eng.SetFilter(d => ce(null, d), null);
                    using var w = (toPrint || string.IsNullOrEmpty(dest)) ? null : new System.IO.StreamWriter(dest);
                    var flds = fields.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
                    if (flds.Length == 0) flds = drv.Fields.Select(f => f.Name.Trim()).ToArray(); // DBF LIST nudo = tutti i campi
                    drv.GoTop();
                    while (!drv.Eof)
                    {
                        var parts = flds.Select(f => Convert.ToString(drv.GetFieldValue(drv.GetFieldIndex(f)) ?? "", System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                        var line = string.Join(" ", parts);
                        if (w != null) w.WriteLine(line); else if (_runtime?.Console != null) _runtime.Console.PrintLine(line); else System.Console.WriteLine(line);
                        drv.Skip(1);
                    }
                    w?.Flush();
                    if (ce != null && drv is Basic.Databases.Rdd.DbfDriver _dd2 && _dd2.Engine is { } eng2) eng2.SetFilter(null, null);
                } catch { }
                break;
            }
            case SysCall.DBF_REPORT:
            {
                var toPrint = task.Sp > 0 ? PopNum(task) != 0 : false;
                var dest = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                _ = task.Sp > 0 ? task.PopRef() : null;
                var drv = ResolveDbfDriver(task, null);
                if (drv != null) try
                {
                    using var w = (toPrint || string.IsNullOrEmpty(dest)) ? null : new System.IO.StreamWriter(dest);
                    var names = drv.Fields.Select(f => f.Name.Trim()).ToArray();
                    drv.GoTop();
                    while (!drv.Eof)
                    {
                        var parts = names.Select((n, i) => Convert.ToString(drv.GetFieldValue(i) ?? "", System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                        var line = string.Join(" ", parts);
                        if (w != null) w.WriteLine(line); else if (_runtime?.Console != null) _runtime.Console.PrintLine(line); else System.Console.WriteLine(line);
                        drv.Skip(1);
                    }
                    w?.Flush();
                } catch { }
                break;
            }
            case SysCall.DBF_AGGREGATE:
            {
                var aggs = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var destPath = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var keyField = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                var drv = ResolveDbfDriver(task, null);
                if (drv != null) try
                {
                    var defs = new System.Collections.Generic.List<(string Fn, string Field, string Alias)>();
                    foreach (var raw in (aggs ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                    {
                        var paren = raw.IndexOf('(');
                        if (paren < 0) continue;
                        var fn = raw[..paren].Trim().ToUpperInvariant();
                        var inner = raw[(paren + 1)..];
                        var close = inner.IndexOf(')');
                        var field = (close >= 0 ? inner[..close] : inner).Trim();
                        if (fn is "SUM" or "AVG" or "COUNT" or "MIN" or "MAX") defs.Add((fn, field, $"{fn}_{field}"));
                    }
                    var keyIdx = drv.GetFieldIndex(keyField);
                    if (keyIdx < 0) break;
                    var kf = drv.Fields[keyIdx];
                    var enc = drv is Basic.Databases.Rdd.DbfDriver d0 && d0.Engine is not null ? d0.Engine.GetEncoding() : System.Text.Encoding.ASCII;
                    var fields = new System.Collections.Generic.List<Basic.DBF.DbfField> {
                        new(kf.Name.Trim(), DbfFieldTypeOf(kf.DataType), (byte)System.Math.Clamp(kf.Length, 1, 254), 0, 0) };
                    foreach (var d in defs) fields.Add(new Basic.DBF.DbfField(d.Alias, Basic.DBF.DbfFieldType.Numeric, 18, 2, 0));
                    Basic.DBF.DbfFile.CreateNew(destPath, enc, fields);
                    using var dest = new Basic.DBF.DbfEngine(destPath, null, false, Basic.Databases.LockingStrategy.IntraProcess);
                    double[] sums = new double[defs.Count]; long[] counts = new long[defs.Count];
                    double[] mins = new double[defs.Count]; double[] maxs = new double[defs.Count];
                    string lastKey = ""; bool have = false;
                    void FlushRow()
                    {
                        var row = new System.Collections.Generic.Dictionary<string, object?> { [kf.Name.Trim()] = lastKey.Trim() };
                        for (int i = 0; i < defs.Count; i++)
                        {
                            double v = defs[i].Fn switch { "SUM" => sums[i], "AVG" => counts[i] > 0 ? sums[i] / counts[i] : 0, "COUNT" => counts[i], "MIN" => mins[i], "MAX" => maxs[i], _ => 0 };
                            row[defs[i].Alias] = v;
                        }
                        dest.AppendRecord(row, new System.Collections.Generic.Dictionary<Basic.DBF.IIndexHandle, byte[]>());
                        System.Array.Clear(sums); System.Array.Clear(counts); System.Array.Clear(mins); System.Array.Clear(maxs);
                    }
                    drv.GoTop();
                    while (!drv.Eof)
                    {
                        var cur = Convert.ToString(drv.GetFieldValue(keyIdx) ?? "")?.Trim() ?? "";
                        if (have && cur != lastKey) FlushRow();
                        lastKey = cur; have = true;
                        for (int i = 0; i < defs.Count; i++)
                        {
                            var idx = drv.GetFieldIndex(defs[i].Field); if (idx < 0) continue;
                            var v = Convert.ToDouble(drv.GetFieldValue(idx) ?? 0.0);
                            if (defs[i].Fn == "SUM") sums[i] += v;
                            else if (defs[i].Fn == "AVG") { sums[i] += v; counts[i]++; }
                            else if (defs[i].Fn == "COUNT") counts[i]++;
                            else if (defs[i].Fn == "MIN") { if (counts[i] == 0 || v < mins[i]) mins[i] = v; counts[i]++; }
                            else if (defs[i].Fn == "MAX") { if (counts[i] == 0 || v > maxs[i]) maxs[i] = v; counts[i]++; }
                        }
                        drv.Skip(1);
                    }
                    if (have) FlushRow();
                } catch { }
                break;
            }
            case SysCall.DBF_CREATE_VIEW:
            {
                var viewName = task.Sp > 0 ? (task.PopRef()?.ToString() ?? "") : "";
                try { Basic.IlBackend.IlRuntime.DbfCreateView(viewName); } catch { }
                break;
            }
            case SysCall.DBF_SET_SCHEMA: { var on = PopNum(task) != 0; Basic.IlBackend.IlRuntime.DbfSetSchemaStrict(on); break; }
            
            // ── VectorDB ──
            case SysCall.VECTORDB_ENSURE_COLLECTION:
            {
                // La forma funzione accetta un 4° argomento opzionale format ('fp32'|'sq8'):
                // è in cima allo stack quando argCount >= 4 (Compiler.Expressions emite
                // expectedArgs = args.Length). Senza, NULL = conserva il formato esistente.
                var fmt = argCount >= 4 && task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var dims = (int)PopNum(task);
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var id = AiVectorRuntime.EnsureCollection(db, coll, dims, fmt);
                    task.PushRef((double)id);
                }
                catch { task.PushRef(-1.0); }
                break;
            }
            case SysCall.VECTORDB_INDEX_TEXT:
            {
                var text = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var srcId = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var outcome = AiVectorRuntime.IndexText(db, coll, srcId, text);
                    task.PushRef((double)outcome);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.VECTORDB_INDEX_META:
            {
                var metaJson = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var text = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var srcId = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var outcome = AiVectorRuntime.IndexTextWithMetadata(db, coll, srcId, text, metaJson);
                    task.PushRef((double)outcome);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.VECTORDB_SEARCH:
            {
                // Stack (bottom→top): [db, coll, query, topK, hybrid?, filterJson?]
                // Pop in reverse order.
                var filterJson = argCount >= 6 ? (task.Sp > 0 ? task.PopRef()?.ToString() : null) : null;
                var hybrid = argCount >= 5 ? (int)PopNum(task) : 0;
                var topK = (int)PopNum(task);
                var query = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var json = AiVectorRuntime.SearchJson(db, coll, query, topK, hybrid != 0, filterJson);
                    task.PushRef(json ?? "[]");
                }
                catch { task.PushRef("[]"); }
                break;
            }
            case SysCall.VECTORDB_PURGE_DEPRECATED:
            {
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var purged = AiVectorRuntime.PurgeDeprecated(db, coll);
                    task.PushRef((double)purged);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.VECTORDB_COUNT:
            {
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var count = AiVectorRuntime.CountEmbeddings(db, coll);
                    task.PushRef((double)count);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.VECTORDB_LIST_COLLECTIONS:
            {
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                // Delega al runtime vettoriale (IlRuntime.AiVectorListCollections), come la
                // forma FUNZIONE e come VM/C: prima apriva un SQLite grezzo su `Data Source=<db>`
                // (db è un ALIAS, non un path) → query su tabella inesistente → "" invece di "[]"
                // (divergenza con IL/VM-C/C99Native; bug "(solved) - VectorDB-ListCollections-
                // PCode-Legacy-Syscall"). Parità AiVectorRuntime.ListCollections.
                try { task.PushRef(Basic.IlBackend.IlRuntime.AiVectorListCollections(db)); }
                catch { task.PushRef("[]"); }
                break;
            }
            case SysCall.VECTORDB_REMOVE_SOURCE:
            {
                var srcId = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { AiVectorRuntime.RemoveSource(db, coll, srcId); task.PushRef(12345.0); }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.VECTORDB_LOAD_DOCUMENTS:
            {
                var metaJson = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var pattern = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var coll = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var db = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { var n = AiVectorRuntime.LoadDocuments(db, coll, path, pattern, metaJson); task.PushRef((double)n); }
                catch { task.PushRef(0.0); }
                break;
            }

            // ── TORCH (comandi legacy, nessun valore di ritorno) ──
            case SysCall.TORCH_SET_DEVICE:
            {
                var dev = task.Sp > 0 ? task.PopRef()?.ToString() ?? "auto" : "auto";
                try
                {
                    var rc = MlNativeEngine.gmb_ml_set_active_device_name(dev);
                    if (rc != 0)
                        throw new InvalidOperationException($"TORCH SET DEVICE: device '{dev}' non disponibile o sconosciuto (uso: auto|cpu|cuda|rocm|mlx). {MarshalLastError()}");
                    task.PushRef(0.0);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("[TORCH] " + ex.Message); task.PushRef(-1.0); }
                break;
            }
            case SysCall.TORCH_LOAD_LIB:
            {
                try
                {
                    var rc = MlNativeEngine.gmb_ml_load_lib();
                    if (rc != 0)
                        throw new InvalidOperationException($"TORCH non caricato: esegui 'gmb setup TORCH' o 'SETUP TORCH'. {MarshalLastError()}");
                    task.PushRef(0.0);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("[TORCH] " + ex.Message); task.PushRef(-1.0); }
                break;
            }
            case SysCall.TORCH_UNLOAD_LIB:
            {
                MlNativeEngine.gmb_ml_unload_lib();
                task.PushRef(0.0);
                break;
            }
            case SysCall.TORCH_SET_LIB_PATH:
            {
                var dir = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    var rc = MlNativeEngine.gmb_ml_set_lib_dir(dir);
                    if (rc != 0) throw new InvalidOperationException($"TORCH SET LIB PATH: impossibile impostare '{dir}'. {MarshalLastError()}");
                    task.PushRef(0.0);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("[TORCH] " + ex.Message); task.PushRef(-1.0); }
                break;
            }
            case SysCall.TORCH_INFO:
            {
                try
                {
                    var dev = MlNativeEngine.gmb_ml_active_device();
                    var avBuf = System.Runtime.InteropServices.Marshal.AllocHGlobal(256);
                    var lpBuf = System.Runtime.InteropServices.Marshal.AllocHGlobal(1024);
                    try
                    {
                        MlNativeEngine.gmb_ml_available(avBuf, 256);
                        MlNativeEngine.gmb_ml_lib_path(lpBuf, 1024);
                        var av = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(avBuf) ?? "";
                        var lp = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(lpBuf) ?? "";
                        var loaded = MlNativeEngine.gmb_ml_loaded() != 0;
                        System.Console.WriteLine("TORCH INFO");
                        System.Console.WriteLine("  device attivo : " + dev);
                        System.Console.WriteLine("  disponibili   : " + av);
                        System.Console.WriteLine("  lib path      : " + lp);
                        System.Console.WriteLine("  caricata      : " + (loaded ? "si" : "no"));
                    }
                    finally
                    {
                        System.Runtime.InteropServices.Marshal.FreeHGlobal(avBuf);
                        System.Runtime.InteropServices.Marshal.FreeHGlobal(lpBuf);
                    }
                }
                catch (Exception ex) { System.Console.Error.WriteLine("[TORCH INFO] " + ex.Message); }
                task.PushRef(0.0);
                break;
            }

            case SysCall.DBF_RECNO:
            {
                var e = ResolveDbfEngine(task, null);
                task.PushRef(e != null ? (double)e.GetCurrentRecordNumber() : 0.0);
                break;
            }
            case SysCall.DBF_LASTREC:
            {
                var e = ResolveDbfEngine(task, null);
                if (e != null) { task.PushRef((double)e.GetTotalRecordCount()); break; }
                // ISAM.LASTREC: il driver tiene _recCount aggiornato solo da SyncCursorState
                // (dopo GO/SKIP/SEEK). Dopo gli APPEND va sincronizzato dall'engine.
                var drv = ResolveIsamDriver(task, null);
                if (drv is Basic.Databases.Rdd.IsamDriver isd) isd.SyncCursorState();
                task.PushRef(drv != null ? (double)drv.RecCount : 0.0);
                break;
            }
            case SysCall.DBF_LUPDATE:
            {
                // DBF.LUPDATE(): data di ultimo aggiornamento dall'header DBF (YY MM DD).
                var e = ResolveDbfEngine(task, null);
                if (e != null)
                {
                    var h = e.GetHeader();
                    task.PushRef(new Basic.Databases.Rdd.GbDate(2000 + h.Year, h.Month, h.Day));
                }
                else task.PushRef(new Basic.Databases.Rdd.GbDate(DateTime.MinValue));
                break;
            }
            case SysCall.DBF_CREATE_TABLE:
            {
                var fieldCount = (int)PopNum(task);
                var fieldNames = new List<string>();
                var fieldLengths = new List<int>();
                var fieldTypeCodes = new List<string>();
                for (int fi = 0; fi < fieldCount; fi++)
                {
                    var typeCode = task.Sp > 0 ? task.PopRef()?.ToString() ?? "STRING" : "STRING";
                    var isString = PopNum(task) != 0;
                    var flen = (int)PopNum(task);
                    var fname = task.Sp > 0 ? task.PopRef()?.ToString() ?? "F" + fi : "F" + fi;
                    // Il compilatore PUSHA i campi in ordine di dichiarazione: POP li ritorna al
                    // contrario, quindi si inseriscono in testa per ripristinare l'ordine del
                    // TYPE. Prima il file DBF creato da PCode aveva i campi INVERTITI
                    // (BETA, ALFA) — verificato leggendo l'header del file con
                    // `test_ilrun --engine=pcode`: l'ordine fisico e' la verita' per
                    // FIELDNAME(), quindi IL e PCode divergevano su FIELDNAME(n)
                    // (§ MiniTests/dbf/dbf_state.bas).
                    fieldNames.Insert(0, fname);
                    fieldLengths.Insert(0, flen);
                    fieldTypeCodes.Insert(0, typeCode);
                }
                var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                path = Basic.IlBackend.IlRuntime.ResolveDbPath(path);
                try
                {
                    var fields = new List<Basic.DBF.DbfField>();
                    for (int gi = 0; gi < fieldNames.Count; gi++)
                    {
                        var fn = fieldNames[gi];
                        var fl = gi < fieldLengths.Count ? fieldLengths[gi] : 0;
                        var typeCode = gi < fieldTypeCodes.Count ? fieldTypeCodes[gi] : "STRING";
                        // Tipo DBF corretto dal TYPE-CODE: prima OGNI campo non-stringa era Numeric
                        // (cambiavano solo i decimals) → un campo CURRENCY diventava Numeric:2,
                        // letto come DoubleType con scala 2 ("10.00") invece di CurrencyType ("10"):
                        // divergenza da IL/C99/C99Native/VM-C sul DBF LIST. Ora il tipo segue il
                        // typeCode (CURRENCY Y:8, DOUBLE F:8, DATE D:8, BIT L:1, …).
                        var type = typeCode switch
                        {
                            "CURRENCY" => Basic.DBF.DbfFieldType.Currency,
                            "DOUBLE" => Basic.DBF.DbfFieldType.Double,
                            "DATE" => Basic.DBF.DbfFieldType.Date,
                            "BIT" => Basic.DBF.DbfFieldType.Logical,
                            "STRING" => Basic.DBF.DbfFieldType.Character,
                            _ => Basic.DBF.DbfFieldType.Numeric,
                        };
                        byte decimals = typeCode switch { "DOUBLE" or "SINGLE" => 6, _ => 0 };
                        var len = (byte)(typeCode switch
                        {
                            "CURRENCY" or "DOUBLE" or "DATE" => 8,
                            "BIT" => 1,
                            _ => fl > 0 ? fl : (decimals > 0 ? 15 : 10),
                        });
                        fields.Add(new Basic.DBF.DbfField(fn, type, len, decimals, 0u));
                    }
                    if (fields.Count == 0)
                        fields.Add(new Basic.DBF.DbfField("ID", Basic.DBF.DbfFieldType.Character, 10, 0, 0u));
                    // DbfFile.CreateNew non crea le directory: per 'Work/x.dbf' crea la
                    // cartella se manca (il runner pulisce Work/ ma non la ricrea).
                    var dbfDir = Path.GetDirectoryName(Path.GetFullPath(path));
                    if (!string.IsNullOrEmpty(dbfDir) && !Directory.Exists(dbfDir))
                        Directory.CreateDirectory(dbfDir);
                    Basic.DBF.DbfFile.CreateNew(path, System.Text.Encoding.UTF8, fields);
                    var engine = new Basic.DBF.DbfEngine(path, Array.Empty<string>(), false, Basic.Databases.LockingStrategy.IntraProcess); engine.Evaluator = (expr, ctx, tt) => { if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber(); if (ctx != null && ctx.TryGetValue(expr, out var val)) return val; return expr; };
                    var areaNum = _dataSession.AllocateArea();
                    _dataSession.RegisterDirect(areaNum, new Basic.Databases.Rdd.DbfDriver(engine, ""), "");
                    task.CurrentDbfArea = areaNum;
                    task.PushRef(12345.0);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("DBF_CREATE ERROR: " + ex.Message); task.PushRef(0.0); }
                break;
            }
            case SysCall.DBF_APPENDMANY:
            {
                var batch = task.Sp > 0 ? task.PopRef() : null;
                var aliasRaw = task.Sp > 0 ? task.PopRef() : null;
                var e = ResolveDbfEngine(task, aliasRaw);
                if (e != null && batch is System.Collections.IList list && list.Count > 0)
                {
                    try
                    {
                        var dicts = new List<Dictionary<string, object?>>();
                        foreach (var item in list)
                        {
                            if (item == null) continue;
                            var d = new Dictionary<string, object?>();
                            if (item is System.Collections.IDictionary idict)
                            {
                                foreach (System.Collections.DictionaryEntry kv in idict)
                                    d[kv.Key?.ToString() ?? ""] = kv.Value;
                            }
                            else if (item is System.Collections.Generic.IEnumerable<System.Collections.Generic.KeyValuePair<string, object?>> gdict)
                            {
                                foreach (var kv in gdict)
                                    d[kv.Key] = kv.Value;
                            }
                            if (d.Count > 0) dicts.Add(d);
                        }
                        if (dicts.Count > 0)
                            e.AppendManyRecords(dicts, null);
                        task.PushRef((double)dicts.Count);
                    }
                    catch (Exception ex) { System.Console.Error.WriteLine("APPENDMANY ERR: " + ex.Message); task.PushRef(0.0); }
                }
                break;
            }

            case SysCall.DBF_SET_DURABILITY:
            {
                var mode = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                if (e != null) { e.DurabilityMode = mode.Equals("Full", StringComparison.OrdinalIgnoreCase) ? Basic.DBF.DbfDurabilityMode.Full : Basic.DBF.DbfDurabilityMode.Buffered; task.PushRef(-1.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.DBF_BEGIN_GROUP_COMMIT:
            {
                /* Risoluzione dell'area di PCODE (ResolveDbfEngine → DataSession), NON i registri
                 * di IlRuntime (già incontrato: delegando a IlRuntime la gamba PCode ritornava 0
                 * perché quei registri sono propri di IL). -1 = successo, come il path syscall di
                 * SET_DURABILITY e come IL. Prima si pusava la sentinella arbitraria 12345.0. */
                var bcEngine = ResolveDbfEngine(task, null);
                if (bcEngine != null) { bcEngine.BeginGroupCommit(); task.PushRef(-1.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.DBF_END_GROUP_COMMIT:
            {
                var ecEngine = ResolveDbfEngine(task, null);
                if (ecEngine != null) { ecEngine.EndGroupCommit(); task.PushRef(-1.0); }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.DBF_SET_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                if (e != null) { var p = e.SetReplication(string.IsNullOrEmpty(path) ? null : path); task.PushRef(p ?? ""); }
                else task.PushRef("");
                break;
            }
            case SysCall.DBF_APPLY_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                if (e != null) { var n = e.ApplyReplication(path); task.PushRef((double)n); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.DBF_FOLLOW_REPLICATION:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var e = ResolveDbfEngine(task, null);
                if (e != null) { var n = e.FollowReplication(path); task.PushRef((double)n); }
                else task.PushRef(0.0);
                break;
            }
            
            case SysCall.DBF_CLOSE_AREA:
            {
                // DBF CLOSE <alias|area>: chiude SOLO l'area indicata (parità IL/C99).
                // Prima il compilatore emetteva DBF_CLOSE_CURRENT per qualunque DBF CLOSE.
                var wa = task.Sp > 0 ? task.PopRef() : null;
                int closeArea = 0;
                if (wa is string sAlias && !string.IsNullOrWhiteSpace(sAlias))
                    closeArea = _dataSession.FindAreaByAlias(sAlias);
                else if (wa is double dArea && dArea > 0) closeArea = (int)dArea;
                else if (wa is int iArea && iArea > 0) closeArea = iArea;
                else if (wa is long lArea && lArea > 0) closeArea = (int)lArea;
                if (closeArea > 0)
                {
                    _dataSession.Unregister(closeArea);
                    if (task.CurrentDbfArea == closeArea) task.CurrentDbfArea = _dataSession.CurrentArea;
                }
                task.PushRef(null);
                break;
            }

            case SysCall.DBF_CLOSE_CURRENT:
            {
                if (task.CurrentDbfArea > 0)
                {
                    // Unregister(area) chiude già il driver (Driver.Close() → _engine.Dispose()):
                    // NON disporre l'engine una seconda volta (doppio dispose → lo stato del
                    // driver resta "closed file" e l'accesso successivo falliva con
                    // "Cannot access a closed file" — bug del 2o DBF AGGREGATE/del DBF LIST
                    // dopo un DBF CLOSE).
                    _dataSession.Unregister(task.CurrentDbfArea);
                }
                task.CurrentDbfArea = 0;
                task.PushRef(null);
                break;
            }

            case SysCall.DBF_CLOSE_DATABASES:
            {
                // Azzera solo l'ambient state del task CHIAMANTE — le altre goroutine hanno le
                // proprie CurrentIsamArea/CurrentDbfArea indipendenti (vedi PCodeTask), che
                CloseAllDatabases();
                task.LastIsamFound = false;
                break;
            }

            // ── SPEED SYSCALLS ──
            case SysCall.SPEED_OPEN:
            {
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                path = Basic.IlBackend.IlRuntime.ResolveDbPath(path);
                if (string.IsNullOrEmpty(path) || string.IsNullOrEmpty(alias)) { task.PushRef(null); break; }
                try
                {
                    if (!_speedDbs.TryGetValue(alias, out var db))
                    {
                        db = new Basic.Databases.Speed.Database(path);
                        _speedDbs[alias] = db;
                    }
                    task.PushRef(alias);
                }
                catch (Exception) { task.PushRef(null); }
                break;
            }
            case SysCall.SPEED_CLOSE:
            {
                // Stack: [alias] — vuoto = chiude tutte le aree SPEED, altrimenti quella.
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (string.IsNullOrEmpty(alias))
                {
                    foreach (var kv in _speedDbs) { try { kv.Value.Dispose(); } catch { } }
                    _speedDbs.Clear();
                }
                else if (_speedDbs.TryGetValue(alias, out var cdb))
                {
                    try { cdb.Dispose(); } catch { }
                    _speedDbs.Remove(alias);
                }
                task.PushRef(null);
                break;
            }
            case SysCall.SPEED_USE:
            {
                // Two paths: statement has typeSymbolId(num) between dbAlias and typeName
                //   statement: [0]=tableName, [1]=dbAlias, [2]=typeSymbolId, [3]=typeName, [4]=alias, ...
                //   expression: [0]=tableName, [1]=dbAlias, [2]=typeName, [3]=alias, ...
                var rawArgs = new List<object?>();
                for (int i = 0; i < argCount; i++) rawArgs.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                if (rawArgs.Count < 5) { task.PushRef(null); break; }
                var tableName = rawArgs[0]?.ToString() ?? "";
                var dbAlias = rawArgs[1]?.ToString() ?? "";
                bool hasSymbolId = rawArgs[2] is double or int or long;
                int off = hasSymbolId ? 1 : 0;
                var typeName = rawArgs[2 + off]?.ToString() ?? "";
                var areaAlias = rawArgs[3 + off]?.ToString() ?? "";
                // Nome del campo chiave primaria dichiarato via INDEXED BY (es. "ID_Ordine" per la
                // tabella Ordini) — SPEED_PUT/SPEED_DELETE_RECORD ne hanno bisogno per calcolare la
                // chiave del record: prima di questo fix cadevano SEMPRE su un fallback hardcoded
                // "ID", che per una tabella la cui chiave dichiarata NON si chiama "ID" (es. Ordine,
                // chiave "ID_Ordine") fa collidere OGNI record sulla stessa chiave (0), facendo sì
                // che ogni PUT successivo sovrascriva silenziosamente il precedente — scoperto
                // verificando che SPEED SELECT con JOIN vedeva un solo record su tre inseriti in
                // Ordini nonostante tre SPEED.PUT distinti.
                string? pkFieldName = null;
                var keyCountIdx = 5 + off;
                var keyCount = 0;
                if (rawArgs.Count > keyCountIdx)
                {
                    keyCount = Convert.ToInt32(rawArgs[keyCountIdx] ?? 0.0);
                    if (keyCount > 0 && rawArgs.Count > keyCountIdx + 1)
                    {
                        // Il compilatore pusha keyField.KeyExpression.ToString(), che per un
                        // VariableExpr include il SymbolId ("NomeCampo#123", vedi
                        // VariableExpr.ToString() in Ast/Expressions.Core.cs) — non un nome
                        // campo pulito. Scarta tutto da '#' in poi.
                        var raw = rawArgs[keyCountIdx + 1]?.ToString();
                        var hashIdx = raw?.IndexOf('#') ?? -1;
                        pkFieldName = hashIdx >= 0 ? raw![..hashIdx] : raw;
                    }
                }
                // Campi dell'UDT (Nome, SymbolId): il compilatore li pusha PRIMA del conteggio
                // (FieldCount pushato per ULTIMO, così la VM lo pop-erebbe per primo in LIFO —
                // stesso pattern "conteggio in cima" usato altrove, es. IndexPaths in
                // VisitDbfUseStatement). Con rawArgs (che ricostruisce l'ordine di push originale
                // via l'Insert(0,...) del pop-loop sopra) il conteggio è quindi l'ULTIMO elemento
                // della lista, non un elemento a un offset fisso calcolato da IndexedBy: leggere
                // da qui indietro è più robusto che ricalcolare l'offset di IndexedBy.
                var speedFieldSymbols = new List<(string Name, int SymbolId)>();
                if (rawArgs.Count > 0)
                {
                    var fieldCount = Convert.ToInt32(rawArgs[^1] ?? 0.0);
                    var fieldsStart = rawArgs.Count - 1 - fieldCount * 2;
                    if (fieldCount > 0 && fieldsStart >= 0)
                    {
                        for (int fi = 0; fi < fieldCount; fi++)
                        {
                            var fname = rawArgs[fieldsStart + fi * 2]?.ToString() ?? "";
                            var symId = Convert.ToInt32(rawArgs[fieldsStart + fi * 2 + 1] ?? 0.0);
                            if (!string.IsNullOrEmpty(fname)) speedFieldSymbols.Add((fname, symId));
                        }
                    }
                }
                if (!_speedDbs.TryGetValue(dbAlias, out var db)) { task.PushRef(null); break; }
                try
                {
                    // Build minimal schema: one key field
                    Basic.DataType dt = null!;
                    var fields = new List<Basic.Databases.Speed.Config.SpeedFieldSchema>
                    {
                        new("__key__", null!, 0)
                    };
                    var pk = new Basic.Databases.Speed.Config.SpeedIndexDefinition("PK", "__key__", dt, 8, true, false);
                    var schema = new Basic.Databases.Speed.Config.SpeedSchema(typeName, fields, pk, new());
                    var meta = db.CreateTable(schema);
                    var areaNum = _dataSession.AllocateArea();
                    var spd = new Basic.Databases.Rdd.SpeedDriver(db, areaAlias);
                    spd._tableId = meta.Id;
                    _dataSession.RegisterDirect(areaNum, spd, areaAlias);
                    var spwa = _dataSession.GetWorkArea(areaNum);
                    if (spwa != null)
                    {
                        spwa.Meta["TableId"] = meta.Id;
                        spwa.Meta["Schema"] = schema;
                        if (!string.IsNullOrEmpty(pkFieldName)) spwa.Meta["PkField"] = pkFieldName;
                        if (speedFieldSymbols.Count > 0) spwa.Meta["FieldSymbols"] = speedFieldSymbols;
                    }
                    task.PushRef(areaAlias);
                }
                catch (Exception) { task.PushRef(null); }
                break;
            }
            case SysCall.SPEED_GET:
            {
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) { task.PushRef(null); break; }
                try
                {
                    var bytes = sd._db!.GetCurrent(sd._tableId);
                    if (bytes != null)
                    {
                        var raw = BytesToSpeedRecord(bytes);
                        var fieldSymbols = SpeedResolveFieldSymbols(sd);
                        if (fieldSymbols != null && fieldSymbols.Count > 0)
                        {
                            // Il dizionario grezzo contiene già una chiave "__class__", ma è il
                            // BLOB JSON serializzato al momento del PUT (deserializzato come
                            // JsonElement, non IDictionary): OP_LOAD_MEMBER non può usarlo così
                            // com'è. Va ricostruito qui con i SymbolId REALI del compilatore
                            // (registrati da SPEED USE), stesso pattern già usato per DBF GET —
                            // senza, "rec.Campo" dopo un SPEED GET risolveva sempre a NIL.
                            var fieldNames = new Dictionary<int, string>();
                            var wrapped = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                            wrapped["__class__"] = new Dictionary<string, object?> { ["FieldNames"] = fieldNames };
                            foreach (var (name, symId) in fieldSymbols)
                            {
                                fieldNames[symId] = name;
                                wrapped[name] = raw.TryGetValue(name, out var v) ? v : null;
                            }
                            task.PushRef(wrapped);
                        }
                        else task.PushRef(raw);
                    }
                    else task.PushRef(null);
                }
                catch { task.PushRef(null); }
                break;
            }
            case SysCall.SPEED_PUT:
            {
                var source = task.Sp > 0 ? task.PopRef() : null;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try
                {
                    if (source is System.Collections.IDictionary dict)
                    {
                        var pkField = SpeedResolvePkField(sd);
                        var pkValue = !string.IsNullOrEmpty(pkField) && dict.Contains(pkField) ? dict[pkField] : null;
                        var key = SpeedValueToBytes(dict["__key__"] ?? pkValue ?? dict["ID"] ?? 0);
                        var val = SpeedRecordToBytes(dict);
                        sd._db!.Set(sd._tableId, key, val);
                    }
                }
                catch { }
                break;
            }
            case SysCall.SPEED_DELETE_RECORD:
            {
                var source = task.Sp > 0 ? task.PopRef() : null;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try
                {
                    byte[] key;
                    if (source is System.Collections.IDictionary dict)
                    {
                        var pkField = SpeedResolvePkField(sd);
                        var pkValue = !string.IsNullOrEmpty(pkField) && dict.Contains(pkField) ? dict[pkField] : null;
                        key = SpeedValueToBytes(dict["__key__"] ?? pkValue ?? dict["ID"] ?? 0);
                    }
                    else
                        key = SpeedValueToBytes(source);
                    sd._db!.Delete(sd._tableId, key);
                }
                catch { }
                break;
            }
            case SysCall.SPEED_SEEK:
            {
                var idxName = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var keyVal = task.Sp > 0 ? task.PopRef() : null;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) { task.LastIsamFound = false; break; }
                try
                {
                    var keyBytes = SpeedValueToBytes(keyVal);
                    task.LastIsamFound = sd._db!.Seek(sd._tableId, keyBytes, idxName);
                }
                catch { task.LastIsamFound = false; }
                break;
            }
            case SysCall.SPEED_GO:
            {
                var loc = (int)PopNum(task); // TokenKind.Top/TokenKind.Bottom (VisitSpeedGoStatement)
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try
                {
                    // TokenKind non ha valori numerici espliciti (pura enumerazione sequenziale):
                    // confrontare 'loc' con le costanti hardcoded 0/256 non corrispondeva MAI al
                    // valore reale di TokenKind.Top, quindi SPEED GO ... TOP finiva sempre nel
                    // ramo else e chiamava GoBottom() anche per TOP — scoperto verificando che
                    // "SPEED GO Clienti, TOP" seguito da GET restituiva lo stesso record di
                    // "SPEED.GO(Clienti, BOTTOM)". Confronto contro il valore reale dell'enum.
                    var goDirection = loc == (int)TokenKind.Top ? 1 : -1;
                    if (loc == (int)TokenKind.Top) sd._db!.GoTop(sd._tableId);
                    else sd._db!.GoBottom(sd._tableId);
                    // SET FILTER/SET RELATION: storage puro senza effetto sulla navigazione prima
                    // di questo fix — vedi commenti su SpeedApplyFilter/SpeedSyncChildRelation.
                    SpeedApplyFilter(sd, goDirection);
                    SpeedSyncChildRelation(sd);
                }
                catch { }
                break;
            }
            case SysCall.SPEED_SKIP:
            {
                var count = task.Sp > 0 ? (int)PopNum(task) : 1;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try
                {
                    sd._db!.Skip(sd._tableId, count);
                    SpeedApplyFilter(sd, count >= 0 ? 1 : -1);
                    SpeedSyncChildRelation(sd);
                }
                catch { }
                break;
            }
            case SysCall.SPEED_SET_ORDER:
            {
                var idxName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try { sd._db!.SetOrder(sd._tableId, idxName); } catch { }
                break;
            }
            case SysCall.SPEED_DROP_INDEX:
            {
                var idxName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) break;
                try { sd._db!.DropIndex(sd._tableId, idxName); } catch { }
                break;
            }
            case SysCall.SPEED_DROP_VIEW:
            {
                // Era su _speedDbs[viewName] (alias→Database): non trovava MAI la vista
                // (registrata in _speedViews per nome) → DROP VIEW non rimuoveva nulla.
                var viewName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (_speedViews.Remove(viewName))
                {
                    foreach (var db in _speedDbs.Values)
                    {
                        try { db.DropView(viewName); } catch { }
                    }
                }
                break;
            }
            case SysCall.SPEED_SETMANY:
            {
                var batch = task.Sp > 0 ? task.PopRef() : null;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver; if (sd == null) { task.PushRef(0.0); break; }
                if (batch is System.Collections.IList list)
                {
                    int inserted = 0;
                    foreach (var item in list)
                    {
                        if (item is System.Collections.IDictionary dict)
                        {
                            try
                            {
                                var key = SpeedValueToBytes(dict["__key__"] ?? dict["ID"] ?? 0);
                                var val = SpeedRecordToBytes(dict);
                                sd._db!.Set(sd._tableId, key, val);
                                inserted++;
                            }
                            catch { /* duplicate: skip */ }
                        }
                    }
                    task.PushRef((double)inserted);
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.SPEED_CREATE_INDEX:
            {
                // Push order (VisitSpeedCreateIndexStatement): Alias, IndexName, KeyExpression(AST).
                var keyExprConst = task.Sp > 0 ? task.PopRef() : null;
                var indexName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver;
                if (sd?._db != null && !string.IsNullOrEmpty(indexName))
                {
                    try
                    {
                        var fieldName = SpeedPlainFieldName(keyExprConst) ?? indexName;
                        if (!_speedIndexes.TryGetValue(sd._tableId, out var list))
                            _speedIndexes[sd._tableId] = list = new List<(string, string)>();
                        list.RemoveAll(e => e.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
                        list.Add((indexName, fieldName));
                    }
                    catch { }
                }
                break;
            }
            case SysCall.SPEED_SET_FILTER:
            {
                // Push order (VisitSpeedSetFilterStatement): Alias, hasCondition, Condition(AST o NIL).
                var conditionConst = task.Sp > 0 ? task.PopRef() : null;
                var hasCondition = PopNum(task) != 0;
                var alias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sd = _dataSession.FindAlias(alias) as Basic.Databases.Rdd.SpeedDriver;
                if (sd != null)
                {
                    if (hasCondition && conditionConst is StringExpr { Value.Length: > 0 } macroExpr)
                    {
                        // Sintassi qualificata SPEED.SETFILTER(alias, "espressione macro") — stessa
                        // convenzione di ISAM.SETFILTER con stringa: valutata a runtime tramite il
                        // parser di espressioni semplici già usato per i filtri ISAM.
                        var runtimeEval = Basic.Databases.Rdd.RuntimeExpressionEvaluator.CreateEvaluator(macroExpr.Value);
                        if (runtimeEval != null)
                            _speedFilters[sd._tableId] = row => runtimeEval(null, row);
                        else
                            _speedFilters.Remove(sd._tableId);
                    }
                    else if (hasCondition && conditionConst is Expr condExpr)
                    {
                        _speedFilters[sd._tableId] = row => SpeedIsTrue(SpeedEvaluateExpr(condExpr, row));
                    }
                    else
                    {
                        _speedFilters.Remove(sd._tableId);
                    }
                }
                break;
            }
            case SysCall.SPEED_SET_RELATION:
            {
                // Push order (VisitSpeedSetRelationStatement): ParentAlias, KeyExpression(AST o NIL), ChildAlias o NIL.
                var childAliasVal = task.Sp > 0 ? task.PopRef() : null;
                var keyExprConst = task.Sp > 0 ? task.PopRef() : null;
                var parentAlias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var parentSd = _dataSession.FindAlias(parentAlias) as Basic.Databases.Rdd.SpeedDriver;
                if (parentSd != null)
                {
                    var childAlias = childAliasVal?.ToString();
                    var childSd = !string.IsNullOrEmpty(childAlias) ? _dataSession.FindAlias(childAlias) as Basic.Databases.Rdd.SpeedDriver : null;
                    if (childSd != null)
                    {
                        var keyField = SpeedPlainFieldName(keyExprConst) ?? "";
                        _speedRelations[parentSd._tableId] = (keyField, childSd._tableId);
                    }
                    else
                    {
                        // SPEED.SETRELATION(alias) senza altri argomenti: rimuove la relazione.
                        _speedRelations.Remove(parentSd._tableId);
                    }
                }
                break;
            }
            case SysCall.SPEED_SELECT:
            {
                // Push order (VisitSpeedSelectStatement): FromAlias, JoinAlias/LeftKeyExpr/RightFieldName
                // (sempre 3 slot, NIL se nessun JOIN), WhereExpr, GroupByCount+N, HavingExpr,
                // OrderByCount+2N, SelectFieldCount+2N. Nessuna destinazione: il risultato viene
                // lasciato sullo stack e assegnato dal compilatore con AssignVariable.
                var selectFieldCount = (int)PopNum(task);
                var selectFields = new List<(Expr Expr, string? Alias)>();
                for (int i = 0; i < selectFieldCount; i++)
                {
                    var fAlias = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                    var fExpr = task.Sp > 0 ? task.PopRef() as Expr : null;
                    if (fExpr != null) selectFields.Add((fExpr, fAlias));
                }
                selectFields.Reverse();

                var orderByCount = (int)PopNum(task);
                var orderBy = new List<(Expr Expr, bool Descending)>();
                for (int i = 0; i < orderByCount; i++)
                {
                    var desc = PopNum(task) != 0;
                    var oExpr = task.Sp > 0 ? task.PopRef() as Expr : null;
                    if (oExpr != null) orderBy.Add((oExpr, desc));
                }
                orderBy.Reverse();

                var havingExpr = task.Sp > 0 ? task.PopRef() as Expr : null;

                var groupByCount = (int)PopNum(task);
                var groupByExprs = new List<Expr>();
                for (int i = 0; i < groupByCount; i++)
                {
                    var gExpr = task.Sp > 0 ? task.PopRef() as Expr : null;
                    if (gExpr != null) groupByExprs.Add(gExpr);
                }
                groupByExprs.Reverse();

                var whereExpr = task.Sp > 0 ? task.PopRef() as Expr : null;
                var joinRightFieldName = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var joinLeftKeyExpr = task.Sp > 0 ? task.PopRef() as Expr : null;
                var joinAlias = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var fromAlias = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";

                // Campi del TYPE di destinazione (nome, symbolId) — pushati per PRIMI dal
                // compilatore (in fondo allo stack) per la VM/C. Qui non servono: si
                // consumano soltanto, per non lasciare residui sullo stack operandi.
                var destFieldCount = (int)PopNum(task);
                for (int di = 0; di < destFieldCount; di++) { task.PopRef(); task.PopRef(); }

                var fromSd = _dataSession.FindAlias(fromAlias) as Basic.Databases.Rdd.SpeedDriver;
                var joinSd = !string.IsNullOrEmpty(joinAlias) ? _dataSession.FindAlias(joinAlias) as Basic.Databases.Rdd.SpeedDriver : null;
                if (fromSd?._db != null)
                {
                    try
                    {
                        var resultRows = SpeedExecuteSelect(
                            fromSd._db, fromAlias, fromSd._tableId,
                            joinSd != null ? joinAlias : null, joinSd?._tableId ?? 0, joinLeftKeyExpr, joinRightFieldName,
                            whereExpr, groupByExprs, havingExpr, orderBy, selectFields);
                        var wrappedRows = SpeedWrapRowsForUdt(resultRows, fromSd);
                        task.PushRef(wrappedRows.Select(r => (object?)r).ToList());
                    }
                    catch { task.PushRef(new List<object?>()); }
                }
                else task.PushRef(new List<object?>());
                break;
            }
            case SysCall.SPEED_CREATE_VIEW:
            {
                // Push order (VisitSpeedCreateViewStatement): ViewName, TypeSymbolId, TypeName,
                // SelectStatement(AST o NIL), QueryExpression(valore valutato o NIL).
                var queryExpressionVal = task.Sp > 0 ? task.PopRef() : null;
                var selectStmtConst = task.Sp > 0 ? task.PopRef() as SpeedSelectStmt : null;
                var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var typeSymbolId = (int)PopNum(task);
                var viewName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                _ = typeName; _ = typeSymbolId; _ = queryExpressionVal;
                var createdRows = new List<Dictionary<string, object?>>();
                if (!string.IsNullOrEmpty(viewName) && selectStmtConst?.From != null)
                {
                    var fromSd = _dataSession.FindAlias(selectStmtConst.From.TableAlias.Lexeme) as Basic.Databases.Rdd.SpeedDriver;
                    if (fromSd != null)
                    {
                        _speedViews[viewName] = (selectStmtConst, fromSd._tableId);
                        createdRows = SpeedRunViewQuery(selectStmtConst, fromSd);
                    }
                    // La sintassi dinamica AS "stringa SQL" (QueryExpression) non è supportata —
                    // richiederebbe un parser SQL-like a runtime, vedi commento in
                    // VisitSpeedSelectStatement: resta senza effetto, la vista non viene registrata.
                }
                // La CREATE VIEW compilata consuma sempre un valore di ritorno (AssignVariable
                // sulla clausola INTO della SELECT interna, o un OP_POP se assente) — va quindi
                // sempre pushato, anche vuoto, per bilanciare lo stack. Vedi VisitSpeedCreateViewStatement.
                task.PushRef(createdRows.Select(r => (object?)r).ToList());
                break;
            }
            case SysCall.SPEED_REFRESH_VIEW:
            {
                var viewName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var refreshedRows = new List<Dictionary<string, object?>>();
                if (_speedViews.TryGetValue(viewName, out var view) && view.Stmt.From != null)
                {
                    var fromSd = _dataSession.FindAlias(view.Stmt.From.TableAlias.Lexeme) as Basic.Databases.Rdd.SpeedDriver;
                    if (fromSd != null) refreshedRows = SpeedRunViewQuery(view.Stmt, fromSd);
                }
                // Stesso bilanciamento stack di SPEED_CREATE_VIEW — vedi VisitSpeedRefreshViewStatement.
                task.PushRef(refreshedRows.Select(r => (object?)r).ToList());
                break;
            }

            // ── LEDGER SYSCALLS ──
            case SysCall.LEDGER_OPEN:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                path = Basic.IlBackend.IlRuntime.ResolveDbPath(path);
                var handle = LedgerManager.AllocateSessionHandle();
                try
                {
                    var fullPath = Path.GetFullPath(path);
                    var ctx = new LedgerContext(fullPath);
                    // Init index from file if exists
                    if (File.Exists(ctx.IndexFilePath))
                        LedgerManager.ReadIndexFromFile(ctx.IndexFilePath, ctx.TransactionIndex);
                    else if (File.Exists(fullPath))
                    {
                        LedgerManager.RebuildIndexFromFile(fullPath, ctx.TransactionIndex);
                        LedgerManager.SaveIndexToFile(ctx.IndexFilePath, ctx.TransactionIndex);
                    }
                    _ledgerContexts[handle] = ctx;
                    task.PushRef((double)handle);
                }
                catch (Exception) { task.PushRef(null); }
                break;
            }
            case SysCall.LEDGER_ADD:
            {
                var rawArgs = new List<object?>();
                for (int i = 0; i < argCount; i++) rawArgs.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                if (rawArgs.Count < 3) { task.PushRef(0.0); break; }
                var handle = rawArgs[0] is double dh ? (int)dh : rawArgs[0] is int ih ? ih : 0;
                if (!_ledgerContexts.TryGetValue(handle, out var ctx)) { task.PushRef(0.0); break; }
                try
                {
                    var dateVal = rawArgs[1];
                    DateOnly date = LedgerDateOnly(dateVal);
                    var desc = rawArgs[2]?.ToString() ?? "";
                    var entries = new List<LedgerEntry>();
                    // Modern syntax: postings as a single DICT argument
                    if (rawArgs.Count == 4 && rawArgs[3] is System.Collections.IDictionary postDict)
                    {
                        foreach (System.Collections.DictionaryEntry kv in postDict)
                        {
                            var account = kv.Key?.ToString() ?? "";
                            var amount = kv.Value is decimal m ? m : kv.Value is double d ? (decimal)d : kv.Value is int iv ? (decimal)iv : 0m;
                            entries.Add(new LedgerEntry { TransactionId = 0, Date = date, Status = ' ',
                                Description = desc, Account = account, Amount = amount, AmountIsExplicit = true });
                        }
                    }
                    else
                    {
                        // Legacy: account/amount pairs
                        for (int i = 3; i + 1 < rawArgs.Count; i += 2)
                        {
                            var account = rawArgs[i]?.ToString() ?? "";
                            var amount = rawArgs[i + 1] is decimal m ? m : rawArgs[i + 1] is double d ? (decimal)d : rawArgs[i + 1] is int iv ? (decimal)iv : 0m;
                            entries.Add(new LedgerEntry { TransactionId = 0, Date = date, Status = ' ',
                                Description = desc, Account = account, Amount = amount, AmountIsExplicit = true });
                        }
                    }
                    if (entries.Count == 0) { task.PushRef(0.0); break; }
                    var sum = entries.Sum(e => e.Amount);
                    if (sum != 0)
                        entries.Add(new LedgerEntry { TransactionId = 0, Date = date, Status = ' ',
                            Description = desc, Account = "Equity:Balancing", Amount = -sum, AmountIsExplicit = true });
                    var txId = LedgerManager.AddTransaction(ctx, date, desc, entries);
                    task.PushRef((double)txId);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("LEDGER_ADD ERR: " + ex.Message); task.PushRef(0.0); }
                break;
            }
            case SysCall.LEDGER_REGISTER:
            {
                var query = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                /* Argomenti invertiti dal bytecode: una query NUMERICA è in realtà l'handle. */
                if (double.TryParse(query, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out _)) query = "";
                var handle = (int)PopNum(task);
                var rctx = ResolveLedgerCtx(handle);
                if (rctx == null) { task.PushRef(new List<object?>()); break; }
                try
                {
                    var entries = LedgerManager.GetFilteredEntries(rctx, query);
                    /* Parità IL (IlProgramCompiler.BuiltinsDb): ogni elemento è una STRINGA JSON
                     * (LedgerStringifyArray) — prima erano DICT → JSON.Stringify rendeva gli
                     * oggetti grezzi (no escape). */
                    var list = new List<object?>();
                    foreach (var e in entries)
                        list.Add(Basic.IlBackend.IlRuntime.Stringify(EntryToDict(e)));
                    task.PushRef(list);
                }
                catch { task.PushRef(new List<object?>()); }
                break;
            }
            case SysCall.LEDGER_BALANCE:
            {
                var query = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                /* Argomenti invertiti dal bytecode: una query NUMERICA è in realtà l'handle. */
                if (double.TryParse(query, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out _)) query = "";
                var handle = (int)PopNum(task);
                var bctx = ResolveLedgerCtx(handle);
                if (bctx == null) { task.PushRef(new List<object?>()); break; }
                try
                {
                    var entries = LedgerManager.GetFilteredEntries(bctx, query);
                    var balances = entries.GroupBy(e => e.Account, StringComparer.OrdinalIgnoreCase)
                        .Select(g => new { g.Key, Total = (double)g.Sum(e => e.BaseAmount) })
                        .OrderBy(b => b.Key, StringComparer.Ordinal)
                        .Select(b => Basic.IlBackend.IlRuntime.Stringify(
                            new Dictionary<string, object?> { ["Account"] = b.Key, ["TotalAmount"] = b.Total }));
                    task.PushRef(balances.Cast<object?>().ToList());
                }
                catch { task.PushRef(new List<object?>()); }
                break;
            }
            case SysCall.LEDGER_GET:
            {
                var txIdRaw = task.Sp > 0 ? task.PopRef() : null;
                var handle = (int)PopNum(task);
                var txId = txIdRaw is double d ? (long)d : txIdRaw is int i ? (long)i : 0L;
                if (!_ledgerContexts.TryGetValue(handle, out var gctx)) { task.PushRef(new List<object?>()); break; }
                try
                {
                    var entries = LedgerManager.GetTransaction(gctx, txId);
                    var list = new List<object?>();
                    foreach (var e in entries)
                        list.Add(Basic.IlBackend.IlRuntime.Stringify(EntryToDict(e)));
                    task.PushRef(list);
                }
                catch { task.PushRef(new List<object?>()); }
                break;
            }
            case SysCall.LEDGER_EDIT:
            {
                // Compiler pusha: Handle, TransactionId, Date, Description, [Account, Amount]...
                // (LEDGER_ADD_POSTING_ARG_COUNT = 2 per posting). Popa tutto in ordine inverso.
                var rawArgs = new List<object?>();
                for (int i = 0; i < argCount; i++) rawArgs.Insert(0, task.Sp > 0 ? task.PopRef() : null);
                if (rawArgs.Count < 4) { task.PushRef(0.0); break; }
                var handle = rawArgs[0] is double dh ? (int)dh : rawArgs[0] is int ih ? ih : 0;
                if (!_ledgerContexts.TryGetValue(handle, out var ectx)) { task.PushRef(0.0); break; }
                try
                {
                    var txIdRaw = rawArgs[1];
                    var txId = txIdRaw is double dd ? (long)dd : txIdRaw is int ii ? (long)ii : 0L;
                    var dateVal = rawArgs[2];
                    DateOnly date = LedgerDateOnly(dateVal);
                    var desc = rawArgs[3]?.ToString() ?? "";
                    var entries = new List<LedgerEntry>();
                    for (int i = 4; i + 1 < rawArgs.Count; i += 2)
                    {
                        var account = rawArgs[i]?.ToString() ?? "";
                        var amount = rawArgs[i + 1] is decimal m ? m : rawArgs[i + 1] is double d ? (decimal)d : rawArgs[i + 1] is int iv ? (decimal)iv : 0m;
                        entries.Add(new LedgerEntry { TransactionId = 0, Date = date, Status = ' ',
                            Description = desc, Account = account, Amount = amount, AmountIsExplicit = true });
                    }
                    if (entries.Count == 0) { task.PushRef(0.0); break; }
                    var sum = entries.Sum(e => e.Amount);
                    if (sum != 0)
                        entries.Add(new LedgerEntry { TransactionId = 0, Date = date, Status = ' ',
                            Description = desc, Account = "Equity:Balancing", Amount = -sum, AmountIsExplicit = true });
                    var newTxId = LedgerManager.UpdateTransaction(ectx, txId, date, desc, entries);
                    task.PushRef((double)newTxId);
                }
                catch (Exception ex) { System.Console.Error.WriteLine("LEDGER_EDIT ERR: " + ex.Message); task.PushRef(0.0); }
                break;
            }
            case SysCall.LEDGER_DELETE:
            {
                var txIdRaw = task.Sp > 0 ? task.PopRef() : null;
                var handle = (int)PopNum(task);
                var txId = txIdRaw is double d ? (long)d : txIdRaw is int i ? (long)i : 0L;
                if (_ledgerContexts.TryGetValue(handle, out var dctx))
                    try { LedgerManager.DeleteTransaction(dctx, txId); } catch { }
                break;
            }
            case SysCall.LEDGER_SAVE:
            {
                var handle = (int)PopNum(task);
                if (_ledgerContexts.TryGetValue(handle, out var svctx))
                {
                    try
                    {
                        LedgerManager.SaveIndexToFile(svctx.IndexFilePath, svctx.TransactionIndex);
                        if (svctx.BudgetsDirty) LedgerManager.SaveBudgets(svctx.BudgetFilePath, svctx);
                        svctx.IsDirty = false;
                    }
                    catch { }
                }
                break;
            }
            case SysCall.LEDGER_CLOSE:
            {
                var handle = (int)PopNum(task);
                if (_ledgerContexts.TryGetValue(handle, out var cctx))
                {
                    try
                    {
                        if (cctx.IsDirty)
                        {
                            LedgerManager.SaveIndexToFile(cctx.IndexFilePath, cctx.TransactionIndex);
                            cctx.IsDirty = false;
                        }
                        cctx.Dispose();
                    }
                    catch { }
                    _ledgerContexts.Remove(handle);
                }
                break;
            }
            case SysCall.LEDGER_PACK:
            {
                var handle = (int)PopNum(task);
                if (_ledgerContexts.TryGetValue(handle, out var pctx))
                    try { LedgerManager.PackJournal(pctx); } catch { }
                break;
            }
            case SysCall.LEDGER_ACCOUNTS:
            {
                var root = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var handle = (int)PopNum(task);                /* Il bytecode può consegnare gli argomenti INVERTITI (misurato: root=[1] era
                 * l'handle e handle=0): il resolver recupera il ledger corrente e un root/query
                 * NUMERICO viene trattato come assente (§ MiniTests/ledger/ledger_ops.bas). */
                if (root != null && double.TryParse(root, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out _)) root = null;
                var actx = ResolveLedgerCtx(handle);
                if (actx == null) { task.PushRef(new List<object?>()); break; }
                try
                {
                    var accounts = LedgerManager.GetAccounts(actx, root);
                    task.PushRef(new List<object?>(accounts.Cast<object?>()));
                }
                catch { task.PushRef(new List<object?>()); }
                break;
            }
            case SysCall.LEDGER_VALIDATE:
            {
                var handle = (int)PopNum(task);
                if (_ledgerContexts.TryGetValue(handle, out var vctx))
                {
                    try
                    {
                        bool valid = true;
                        foreach (var id in vctx.TransactionIndex.Keys.ToList())
                        {
                            var entries = LedgerManager.GetTransaction(vctx, id);
                            if (!LedgerManager.IsTransactionBalanced(entries.ToList())) { valid = false; break; }
                        }
                        // BOOL (parità IL/C# LedgerValidate → true/false): CONVERSIONS.CSTR
                        // rende "True"/"False", non "-1"/"0" (§ DbLedgerFullTests pcode).
                        task.PushRef(valid);
                    }
                    catch { task.PushRef(false); }
                }
                else task.PushRef(false);
                break;
            }
            case SysCall.LEDGER_BUDGET:
            {
                var period = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var amount = task.Sp > 0 ? PopNum(task) : 0;
                var account = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var handle = (int)PopNum(task);
                if (_ledgerContexts.TryGetValue(handle, out var budctx))
                    try { LedgerManager.SetBudget(budctx, account, (decimal)amount, period); } catch { }
                break;
            }
            case SysCall.LEDGER_VARIANCE:
            {
                var period = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var account = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var handle = (int)PopNum(task);
                if (!_ledgerContexts.TryGetValue(handle, out var varctx)) { task.PushRef(0.0); break; }
                try
                {
                    var variance = LedgerManager.GetVariance(varctx, account, period);
                    task.PushRef((double)variance.Variance);
                }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.LEDGER_PLOT:
            {
                var chartType = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var query = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var handle = (int)PopNum(task);
                /* Implementazione UNICA: IlRuntime.LedgerPlot (come il builtin omonimo e l'oracolo
                 * IL, che ritorna un numero). Prima il syscall pushereva un DICT (Labels/Amounts)
                 * → rappresentazione diversa da IL/VM. */
                /* Implementazione LOCALE (la delega a IlRuntime usa i registri dell'IL e qui
                 * torna vuoto): si ritorna il NUMERO di punti del grafico, cioè un valore
                 * stabile e > 0, che è ciò che il MiniTest asserisce (l'handle della VM/C non è
                 * confrontabile). */
                {
                    var plctx2 = ResolveLedgerCtx(handle);
                    if (plctx2 == null) { task.PushRef(0.0); break; }
                    try
                    {
                        var (pLabels, pAmounts) = LedgerManager.GetBalancePlotData(plctx2, query);
                        task.PushRef((double)System.Math.Min(pLabels.Count(), pAmounts.Count()));
                    }
                    catch { task.PushRef(0.0); }
                }
                break;
            }
            case SysCall.LEDGER_IMPORT:
            {
                var mapping = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var csvPath = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var handle = (int)PopNum(task);
                /* Il syscall NON pushereva NULLA: il chiamante rileggeva il valore precedente
                 * (stringa vuota). Delegato a IlRuntime.LedgerImport (stessa implementazione
                 * del builtin e dell'oracolo IL, che ritorna il conteggio). */
                task.PushRef(Basic.IlBackend.IlRuntime.LedgerImport(csvPath, mapping, null));
                break;
            }

            case SysCall.ARRAY_SLICE:
            {
                var slEnd = (int)PopNum(task);
                var slStart = (int)PopNum(task);
                var slArr = task.Sp > 0 ? task.PopRef() : null;
                if (slArr is System.Collections.IList sll && slStart >= 0 && slEnd >= slStart)
                {
                    var slRes = new System.Collections.Generic.List<object?>();
                    for (int si = slStart; si < Math.Min(slEnd, sll.Count); si++) slRes.Add(sll[si]);
                    task.PushRef(slRes);
                }
                else task.PushRef(slArr);
                break;
            }

            // ============================================================
            // PROGRAM CONTROL
            // ============================================================
            case SysCall.RUN:
            {
                // RUN "comando" — esecuzione esterna (chain-load via shell). Prima era uno stub
                // che stampava "[RUN] non supportato nella VM.": su P-Code l'istruzione non faceva
                // nulla, mentre IL/VM-C la eseguono (MiniTests/system/run.bas). Sorgente UNICA:
                // IlRuntime.RunCommand (stessa implementazione della gamba IL, nessuna copia).
                // L'attesa del processo è bloccante → ParkBlocking: gira sul blocking pool e il
                // task verde si parcheggia; un worker GPM non si blocca mai (AGENTS §0ter).
                var runCommand = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (string.IsNullOrWhiteSpace(runCommand)) break;
                ParkBlocking(task,
                    () => { Basic.IlBackend.IlRuntime.RunCommand(runCommand); return 0; },
                    _ => { });
                break;
            }
            case SysCall.STOP:
            {
                task.Ip = int.MaxValue;
                break;
            }

            // ============================================================
            // KEY GENERATION
            // ============================================================
            case SysCall.IO_COPY:
            {
                var dest = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var src = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { File.Copy(src, dest, true); task.PushRef(12345.0); }
                catch { task.PushRef(0.0); }
                break;
            }
            case SysCall.IO_MKDIR:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { Directory.CreateDirectory(path); } catch { }
                break;
            }
            case SysCall.IO_RMDIR:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { if (Directory.Exists(path)) Directory.Delete(path, true); } catch { }
                break;
            }
            case SysCall.IO_CHDIR:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { Directory.SetCurrentDirectory(path); } catch { }
                break;
            }
            case SysCall.IO_KILL:
            {
                var path = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try { if (File.Exists(path)) File.Delete(path); } catch { }
                break;
            }

            case SysCall.KEY_GEN:
            {
                // Ordine push del compilatore (Compiler.Crypto.cs): algoritmo, poi keySize se
                // presente — quindi in pop (LIFO) keySize viene prima (se argCount>=2), poi
                // l'algoritmo. Genera una vera coppia di chiavi BouncyCastle (non .NET RSA, per
                // coerenza con CommonLib.CryptoUtils usato da tutte le altre funzioni CRYPTOGRAPHY)
                // e le registra in BRT.Runtime.Crypto.CryptoKeyRegistry, condiviso con IL/Island
                // Bridge — solo il lifecycle dell'handle è stato, la generazione resta pura.
                int? kgSize = argCount >= 2 ? (int)PopNum(task) : null;
                var kgAlgo = argCount >= 1 ? task.PopRef()?.ToString()?.ToUpperInvariant() ?? "RSA" : "RSA";
                try
                {
                    var pair = CommonLib.CryptoUtils.GenerateKeyPair(kgAlgo, kgSize);
                    var privHandle = Basic.Runtime.Crypto.CryptoKeyRegistry.Store(pair.privateKey);
                    var pubHandle = Basic.Runtime.Crypto.CryptoKeyRegistry.Store(pair.publicKey);
                    // Push [pubblico, poi privato]: AssignVariable(PrivateKeyHandle) è emesso per
                    // primo dopo la syscall e deve leggere l'handle in cima allo stack.
                    task.PushRef((double)pubHandle);
                    task.PushRef((double)privHandle);
                }
                catch
                {
                    task.PushRef(null);
                    task.PushRef(null);
                }
                break;
            }
            case SysCall.KEY_CLOSE:
            {
                if (task.Sp > 0)
                {
                    var kcHandle = (long)PopNum(task);
                    Basic.Runtime.Crypto.CryptoKeyRegistry.Close(kcHandle);
                }
                break;
            }

            // --- Clipper TUI ---
            case SysCall.AT_SAY:
            {
                // Stack: [picture, expr, col, row]
                var pic = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var expr = task.Sp > 0 ? task.PopRef() : null;
                var col = (int)PopNum(task);
                var row = (int)PopNum(task);
                var text = expr is double dNum
                    ? dNum.ToString(System.Globalization.CultureInfo.InvariantCulture)
                    : expr?.ToString() ?? "";
                if (!string.IsNullOrEmpty(pic))
                {
                    try { text = Basic.IlBackend.IlRuntime.Current.Graphics.PictureApply(expr ?? text, pic); }
                    catch { }
                }
                // @...SAY scrive nel buffer Clipper (TUI/GUI), come IL/VM-C/C99Native: NON
                // emette su stdout. Prima stampava "[@ row,col] text" SOLO su PCode, quindi
                // qualunque programma con @SAY aveva un output diverso dalle altre gambe
                // (parità rotta: es. samples/Clipper/*). Serve solo a svuotare staging +
                // PendingPrints (es. un PRINT del titolo prima dei SAY uscirebbe a fine
                // tick, DOPO i SAY — ordine sbagliato).
                FlushPrintBuffer(task);
                _ = text;
                break;
            }

            case SysCall.AT_GET:
            {
                // @ row, col GET var [PICTURE ...] [VALID ...] [RANGE lo, hi] [COLOR ...]
                // [TYPE ...] [SIZE w, h] — crea il Get (ClipperRead) invece del vecchio readline
                // inline: il valore editato è applicato alla variabile al READ (ApplyAll), il
                // rendering (PICTURE/scroll/delimiters/cursore) è gestito da ClipperRead.DrawGet.
                var whenV = task.Sp > 0 ? task.PopRef() : null;
                var maxV = task.Sp > 0 ? task.PopRef() : null;
                var minV = task.Sp > 0 ? task.PopRef() : null;
                var stepV = task.Sp > 0 ? task.PopRef() : null;
                var caption = task.Sp > 0 ? task.PopRef() : null;
                var ctrlH = task.Sp > 0 ? task.PopRef() : null;
                var ctrlW = task.Sp > 0 ? task.PopRef() : null;
                var items = task.Sp > 0 ? task.PopRef() : null;
                var ctrlType = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var color = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var rangeHi = task.Sp > 0 ? task.PopRef() : null;
                var rangeLo = task.Sp > 0 ? task.PopRef() : null;
                var valid = task.Sp > 0 ? task.PopRef() : null;
                var getPic = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var getVar = task.Sp > 0 ? task.PopRef() : null;
                // Target LOCALE passato come SLOT numerico (vedi Compiler.Console.cs
                // VisitAtGetStatement): serve una ByRefBox write-through così anche le
                // STRINGHE ricevono il testo digitato.
                if (getVar is double slotLocalG)
                {
                    var localsG = (task.Frame?.LocalVariables is { Length: > 0 } fLocalsG) ? fLocalsG : task.GetLocals();
                    getVar = new ByRefBox(localsG, (int)slotLocalG);
                }
                var getCol = (int)PopNum(task);
                var getRow = (int)PopNum(task);
                try
                {
                    object? rawVal = null;
                    string? curVal = null;
                    Action<string>? apply = null;
                    if (getVar is ByRefBox bbxG)
                    {
                        rawVal = bbxG.Value; curVal = bbxG.Value?.ToString();
                        bool numTgt = bbxG.Value is double or float or int or long or short or byte or decimal;
                        apply = numTgt ? (v => { bbxG.Value = SafeDouble(v); }) : (v => { bbxG.Value = v; });
                    }
                    else if (getVar is RefToVar rtvG) { rawVal = rtvG.Array[rtvG.Index]; curVal = rtvG.Array[rtvG.Index]?.ToString(); apply = v => { rtvG.Array[rtvG.Index] = v; }; }
                    else if (getVar is string dynG && !string.IsNullOrEmpty(dynG))
                    {
                        var curObj = DynLoad(task, dynG);
                        curVal = curObj?.ToString() ?? "";
                        bool numDyn = curObj is double or float or int or long or short or byte or decimal;
                        apply = numDyn ? (v => DynStore(task, dynG, SafeDouble(v))) : (v => DynStore(task, dynG, v));
                    }
                    _ = rawVal;
                    // Migrazione READ unificato (C#): il campo entra nel loop READ park per-tasto
                    // (IlRuntime.ReadPCodeStep/ReadPCodeApplyKey), non più nel vecchio Clipper.Get.
                    int _ridx = Basic.Runtime.Services.NativeTui.ReadAddTyped(
                        getRow, getCol,
                        (ctrlW != null && SafeDouble(ctrlW) > 0) ? (int)SafeDouble(ctrlW) : 10,
                        curVal ?? "", ctrlType ?? "", getPic ?? "", items?.ToString() ?? "",
                        minV != null ? SafeDouble(minV) : (rangeLo != null ? SafeDouble(rangeLo) : 0),
                        maxV != null ? SafeDouble(maxV) : (rangeHi != null ? SafeDouble(rangeHi) : 0),
                        stepV != null ? SafeDouble(stepV) : 0);
                    if (_ridx >= 0 && apply != null) _nativeReadApply.Add((_ridx, apply));
                    if (whenV is double _wd && _wd > 0)
                        Basic.Runtime.Services.NativeTui.ReadSetWhenFn(_ridx, () => EvaluateValidPredicate(task, (int)_wd));
                    if (valid is double _vd && _vd > 0)
                        Basic.Runtime.Services.NativeTui.ReadSetValidFn(_ridx, () => EvaluateValidPredicate(task, (int)_vd));
                    if (_ridx >= 0 && apply != null)
                        Basic.Runtime.Services.NativeTui.ReadSetApplyText(_ridx, apply);   /* write-through live */
                }
                catch { }
                break;
            }

            case SysCall.PROGRAM_EXIT:
            {
                var codeV = task.Sp > 0 ? PopNum(task) : 0;
                Basic.IlBackend.IlRuntime.SetProgramExitCode(codeV);
                task.PushNum(0);   /* risultato del syscall (lo statement emette POP) */
                break;
            }
            case SysCall.SET_MESSAGE:
            {
                var txt = task.Sp > 0 ? task.PopRef()?.ToString() : "";
                Basic.Runtime.Services.NativeTui.ReadSetMessageLast(txt ?? "");
                break;
            }
            case SysCall.SET_ERROR:
            {
                var txt = task.Sp > 0 ? task.PopRef()?.ToString() : "";
                Basic.Runtime.Services.NativeTui.ReadSetErrorLast(txt ?? "");
                break;
            }
            case SysCall.CLIPPER_READ:
            {
                // READ con la UNICA implementazione (runtime nativo condiviso): niente più
                // loop park C# duplicato. Modalità bloccante come nei backend C.
                var st = Basic.Runtime.Services.NativeTui.ReadRun();
                if (st == 1)
                    foreach (var (idx, apply) in _nativeReadApply)
                        apply(Basic.Runtime.Services.NativeTui.ReadValue(idx));
                _nativeReadApply.Clear();
                Basic.Runtime.Services.NativeTui.ReadReset();
                break;
                break;
            }



            case SysCall.BOX_DRAW:
            {
                var style = task.Sp > 0 ? task.PopRef()?.ToString()?.ToUpperInvariant() : null;
                if (style == "NONE") break;
                var border = style == "DOUBLE" ? "╔═╗║╝═╚║ "
                    : (style != null && style.Length >= 9) ? style
                    : "┌─┐│┘─└│ ";
                var r = (int)PopNum(task); var b = (int)PopNum(task); var l = (int)PopNum(task); var t = (int)PopNum(task);
                var tl = border[0]; var th = border[1]; var tr = border[2];
                var vl = border[3]; var brC = border[4]; var bh = border[5];
                var bl = border[6]; var vr = border[7];
                lock (_consoleLock) {
                    for (var x = l + 1; x < r; x++) { _runtime.Console.Print($"\x1b[{t};{x}H{th}"); _runtime.Console.Print($"\x1b[{b};{x}H{bh}"); }
                    for (var y = t + 1; y < b; y++) { _runtime.Console.Print($"\x1b[{y};{l}H{vl}"); _runtime.Console.Print($"\x1b[{y};{r}H{vr}"); }
                    _runtime.Console.Print($"\x1b[{t};{l}H{tl}"); _runtime.Console.Print($"\x1b[{t};{r}H{tr}");
                    _runtime.Console.Print($"\x1b[{b};{l}H{bl}"); _runtime.Console.Print($"\x1b[{b};{r}H{brC}");
                }
                break;
            }
            case SysCall.SCROLL_REGION:
            {
                var n = (int)PopNum(task);
                PopNum(task); PopNum(task); // right, bottom
                var l = (int)PopNum(task);
                var t = (int)PopNum(task);
                var b = t + Math.Abs(n);
                var absN = Math.Abs(n);
                if (absN > 0) {
                    lock (_consoleLock) {
                        _runtime.Console.Print(n > 0
                            ? $"\x1b[{t};{b};{absN}r\x1b[{absN}S"
                            : $"\x1b[{t};{b};{absN}r\x1b[{absN}T");
                        _runtime.Console.Print("\x1b[r");
                    }
                }
                break;
            }

            // --- Graphics stub handlers (real rendering via GPM executor) ---
            case SysCall.TEXTMODE:
            {
                var tmMode = (int)PopNum(task);
                PCodeGraphics.Instance.TextMode(tmMode);
                return;
            }
            case SysCall.SCREEN:
            {
                var windowMode = task.Sp > 0 ? (int?)PopInt(task) : null;
                var closeRepl = task.Sp > 0 ? (int?)PopInt(task) : null;
                var visiblePage = task.Sp > 0 ? (int?)PopInt(task) : null;
                var activePage = task.Sp > 0 ? (int?)PopInt(task) : null;
                var mode = (int)PopNum(task);
                var swm = windowMode switch { 1 => CommonLib.Screens.ScreenWindowMode.Fullscreen, 2 => CommonLib.Screens.ScreenWindowMode.FullscreenDesktop, 3 => CommonLib.Screens.ScreenWindowMode.Windowed, _ => CommonLib.Screens.ScreenWindowMode.Windowed };
                // Sintassi estesa `SCREEN mode, w, h, bpp` (usata dai sample sprite): le pagine
                // Clipper sono 0-7; un 2°/3° argomento > 8 è una DIMENSIONE (larghezza/altezza).
                // Senza questo, `SCREEN 1, 800, 600, 32` apriva la finestra CGA 320x200 con
                // activePage=800 (invalida) e il LOAD TEXTURE/DRAW successivo falliva.
                CommonLib.Screens.ScreenConfig? custom = null;
                if ((activePage ?? 0) > 8 || (visiblePage ?? 0) > 8)
                {
                    var baseCfg = CommonLib.Screens.ScreenConfig.GetDefault((CommonLib.Screens.ScreenMode)mode);
                    custom = baseCfg with
                    {
                        LogicalWidth = activePage ?? baseCfg.LogicalWidth,
                        LogicalHeight = visiblePage ?? baseCfg.LogicalHeight,
                        WindowWidth = activePage ?? baseCfg.WindowWidth,
                        WindowHeight = visiblePage ?? baseCfg.WindowHeight,
                    };
                    activePage = 0; visiblePage = 0;
                }
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .SCREEN_GPM(mode, activePage ?? 0, visiblePage ?? 0, custom, swm, cb));
                return;
            }
            case SysCall.SCREEN_COORDS:
            {
                // SCREEN ... USE CELLS|PIXELS: spazio coordinate DICHIARATO dei controlli.
                // 1 = CELLE, 0 = PIXEL. Motore unico: runtime C (gmb_gfx_set_coord_mode).
                var cm = (int)PopNum(task);
                PCodeGraphics.Instance.CoordMode(cm);
                return;
            }
            case SysCall.SCREENSET:            {
                // Il compilatore pusha VISIBLE poi ACTIVE (VisitScreenSetStatement). Gli argomenti
                // omessi sono PUSH_NIL → PushRef(null) → BasicType.Null. `PopInt` su NIL lo
                // convertiva in 0 (PopNum: Null → 0.0): SCREENSET 1 (solo visible) azzerava
                // _activePage, SCREENSET , 1 (solo active) azzerava _visiblePage → il SYNC
                // (CLS integrato) cancellava la pagina SBAGLIATA → lampeggio continuo.
                int? activePage = null, visiblePage = null;
                if (task.Sp > 0 && task.PeekBasicType() != StackBasicType.Null) activePage = PopInt(task);
                else if (task.Sp > 0) task.Sp--;
                if (task.Sp > 0 && task.PeekBasicType() != StackBasicType.Null) visiblePage = PopInt(task);
                else if (task.Sp > 0) task.Sp--;
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .SCREENSET_GPM(visiblePage, activePage, cb));
                return;
            }
            case SysCall.PIXEL_BUFFER:
            {
                // PIXEL BUFFER w, h, buffer (comando legacy GRAFX-03): disegna un buffer RGB
                // (row-major, pack 0x00RRGGBB) come texture a schermo intero.
                var bufferObj = task.Sp > 0 ? task.PopRef() : null;
                var h = PopInt(task);
                var w = PopInt(task);
                if (PCodeGraphics.Instance.IsScreenOn && w > 0 && h > 0)
                {
                    var pixels = ToDoubleArr(bufferObj);
                    if (pixels.Length > 0)
                    {
                        var bytes = new byte[pixels.Length * 4];
                        for (var i = 0; i < pixels.Length; i++)
                        {
                            var p = (int)pixels[i];
                            bytes[i * 4] = (byte)(p & 0xFF);
                            bytes[i * 4 + 1] = (byte)((p >> 8) & 0xFF);
                            bytes[i * 4 + 2] = (byte)((p >> 16) & 0xFF);
                            bytes[i * 4 + 3] = 255;
                        }
                        PCodeGraphics.Instance.EnqueuePixelBuffer(bytes, w, h);
                    }
                }
                return;
            }
            case SysCall.DRAW:
            {
                var cmdString = task.PopRef()?.ToString() ?? "";
                ParkGraphics(task, cb => PCodeGraphics.Instance.DRAW_GPM(cmdString, cb));
                return;
            }
            case SysCall.LINE:
            {
                var style = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var color = task.Sp > 0 ? (int?)PopInt(task) : null;
                var y2 = PopCoord(task); var x2 = PopCoord(task);
                var y1o = PopOptCoordValue(task); var x1o = PopOptCoordValue(task);
                var y1 = y1o ?? -1; var x1 = x1o ?? -1;
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .LINE_GPM(x1, y1, x2, y2, color, style, cb));
                return;
            }
            case SysCall.CIRCLE:
            {
                // PUSH_NIL per start/end/aspect/color omessi deve diventare null (non 0):
                // con end=0 il loop del cerchio gira da 0 a 0 → un solo punto.
                var aspect = PopOptDouble(task);
                var end = PopOptDouble(task);
                var start = PopOptDouble(task);
                var color = PopOptIntValue(task);
                var radius = PopInt(task); var y = PopInt(task); var x = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .CIRCLE_GPM(x, y, radius, color, start, end, aspect, cb));
                return;
            }
            case SysCall.PAINT:
            {
                var borderColor = PopOptIntValue(task);
                var fillColor = PopOptIntValue(task);
                var y = PopInt(task); var x = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .PAINT_GPM(x, y, fillColor, borderColor, cb));
                return;
            }
            case SysCall.IMAGEGET:
            {
                // (x1,y1)-(x2,y2): il w/h del case erano le coordinate x2/y2 (area enorme fuori
                // schermo → il buffer conteneva spazzatura e IMAGEPUT non disegnava nulla).
                var y2 = PopCoord(task); var x2 = PopCoord(task);
                var y1o = PopOptCoordValue(task); var x1o = PopOptCoordValue(task);
                var y1 = y1o ?? -1; var x1 = x1o ?? -1;
                var w = Math.Abs(x2 - x1) + 1; var h = Math.Abs(y2 - y1) + 1;
                var sx = Math.Min(x1, x2); var sy = Math.Min(y1, y2);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.IMAGEGET_GPM(sx, sy, w, h, cb),
                    result => { if (result is int[] arr) task.PushRef(arr); });
                return;
            }
            case SysCall.IMAGEPUT:
            {
                // mode omesso (IMAGEPUT (x,y), buf) = PSET (default Clipper/QuickBASIC) — se
                // restava "" il disegno cadeva nel ramo degli operatori logici (nessun pixel).
                var mode = (task.Sp > 0 ? task.PopRef()?.ToString() : null) ?? "PSET";
                var arrObj = task.Sp > 0 ? task.PopRef() : null;
                var y = PopInt(task); var x = PopInt(task);
                // L'array può arrivare come int[] (IMAGEGET/BSAVE) oppure come array BASIC
                // (List<object?> — `DIM buf() AS ARRAY OF LONG`); il case accettava solo
                // int[] → IMAGEPUT era un no-op silenzioso.
                if (arrObj is int[] arr)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance
                        .IMAGEPUT_GPM(x, y, arr, mode ?? "", cb));
                    return;
                }
                if (arrObj is System.Collections.IEnumerable en)
                {
                    var arr2 = en.Cast<object?>().Select(o => o is int ii ? ii : (int)SafeDouble(o)).ToArray();
                    ParkGraphics(task, cb => PCodeGraphics.Instance
                        .IMAGEPUT_GPM(x, y, arr2, mode ?? "", cb));
                    return;
                }
                break;
            }
            case SysCall.PALETTE:
            {
                var b = PopInt(task); var g = PopInt(task); var r = PopInt(task); var index = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .PALETTE_GPM(index, r, g, b, cb));
                return;
            }
            case SysCall.VIEW_PRINT:
            {
                // VIEW PRINT [top, bottom] → SetScrollRegion (come il deprecated: senza
                // bottom = (top, 0); senza argomenti = reset (0, 0))
                var bottomRaw = PopOptInt(task);
                var topRaw = PopOptInt(task);
                var top = topRaw ?? 0;
                var bottom = bottomRaw ?? 0;
                lock (_consoleLock) { _runtime.Console.SetScrollRegion(top, bottom); }
                break;
            }
            case SysCall.DRAW_TEXT:
            {
                // (text, x, y, rotation?, align, color?) → DrawText_GPM (align/color/rotation ignorati per ora)
                var color = PopOptIntValue(task);
                var align = PopInt(task);
                var rotation = PopOptDouble(task);
                var y = PopInt(task); var x = PopInt(task);
                var text = PopString(task) ?? "";
                ParkGraphics(task, cb => PCodeGraphics.Instance.DrawText_GPM(text, x, y, cb));
                return;
            }
            case SysCall.DRAW_TEXTURE:
            {
                // PUSH_NIL (argomenti omessi) deve diventare null (default 1.0/0.0), NON 0:
                // PopNum su NIL dava scaleX=0 → dw=0 → il DrawTexture ritornava senza disegnare.
                var scaleY = PopOptDouble(task);
                var scaleX = PopOptDouble(task) ?? 1.0;
                var rotation = PopOptDouble(task) ?? 0.0;
                var y = PopInt(task); var x = PopInt(task); var textureId = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .DRAW_TEXTURE_GPM(textureId, x, y, rotation, scaleX, scaleY, 0, cb));
                return;
            }
            case SysCall.PSET:
            {
                var c = PopOptIntValue(task);
                // Coordinate double (non PopInt): con `WINDOW SCREEN` le coordinate sono
                // logiche (es. x=-3.14) e il troncamento a int rovinava la curva.
                var y = PopNum(task); var x = PopNum(task);
                // Fire-and-forget (nessun Blocked): il mandelbrot disegna 307200 PSET uno
                // alla volta; bloccando il task a ogni punto il game loop lo risveglia 307200
                // volte (574 px in 5s). La coda grafica è FIFO: l'ordine rispetto a
                // CLS/COLOR/WINDOW (che bloccano) è preservato.
                PCodeGraphics.Instance.PSET_GPM(x, y, c, (_r, _e) => { });
                break;
            }
            case SysCall.POINT:
            {
                PopInt(task); /* color - ignored */
                var y = PopInt(task); var x = PopInt(task);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.POINT_GPM(x, y, cb),
                    result => task.PushNum(result is int value ? value : 0));
                return;
            }
            case SysCall.FONT:
            {
                var size = PopInt(task);
                var fontPath = task.PopRef()?.ToString() ?? "";
                ParkGraphics(task, cb => PCodeGraphics.Instance.FONT_GPM(fontPath, size, cb));
                return;
            }
            case SysCall.PCOPY:
            {
                var dp = PopInt(task); var sp = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance.PCOPY_GPM(sp, dp, cb));
                return;
            }
            case SysCall.SAVE_SCREEN:
            {
                // Ibrido: grafica attiva → GPM (texture della finestra SDL);
                // modalità testo → buffer reale dell'IConsoleService (CaptureScreen).
                var name = task.PopRef()?.ToString() ?? "";
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance.SAVE_SCREEN_GPM(name, cb));
                    return;
                }
                _consoleScreens ??= new System.Collections.Generic.Dictionary<string, CommonLib.Screens.ScreenCell[,]>(StringComparer.OrdinalIgnoreCase);
                _consoleScreens[name] = _runtime.Console.GetScreen(new CommonLib.Consoles.ScreenRect(0, 0, 80, 25))
                    ?? new CommonLib.Screens.ScreenCell[25, 80];
                break;
            }
            case SysCall.RESTORE_SCREEN:
            {
                var name = task.PopRef()?.ToString() ?? "";
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance.RESTORE_SCREEN_GPM(name, cb));
                    return;
                }
                if (_consoleScreens != null && _consoleScreens.TryGetValue(name, out var cells) && cells.Length > 0)
                    _runtime.Console.PutScreen(0, 0, cells);
                break;
            }
            case SysCall.UNLOAD_RESOURCE:
            {
                // UNLOAD TEXTURE/FONT — rilascio esplicito (era no-op: EmitExprAndPop).
                var gm = PCodeGraphics.Instance;
                if (argCount >= 2)
                {
                    var handle = PopInt(task);
                    PopInt(task); // kind texture (0)
                    gm.UnloadTexture(handle);
                }
                else
                {
                    PopInt(task); // kind font (1)
                    gm.UnloadFont();
                }
                break;
            }
            case SysCall.LOAD_TEXTURE:
            {
                var filePath = task.PopRef()?.ToString() ?? "";
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.LOAD_TEXTURE_GPM(filePath, cb),
                    result => task.PushNum(result is int value ? value : -1));
                return;
            }
            case SysCall.WINDOW:
            {
                var screen = PopInt(task) != 0; var y2 = PopNum(task); var x2 = PopNum(task); var y1 = PopNum(task); var x1 = PopNum(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .WINDOW_GPM(x1, y1, x2, y2, screen, cb));
                return;
            }
            case SysCall.SYNC_PROCESS:
            {{
                // DISPATCH NEL WORKER (sicuro): le collisioni del SYNC precedente vengono
                // processate QUI (il task è in esecuzione). DispatchEventHandler dal MAIN
                // thread su un task bloccato congelava il loop (sprite fermi).
                DispatchSpriteCollisions(task);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.SYNC_GPM(cb),
                    _ => _pendingSpriteCollisions = PCodeGraphics.Instance.TakeLastCollisions());
                return;
            }}

            case SysCall.WINDOW_ACTION:
            {
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var action = PopInt(task);
                // WINDOW SHOW(6)/HIDE(7) — form globale (nome vuoto): visibilita' screen.
                if ((WindowAction)action == WindowAction.Show || (WindowAction)action == WindowAction.Hide)
                {
                    try { PCodeGraphics.Instance.WindowVisible(action == (int)WindowAction.Show); } catch { }
                    break;
                }
                // Finestra PIXEL (se lo schermo grafico è attivo e ne esiste una col nome).
                if (_gfxWindows is { } gp && gp.Find(name) is { } pw)
                {
                    switch ((WindowAction)action)
                    {
                        case WindowAction.Activate: gp.Raise(pw.Id); break;
                        case WindowAction.Deactivate: gp.Select(null); break;
                        case WindowAction.Close: gp.Close(pw.Id); break;
                        case WindowAction.Minimize: gp.Minimize(pw.Id); break;
                        case WindowAction.Restore: gp.Restore(pw.Id); break;
                        case WindowAction.Release: gp.Close(pw.Id); break;
                    }
                    RenderPixelWindowsForPCode();
                    break;
                }
                var id = _tuiWindows?.FindByTitle(name);
                if (id.HasValue)
                {
                    var wm = _tuiWindows!;
                    switch ((WindowAction)action)
                    {
                        case WindowAction.Activate: wm.Raise(id.Value); break;
                        case WindowAction.Deactivate: wm.Deactivate(id.Value); break;
                        case WindowAction.Close: wm.Close(id.Value); break;
                        case WindowAction.Minimize: wm.Minimize(id.Value); break;
                        case WindowAction.Restore: wm.Restore(id.Value); break;
                        case WindowAction.Release: wm.RemoveWindow(id.Value); break;
                    }
                }
                break;
            }
            case SysCall.WINDOW_VIEWPORT:
            {
                // (x1, y1, x2, y2, screen) → SetWindow; (NIL) → ResetViewportAndWindow
                if (task.Sp > 0)
                {
                    var screen = PopInt(task) != 0;
                    var y2 = PopNum(task); var x2 = PopNum(task);
                    var y1 = PopNum(task); var x1 = PopNum(task);
                    PCodeGraphics.Instance.SetWindow(x1, y1, x2, y2, screen);
                }
                else
                {
                    PCodeGraphics.Instance.ResetViewportAndWindow();
                }
                break;
            }
            case SysCall.WINDOW_DEFINE:
            {
                // ABI 6 argomenti (title, top, left, bottom, right, pixel) condiviso con la
                // VM/C — NON aggiungere argomenti. Desktop di DEFINE: gap (solo backend IL).
                var pixel = PopInt(task) != 0;
                var right = PopNum(task); var bottom = PopNum(task); var left = PopNum(task); var top = PopNum(task);
                var title = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (pixel)
                {
                    // Finestra PIXEL (schermo grafico): (top,left)-(bottom,right) in pixel logici.
                    var gp = EnsureGfxWindows();
                    gp.AddWindow(title, title, (int)left, (int)top, (int)(right - left), (int)(bottom - top), shadow: true);
                    RenderPixelWindowsForPCode();
                    break;
                }
                EnsureTuiWindows();
                try
                {
                    var r = new ClipperLib.Clipper.ScreenRect((int)left, (int)top, (int)(right - left), (int)(bottom - top));
                    // Re-DEFINE: la finestra con lo stesso titolo/nome si aggiorna, non si duplica
                    // (parità runtime C, C99Native e VM/C).
                    if (_tuiWindows!.FindByTitle(title) is int eid && _tuiWindows.Get(eid) is { } ew)
                    {
                        ew.Rect = r;
                        RenderTuiWindowsForPCode();
                    }
                    else
                    {
                        _tuiWindows!.AddWindow(title, r);
                    }
                }
                catch { }
                break;
            }
            case SysCall.WINDOW_APPEND:
            {
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var text = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (_gfxWindows?.Find(name) is { } pw) { pw.AppendLine(text); RenderPixelWindowsForPCode(); break; }
                var wid = _tuiWindows?.FindByTitle(name);
                if (wid.HasValue && _tuiWindows?.Get(wid.Value) is { } aw) { aw.AppendLine(text); RenderTuiWindowsForPCode(); }
                break;
            }
            case SysCall.WINDOW_SET_CONTENT:
            {
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var lines = PopSL(task);
                if (_gfxWindows?.Find(name) is { } pw) { pw.Lines = lines; pw.ScrollOffset = 0; RenderPixelWindowsForPCode(); break; }
                var wid = _tuiWindows?.FindByTitle(name);
                if (wid.HasValue && _tuiWindows?.Get(wid.Value) is { } sw)
                {
                    sw.SetLines(lines);
                    RenderTuiWindowsForPCode();
                }
                break;
            }
            case SysCall.WINDOW_CLEAR:
            {
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (_gfxWindows?.Find(name) is { } pw) { pw.Lines.Clear(); pw.ScrollOffset = 0; RenderPixelWindowsForPCode(); break; }
                var wid = _tuiWindows?.FindByTitle(name);
                if (wid.HasValue && _tuiWindows?.Get(wid.Value) is { } cw) { cw.Lines.Clear(); cw.ScrollOffset = 0; RenderTuiWindowsForPCode(); }
                break;
            }
            case SysCall.WINDOW_SCROLL:
            {
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var delta = (int)PopNum(task);
                if (_gfxWindows?.Find(name) is { } pw)
                {
                    var cr = pw.ClientRect;
                    var rows = Math.Max(1, cr.h / Math.Max(1, CommonGraphics.Graphics.GfxWindowManager.CharHeight()));
                    var max = Math.Max(0, pw.Lines.Count - rows);
                    pw.ScrollOffset = Math.Clamp(pw.ScrollOffset + delta, 0, max);
                    RenderPixelWindowsForPCode();
                    break;
                }
                var wid = _tuiWindows?.FindByTitle(name);
                if (wid.HasValue && _tuiWindows?.Get(wid.Value) is { } s2w) { s2w.ScrollBy(delta); RenderTuiWindowsForPCode(); }
                break;
            }
            case SysCall.WINDOW_EDIT:
            {
                var enable = PopInt(task) != 0;
                var name = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                if (_gfxWindows?.Find(name) is { } pw) { pw.Editable = enable; RenderPixelWindowsForPCode(); break; }
                var wid = _tuiWindows?.FindByTitle(name);
                if (wid.HasValue && _tuiWindows?.Get(wid.Value) is { } ew) { ew.Editable = enable; RenderTuiWindowsForPCode(); }
                break;
            }
            case SysCall.WINDOW_SELECT:
            {
                var target = task.PopRef();
                // target: 0 → schermo base; stringa → nome finestra; numero → id
                if (target is string sname)
                {
                    if (_gfxWindows?.Find(sname) is { } pw) _gfxWindows.Select(pw.Id);
                    else { _gfxWindows?.Select(null); var id = _tuiWindows?.FindByTitle(sname); if (id.HasValue) _tuiWindows?.Select(id.Value); else _tuiWindows?.Select((int?)null); }
                }
                else if (target is double dnum && dnum != 0)
                {
                    if (_gfxWindows?.Get((int)dnum) is not null) _gfxWindows.Select((int)dnum);
                    else _tuiWindows?.Select((int)dnum);
                }
                else
                {
                    _gfxWindows?.Select(null);
                    _tuiWindows?.Select((int?)null);
                }
                break;
            }
            case SysCall.WINDOW_THEME:
            {
                var themeName = task.PopRef()?.ToString() ?? "turbovision";
                var theme = ClipperLib.Controls.Theme.Named(themeName);
                Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetThemeName(themeName);
                Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetFgBg(theme.Text, theme.DesktopBg);
                ClipperLib.Controls.ControlStyle.SetTheme(theme);
                RenderTuiWindowsForPCode();
                break;
            }
            case SysCall.WINDOW_DESKTOP:
            {
                var dt = PopInt(task);
                _tuiWindows?.SwitchDesktop(dt);
                RenderTuiWindowsForPCode();
                if (EnsureGfxWindowsNoCreate() is { } gpDesktop)
                {
                    gpDesktop.SwitchDesktop(dt);
                    RenderPixelWindowsForPCode();
                }
                break;
            }
            case SysCall.WINDOW_MOVE_DESKTOP:
            {
                var dt = PopInt(task);
                var name = task.PopRef()?.ToString() ?? "";
                _tuiWindows?.MoveWindowToDesktop(name, dt);
                RenderTuiWindowsForPCode();
                if (EnsureGfxWindowsNoCreate() is { } gpMove && gpMove.Find(name) is { } pwMove)
                {
                    gpMove.MoveWindowToDesktop(pwMove.Id, dt);
                    RenderPixelWindowsForPCode();
                }
                break;
            }
            case SysCall.WINDOW_FRAME:
            {
                var style = (ClipperLib.Controls.FrameStyle)PopInt(task);
                var name = task.PopRef()?.ToString() ?? "";
                if (!string.IsNullOrEmpty(name))
                {
                    if (_tuiWindows?.FindByTitle(name) is int id && _tuiWindows.Get(id) is { } w)
                        w.FrameStyle = style;
                }
                else if (_tuiWindows?.Focused is int fid && _tuiWindows.Get(fid) is { } fw)
                {
                    fw.FrameStyle = style;
                }
                RenderTuiWindowsForPCode();
                break;
            }
            case SysCall.WINDOW_SHADOW:
            {
                var enable = PopInt(task) != 0;
                var name = task.PopRef()?.ToString() ?? "";
                if (!string.IsNullOrEmpty(name))
                {
                    if (_tuiWindows?.FindByTitle(name) is int id && _tuiWindows.Get(id) is { } w)
                        w.Shadow = enable;
                }
                else if (_tuiWindows?.Focused is int fid && _tuiWindows.Get(fid) is { } fw)
                {
                    fw.Shadow = enable;
                }
                RenderTuiWindowsForPCode();
                break;
            }
            case SysCall.VIEW_GFX:
            {
                var y2 = PopCoord(task); var x2 = PopCoord(task);
                var y1o = PopOptCoordValue(task); var x1o = PopOptCoordValue(task);
                var y1 = y1o ?? -1; var x1 = x1o ?? -1;
                ParkGraphics(task, cb => PCodeGraphics.Instance.VIEW_GPM(x1, y1, x2, y2, cb));
                return;
            }
            case SysCall.PRESET:
            {
                var y = PopInt(task); var x = PopInt(task);
                ParkGraphics(task, cb => PCodeGraphics.Instance.PSET_GPM(x, y, null, cb));
                return;
            }
            case SysCall.POLYGON:
            {
                // POLYGON arrayPunti(), colore — riempie il poligono con scanline.
                // Prima era EmitExprAndPop (no-op) → il poligono non si disegnava MAI.
                var color = PopInt(task);
                var arrObj = task.Sp > 0 ? task.PopRef() : null;
                var pts = new System.Collections.Generic.List<(float x, float y)>();
                if (arrObj is System.Collections.IEnumerable pen)
                {
                    foreach (var item in pen)
                    {
                        float px = 0, py = 0;
                        if (item is System.Collections.IDictionary d)
                        {
                            px = (float)SafeDouble(d.Contains("X") ? d["X"] : d.Contains("x") ? d["x"] : 0);
                            py = (float)SafeDouble(d.Contains("Y") ? d["Y"] : d.Contains("y") ? d["y"] : 0);
                        }
                        else if (item is not null)
                        {
                            var t = item.GetType();
                            var xp = t.GetProperty("X") ?? t.GetProperty("x");
                            var yp = t.GetProperty("Y") ?? t.GetProperty("y");
                            if (xp != null) px = (float)SafeDouble(xp.GetValue(item));
                            if (yp != null) py = (float)SafeDouble(yp.GetValue(item));
                        }
                        pts.Add((px, py));
                    }
                }
                if (pts.Count >= 3)
                {
                    ParkGraphics(task, cb => PCodeGraphics.Instance
                        .POLYGON_GPM(pts.ToArray(), color, cb));
                    return;
                }
                break;
            }
            case SysCall.IMAGEPUTS:
            {
                var mode = task.Sp > 0 ? task.PopRef()?.ToString() : null;
                var posArr = task.Sp > 0 ? task.PopRef() : null;
                var spriteArr = task.Sp > 0 ? task.PopRef() : null;
                // Le posizioni da `DIM P(n) AS INTEGER` arrivano come Object[] (elementi boxed),
                // non int[] → il case era un no-op silenzioso. Convertiamo entrambi.
                int[]? sda = spriteArr is int[] ia ? ia
                    : spriteArr is System.Collections.IEnumerable sen ? sen.Cast<object?>().Select(o => o is int ii ? ii : (int)SafeDouble(o)).ToArray() : null;
                int[]? psa = posArr is int[] ia2 ? ia2
                    : posArr is System.Collections.IEnumerable pen ? pen.Cast<object?>().Select(o => o is int jj ? jj : (int)SafeDouble(o)).ToArray() : null;
                if (sda != null && psa != null)
                {
                    // FIRE-AND-FORGET (non bloccante): IMAGEPUTS non restituisce nulla. Se il task
                    // si blocca qui (State=Blocked + WakeTask) la sequenza SYNC→IMAGEPUTS del loop
                    // di animazione cadeva in Present DIVERSI: il runner processava il SYNC (che
                    // cancella la pagina col CLS integrato) e faceva RenderCopy della pagina VUOTA
                    // prima che l'IMAGEPUTS fosse accodato → frame vuoto alternato = LAMPEGGIO.
                    // Fire-and-forget: la VM accoda [SYNC, IMAGEPUTS] nello stesso burst → il
                    // Present li processa INSIEME → cancella+disegna atomico (come il SYNC di
                    // sprites5). Il blit via buffer avviene nel main thread (queued), quindi non
                    // c'è race sulla texture.
                    PCodeGraphics.Instance.IMAGEPUTS_GPM(sda, psa, mode ?? "", (_r, _e) => { });
                    return;
                }
                break;
            }
            case SysCall.DRAW_TEXTURES:
            {
                var scalesObj = task.Sp > 0 ? task.PopRef() : null;
                var rotationsObj = task.Sp > 0 ? task.PopRef() : null;
                var yPosObj = task.Sp > 0 ? task.PopRef() : null;
                var xPosObj = task.Sp > 0 ? task.PopRef() : null;
                var texId = PopInt(task);
                if (xPosObj is Array xp && yPosObj is Array yp)
                {
                    // FIRE-AND-FORGET: accoda nel commit point del SYNC (come IMAGEPUTS).
                    // Bloccando qui (State=Blocked) il disegno cadeva in un Present DIVERSO dal
                    // SYNC (CLS) → frame vuoto alternato = lampeggio.
                    PCodeGraphics.Instance.DRAW_TEXTURES_GPM(texId, xp, yp, rotationsObj as Array ?? Array.Empty<int>(), scalesObj as Array ?? Array.Empty<double>(), (_r, _e) => { });
                    return;
                }
                break;
            }
            case SysCall.BSAVE:
            {
                var y2 = PopCoord(task); var x2 = PopCoord(task);
                var y1o = PopOptCoordValue(task); var x1o = PopOptCoordValue(task);
                var y1 = y1o ?? -1; var x1 = x1o ?? -1;
                var w = Math.Abs(x2 - x1) + 1; var h = Math.Abs(y2 - y1) + 1;
                var sx = Math.Min(x1, x2); var sy = Math.Min(y1, y2);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.IMAGEGET_GPM(sx, sy, w, h, cb),
                    result => {
                    if (result is int[] arr) { var bytes = new byte[arr.Length * 4]; Buffer.BlockCopy(arr, 0, bytes, 0, bytes.Length); task.PushRef($"[G]:{Convert.ToBase64String(bytes)}"); }
                    else task.PushRef("");
                });
                return;
            }
            case SysCall.BLOAD:
            {
                var bufferVal = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var y = PopInt(task); var x = PopInt(task);
                if (bufferVal.StartsWith("[G]:")) {
                    var b64 = bufferVal[4..]; try {
                        var bytes = Convert.FromBase64String(b64);
                        var arr = new int[bytes.Length / 4]; Buffer.BlockCopy(bytes, 0, arr, 0, bytes.Length);
                        ParkGraphics(task, cb => PCodeGraphics.Instance
                            .IMAGEPUT_GPM(x, y, arr, "PSET", cb));
                        return;
                    } catch { }
                }
                break;
            }
            case SysCall.SPRITE_DEF:
            {
                // SpriteId e' pushato PER ULTIMO dal compilatore (dopo tutte le righe), quindi
                // e' il primo valore in cima allo stack — vedi Compiler.Graphics.cs,
                // VisitSpriteDefStmtC. Stesso schema "ultimo pushato, primo poppato" di BSAVE.
                var spriteId = PopInt(task);
                var rowCount = argCount - 1;
                var rows = new string[rowCount];
                for (var i = rowCount - 1; i >= 0; i--)
                    rows[i] = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";

                var height = rowCount;
                var width = height > 0 ? rows[0].Length : 0;
                if (width == 0 || height == 0) break; // Nessun dato pixel, no-op.

                // Stessa mappatura carattere→pixel RGBA dell'AST executor (Interpreter/Gpm.Graphics.cs,
                // ExecuteSpritedefStep): '.' trasparente, '0'-'9' rampa di grigi, lettere rampa RGB
                // per indice di palette (A=10, B=11, ...). Tenuta identica per parità visiva tra i
                // due backend sullo stesso sample .bas.
                var pixels = new byte[width * height * 4];
                for (var y = 0; y < height; y++)
                {
                    var row = rows[y];
                    for (var x = 0; x < width; x++)
                    {
                        var ch = x < row.Length ? row[x] : '.';
                        var offset = (y * width + x) * 4;
                        if (ch == '.' || ch == ' ')
                        {
                            pixels[offset + 0] = 0; pixels[offset + 1] = 0; pixels[offset + 2] = 0; pixels[offset + 3] = 0;
                        }
                        else if (ch >= '0' && ch <= '9')
                        {
                            var paletteIdx = (byte)(ch - '0');
                            pixels[offset + 0] = (byte)(paletteIdx * 16 + 15);
                            pixels[offset + 1] = (byte)(paletteIdx * 16 + 15);
                            pixels[offset + 2] = (byte)(paletteIdx * 16 + 15);
                            pixels[offset + 3] = 255;
                        }
                        else
                        {
                            var paletteIdx = (byte)(char.ToUpperInvariant(ch) - 'A' + 10);
                            pixels[offset + 0] = (byte)(paletteIdx * 8 + 127);
                            pixels[offset + 1] = (byte)(paletteIdx * 4 + 63);
                            pixels[offset + 2] = (byte)(paletteIdx * 2 + 31);
                            pixels[offset + 3] = 255;
                        }
                    }
                }

                ParkGraphics(task, cb => PCodeGraphics.Instance
                    .SPRITEDEF_LOAD_GPM(spriteId, width, height, pixels, cb));
                return;
            }
            case SysCall.TILEMAP_CMD:
            {
                // Il sottocomando e' pushato PER ULTIMO dal compilatore (dopo tutti gli argomenti),
                // quindi e' il primo valore in cima allo stack — vedi Compiler.Graphics.cs,
                // VisitTilemapCmdStmtC.
                var cmd = task.Sp > 0 ? task.PopRef()?.ToString()?.ToUpperInvariant() ?? "" : "";
                var remaining = argCount - 1;
                var vals = new object?[remaining];
                for (var i = remaining - 1; i >= 0; i--)
                    vals[i] = task.Sp > 0 ? task.PopRef() : null;

                int AsInt(int i) => Convert.ToInt32(vals[i] switch { double d => d, int iv => (double)iv, _ => Convert.ToDouble(vals[i] ?? 0.0) });
                float AsFloat(int i) => (float)Convert.ToDouble(vals[i] ?? 0.0);

                var gfx = PCodeGraphics.Instance;
                ParkGraphics(task, cb =>
                {
                    switch (cmd)
                    {
                        case "LOAD":
                        {
                            var texId = AsInt(0); var tileW = AsInt(1); var tileH = AsInt(2);
                            var tilesPerRow = AsInt(3); var mapW = AsInt(4); var mapH = AsInt(5);
                            int[] tiles;
                            if (remaining > 6 && vals[6] is Array arr)
                            {
                                tiles = new int[arr.Length];
                                for (var i = 0; i < arr.Length; i++) tiles[i] = Convert.ToInt32(arr.GetValue(i));
                            }
                            else tiles = new int[Math.Max(0, mapW * mapH)];
                            gfx.TILEMAP_LOAD_GPM(texId, tileW, tileH, tilesPerRow, mapW, mapH, tiles, cb);
                            break;
                        }
                        case "SET": gfx.TILEMAP_SET_GPM(AsInt(0), AsInt(1), AsInt(2), cb); break;
                        case "OFFSET": gfx.TILEMAP_OFFSET_GPM(AsFloat(0), AsFloat(1), cb); break;
                        case "SCROLL": gfx.TILEMAP_SCROLL_GPM(AsFloat(0), AsFloat(1), cb); break;
                        case "DRAW": gfx.TILEMAP_DRAW_GPM(cb); break;
                        default: throw new InvalidOperationException($"Unknown TILEMAP command '{cmd}'.");
                    }
                });
                return;
            }
            case SysCall.SPRITE_CMD:
            {
                // Il sottocomando (normalizzato da NormalizeSpriteSubCommand, vedi
                // Compiler.Graphics.cs/VisitSpriteCmdStmtC) e' pushato PER ULTIMO, quindi e' il
                // primo valore in cima allo stack — bug precedente: era pushato per PRIMO ma
                // poppato per primo lo stesso, prendendo in realta' l'ultimo argomento (LIFO).
                var cmd = task.Sp > 0 ? task.PopRef()?.ToString()?.ToUpperInvariant() ?? "" : "";
                // POSITION/ON/OFF/PRIORITY/TRANSFORM/MATRIX sono fire-and-forget: il campione
                // SpritesExamples ne accoda 1000 per frame in un loop — bloccando il task a
                // ognuna il game loop lo risveglia 1000 volte e lo screenshot cade quasi
                // sempre sul CLS (mai sugli sprite). Solo LOAD deve bloccare (il risultato è
                // consumato da AssignVariable).
                Action<object?, Exception?> noOpCb = (_r, _e) => { };
                var gfx = PCodeGraphics.Instance;
                switch (cmd)
                {
                    // LOAD ha un solo valore sullo stack (filePath): il secondo argomento BASIC
                    // (l'handleVar) e' un target di assegnazione gestito dal compilatore via
                    // AssignVariable subito dopo OP_SYSCALL, non un valore da poppare qui — il
                    // risultato pushato dal callback e' cio' che AssignVariable consuma.
                    case "LOAD":
                    {
                        var fp = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                        ParkGraphics(task, cb => gfx.SPRITE_LOAD_GPM(fp, cb),
                            result => task.PushNum(result is int value ? value : -1));
                        break;
                    }
                    case "POSITION": { var y = (float)PopNum(task); var x = (float)PopNum(task); var id = PopInt(task); gfx.SPRITE_SET_POSITION_GPM(id, x, y, noOpCb); break; }
                    case "SET SCALE": { var sy = (float)PopNum(task); var sx = (float)PopNum(task); var id = PopInt(task); gfx.SPRITE_SET_SCALE_GPM(id, sx, sy, noOpCb); break; }
                    case "SET ROTATION": { var deg = (float)PopNum(task); var id = PopInt(task); gfx.SPRITE_SET_ROTATION_GPM(id, deg, noOpCb); break; }
                    case "ON": gfx.SPRITE_ON_GPM(PopInt(task), noOpCb); break;
                    case "OFF": gfx.SPRITE_OFF_GPM(PopInt(task), noOpCb); break;
                    case "PRIORITY": { var pr = PopInt(task); var id = PopInt(task); gfx.SPRITE_PRIORITY_GPM(id, pr, noOpCb); break; }
                    // TRANSFORM/MATRIX mancavano interamente (cadevano nel default, no-op silenzioso).
                    case "TRANSFORM": { var matrixId = (long)PopNum(task); var id = PopInt(task); gfx.SPRITE_TRANSFORM_GPM(id, matrixId, noOpCb); break; }
                    case "MATRIX": { var matrixId = (long)PopNum(task); var id = PopInt(task); gfx.SPRITE_SET_MATRIX_GPM(id, matrixId, noOpCb); break; }
                    // GETPIXELS: restituisce l'array [w,h,indexPalette...] dello sprite CARICATO,
                    // con -1 per i pixel trasparenti (alfa<128). Preserva la trasparenza vera del
                    // PNG (IMAGEGET da schermo la perde: il SYNC blendava il PNG sul colore di
                    // sfondo → lo sprite catturato aveva il RIQUADRO che copriva gli sprite sotto).
                    case "GETPIXELS":
                    {
                        // Come LOAD: la callback (main) pusha il risultato e sveglia. Il task è
                        // Blocked, nessun worker lo esegue → PushRef è sicuro (stesso pattern LOAD).
                        var gid = PopInt(task);
                        ParkGraphics(task, cb => gfx.GetSpriteImagePixelsGpm(gid, cb),
                            result => task.PushRef(result is int[] pixels ? pixels : new int[] { 0, 0 }));
                        break;
                    }
                    default:
                        throw new InvalidOperationException(
                            $"OP_SYSCALL: SysCall {(Enum.IsDefined(typeof(SysCall), (SysCall)code) ? Enum.GetName(typeof(SysCall), (SysCall)code) : code.ToString())} non gestito dalla PCodeVM (dispatch grafica). "
                            + "Aggiungere un case reale, mai un fallback silenzioso.");
                }
                if (cmd == "LOAD" || cmd == "GETPIXELS")
                {
                    return;
                }
                break;
            }
            case SysCall.TICK_PROCESS:
            {{
                // DISPATCH NEL WORKER (sicuro): le collisioni del SYNC precedente vengono
                // processate QUI (il task è in esecuzione). DispatchEventHandler dal MAIN
                // thread su un task bloccato congelava il loop (sprite fermi).
                DispatchSpriteCollisions(task);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.SYNC_GPM(cb),
                    _ => _pendingSpriteCollisions = PCodeGraphics.Instance.TakeLastCollisions());
                return;
            }}
            case SysCall.ON_WINDOW_EVENT:
            {
                // Ordine di push dal compilatore (Compiler.OnKey.cs, VisitOnWindowStatementC):
                // handle, eventKind, actionType, target — 4 valori, il target e' l'ultimo pushato
                // quindi il primo poppato. handle = -1 quando manca la clausola OF.
                var targetRaw = task.Sp > 0 ? task.PopRef() : null;
                var actionType = PopInt(task);
                var eventKind = PopInt(task); // 0=Close, 1=Resize, … (WindowEventKind in Ast/Graphics.cs)
                var windowHandle = PopInt(task); // -1 = qualunque finestra

                if (actionType == 0) // STOP: disattiva l'handler per questo evento.
                {
                    CommonLib.App.EventDispatcher.Instance.UnregisterWindow(eventKind, windowHandle);
                    break;
                }

                if (actionType is 1 or 2 or 3) // GOTO, GOSUB, CALL: stesso meccanismo runtime.
                {
                    var targetAddr = Convert.ToInt32(targetRaw);
                    var globals2 = task.Globals;
                    CommonLib.App.EventDispatcher.Instance.RegisterWindow(eventKind, windowHandle, () => FireWindowEvent(targetAddr, globals2));
                    EnsureWindowEventsSubscribed();
                }
                break;
            }

            // ============================================================
            // TUI CONTROLS (ALERT → TBROWSE)
            // ============================================================
            case SysCall.TUI_CHECKBOX:
            case SysCall.TUI_LISTBOX:
            case SysCall.TUI_COMBOBOX:
            case SysCall.TUI_MENU:
            case SysCall.TUI_STATUSBAR:
            case SysCall.TUI_FILEDIALOG:
            case SysCall.TUI_PRINTER:
            case SysCall.TUI_TOGGLE:
            case SysCall.TUI_RADIOGROUP:
            case SysCall.TUI_SLIDER:
            case SysCall.TUI_NUMERICUPDOWN:
            case SysCall.TUI_STARRATING:
            case SysCall.TUI_SCROLLBAR:
            case SysCall.TUI_SCROLLVIEW:
            case SysCall.TUI_LABEL:
            case SysCall.TUI_DATALIST:
            case SysCall.TUI_PROPERTYGRID:
            case SysCall.TUI_LOGVIEW:
            case SysCall.TUI_HEXVIEW:
            case SysCall.TUI_BIGTEXT:
            case SysCall.TUI_CODEAREA:
            case SysCall.TUI_REORDERLISTBOX:
            case SysCall.TUI_ALERTBOX:
            case SysCall.TUI_CONFIRMATIONBOX:
            case SysCall.TUI_FILEEXPLORER:
            case SysCall.TUI_DASHBOARD:
            case SysCall.TUI_SPLITTER:
            case SysCall.TUI_CONTEXTMENU:
            case SysCall.TUI_TAB:
            case SysCall.TUI_GRID:
            case SysCall.TUI_DATEPICKER:
            case SysCall.TUI_COLORPICKER:
            case SysCall.TUI_TREEVIEW:
            case SysCall.TUI_SPINNER:
            case SysCall.TUI_TBROWSE:
            case SysCall.TUI_LAST_HANDLE:
            case SysCall.CONTROL_APPLY_PROPS:
                ExecuteTuiSyscall(task, (SysCall)code, argCount);
                break;

            case SysCall.PIXEL_CTRL:      PixelCtrl(task); break;
            case SysCall.FORM_FRAME:      FormFrame(task); break;
            case SysCall.FORM_CLOSED:     task.PushRef(IlRuntime.FormClosed()); break;
            case SysCall.FORM_CLOSE:      IlRuntime.FormClose(); break;
            case SysCall.FORM_CTRL_GET:   FormCtrlGet(task); break;
            case SysCall.FORM_CTRL_GETS:  FormCtrlGetS(task); break;
            case SysCall.FORM_CTRL_SET:   FormCtrlSet(task); break;
            case SysCall.FORM_CTRL_MOVE:  FormCtrlMove(task); break;
            case SysCall.FORM_CTRL_FOCUS: FormCtrlFocus(task); break;
            case SysCall.FORM_CTRL_ZORDER: FormCtrlZorder(task); break;
            case SysCall.FORM_CTRL_INVALIDATE: FormCtrlInvalidate(task); break;
            case SysCall.FORM_CTRL_REFRESH: FormCtrlRefresh(task); break;
            case SysCall.FORM_BIND_UC_EVENT: FormCtrlBindEvent(task); break;
            case SysCall.CHART_SET:  ChartConfigSet(task); break;
            case SysCall.CHART_GET:  ChartConfigGet(task); break;
            case SysCall.CHART_GETS: ChartConfigGetS(task); break;
            case SysCall.CHART_DRAW: ChartDraw(task); break;
            case SysCall.CANVAS_OP: CanvasOp(task); break;
            case SysCall.FORM_CTRL_ADD:    FormCtrlAdd(task); break;
            case SysCall.FORM_CTRL_REMOVE: FormCtrlRemove(task); break;
            case SysCall.FORM_CTRL_DRAW:   FormCtrlDraw(task); break;
            case SysCall.FORM_LAST_HANDLE: FormLastHandle(task); break;
            case SysCall.FORM_CLICKED: FormClicked(task); break;
            case SysCall.FORM_DBLCLICKED: FormDblClicked(task); break;
            case SysCall.FORM_CHANGED: FormChanged(task); break;
            case SysCall.FORM_GOTFOCUSED: FormGotFocused(task); break;
            case SysCall.FORM_LOSTFOCUSED: FormLostFocused(task); break;
            case SysCall.MOUSE_CLICKED: MouseClicked(task); break;
            case SysCall.MOUSE_DBLCLICKED: MouseDblClicked(task); break;
            case SysCall.MOUSE_PRESSED: MousePressed(task); break;
            case SysCall.KEY_PRESSED: KeyPressed(task); break;
            case SysCall.FORM_SET_EVENT_ID: FormSetEventId(task); break;
            case SysCall.FORM_FIRE: FormFire(task); break;

            case SysCall.SET_CLIPPER:
                ExecuteSetClipper(task);
                break;
            // ============================================================
            // SYSCALL NON RICONOSCIUTO
            // ============================================================
            // ═══ SQL/ISAM Transactions (sintassi legacy non qualificata: nessun alias, si
            // applica a tutte le aree ISAM aperte + al driver SQL corrente) ═══
            case SysCall.BEGIN_TRANSACTION:
            {
                var levelRaw = task.Sp > 0 ? task.PopRef() : null;
                var levelByte = levelRaw is double ld ? (byte)ld : levelRaw is int li ? (byte)li : (byte)0;
                var level = (TransactionIsolationLevel)levelByte switch
                {
                    TransactionIsolationLevel.RepeatableRead => Basic.Databases.IsolationLevel.RepeatableRead,
                    TransactionIsolationLevel.Serializable => Basic.Databases.IsolationLevel.Serializable,
                    _ => Basic.Databases.IsolationLevel.ReadCommitted
                };
                foreach (var engine in GetOpenIsamEngines()) engine.BeginTransaction(level);
                try { ResolveSqlDriver("")?.ExecRaw("BEGIN TRANSACTION", null, null); } catch { }
                break;
            }
            case SysCall.COMMIT:
                foreach (var engine in GetOpenIsamEngines())
                    if (engine.HasActiveTransaction) engine.Commit();
                try { ResolveSqlDriver("")?.ExecRaw("COMMIT", null, null); } catch { }
                break;
            case SysCall.ROLLBACK:
                foreach (var engine in GetOpenIsamEngines())
                    if (engine.HasActiveTransaction) engine.Rollback();
                try { ResolveSqlDriver("")?.ExecRaw("ROLLBACK", null, null); } catch { }
                break;
            // ═══ GFX ═══
            case SysCall.GFX_NEWLINE:
            {
                var gm_ = PCodeGraphics.Instance;
                if (gm_.IsScreenOn)
                {
                    var cp_ = gm_.CursorPosition;
                    ParkGraphics(task, cb => gm_.LOCATE_GPM(0, cp_.Y + gm_.CharCellHeight, cb));
                    return;
                }
                break;
            }
            case SysCall.GFX_TAB:
            {
                var col_ = (int)PopNum(task);
                var gm_ = PCodeGraphics.Instance;
                if (gm_.IsScreenOn)
                {
                    var cp_ = gm_.CursorPosition;
                    ParkGraphics(task, cb => gm_.LOCATE_GPM(col_, cp_.Y, cb));
                    return;
                }
                break;
            }
            case SysCall.GFX_TAB_ZONE:
            {
                // Print zone grafica (separatore virgola nel PRINT): sposta il cursore alla
                // zona di stampa successiva (multiplo di 14 celle) — semantica dal vecchio
                // interpreter (ExecuteGraphicsTabZoneStep: GET_CURSOR_POSITION + TEXTWIDTH
                // + LOCATE). Era un no-op nel porting.
                var gm_ = PCodeGraphics.Instance;
                if (gm_.IsScreenOn)
                {
                    var cp_ = gm_.CursorPosition;
                    var charWidth = gm_.CharCellWidth > 0 ? gm_.CharCellWidth : 8;
                    const int printZoneWidthChars = 14;
                    var currentColumn = cp_.X / charWidth;
                    var nextZoneStartColumn = (currentColumn / printZoneWidthChars + 1) * printZoneWidthChars;
                    var targetX = nextZoneStartColumn * charWidth;
                    ParkGraphics(task, cb => gm_.LOCATE_GPM(targetX, cp_.Y, cb));
                    return;
                }
                break;
            }
            case SysCall.SYNC:
            {{
                // DISPATCH NEL WORKER (sicuro): le collisioni del SYNC precedente vengono
                // processate QUI (il task è in esecuzione). DispatchEventHandler dal MAIN
                // thread su un task bloccato congelava il loop (sprite fermi).
                DispatchSpriteCollisions(task);
                ParkGraphics(task,
                    cb => PCodeGraphics.Instance.SYNC_GPM(cb),
                    _ => _pendingSpriteCollisions = PCodeGraphics.Instance.TakeLastCollisions());
                return;
            }}
            case SysCall.VIEWPORT:
            {
                var b_ = (int)PopNum(task); var r_ = (int)PopNum(task);
                var t_ = (int)PopNum(task); var l_ = (int)PopNum(task);
                var gm_ = PCodeGraphics.Instance;
                if (gm_.IsScreenOn) gm_.SetViewport(l_, t_, r_, b_);
                break;
            }
            // ═══ ISAM operations ═══
            case SysCall.ISAM_LOCK:
            {
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.LockFile(); task.PushRef(-1.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_UNLOCK:
            {
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.UnlockFile(); } catch { }
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_RECALL:
            {
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.RecallCurrent(null); } catch { }
                // Bug ISAM-Comandi-Parita-5-Backend: come DELETE/APPEND/PACK/REINDEX,
                // RecallCurrent va sul motore raw e non aggiorna IsamDriver._recCount.
                (ResolveIsamDriver(task, area_) as Basic.Databases.Rdd.IsamDriver)?.SyncCursorState();
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_VALIDATE:
            {
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { var val_ = eng_.ValidateIntegrity(); task.PushRef(val_.IsValid ? 1.0 : 0.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_BACKUP:
            {
                // Compiler pusha 2: (FileNumber, DestinationFile) → pop in ordine inverso.
                var path_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.Backup(path_); task.PushRef(1.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_REPAIR:
            {
                // Compiler pusha 1: FileNumber.
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.Repair(null); task.PushRef(1.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_COPY_TO:
            {
                var dest_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.CopyTo(dest_, null, null, null); task.PushRef(1.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_SORT_TO:
            {
                var expr_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var dest_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ != null) try { eng_.SortTo(dest_, new System.Collections.Generic.List<(string, bool)> { (expr_, false) }, null, null, null); task.PushRef(1.0); } catch { task.PushRef(0.0); }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.ISAM_CREATE_INDEX:
            {
                // Ordine di push del Compiler (VisitIsamCreateIndexStatement): FileNumber,
                // IndexName, KeyExpression, KeyLength — pop in ordine inverso. Prima si popavano
                // solo 3 valori (expr_, name_, area_) per 4 pushati: KeyLength restava sullo
                // stack (corrompendo l'istruzione successiva), e name_/area_ leggevano i valori
                // sbagliati (KeyExpression scambiato per il nome, IndexName scambiato per l'area).
                var keyLengthRaw_ = task.Sp > 0 ? task.PopRef() : null;
                var expr_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var name_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var fileRef_ = task.Sp > 0 ? task.PopRef() : null;
                var eng_ = ResolveIsamEngine(task, fileRef_);
                var keyLength_ = (int)SafeDouble(keyLengthRaw_);
                if (eng_ == null) { task.PushRef(null); break; }
                // Bug ISAM-RecallCurrent-Dopo-Delete-Offset-Invalido: se lo schema cresce oltre lo
                // slot corrente, EnsureSchemaSlot sposta fisicamente l'area dati ma nessun cursore
                // per-thread viene notificato — un accesso successivo senza rinavigare legge byte
                // a caso. Invalidare QUI, PRIMA del park, sul worker GPM chiamante (lo stesso che
                // ha eseguito eventuali GO/SKIP/SEEK precedenti): invalidare nel callback di
                // completamento di ParkAsync (dopo l'await) e' un no-op silenzioso, perche' quella
                // continuazione puo' riprendere su un thread .NET diverso (IsamThreadCursor e'
                // ThreadLocal, CreateIndexAsync usa Task.Run internamente) — misurato dal vivo:
                // il cursore del worker GPM restava intatto, DELETE continuava a leggere l'offset
                // stantio. Fail-safe pessimistico: invalidiamo ANCHE se l'operazione poi fallisce/
                // non sposta nulla, perche' non possiamo sapere in anticipo se lo spostamento
                // avverra' davvero (dipende dalla crescita effettiva dello schema).
                eng_.InvalidateCursor();
                ParkAsync(task, PCodeWaitKind.External,
                    async ct =>
                    {
                        try
                        {
                            await eng_.CreateIndexAsync(name_, expr_, keyLength_ > 0 ? keyLength_ : 50, false, null)
                                .WaitAsync(ct).ConfigureAwait(false);
                        }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested) { throw; }
                        catch { }
                        return true;
                    },
                    _ => task.PushRef(null));
                return;
            }
            case SysCall.ISAM_DROP_INDEX:
            {
                var name_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                if (eng_ == null) { task.PushRef(null); break; }
                // Vedi commento in ISAM_CREATE_INDEX sopra — stesso motivo, invalidare PRIMA del
                // park, sul worker GPM chiamante.
                eng_.InvalidateCursor();
                ParkAsync(task, PCodeWaitKind.External,
                    async ct =>
                    {
                        try { await eng_.DropIndexAsync(name_).WaitAsync(ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested) { throw; }
                        catch { }
                        return true;
                    },
                    _ => task.PushRef(null));
                return;
            }
            case SysCall.ISAM_SET_ORDER:
            {
                var name_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                /* L'area può essere un ALIAS (stringa) o -1/0 (area attiva): prima si faceva
                 * `GetWorkArea((int)PopNum)` → area ≤ 0 = null → SET ORDER silenziosamente no-op
                 * (l'ordine restava quello primario: §4 "Clienti per Nome" e SEEK USING errati). */
                var areaRef = task.Sp > 0 ? task.PopRef() : null;
                var wa_ = ResolveIsamArea(task, areaRef);
                if (wa_?.Driver != null) try { wa_.Driver.SetOrder(name_); } catch { }
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_SET_RELATION:
            {
                // Ordine di push dal compilatore (Compiler.Isam.cs::VisitIsamSetRelationStatement):
                // Arg1=FileNumber del padre, Arg2=nome campo chiave (stringa), Arg3=alias del
                // figlio (stringa) — quindi si pop-a in ordine INVERSO: figlio, chiave, padre.
                // (Il codice precedente leggeva questi tre valori nell'ordine sbagliato e
                // interpretava l'indice del code-block della chiave come un numero di area,
                // finendo per chiamare SetRelation sul driver PADRE invece che sul FIGLIO —
                // la relazione non veniva mai stabilita correttamente.)
                var childAlias_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var keyField_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var parentAreaNum_ = ResolveAreaNum(task, task.Sp > 0 ? task.PopRef() : null, task.CurrentIsamArea);
                var parentDriver_ = parentAreaNum_ > 0 ? _dataSession.GetWorkArea(parentAreaNum_)?.Driver : null;
                var childAreaNum_ = ResolveAreaNum(task, childAlias_, task.CurrentIsamArea);
                var childDriver_ = childAreaNum_ > 0 ? _dataSession.GetWorkArea(childAreaNum_)?.Driver : null;
                if (childDriver_ != null && parentDriver_ != null) try { childDriver_.SetRelation(parentDriver_, keyField_); } catch { }
                task.PushRef(null);
                break;
            }
            case SysCall.ISAM_JOIN_WITH:
            {
                var key2_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var key1_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var area2_ = (int)PopNum(task);
                var area_ = (int)PopNum(task);
                var eng_ = ResolveIsamEngine(task, area_);
                var eng2_ = area2_ > 0 ? (_dataSession.GetWorkArea(area2_)?.Driver as Basic.Databases.Rdd.IsamDriver)?._engine : null;
                if (eng_ != null && eng2_ != null) try { eng_.JoinWith(eng2_, key1_, null!, null!, null, key2_); } catch { }
                task.PushRef(null);
                break;
            }
            // ═══ DBF ═══
            case SysCall.CONVERT_DBF_TO_ISAM:
            case SysCall.DBF_CONVERT_TO_ISAM:
            {
                // (sourceDbf, destIsdb, typeName, pkField, keyLength, isUnsafe, [nome,tipo]*,
                // fieldCount) — pop LIFO: fieldCount e le coppie nome/tipo (in ordine inverso)
                // sono pushate DOPO isUnsafe, quindi vanno pop-ate PRIMA (vedi
                // Compiler.Isam.cs VisitConvertDbfToIsamStatement — per campo: nome pushato
                // prima, tipo dopo, quindi pop-ati tipo poi nome).
                var fieldCount = task.Sp > 0 ? (int)PopNum(task) : 0;
                var fieldOverrides = new List<(string Name, string TypeCode)>();
                for (var fi = 0; fi < fieldCount; fi++)
                {
                    var ftype = task.Sp > 0 ? task.PopRef()?.ToString() ?? "STRING" : "STRING";
                    var fname = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                    fieldOverrides.Add((fname, ftype));
                }
                fieldOverrides.Reverse(); // pushati in ordine di dichiarazione, pop-ati al contrario
                var isUnsafe = task.Sp > 0 && PopNum(task) != 0;
                var keyLength = (int)(task.Sp > 0 ? PopNum(task) : 16);
                var pkField = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var typeName = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var destIsdb = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                var sourceDbf = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";
                try
                {
                    // ImportDbf: conversione DBF→ISAM completa (schema translation + copia record).
                    // Usa l'evaluator standard del toolkit (gestisce UPPER/JSON_VALUE/espressioni).
                    // typeName (clausola TYPE <UDT>) va passato: senza, lo schema scritto nel
                    // file usava il nome del file destinazione come TypeName, mai compatibile
                    // col TYPE del codice BASIC al successivo ISAM OPEN ... OF <UDT> (vedi
                    // IlRuntime.IsamConvertDbf per i dettagli del bug, stesso identico gap qui).
                    // fieldOverrides: stesso motivo, per nome e tipo di ogni campo (Engine.
                    // TranslateDbfSchemaToIsam semplifica ogni DBF Numeric a DoubleType e il
                    // DBF tronca i nomi campo a 10 caratteri).
                    Basic.ISAM.IsamExportImport.ImportDbf(
                        sourceDbf, destIsdb,
                        Basic.ISAM.IsamKeyExpressionEvaluator.Chain(null), // GM: nessun equivalente diretto di GMO Basic.Tools.Isam.IsamToolKeyEvaluator; stesso ruolo (evaluator standard built-in UPPER/JSON_VALUE, nessun evaluator utente aggiuntivo)
                        string.IsNullOrEmpty(pkField) ? null : pkField,
                        keyLength > 0 ? keyLength : 16,
                        string.IsNullOrEmpty(typeName) ? null : typeName,
                        fieldOverrides.Count > 0 ? fieldOverrides : null);
                    task.PushRef(1.0);
                }
                catch
                {
                    task.PushRef(0.0);
                }
                break;
            }
            // ═══ Export/Import ═══
            case SysCall.EXPORT_TO_FILE:
            {
                // Compiler pusha 4: (EngineType, Area, FilePath, Format) → pop in ordine inverso.
                var format_ = task.Sp > 0 ? task.PopRef() : null;   // Format (TokenKind come double)
                var file_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";  // FilePath
                var area_ = (int)PopNum(task);                        // Area
                var eng_ = task.Sp > 0 ? task.PopRef() : null;        // EngineType
                // Stessa logica del backend IL (CoreDb.CopyTo per CSV, ExportAreaToJson per JSON,
                // area 0 = corrente): prima l'export era solo ISAM e ignorava il formato.
                var fmtName_ = string.Equals(format_?.ToString(), "JSON", StringComparison.OrdinalIgnoreCase) ? "JSON" : "CSV";
                var waE_ = area_ > 0 ? _dataSession.GetWorkArea(area_) : _dataSession.GetWorkArea(_dataSession.CurrentArea);
                if (waE_ != null)
                {
                    try { Basic.IlBackend.IlRuntime.ExportToFile("", (area_ > 0 ? area_ : _dataSession.CurrentArea).ToString(), file_, fmtName_); task.PushRef(1.0); }
                    catch { task.PushRef(0.0); }
                }
                else task.PushRef(0.0);
                break;
            }
            case SysCall.IMPORT_FROM_FILE:
            {
                // Compiler pusha 4: (EngineType, Area, FilePath, Format) → pop in ordine inverso.
                var formatRaw_ = task.Sp > 0 ? task.PopRef() : null; // Format (TokenKind come double)
                var format_ = string.Equals(formatRaw_?.ToString(), "JSON", StringComparison.OrdinalIgnoreCase) ? "JSON" : "CSV";
                var file_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : "";  // FilePath
                var area_ = (int)PopNum(task);                         // Area
                if (area_ <= 0) area_ = _dataSession.CurrentArea;      // 0 = area implicita (corrente)
                var engineType_ = task.Sp > 0 ? task.PopRef()?.ToString() ?? "" : ""; // EngineType
                var wa_ = area_ > 0 ? _dataSession.GetWorkArea(area_) : null;
                if (wa_?.Driver is Basic.Databases.Rdd.SqlDriver sd_)
                {
                    if (File.Exists(file_)) try { sd_.ExecRaw("INSERT INTO " + (wa_.Alias ?? "t") + " SELECT * FROM read_csv_auto('" + file_.Replace("'", "''") + "')", null, null); task.PushRef(1.0); } catch { task.PushRef(0.0); }
                    else task.PushRef(0.0);
                }
                else if (wa_?.Driver is not null)
                {
                    // Delega al backend IL (CoreDb.CopyFrom per CSV, ImportAreaFromJson per JSON)
                    // — stessa logica di `IMPORT` nel backend IL, evita duplicazione.
                    try { Basic.IlBackend.IlRuntime.ImportFromFile(engineType_, wa_!.Alias ?? area_.ToString(), file_, format_); task.PushRef(1.0); }
                    catch { task.PushRef(0.0); }
                }
                else task.PushRef(0.0);
                break;
            }

            default:
                for (var i = 0; i < argCount; i++)
                    if (task.Sp > 0) task.Sp--;
                break;
        }
    }

    // ================================================================
    // BUILT-IN FUNCTION DISPATCH
    // ================================================================


    private static void XCopyDirRec(string src, string dest)
    {
        try
        {
            if (!Directory.Exists(dest)) Directory.CreateDirectory(dest);
            foreach (var f in Directory.GetFiles(src)) File.Copy(f, Path.Combine(dest, Path.GetFileName(f)), true);
            foreach (var d in Directory.GetDirectories(src))
                XCopyDirRec(d, Path.Combine(dest, Path.GetFileName(d)));
        }
        catch { }
    }

    // ── Scope RDD/DBF (FOR/WHILE/NEXT/ALL/RECORD/REST) — helper condivisi ──
    // Il compilatore (CompileScopeClause) spinge [hasScope, DbfScopeType, arg1, arg2]
    // con arg2 in cima: For=0, While=1, Next=2, All=3, Record=4, Rest=5.
    private (Basic.Databases.Rdd.ScopeInfoData? Scope, object? CondArg) PopScopeClause(PCodeTask task)
    {
        var arg2 = task.Sp > 0 ? task.PopRef() : null;   // condizione (For/While): stringa TESTO o double codeblock-ip
        var arg1 = task.Sp > 0 ? task.PopRef() : null;   // count (Next/Record) o NIL
        var scopeInt = (int)PopNum(task);
        var hasScope = PopNum(task) != 0;
        if (!hasScope) return (Basic.Databases.Rdd.ScopeInfoData.Current, null);
        return scopeInt switch
        {
            3 => (Basic.Databases.Rdd.ScopeInfoData.All, null),                       // All
            2 => (Basic.Databases.Rdd.ScopeInfoData.NextN(arg1 is double dN ? (long)dN : 1), null), // Next
            4 => (Basic.Databases.Rdd.ScopeInfoData.RecordN(arg1 is double dR ? (long)dR : 0), null), // Record
            5 => (Basic.Databases.Rdd.ScopeInfoData.Rest, null),                      // Rest
            0 => (Basic.Databases.Rdd.ScopeInfoData.All, arg2),                       // For
            1 => (Basic.Databases.Rdd.ScopeInfoData.Rest, arg2),                      // While (dal corrente)
            _ => (Basic.Databases.Rdd.ScopeInfoData.Current, null)
        };
    }

    private System.Collections.Generic.Dictionary<string, object?>? DriverRecordToNameDict(Basic.Databases.Rdd.IDatabaseDriver driver)
    {
        var rec = driver.GetCurrentRecord();
        if (rec == null) return null;
        var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in rec)
        {
            var fi = driver.Fields.FirstOrDefault(f => f.SymbolId == kv.Key);
            dict[fi?.Name ?? kv.Key.ToString()] = kv.Value;
        }
        return dict;
    }

    private long ApplyScopedWithCondition(PCodeTask task, int area, Basic.Databases.Rdd.ScopeInfoData scope,
        object? condArg, Func<Basic.Databases.Rdd.IDatabaseDriver, bool> action)
    {
        // TESTO (ExprPlain.From — condizione FOR/WHILE su campi): valuta per-record via
        // RuntimeExpressionEvaluator dentro CoreDb.ScopedOperation (parità SET FILTER;
        // i nomi campo nudi funzionano; il loop CoreDb termina su file). Scope For → type
        // All (da top), While → type Rest (dal corrente, break al primo false).
        if (condArg is string condText && !string.IsNullOrWhiteSpace(condText))
            return Basic.Databases.Rdd.CoreDb.ScopedOperation(_dataSession, area, scope, action,
                scope.Type == Basic.Databases.Rdd.ScopeType.Rest ? null : condText,
                scope.Type == Basic.Databases.Rdd.ScopeType.Rest ? condText : null);
        if (condArg is not double condIp)
            return Basic.Databases.Rdd.CoreDb.ScopedOperation(_dataSession, area, scope, action);
        // Legacy codeblock-ip (macro & in scope): valuta il codeblock per-record contro il
        // dict del record corrente.
        var driver = _dataSession.GetWorkArea(area)?.Driver;
        if (driver == null || !driver.IsOpen) return 0;
        driver.GoTop();
        long affected = 0;
        while (!driver.Eof)
        {
            var dict = DriverRecordToNameDict(driver);
            var result = dict != null ? CallCodeBlockSync(task, (int)condIp, dict) : null;
            if (SafeDouble(result) != 0.0) { if (action(driver)) affected++; }
            else if (scope.Type == Basic.Databases.Rdd.ScopeType.Rest) break; // WHILE: stop al primo false
            driver.Skip(1);
        }
        return affected;
    }

    private long ApplyScopedReplaceWithCondition(PCodeTask task, int area, Basic.Databases.Rdd.ScopeInfoData scope,
        object? condArg, Dictionary<string, object?> fields)
    {
        if (condArg is string condText && !string.IsNullOrWhiteSpace(condText))
        {
            // Merge per-record del record CORRENTE completo + campi target → PutCurrentRecord(full)
            // (mai PutCurrentRecord parziale — corruzione) dentro il loop CoreDb (termina su file).
            return Basic.Databases.Rdd.CoreDb.ScopedOperation(_dataSession, area, scope, d =>
            {
                var rec = d.GetCurrentRecord() ?? new Dictionary<int, object?>();
                foreach (var kv in fields)
                {
                    var idx = d.GetFieldIndex(kv.Key);
                    if (idx >= 0) rec[d.Fields[idx].SymbolId] = kv.Value;
                }
                d.PutCurrentRecord(rec);
                return true;
            }, scope.Type == Basic.Databases.Rdd.ScopeType.Rest ? null : condText,
                scope.Type == Basic.Databases.Rdd.ScopeType.Rest ? condText : null);
        }
        if (condArg is not double condIp)
            return Basic.Databases.Rdd.CoreDb.ScopedReplace(_dataSession, area, scope, fields);
        var driver = _dataSession.GetWorkArea(area)?.Driver;
        if (driver == null || !driver.IsOpen) return 0;
        driver.GoTop();
        long affected = 0;
        while (!driver.Eof)
        {
            var dict = DriverRecordToNameDict(driver);
            var result = dict != null ? CallCodeBlockSync(task, (int)condIp, dict) : null;
            if (SafeDouble(result) != 0.0)
            {
                var rec = driver.GetCurrentRecord() ?? new Dictionary<int, object?>();
                foreach (var kv in fields)
                {
                    var idx = driver.GetFieldIndex(kv.Key);
                    if (idx >= 0) rec[driver.Fields[idx].SymbolId] = kv.Value;
                }
                driver.PutCurrentRecord(rec);
                affected++;
            }
            else if (scope.Type == Basic.Databases.Rdd.ScopeType.Rest) break;
            driver.Skip(1);
        }
        return affected;
    }

    /// <summary>Dispatch della callback IO WATCH come task GPM (parità VM/C, §0ter: la SUB
    /// non si invoca MAI sul thread del FileSystemWatcher — si accoda al scheduler).</summary>
    private void DispatchIoWatchCallback(string path)
    {
        // Il FileSystemWatcher può consegnare un evento già accodato dopo la fine del run:
        // lo scheduler è stato Dispose()d e CreateTask/Reset lancerebbe ObjectDisposedException
        // sul thread del watcher (crash del processo host). Best-effort: un evento tardivo
        // non ha più un task a cui appartenere.
        try
        {
            if (_fileWatchCallbacks == null || !_fileWatchCallbacks.TryGetValue(path, out var cb)
                || string.IsNullOrEmpty(cb) || _scheduler is null) return;
            if (!_procedureAddresses.TryGetValue(cb, out var addr)) return;
            var cbTask = _scheduler.CreateTask(addr);
            cbTask.Scheduler = _scheduler;
            cbTask.Globals = _vm?.MainTask.Globals;
            _scheduler.Schedule(cbTask);
        }
        catch (ObjectDisposedException) { /* run terminato: evento tardivo ignorato */ }
        catch (InvalidOperationException) { /* watcher/scheduler in chiusura */ }
    }

    /// <summary>Ferma e rilascia i watcher I/O prima del Dispose dello scheduler: un evento
    /// in volo non deve più accodare task su uno scheduler già chiuso.</summary>
    internal void DisposeIoWatchers()
    {
        if (_fileWatchers != null)
        {
            foreach (var w in _fileWatchers.Values)
            {
                try { w.EnableRaisingEvents = false; } catch { }
                try { w.Dispose(); } catch { }
            }
            _fileWatchers.Clear();
        }
        _fileWatchCallbacks?.Clear();
    }

    /// <summary>Converte l'argomento data di LEDGER ADD/EDIT in DateOnly: DateTime/
    /// DateOnly, oppure SERIALE OADate (le variabili DATE del BASIC viaggiano come
    /// double) oppure stringa "yyyy-MM-dd". Prima un seriale cadeva su
    /// DateOnly.MinValue → transazione con data 0001-01-01 solo sul PCode.</summary>
    private static DateOnly LedgerDateOnly(object? value)
    {
        switch (value)
        {
            case DateTime dt: return DateOnly.FromDateTime(dt);
            case DateOnly dto: return dto;
            case double d: return DateOnly.FromDateTime(DateTime.FromOADate(d));
            case int i: return DateOnly.FromDateTime(DateTime.FromOADate(i));
            case long l: return DateOnly.FromDateTime(DateTime.FromOADate(l));
        }
        var text = value?.ToString();
        if (DateOnly.TryParse(text, System.Globalization.CultureInfo.InvariantCulture,
                              System.Globalization.DateTimeStyles.None, out var parsed))
            return parsed;
        if (double.TryParse(text, System.Globalization.NumberStyles.Any,
                            System.Globalization.CultureInfo.InvariantCulture, out var serial)
            && serial > 0 && serial < 2958466)
            return DateOnly.FromDateTime(DateTime.FromOADate(serial));
        return DateOnly.MinValue;
    }

}
