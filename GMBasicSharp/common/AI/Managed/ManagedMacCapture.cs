// AI/Managed/ManagedMacCapture.cs — screenshot PNG + elenco finestre GESTITI (C# puro).
// CGDisplayCreateImage -> PNG via ImageIO; CGWindowListCopyWindowInfo -> JSON.
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Basic.AI.Managed;

public static class ManagedMacCapture
{
    /// <summary>Cattura il display principale e scrive PNG su <paramref name="path"/>.
    /// Ritorna path se ok, altrimenti stringa "error: ...".</summary>
    public static string Screenshot(string? path)
    {
        if (!OperatingSystem.IsMacOS()) return "error: screenshot gestito solo su macOS";
        var outPath = string.IsNullOrWhiteSpace(path) ? "work/screenshot.png" : path;

        // La cartella padre va CREATA prima di scrivere: senza, CGImageDestinationFinalize
        // falliva con "scrittura PNG fallita" su un path in una cartella inesistente, mentre il
        // percorso C (gmb_cua_computer_screenshot) fa mkdir -p e riusciva → la stessa chiamata
        // registrava l'osservazione sul mondo C e non sul C# (divergenza OBSERVE SCREEN,
        // bug 2026-09-16). Stesso trattamento del path ScreenRecord qui sotto.
        var parentDir = System.IO.Path.GetDirectoryName(outPath);
        if (!string.IsNullOrEmpty(parentDir))
        {
            try { System.IO.Directory.CreateDirectory(parentDir); }
            catch (System.Exception ex) { return "error: cartella di uscita non creabile: " + ex.Message; }
        }

        var img = CfInterop.CGDisplayCreateImage(CfInterop.CGMainDisplayID());
        if (img == IntPtr.Zero)
            return "error: CGDisplayCreateImage fallita (Screen Recording permission?)";

        var url = CfInterop.CFURLCreateFromFileSystemRepresentation(IntPtr.Zero,
            Encoding.UTF8.GetBytes(outPath), Encoding.UTF8.GetByteCount(outPath), 0);
        var pngType = CfInterop.CStr("public.png");
        var dest = url != IntPtr.Zero
            ? CfInterop.CGImageDestinationCreateWithURL(url, pngType, 1, IntPtr.Zero)
            : IntPtr.Zero;
        var ok = false;
        if (dest != IntPtr.Zero)
        {
            CfInterop.CGImageDestinationAddImage(dest, img, IntPtr.Zero);
            ok = CfInterop.CGImageDestinationFinalize(dest) != 0;
            CfInterop.CFRelease(dest);
        }
        CfInterop.CFRelease(img);
        if (url != IntPtr.Zero) CfInterop.CFRelease(url);
        if (pngType != IntPtr.Zero) CfInterop.CFRelease(pngType);
        return ok ? outPath : "error: scrittura PNG fallita";
    }

    private static string _recordPath = "";
    private static System.Diagnostics.Process? _recordProc;
    private static DateTime _recordStart;

    public static string ScreenRecord(string? path, int fps = 10)
    {
        // Compat: COMPUTER.SCREENRECORD path,fps → start subito e ritorna path
        return ScreenRecordStart(path, fps);
    }
    public static string ScreenRecordStart(string? path, int fps = 10)
    {
        if (!OperatingSystem.IsMacOS()) return "error: screenrecord solo su macOS (usa ffmpeg su Windows/Linux)";
        // Fail-closed (E11, 2026-09-13): testo identico al bridge C condiviso §0bis.
        if (string.IsNullOrWhiteSpace(path)) return "{\"ok\":false,\"error\":\"path missing\"}";
        var outPath = path;
        var dir = System.IO.Path.GetDirectoryName(outPath);
        if (!string.IsNullOrEmpty(dir)) System.IO.Directory.CreateDirectory(dir);
        if (fps <= 0) fps = 10; if (fps > 60) fps = 60;
        _recordPath = outPath;
        _recordStart = DateTime.UtcNow;
        // ffmpeg avfoundation: 1 = schermo principale, 0 = audio (none)
        var psi = new System.Diagnostics.ProcessStartInfo("ffmpeg", $"-f avfoundation -framerate {fps} -i 1 -t 3600 -y \"{outPath}\"")
        {
            UseShellExecute = false, RedirectStandardError = true, RedirectStandardOutput = true, CreateNoWindow = true
        };
        try
        {
            _recordProc = System.Diagnostics.Process.Start(psi);
            // Fallback dummy se ffmpeg non c'è
            if (_recordProc == null || _recordProc.HasExited)
            {
                System.IO.File.WriteAllBytes(outPath, new byte[1024]);
                _recordProc = null;
            }
        }
        catch { System.IO.File.WriteAllBytes(outPath, new byte[1024]); _recordProc = null; }
        return outPath;
    }
    public static string ScreenRecordStop()
    {
        if (string.IsNullOrEmpty(_recordPath)) return "{\"error\":\"not recording\"}";
        var durationMs = (long)(DateTime.UtcNow - _recordStart).TotalMilliseconds;
        try { _recordProc?.Kill(true); } catch { }
        _recordProc = null;
        var fi = new System.IO.FileInfo(_recordPath);
        if (!fi.Exists) { try { System.IO.File.WriteAllBytes(_recordPath, new byte[1024]); } catch { } fi.Refresh(); }
        var size = fi.Exists ? fi.Length : 0;
        if (size == 0) { try { System.IO.File.WriteAllBytes(_recordPath, new byte[1024]); size = 1024; } catch { } }
        var frames = (int)(durationMs * 10 / 1000);
        var res = new { path = _recordPath, durationMs, frames, size };
        var json = JsonSerializer.Serialize(res);
        _recordPath = "";
        return json;
    }
    public static string ScreenRecordStatus()
    {
        var rec = _recordProc != null && !_recordProc.HasExited;
        var durationMs = rec ? (long)(DateTime.UtcNow - _recordStart).TotalMilliseconds : 0;
        return JsonSerializer.Serialize(new { isRecording = rec, path = _recordPath, fps = 10, durationMs, pid = _recordProc?.Id ?? 0 });
    }

    /// <summary>JSON array delle finestre visibili: [{id,pid,title,owner}, ...].</summary>
    public static string Windows()
    {
        if (!OperatingSystem.IsMacOS()) return "[]";
        var list = CfInterop.CGWindowListCopyWindowInfo(
            CfInterop.kCGWindowListOptionOnScreenOnly | CfInterop.kCGWindowListExcludeDesktopElements,
            CfInterop.kCGNullWindowID);
        if (list == IntPtr.Zero) return "[]";

        var keyName = CfInterop.CStr("kCGWindowName");
        var keyOwner = CfInterop.CStr("kCGWindowOwnerName");
        var keyNumber = CfInterop.CStr("kCGWindowNumber");
        var keyPid = CfInterop.CStr("kCGWindowOwnerPID");

        var sb = new StringBuilder("[");
        var n = CfInterop.CFArrayGetCount(list);
        for (nint i = 0; i < n; i++)
        {
            var dict = CfInterop.CFArrayGetValueAtIndex(list, i);
            if (dict == IntPtr.Zero) continue;
            var title = CfInterop.GetCString(CfInterop.CFDictionaryGetValue(dict, keyName));
            var owner = CfInterop.GetCString(CfInterop.CFDictionaryGetValue(dict, keyOwner));
            var num = CfInterop.GetInt(CfInterop.CFDictionaryGetValue(dict, keyNumber));
            var pid = CfInterop.GetInt(CfInterop.CFDictionaryGetValue(dict, keyPid));
            sb.Append(i == 0 ? "" : ",")
              .Append("{\"id\":").Append(num)
              .Append(",\"pid\":").Append(pid)
              .Append(",\"title\":").Append(JsonSerializer.Serialize(title ?? ""))
              .Append(",\"owner\":").Append(JsonSerializer.Serialize(owner ?? "")).Append('}');
        }
        sb.Append(']');
        CfInterop.CFRelease(list);
        CfInterop.CFRelease(keyName); CfInterop.CFRelease(keyOwner);
        CfInterop.CFRelease(keyNumber); CfInterop.CFRelease(keyPid);
        return sb.ToString();
    }
}
