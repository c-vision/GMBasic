// AI/Managed/ManagedMacAx.cs — UI (L1) AX GESTITO (C# puro) su macOS per IL/PCodeVM.
// Cerca nell'albero AX dell'app frontmost per titolo/descrizione/valore, PRESS con
// fallback click al centro (ManagedMacInput), TYPE focus+unicode. Nessun runtime C.
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Basic.AI.Managed;

public static class ManagedMacAx
{
    private const int kAXErrorSuccess = 0;

    [StructLayout(LayoutKind.Sequential)] private struct CGPoint { public double X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct CGSize { public double W, H; }

    static ManagedMacAx() { CfInterop.Ensure(); }

    [DllImport("ApplicationServices", EntryPoint = "AXIsProcessTrusted")]
    private static extern byte AXIsProcessTrusted();
    [DllImport("ApplicationServices", EntryPoint = "AXUIElementCreateSystemWide")]
    private static extern IntPtr AXUIElementCreateSystemWide();
    [DllImport("ApplicationServices", EntryPoint = "AXUIElementCopyAttributeValue")]
    private static extern int AXUIElementCopyAttributeValue(IntPtr el, IntPtr attr, out IntPtr value);
    [DllImport("ApplicationServices", EntryPoint = "AXUIElementPerformAction")]
    private static extern int AXUIElementPerformAction(IntPtr el, IntPtr action);
    [DllImport("ApplicationServices", EntryPoint = "AXUIElementSetAttributeValue")]
    private static extern int AXUIElementSetAttributeValue(IntPtr el, IntPtr attr, IntPtr value);
    [DllImport("ApplicationServices", EntryPoint = "AXValueGetValue")]
    private static extern byte AXValueGetValue(IntPtr axv, int type, out CGPoint p);
    [DllImport("ApplicationServices", EntryPoint = "AXValueGetValue")]
    private static extern byte AXValueGetValueSize(IntPtr axv, int type, out CGSize s);
    private const int kAXValueCGPointType = 1;
    private const int kAXValueCGSizeType = 2;

    private static IntPtr Attr(string name) => CfInterop.CStr(name);

    public static bool Available() => OperatingSystem.IsMacOS() && AXIsProcessTrusted() != 0;

    private static IntPtr FrontApp()
    {
        var sys = AXUIElementCreateSystemWide();
        if (sys == IntPtr.Zero) return IntPtr.Zero;
        var key = Attr("AXFocusedApplication");
        int rc = AXUIElementCopyAttributeValue(sys, key, out var app);
        CfInterop.CFRelease(key);
        CfInterop.CFRelease(sys);
        return rc == kAXErrorSuccess ? app : IntPtr.Zero;
    }

    private static string? ReadStrAttr(IntPtr el, IntPtr key)
    {
        if (AXUIElementCopyAttributeValue(el, key, out var v) != kAXErrorSuccess) return null;
        var s = CfInterop.GetCString(v);
        if (v != IntPtr.Zero) CfInterop.CFRelease(v);
        return s;
    }

    private static bool IsTextField(string? role) =>
        role is not null && (role.Contains("TextArea") || role.Contains("TextField") ||
                             role.Contains("ComboBox") || role.Contains("SearchField") ||
                             role.Contains("SecureTextField"));

    private static IntPtr Find(IntPtr el, bool wantField, ref string role, ref string title,
        ref int budget, int depth)
    {
        if (depth > 40 || budget <= 0 || el == IntPtr.Zero) return IntPtr.Zero;
        budget--;
        var target = _lastNeedle ?? "";
        var rkey = Attr("AXRole"); var roleNow = ReadStrAttr(el, rkey); CfInterop.CFRelease(rkey);
        bool hit = false;
        if (!wantField || IsTextField(roleNow))
        {
            foreach (var a in new[] { "AXTitle", "AXDescription", wantField ? "AXPlaceholderValue" : "AXValue" })
            {
                var k = Attr(a); var v = ReadStrAttr(el, k); CfInterop.CFRelease(k);
                if (v is not null && target.Length > 0 &&
                    v.IndexOf(target, StringComparison.OrdinalIgnoreCase) >= 0) { hit = true; break; }
            }
        }
        if (hit)
        {
            role = roleNow ?? "?";
            var tkey = Attr("AXTitle"); title = ReadStrAttr(el, tkey) ?? ""; CfInterop.CFRelease(tkey);
            return CfInterop.CFRetain(el);
        }
        var ckey = Attr("AXChildren");
        IntPtr found = IntPtr.Zero;
        if (AXUIElementCopyAttributeValue(el, ckey, out var children) == kAXErrorSuccess && children != IntPtr.Zero)
        {
            var n = CfInterop.CFArrayGetCount(children);
            for (nint i = 0; i < n && found == IntPtr.Zero; i++)
                found = Find(CfInterop.CFArrayGetValueAtIndex(children, i), wantField, ref role, ref title, ref budget, depth + 1);
            CfInterop.CFRelease(children);
        }
        CfInterop.CFRelease(ckey);
        // i menu dell'app vivono sotto AXMenuBar (non nei children) — provali per UI.CLICK su voci di menu
        if (found == IntPtr.Zero)
        {
            var mkey = Attr("AXMenuBar");
            if (AXUIElementCopyAttributeValue(el, mkey, out var mb) == kAXErrorSuccess && mb != IntPtr.Zero)
            {
                found = Find(mb, wantField, ref role, ref title, ref budget, depth + 1);
                CfInterop.CFRelease(mb);
            }
            CfInterop.CFRelease(mkey);
        }
        return found;
    }

    private static string? _lastNeedle;

    private static bool PressOrCenterClick(IntPtr el)
    {
        var press = Attr("AXPress");
        int rc = AXUIElementPerformAction(el, press);
        CfInterop.CFRelease(press);
        if (rc == kAXErrorSuccess) return true;

        double x = -1, y = -1, w = 0, h = 0;
        var pkey = Attr("AXPosition"); var vp = IntPtr.Zero;
        if (AXUIElementCopyAttributeValue(el, pkey, out vp) == kAXErrorSuccess && vp != IntPtr.Zero &&
            AXValueGetValue(vp, kAXValueCGPointType, out var pt) != 0) { x = pt.X; y = pt.Y; }
        if (vp != IntPtr.Zero) CfInterop.CFRelease(vp); CfInterop.CFRelease(pkey);
        var skey = Attr("AXSize"); var vs = IntPtr.Zero;
        if (AXUIElementCopyAttributeValue(el, skey, out vs) == kAXErrorSuccess && vs != IntPtr.Zero &&
            AXValueGetValueSize(vs, kAXValueCGSizeType, out var sz) != 0) { w = sz.W; h = sz.H; }
        if (vs != IntPtr.Zero) CfInterop.CFRelease(vs); CfInterop.CFRelease(skey);
        return x >= 0 && y >= 0 && ManagedMacInput.Click(x + w / 2, y + h / 2) == 0;
    }

    public static string FindJson(string target)
    {
        _lastNeedle = target ?? "";
        if (!Available()) return "{\"found\":false,\"error\":\"AX non disponibile (Accessibilità)\"}";
        var app = FrontApp();
        if (app == IntPtr.Zero) return "{\"found\":false}";
        var needle = CfInterop.CStr(target ?? "");
        var budget = 4000;
        string role = "?", title = "";
        var el = Find(app, false, ref role, ref title, ref budget, 0);
        CfInterop.CFRelease(needle); CfInterop.CFRelease(app);
        if (el == IntPtr.Zero) return "{\"found\":false}";
        var json = JsonSerializer.Serialize(new { found = true, target, role, title, level = "AX" });
        CfInterop.CFRelease(el);
        return json;
    }

    public static string FindAllJson(string target)
    {
        _lastNeedle = target ?? "";
        // Guardia deterministica (stessa di Find): needle vuota → nessun hit, senza toccare AX.
        if (!Available() || string.IsNullOrEmpty(target)) return "[]";
        var app = FrontApp();
        if (app == IntPtr.Zero) return "[]";
        var needle = CfInterop.CStr(target ?? "");
        var budget = 4000;
        var hits = new System.Collections.Generic.List<(string role, string title)>();
        Collect(app, false, hits, ref budget, 0);
        CfInterop.CFRelease(needle); CfInterop.CFRelease(app);
        return JsonSerializer.Serialize(hits.ConvertAll(h => new { role = h.role, title = h.title, level = "AX" }));
    }

    private static void Collect(IntPtr el, bool wantField,
        System.Collections.Generic.List<(string role, string title)> hits, ref int budget, int depth)
    {
        if (depth > 40 || budget <= 0 || el == IntPtr.Zero) return;
        budget--;
        var target = _lastNeedle ?? "";
        var rkey = Attr("AXRole"); var roleNow = ReadStrAttr(el, rkey); CfInterop.CFRelease(rkey);
        bool hit = false;
        if (!wantField || IsTextField(roleNow))
        {
            foreach (var a in new[] { "AXTitle", "AXDescription", wantField ? "AXPlaceholderValue" : "AXValue" })
            {
                var k = Attr(a); var v = ReadStrAttr(el, k); CfInterop.CFRelease(k);
                if (v is not null && target.Length > 0 &&
                    v.IndexOf(target, StringComparison.OrdinalIgnoreCase) >= 0) { hit = true; break; }
            }
        }
        if (hit)
        {
            var tkey = Attr("AXTitle"); var title = ReadStrAttr(el, tkey) ?? ""; CfInterop.CFRelease(tkey);
            hits.Add((roleNow ?? "?", title));
        }
        var ckey = Attr("AXChildren");
        if (AXUIElementCopyAttributeValue(el, ckey, out var children) == kAXErrorSuccess && children != IntPtr.Zero)
        {
            var n = CfInterop.CFArrayGetCount(children);
            for (nint i = 0; i < n; i++)
                Collect(CfInterop.CFArrayGetValueAtIndex(children, i), wantField, hits, ref budget, depth + 1);
            CfInterop.CFRelease(children);
        }
        CfInterop.CFRelease(ckey);
        // Come Find: anche i menu sotto AXMenuBar.
        var mkey = Attr("AXMenuBar");
        if (AXUIElementCopyAttributeValue(el, mkey, out var mb) == kAXErrorSuccess && mb != IntPtr.Zero)
        {
            Collect(mb, wantField, hits, ref budget, depth + 1);
            CfInterop.CFRelease(mb);
        }
        CfInterop.CFRelease(mkey);
    }

    public static int ClickOk(string target)
    {
        _lastNeedle = target ?? "";
        if (!Available()) return 0;
        var app = FrontApp();
        if (app == IntPtr.Zero) return 0;
        var needle = CfInterop.CStr(target ?? "");
        var budget = 4000; string role = "?", title = "";
        var el = Find(app, false, ref role, ref title, ref budget, 0);
        CfInterop.CFRelease(needle); CfInterop.CFRelease(app);
        if (el == IntPtr.Zero) return 0;
        var ok = PressOrCenterClick(el);
        CfInterop.CFRelease(el);
        return ok ? 1 : 0;
    }

    public static string TypeJson(string target, string text)
    {
        _lastNeedle = target ?? "";
        if (!Available()) return "{\"ok\":false,\"error\":\"AX non disponibile\"}";
        var app = FrontApp();
        if (app == IntPtr.Zero) return "{\"ok\":false,\"error\":\"nessuna app\"}";
        var needle = CfInterop.CStr(target ?? "");
        var budget = 4000; string role = "?", title = "";
        var el = Find(app, true, ref role, ref title, ref budget, 0);
        if (el == IntPtr.Zero)
        { budget = 4000; el = Find(app, false, ref role, ref title, ref budget, 0); }
        CfInterop.CFRelease(needle); CfInterop.CFRelease(app);
        if (el == IntPtr.Zero) return "{\"ok\":false,\"error\":\"campo non trovato\"}";
        var fkey = Attr("AXFocused");
        try
        {
            var kcf = CfInterop.KCFBooleanTrueRef();
            AXUIElementSetAttributeValue(el, fkey, kcf);
        }
        catch { }
        CfInterop.CFRelease(fkey);
        var ok = ManagedMacInput.Type(text ?? "") == 0;
        CfInterop.CFRelease(el);
        return ok ? "{\"ok\":true,\"role\":\"" + role + "\"}" : "{\"ok\":false,\"error\":\"Accessibilità negata\"}";
    }
}
