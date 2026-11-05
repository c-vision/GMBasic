// FILE: PCode/PCodeProcessor.Pixel.cs
// P-CODE VM: controlli pixel + CONTROL + FORM_* (bridge grafico condiviso, come IL).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.IlBackend;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private void PixelCtrl(PCodeTask task)
    {
        var listRef = task.PopRef();
        var spec = PopS(task);
        var name = PopS(task);
        var kind = PopS(task);
        var args = new List<object>();
        if (listRef is System.Collections.IEnumerable en) foreach (var o in en) args.Add(o);
        try
        {
            IlRuntime.FormAddControl(kind, args.ToArray(), name);
            if (spec.Length > 0) IlRuntime.FormSetLastStyle(spec);
        }
        catch { }
    }

    private void FormLastHandle(PCodeTask task) { task.PushRef(IlRuntime.FormLastHandle()); }
    private void FormClicked(PCodeTask task) { int h = PopInt(task); task.PushRef(IlRuntime.FormClicked(h)); }
    private void FormDblClicked(PCodeTask task) { int h = PopInt(task); task.PushRef(IlRuntime.FormDblClicked(h)); }
    private void FormChanged(PCodeTask task) { int h = PopInt(task); task.PushRef(IlRuntime.FormChanged(h)); }
    private void FormGotFocused(PCodeTask task) { int h = PopInt(task); task.PushRef(IlRuntime.FormGotFocused(h)); }
    private void FormLostFocused(PCodeTask task) { int h = PopInt(task); task.PushRef(IlRuntime.FormLostFocused(h)); }
    private void RunControlHandlerSync(int targetAddr, object?[]? globals)
    {
        var sched = _scheduler;
        if (sched == null) return;
        var t = sched.CreateTask(targetAddr);
        t.Scheduler = sched; t.Globals = globals;
        try { while (t.State != Basic.Runtime.Gpm.PCodeTaskState.Finished && t.PendingException == null) ExecuteTaskTick(t); } catch { }
    }
    private readonly System.Collections.Generic.Dictionary<(int, int), int> _controlHandlerAddr = new();
    /* §11.7-3: metodi-evento UserControl → (symbolId, istanza): invocati con SELF=istanza (frame PCode). */
    private readonly System.Collections.Generic.Dictionary<(int, int), (int symId, object? inst)> _ucEventMap = new();
    private void FormFire(PCodeTask task)
    {
        var ev = PopString(task); int h = PopInt(task);
        string e = ev ?? "";
        int evIndex = IlRuntime.FormEventIndex(e);
        IlRuntime.FormSetLastEvent(h, e);   /* stato, senza firing */
        if (_controlHandlerAddr.TryGetValue((h, evIndex), out var addr) && addr >= 0)
        {
            /* Esecuzione SINCRONA: GOSUB nel task corrente (ritorna dopo il FIRE EVENT). */
            task.GosubContinuations.Push(new PCodeGosubContinuation(task.Ip, task.Sp, task.TryStack.Count, task.CleanupStack.Count, task.Frame));
            task.Ip = addr;
            return;
        }
        /* §11.7-3: metodo-evento UserControl → frame PCode con SELF=istanza (GPM: task corrente, run-to-completion). */
        if (_ucEventMap.TryGetValue((h, evIndex), out var ucev) &&
            _procedureAddresses.TryGetValue("@sym" + ucev.symId, out var ucmAddr))
        {
            var uclocals = CreateLocals();
            uclocals[0] = ucev.inst;
            var ucframe = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = uclocals };
            EnterProcedureErrorScope(task, ucframe);
            task.Frame = ucframe;
            task.Ip = ucmAddr;
            return;
        }
        IlRuntime.FormFireEvent(h, e);   /* fallback: handler C# (ControlEventHandlers2) */
    }
    private void FormSetEventId(PCodeTask task)
    {
        var targetRaw = task.Sp > 0 ? task.PopRef() : null;
        var actionType = PopInt(task);
        var evIndex = PopInt(task);
        var handle = PopInt(task);
        var ed = CommonLib.App.EventDispatcher.Instance;
        if (actionType == 0) { _controlHandlerAddr[(handle, evIndex)] = -1; ed.UnregisterControlEvent(handle, evIndex); IlRuntime.UnregisterControlThunk(handle, evIndex); }
        else if (targetRaw != null)
        {
            var targetAddr = System.Convert.ToInt32(targetRaw);
            _controlHandlerAddr[(handle, evIndex)] = targetAddr;
            var globals = task.Globals;
            ed.RegisterControlEvent(handle, evIndex, () => FireWindowEvent(targetAddr, globals));
            IlRuntime.RegisterControlThunk(handle, evIndex, ed.DispatchControlEvent);
        }
    }
    private void MouseClicked(PCodeTask task) { task.PushRef(IlRuntime.MouseClicked()); }
    private void MouseDblClicked(PCodeTask task) { task.PushRef(IlRuntime.MouseDblClicked()); }
    private void MousePressed(PCodeTask task) { task.PushRef(IlRuntime.MousePressed()); }
    private void KeyPressed(PCodeTask task) { task.PushRef(IlRuntime.KeyPressed()); }
    private void FormFrame(PCodeTask task) { var ms = PopI(task); IlRuntime.UiFrame(ms); }
    private void FormCtrlGet(PCodeTask task) { var prop = PopS(task); var name = PopS(task); task.PushRef(IlRuntime.FormControlGet(name, prop)); }
    private void FormCtrlGetS(PCodeTask task) { var prop = PopS(task); var name = PopS(task); task.PushRef(IlRuntime.FormControlGetStr(name, prop)); }
    private void FormCtrlSet(PCodeTask task) { var str = PopS(task); var num = PopI(task); var prop = PopS(task); var name = PopS(task); IlRuntime.FormControlSet(name, prop, num, str); }
    private void FormCtrlMove(PCodeTask task) { var h = PopI(task); var w = PopI(task); var y = PopI(task); var x = PopI(task); var name = PopS(task); IlRuntime.FormControlMove(name, x, y, w, h); }
    private void FormCtrlFocus(PCodeTask task) { var name = PopS(task); IlRuntime.FormControlFocus(name); }
    private void FormCtrlZorder(PCodeTask task) { var front = PopI(task); var name = PopS(task); IlRuntime.FormControlZorder(name, front); }
    private void FormCtrlInvalidate(PCodeTask task) { var name = PopS(task); IlRuntime.FormControlInvalidate(name); }
    private void FormCtrlRefresh(PCodeTask task) { var name = PopS(task); IlRuntime.FormControlRefresh(name); }
    private void FormCtrlBindEvent(PCodeTask task) { var inst = task.PopRef(); var symId = PopI(task); PopS(task); var evIndex = PopI(task); var handle = PopI(task); _ucEventMap[(handle, evIndex)] = (symId, inst); }
    private void FormCtrlAdd(PCodeTask task) { var name = PopS(task); var container = PopS(task); var h = PopI(task); var w = PopI(task); var y = PopI(task); var x = PopI(task); var inst = task.PopRef(); IlRuntime.FormControlAdd(inst, x, y, w, h, container, name); }
    private void FormCtrlRemove(PCodeTask task) { var inst = task.PopRef(); IlRuntime.FormControlRemove(inst); }
    private void FormCtrlDraw(PCodeTask task) { var s = PopS(task); var d = task.PopNum(); var c = task.PopNum(); var b = task.PopNum(); var a = task.PopNum(); var op = task.PopNum(); var h = task.PopNum(); IlRuntime.FormControlDraw(h, op, a, b, c, d, s); }
    private void ChartConfigSet(PCodeTask task) { var str = PopS(task); var num = PopNum(task); var prop = PopS(task); var h = PopI(task); IlRuntime.ChartConfigSet(h, prop, num, str); }
    private void ChartConfigGet(PCodeTask task) { var prop = PopS(task); var h = PopI(task); task.PushRef(IlRuntime.ChartConfigGet(h, prop)); }
    private void ChartConfigGetS(PCodeTask task) { var prop = PopS(task); var h = PopI(task); task.PushRef(IlRuntime.ChartConfigGetStr(h, prop)); }
    private void ChartDraw(PCodeTask task) { var arr = task.PopRef(); var op = PopS(task); var h = PopI(task); task.PushRef(IlRuntime.ChartDraw(h, op, ToDoubleArray(arr))); }
    // CANVAS <op> a0..a3 path$ — ordine di push: op, a0, a1, a2, a3, path.
    private void CanvasOp(PCodeTask task)
    {
        var path = PopS(task);
        var a3 = PopNum(task); var a2 = PopNum(task); var a1 = PopNum(task); var a0 = PopNum(task);
        var op = PopS(task);
        task.PushRef(IlRuntime.CanvasOp(op, a0, a1, a2, a3, path));
    }
}
