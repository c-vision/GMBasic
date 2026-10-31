// FILE: CommonLib.App.EventDispatcher.cs (C-backed — ritiro ClipperLib)
// Lo stato di registrazione key/window/mouse vive nel runtime C
// (gmb_event_dispatcher_t, libgmb_gfx): il C# registra le sue action come HOOK
// opzionali null-safe (fn pointer + GCHandle come ctx) e il C fa il dispatch.
// Le parti non presenti nel dispatcher C (sprite collision, eventi controllo)
// restano gestite in C# (additive). Fallback managed se la dylib non è caricabile.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using CommonLib.Keyboard;

namespace CommonLib.App;

/// <summary>
/// Dispatcher eventi unificato (ON KEY / ON MOUSE / ON WINDOW / CTRL+C).
/// Store key/window/mouse nel C (<c>gmb_event_dispatcher_t</c>); il C# registra i suoi
/// callback e il C li invoca (hook opzionali, null-safe). Additivi in C#: collisioni
/// sprite e eventi controllo.
/// </summary>
public sealed class EventDispatcher
{
    public static EventDispatcher Instance { get; } = new();

    private const string Lib = "gmb_gfx";

    // ── P/Invoke verso il dispatcher C (libgmb_gfx) ──
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr gmb_event_dispatcher_new();
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_register_key(IntPtr d, int combo, IntPtr fn, IntPtr ctx);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_register_window(IntPtr d, int kind, IntPtr fn, IntPtr ctx);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_register_mouse(IntPtr d, int kind, IntPtr fn, IntPtr ctx);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_event_dispatcher_dispatch_key(IntPtr d, int combo);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_dispatch_window(IntPtr d, int kind);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_dispatch_mouse(IntPtr d, int kind, int x, int y);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_event_dispatcher_has_key(IntPtr d, int combo);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_unregister_key(IntPtr d, int combo);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_unregister_window(IntPtr d, int kind);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_event_dispatcher_unregister_mouse(IntPtr d, int kind);

    /* HANDLE PIGRO: `gmb_event_dispatcher_new()` è una P/Invoke su libgmb_gfx e il tipo viene
     * toccato all'avvio: creandolo nel ctor statico si caricava `libgmb_gfx` (+SDL, ~8 MB) anche
     * per un programma che stampa una riga. Ora nasce alla PRIMA operazione di evento. */
    private static IntPtr _d;
    private static bool _dInit;

    private static IntPtr D()
    {
        if (!_dInit)
        {
            _dInit = true;
            try { _d = gmb_event_dispatcher_new(); }
            catch { _d = IntPtr.Zero; }
        }
        return _d;
    }

    // ── Trampolini (una volta sola): il C li invoca col ctx (GCHandle dell'action) ──
    private delegate void CtxCb(IntPtr ctx);
    private delegate void CtxXYCb(IntPtr ctx, int x, int y);

    private static readonly CtxCb _keyTramp = KeyTramp;
    private static readonly CtxCb _winTramp = WinTramp;
    private static readonly CtxXYCb _mouseTramp = MouseTramp;
    private static readonly IntPtr _keyTrampPtr = Marshal.GetFunctionPointerForDelegate(_keyTramp);
    private static readonly IntPtr _winTrampPtr = Marshal.GetFunctionPointerForDelegate(_winTramp);
    private static readonly IntPtr _mouseTrampPtr = Marshal.GetFunctionPointerForDelegate(_mouseTramp);

    private static void KeyTramp(IntPtr ctx) { try { ((Action)GCHandle.FromIntPtr(ctx).Target!)(); } catch { } }
    private static void WinTramp(IntPtr ctx) { try { ((Action)GCHandle.FromIntPtr(ctx).Target!)(); } catch { } }
    private static void MouseTramp(IntPtr ctx, int x, int y) { try { ((Action<int, int>)GCHandle.FromIntPtr(ctx).Target!)(x, y); } catch { } }

    private readonly object _lock = new();
    private readonly List<(int Combo, GCHandle H)> _keyHandles = new();
    private readonly List<(int Kind, GCHandle H)> _winHandles = new();
    private readonly List<(int Kind, GCHandle H)> _mouseHandles = new();

    // Fallback managed (solo se la dylib non è caricabile → dispatcher C assente).
    private readonly List<(KeyCombination Combo, Action Action)> _keyFb = new();
    private readonly List<(int Kind, Action Action)> _winFb = new();
    private readonly List<(int Kind, Action<int, int> Action)> _mouseFb = new();

    // Additivi (non nel dispatcher C).
    private readonly List<(bool Plural, Action Handler)> _spriteCollisionHandlers = new();
    private readonly List<(int Control, int Ev, Action Action)> _controlHandlers = new();

    private static int Encode(KeyCombination c) => (int)c.Key | ((int)c.Modifiers << 16);

    // ── Registrazione ──
    public void RegisterKey(KeyCombination combo, Action action)
    {
        if (action is null) return;
        int comboInt = Encode(combo);
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _keyFb.Add((combo, action)); return; }
            var h = GCHandle.Alloc(action);
            _keyHandles.Add((comboInt, h));
            gmb_event_dispatcher_register_key(D(), comboInt, _keyTrampPtr, GCHandle.ToIntPtr(h));
        }
    }

    public void UnregisterKey(KeyCombination combo)
    {
        int comboInt = Encode(combo);
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _keyFb.RemoveAll(h => h.Combo == combo); return; }
            for (int i = _keyHandles.Count - 1; i >= 0; i--)
                if (_keyHandles[i].Combo == comboInt) { _keyHandles[i].H.Free(); _keyHandles.RemoveAt(i); }
            gmb_event_dispatcher_unregister_key(D(), comboInt);
        }
    }

    /// <summary>True se ON KEY (o equivalente) ha un handler per questa combinazione.</summary>
    public bool HasKeyHandler(KeyCombination combo)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero) return _keyFb.Exists(h => h.Combo == combo);
            return gmb_event_dispatcher_has_key(D(), Encode(combo)) != 0;
        }
    }

    public void RegisterWindow(int kind, Action action)
    {
        if (action is null) return;
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _winFb.Add((kind, action)); return; }
            var h = GCHandle.Alloc(action);
            _winHandles.Add((kind, h));
            gmb_event_dispatcher_register_window(D(), kind, _winTrampPtr, GCHandle.ToIntPtr(h));
        }
    }

    public void UnregisterWindow(int kind)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _winFb.RemoveAll(h => h.Kind == kind); return; }
            for (int i = _winHandles.Count - 1; i >= 0; i--)
                if (_winHandles[i].Kind == kind) { _winHandles[i].H.Free(); _winHandles.RemoveAt(i); }
            gmb_event_dispatcher_unregister_window(D(), kind);
        }
    }

    public void RegisterMouse(int kind, Action<int, int> action)
    {
        if (action is null) return;
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _mouseFb.Add((kind, action)); return; }
            var h = GCHandle.Alloc(action);
            _mouseHandles.Add((kind, h));
            gmb_event_dispatcher_register_mouse(D(), kind, _mouseTrampPtr, GCHandle.ToIntPtr(h));
        }
    }

    public void UnregisterMouse(int kind)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero) { _mouseFb.RemoveAll(h => h.Kind == kind); return; }
            for (int i = _mouseHandles.Count - 1; i >= 0; i--)
                if (_mouseHandles[i].Kind == kind) { _mouseHandles[i].H.Free(); _mouseHandles.RemoveAt(i); }
            gmb_event_dispatcher_unregister_mouse(D(), kind);
        }
    }

    // ── Additivi C# ──
    public void RegisterSpriteCollision(bool plural, Action handler)
    {
        lock (_lock) _spriteCollisionHandlers.Add((plural, handler));
    }

    public void UnregisterSpriteCollision(bool plural)
    {
        lock (_lock) _spriteCollisionHandlers.RemoveAll(h => h.Plural == plural);
    }

    public void RegisterControlEvent(int control, int ev, Action action) { lock (_lock) _controlHandlers.Add((control, ev, action)); }
    public void UnregisterControlEvent(int control, int ev) { lock (_lock) _controlHandlers.RemoveAll(h => h.Control == control && h.Ev == ev); }
    public void DispatchControlEvent(int control, int ev)
    {
        List<Action> actions;
        lock (_lock) actions = _controlHandlers.Where(h => h.Control == control && h.Ev == ev).Select(h => h.Action).ToList();
        foreach (var a in actions) { try { a(); } catch { } }
    }

    // ── Dispatch ──
    /// <summary>Inoltra un evento tastiera. Ritorna true se l'evento è stato consumato.</summary>
    public bool DispatchKey(KeyCombination combo)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero)
            {
                var acts = _keyFb.Where(h => h.Combo == combo).Select(h => h.Action).ToList();
                foreach (var a in acts) { try { a(); } catch { } }
                return acts.Count > 0;
            }
            return gmb_event_dispatcher_dispatch_key(D(), Encode(combo)) != 0;
        }
    }

    public void DispatchWindow(int kind)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero)
            {
                var acts = _winFb.Where(h => h.Kind == kind).Select(h => h.Action).ToList();
                foreach (var a in acts) { try { a(); } catch { } }
                return;
            }
            gmb_event_dispatcher_dispatch_window(D(), kind);
        }
    }

    public void DispatchMouse(int kind, int x, int y)
    {
        lock (_lock)
        {
            if (_d == IntPtr.Zero)
            {
                var acts = _mouseFb.Where(h => h.Kind == kind).Select(h => h.Action).ToList();
                foreach (var a in acts) { try { a(x, y); } catch { } }
                return;
            }
            gmb_event_dispatcher_dispatch_mouse(D(), kind, x, y);
        }
    }

    /// <summary>Inoltra una collisione sprite. plural=true → handler "COLLISIONS"
    /// (invocato ad ogni tick con l'array aggiornato); false → handler singolo.</summary>
    public void DispatchSpriteCollision(bool plural)
    {
        List<Action> actions;
        lock (_lock)
        {
            actions = _spriteCollisionHandlers.Where(h => h.Plural == plural).Select(h => h.Handler).ToList();
        }
        foreach (var a in actions) { try { a(); } catch { } }
    }
}
