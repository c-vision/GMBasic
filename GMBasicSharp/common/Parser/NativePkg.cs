// FILE: Interpreter.Parser.NativePkg.cs
// Binding P/Invoke al Package Manager di RIFERIMENTO in C (native/gmb_runtime_c → libgmb_pkg).
// Direzione unica C#→C: il C# CONSUMA la fonte C, non tiene una copia divergente.
using System.Runtime.InteropServices;

namespace Basic;

public static class NativePkg
{
    private const string Lib = "gmb_pkg";

    static NativePkg()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativePkg).Assembly, Resolve); }
        catch { /* resolver già impostato (altra lib dell'assembly) */ }
        // Barra di avanzamento dei download (disegnata dal C su stderr) SOLO se stderr è
        // una console interattiva; mai su pipe/file (output pulito per script e test).
        try { registry_set_progress(Console.IsErrorRedirected ? 0 : 1); } catch { }
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly asm, DllImportSearchPath? sp)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    /// <summary>Specchio di <c>gmb_semver_t</c> (native/gmb_runtime_c/include/gmb_semver.h).</summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Csemver
    {
        public int major, minor, patch, hasPre;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string prerelease;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string build;
    }

    [DllImport(Lib, EntryPoint = "gmb_semver_parse", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int semver_parse(string s, out Csemver outv);

    [DllImport(Lib, EntryPoint = "gmb_semver_satisfies", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int semver_satisfies(in Csemver v, string range);

    [DllImport(Lib, EntryPoint = "gmb_semver_cmp", CallingConvention = CallingConvention.Cdecl)]
    private static extern int semver_cmp(in Csemver a, in Csemver b);

    [DllImport(Lib, EntryPoint = "gmb_semver_find_best", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int semver_find_best(int nc, string[] constraints, int nv, string[] versions, [In, Out] byte[] outbuf, UIntPtr cap);

    public static bool Parse(string s, out Csemver c) => semver_parse(s, out c) == 1;
    public static bool Satisfies(in Csemver v, string range) => semver_satisfies(v, range) == 1;
    public static int Cmp(in Csemver a, in Csemver b) => semver_cmp(a, b);

    public static string? FindBest(string[] constraints, string[] versions)
    {
        var buf = new byte[256];
        if (semver_find_best(constraints.Length, constraints, versions.Length, versions, buf, (UIntPtr)buf.Length) != 1)
            return null;
        var end = Array.IndexOf(buf, (byte)0);
        if (end < 0) end = buf.Length;
        return System.Text.Encoding.ASCII.GetString(buf, 0, end);
    }

    // ── Manifest (gmb_pkg_manifest) ──────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_parse", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr manifest_parse(string json);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void manifest_free(IntPtr m);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_get", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_get(IntPtr m, string path, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_get_num", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_get_num(IntPtr m, string path, out double outv);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_count", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_count(IntPtr m, string path);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_item", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_item(IntPtr m, string path, int i, [In, Out] byte[] key, UIntPtr kcap, [In, Out] byte[] val, UIntPtr vcap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_has", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_has(IntPtr m, string path);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_is_list", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_is_list(IntPtr m, string path);

    public static IntPtr ManifestParse(string json) => manifest_parse(json);
    public static void ManifestFree(IntPtr m) { if (m != IntPtr.Zero) manifest_free(m); }
    public static bool ManifestHas(IntPtr m, string path) => manifest_has(m, path) == 1;
    public static bool ManifestIsList(IntPtr m, string path) => manifest_is_list(m, path) == 1;
    public static int ManifestCount(IntPtr m, string path) => manifest_count(m, path);

    public static string ManifestGet(IntPtr m, string path)
    {
        var buf = new byte[2048];
        if (manifest_get(m, path, buf, (UIntPtr)buf.Length) != 1) return "";
        return CStr(buf);
    }

    public static bool ManifestGetNum(IntPtr m, string path, out double v)
        => manifest_get_num(m, path, out v) == 1;

    public static bool ManifestItem(IntPtr m, string path, int i, out string key, out string val)
    {
        var kb = new byte[512];
        var vb = new byte[8192];
        var ok = manifest_item(m, path, i, kb, (UIntPtr)kb.Length, vb, (UIntPtr)vb.Length) == 1;
        key = ok ? CStr(kb) : "";
        val = ok ? CStr(vb) : "";
        return ok;
    }

    private static string CStr(byte[] buf)
    {
        var n = Array.IndexOf(buf, (byte)0);
        if (n < 0) n = buf.Length;
        return System.Text.Encoding.UTF8.GetString(buf, 0, n);
    }

    // ── Lock + integrità (gmb_pkg_lock) ──────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_sha256_file", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int sha256_file(string path, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_pkg_hash_files", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int pkg_hash_files(string installDir, string manifestJson, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_read", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr lock_read(string path);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void lock_free(IntPtr l);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int lock_count(IntPtr l);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_entry", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int lock_entry(IntPtr l, int i, [In, Out] byte[] name, UIntPtr nc, [In, Out] byte[] ver, UIntPtr vc,
                                        [In, Out] byte[] ct, UIntPtr cc, [In, Out] byte[] be, UIntPtr bc, [In, Out] byte[] pl, UIntPtr pc);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_hash_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int lock_hash_count(IntPtr l, int i);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_hash", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int lock_hash(IntPtr l, int i, int j, [In, Out] byte[] f, UIntPtr fc, [In, Out] byte[] h, UIntPtr hc);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_write", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int lock_write(string path, string body);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_verify", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int lock_verify(IntPtr l, string cacheRoot, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_lock_write_with_history", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int lock_write_with_history(string projectDir, string lockPath, string body);

    public static string Sha256File(string path)
    {
        var buf = new byte[65];
        return sha256_file(path, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
    }

    public static string PkgHashFiles(string installDir, string manifestJson)
    {
        var buf = new byte[1 << 21]; // 2 MB (fingerprint: file:hash,…)
        return pkg_hash_files(installDir, manifestJson, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
    }

    public static IntPtr LockRead(string path) => lock_read(path);
    public static void LockFree(IntPtr l) { if (l != IntPtr.Zero) lock_free(l); }
    public static int LockCount(IntPtr l) => lock_count(l);
    public static int LockHashCount(IntPtr l, int i) => lock_hash_count(l, i);
    public static bool LockWrite(string path, string body) => lock_write(path, body) == 1;
    /// <summary>Scrive il lock e archivia lo storico in &lt;projectDir&gt;/.gmbasic/lock-history/ (delega al C).</summary>
    public static bool LockWriteWithHistory(string projectDir, string lockPath, string body)
        => lock_write_with_history(projectDir, lockPath, body) == 1;

    public static bool LockEntry(IntPtr l, int i, out string name, out string ver, out string ct, out string backend, out string platform)
    {
        var nb = new byte[512]; var vb = new byte[64]; var cb = new byte[64]; var bb = new byte[64]; var pb = new byte[64];
        var ok = lock_entry(l, i, nb, (UIntPtr)nb.Length, vb, (UIntPtr)vb.Length, cb, (UIntPtr)cb.Length,
                            bb, (UIntPtr)bb.Length, pb, (UIntPtr)pb.Length) == 1;
        name = ok ? CStr(nb) : ""; ver = ok ? CStr(vb) : ""; ct = ok ? CStr(cb) : "";
        backend = ok ? CStr(bb) : ""; platform = ok ? CStr(pb) : "";
        return ok;
    }

    public static bool LockHash(IntPtr l, int i, int j, out string file, out string hash)
    {
        var fb = new byte[4096]; var hb = new byte[128];
        var ok = lock_hash(l, i, j, fb, (UIntPtr)fb.Length, hb, (UIntPtr)hb.Length) == 1;
        file = ok ? CStr(fb) : ""; hash = ok ? CStr(hb) : "";
        return ok;
    }

    public static int LockVerify(IntPtr l, string cacheRoot, out string report)
    {
        var buf = new byte[1 << 20];
        var n = lock_verify(l, cacheRoot, buf, (UIntPtr)buf.Length);
        report = CStr(buf);
        return n;
    }

    // ── Trust: keyring + HMAC (gmb_pkg_trust) ────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_hmac_hex", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int hmac_hex(string key, byte[] data, UIntPtr len, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_hmac_file_hex", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int hmac_file_hex(string key, string file, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_key_fingerprint", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int key_fingerprint(string key, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_keyring_get", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int keyring_get(string path, string name, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_keyring_set", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int keyring_set(string path, string name, string key);
    [DllImport(Lib, EntryPoint = "gmb_pkg_keyring_remove", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int keyring_remove(string path, string name);
    [DllImport(Lib, EntryPoint = "gmb_pkg_keyring_count", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int keyring_count(string path);
    [DllImport(Lib, EntryPoint = "gmb_pkg_keyring_item", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int keyring_item(string path, int i, [In, Out] byte[] name, UIntPtr ncap, [In, Out] byte[] key, UIntPtr kcap);

    public static string HmacFileHex(string key, string file)
    {
        var buf = new byte[65];
        return hmac_file_hex(key, file, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
    }

    public static string KeyFingerprint(string key)
    {
        var buf = new byte[32];
        return key_fingerprint(key, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
    }

    public static string KeyringGet(string path, string name)
    {
        var buf = new byte[8192];
        return keyring_get(path, name, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
    }

    public static bool KeyringSet(string path, string name, string key) => keyring_set(path, name, key) == 1;
    public static bool KeyringRemove(string path, string name) => keyring_remove(path, name) == 1;
    public static int KeyringCount(string path) => keyring_count(path);

    public static bool KeyringItem(string path, int i, out string name, out string key)
    {
        var nb = new byte[1024]; var kb = new byte[8192];
        var ok = keyring_item(path, i, nb, (UIntPtr)nb.Length, kb, (UIntPtr)kb.Length) == 1;
        name = ok ? CStr(nb) : ""; key = ok ? CStr(kb) : "";
        return ok;
    }

    // ── Glob (gmb_pkg_glob) ──────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_glob_match", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int glob_match(string pattern, string relPath);
    public static bool GlobMatch(string pattern, string relPath) => glob_match(pattern, relPath) == 1;

    // ── Cache content-addressable (gmb_pkg_contentaddr) ──────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_content_intern", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int content_intern(string contentRoot, string filePath, [In, Out] byte[] sha, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_content_intern_dir", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int content_intern_dir(string contentRoot, string dir);
    [DllImport(Lib, EntryPoint = "gmb_pkg_content_gc", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int content_gc(string contentRoot, string cacheRoot);

    public static string? ContentIntern(string contentRoot, string filePath)
    {
        var buf = new byte[65];
        return content_intern(contentRoot, filePath, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
    }
    public static int ContentInternDir(string contentRoot, string dir) => content_intern_dir(contentRoot, dir);
    public static int ContentGc(string contentRoot, string cacheRoot) => content_gc(contentRoot, cacheRoot);

    // ── Resolver (gmb_pkg_resolver, builder ABI) ─────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_new", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr resolve_ctx_new();
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_set", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_set(IntPtr c, string cacheRoot, string vendorDir, string backend, string platform, string edition, int offline);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_set_registry", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_set_registry(IntPtr c, string url);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_cache_root", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_cache_root(IntPtr c, string dir);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_req", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_req(IntPtr c, string name, string range);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_override", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_override(IntPtr c, string name, string range);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_feature", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_feature(IntPtr c, string f);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_locked", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_locked(IntPtr c, string name, string version);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_local", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_local(IntPtr c, string name, string dir);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_add_registry_scope", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void resolve_ctx_add_registry_scope(IntPtr c, string scope, string url);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve_ctx_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void resolve_ctx_free(IntPtr c);
    [DllImport(Lib, EntryPoint = "gmb_pkg_resolve", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pkg_resolve(IntPtr c);
    [DllImport(Lib, EntryPoint = "gmb_pkg_result_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int result_count(IntPtr r);
    [DllImport(Lib, EntryPoint = "gmb_pkg_result_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr result_error(IntPtr r);
    [DllImport(Lib, EntryPoint = "gmb_pkg_result_item", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int result_item(IntPtr r, int i, [In, Out] byte[] name, UIntPtr ncap, [In, Out] byte[] version, UIntPtr vcap,
                                          [In, Out] byte[] installDir, UIntPtr icap, [In, Out] byte[] lockEntry, UIntPtr lcap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_result_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void result_free(IntPtr r);

    // ── Registry (gmb_pkg_registry) ──────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_get", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_get(string url, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_tag", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_tag(string baseUrl, string name, string tag, [In, Out] byte[] outv, UIntPtr cap);

    /// <summary>GET testuale (delega al C: gmb_pkg_registry_get). Null se irraggiungibile.</summary>
    public static string? RegistryGet(string url)
    {
        var buf = new byte[1 << 20];
        return registry_get(url, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
    }

    [DllImport(Lib, EntryPoint = "gmb_net_http_download", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int net_http_download(string url, string destPath, int showProgress);
    /// <summary>Scarica un URL su file in streaming (delega al C: gmb_net_http_download).</summary>
    public static bool HttpDownload(string url, string destPath, bool showProgress = false)
        => net_http_download(url, destPath, showProgress ? 1 : 0) == 0;
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_detail", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr registry_detail(string baseUrl, string name);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_detail_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int registry_detail_count(IntPtr d);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_detail_version", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_detail_version(IntPtr d, int i, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_detail_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void registry_detail_free(IntPtr d);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_download", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_download(string baseUrl, string name, string version, string qs, string destPath);

    public static string? RegistryTag(string baseUrl, string name, string tag)
    {
        var buf = new byte[256];
        return registry_tag(baseUrl, name, tag, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
    }

    public static List<string> RegistryVersions(string baseUrl, string name)
    {
        var list = new List<string>();
        var d = registry_detail(baseUrl, name);
        if (d == IntPtr.Zero) return list;
        try
        {
            var n = registry_detail_count(d);
            for (var i = 0; i < n; i++)
            {
                var b = new byte[128];
                if (registry_detail_version(d, i, b, (UIntPtr)b.Length) == 1) list.Add(CStr(b));
            }
        }
        finally { registry_detail_free(d); }
        return list;
    }

    public static bool RegistryDownload(string baseUrl, string name, string version, string qs, string destPath)
        => registry_download(baseUrl, name, version, qs ?? "", destPath) == 1;

    // ── HTTP autenticato del registry (publish/admin) — C gmb_pkg_registry_post/get_auth ──
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_post", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_post(string url, string json, string bearer, [In, Out] byte[] outv, UIntPtr cap, out int status);
    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_get_auth", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int registry_get_auth(string url, string bearer, [In, Out] byte[] outv, UIntPtr cap, out int status);
    /// <summary>POST JSON autenticato al registry (delega al C). Body/status via out; true = 2xx.</summary>
    public static bool RegistryPost(string url, string json, string? bearer, out int status, out string body)
    {
        var buf = new byte[1 << 20];
        var ok = registry_post(url, json, bearer ?? "", buf, (UIntPtr)buf.Length, out status) == 1;
        body = CStr(buf);
        return ok;
    }
    /// <summary>GET autenticato al registry (delega al C). Body/status via out; true = 2xx.</summary>
    public static bool RegistryGetAuth(string url, string? bearer, out int status, out string body)
    {
        var buf = new byte[1 << 20];
        var ok = registry_get_auth(url, bearer ?? "", buf, (UIntPtr)buf.Length, out status) == 1;
        body = CStr(buf);
        return ok;
    }

    // ── Edit manifest (gmb add/remove) — C gmb_pkg_manifest_set_dep/remove_dep ──
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_set_dep", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_set_dep(string path, string section, string name, string spec);
    [DllImport(Lib, EntryPoint = "gmb_pkg_manifest_remove_dep", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int manifest_remove_dep(string path, string name);
    /// <summary>Aggiunge/aggiorna una dipendenza nel manifest su disco (delega al C). true = ok.</summary>
    public static bool SetDep(string path, string section, string name, string spec) => manifest_set_dep(path, section, name, spec) == 1;
    /// <summary>Rimuove una dipendenza da dependencies+devDependencies nel manifest su disco (delega al C). true = ok.</summary>
    public static bool RemoveDep(string path, string name) => manifest_remove_dep(path, name) == 1;

    [DllImport(Lib, EntryPoint = "gmb_unzip_files", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int unzip_files(string zip, string outdir);
    /// <summary>Estrae un archivio zip in <paramref name="outdir"/> (delega al C: gmb_unzip_files). true = ok (il C ritorna 0).</summary>
    public static bool Unzip(string zip, string outdir) => unzip_files(zip, outdir) == 0;

    [DllImport(Lib, EntryPoint = "gmb_pkg_registry_set_progress", CallingConvention = CallingConvention.Cdecl)]
    private static extern void registry_set_progress(int on);
    /// <summary>Abilita/disabilita la barra di avanzamento dei download (disegnata dal C su stderr).</summary>
    public static void SetDownloadProgress(bool on) => registry_set_progress(on ? 1 : 0);

    // ── Proiezioni host (C: gmb_pkg_binary_path / gmb_pkg_module_sources) ────
    [DllImport(Lib, EntryPoint = "gmb_pkg_binary_path", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int binary_path(string installDir, string backend, string platform, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_module_sources", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int module_sources(string installDir, [In, Out] byte[] outv, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_installed_public_key", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int installed_public_key(string installDir, [In, Out] byte[] outv, UIntPtr cap);
    /// <summary>Chiave pubblica di firma di un pacchetto installato (delega al C). Null se non firmato.</summary>
    public static string? InstalledPublicKey(string installDir)
    {
        var buf = new byte[8192];
        return installed_public_key(installDir, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
    }

    /// <summary>Path assoluto del binario scelto per backend+platform (delega al C). Null se nessuno.</summary>
    public static string? BinaryPath(string installDir, string backend, string platform)
    {
        var buf = new byte[4096];
        return binary_path(installDir, backend, platform, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
    }
    /// <summary>Directory sorgente esportate (.bas) di un pacchetto (delega al C).</summary>
    public static IEnumerable<string> ModuleSources(string installDir)
    {
        var buf = new byte[16384];
        var n = module_sources(installDir, buf, (UIntPtr)buf.Length);
        var s = n > 0 ? CStr(buf) : "";
        return s.Length == 0 ? Array.Empty<string>() : s.Split('\n', StringSplitOptions.RemoveEmptyEntries);
    }

    // ── Artefatto (gmb_pkg_artifact) ─────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_select", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr artifact_select(string installDir, string backend, string platform);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void artifact_free(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ct", CallingConvention = CallingConvention.Cdecl)]
    private static extern int artifact_ct(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_source_dir", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_source_dir(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_binary", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_binary(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_chunk", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_chunk(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ffi_link", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_ffi_link(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_declare", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_declare(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_backend", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_backend(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_platform", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_platform(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ffi_lib_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int artifact_ffi_lib_count(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ffi_lib", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_ffi_lib(IntPtr a, int i);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ffi_header_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int artifact_ffi_header_count(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_artifact_ffi_header", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr artifact_ffi_header(IntPtr a, int i);

    public sealed record ArtifactData(int ContentType, string? SourceDir, string? Binary, string? Chunk,
        string? FfiLink, string? Declare, string Backend, string Platform, string[] FfiLibs, string[] FfiHeaders);

    public static ArtifactData? ArtifactSelect(string installDir, string backend, string platform)
    {
        var h = artifact_select(installDir, backend, platform);
        if (h == IntPtr.Zero) return null;
        try
        {
            var libs = new List<string>();
            for (var i = 0; i < artifact_ffi_lib_count(h); i++) { var s = Ptr(artifact_ffi_lib(h, i)); if (s != null) libs.Add(s); }
            var hdrs = new List<string>();
            for (var i = 0; i < artifact_ffi_header_count(h); i++) { var s = Ptr(artifact_ffi_header(h, i)); if (s != null) hdrs.Add(s); }
            return new ArtifactData(artifact_ct(h), Ptr(artifact_source_dir(h)), Ptr(artifact_binary(h)), Ptr(artifact_chunk(h)),
                Ptr(artifact_ffi_link(h)), Ptr(artifact_declare(h)), Ptr(artifact_backend(h)) ?? "any", Ptr(artifact_platform(h)) ?? "any",
                libs.ToArray(), hdrs.ToArray());
        }
        finally { artifact_free(h); }
    }

    private static string? Ptr(IntPtr p) => p == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(p);

    // ── Install-time: source / samples / build (gmb_pkg_install) ─────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_ensure_source", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ensure_source(string installDir, string platform);
    [DllImport(Lib, EntryPoint = "gmb_pkg_copy_samples", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int copy_samples(string installDir, string projectRoot);
    [DllImport(Lib, EntryPoint = "gmb_pkg_ensure_build", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ensure_build(string installDir, string platform);
    [DllImport(Lib, EntryPoint = "gmb_pkg_run_build", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int run_build(string installDir, string platform);

    // ── Pack (gmb_pkg_pack) ──────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_pack_files", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr pack_files(string dir, string backendsCsv);
    [DllImport(Lib, EntryPoint = "gmb_pkg_pack_files_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int pack_files_count(IntPtr f);
    [DllImport(Lib, EntryPoint = "gmb_pkg_pack_files_get", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr pack_files_get(IntPtr f, int i);
    [DllImport(Lib, EntryPoint = "gmb_pkg_pack_files_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void pack_files_free(IntPtr f);
    [DllImport(Lib, EntryPoint = "gmb_pkg_pack", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int pkg_pack(string dir, string outFile, string hmacKey);

    public static List<string> PackFiles(string dir, string? backendsCsv)
    {
        var list = new List<string>();
        var h = pack_files(dir, backendsCsv ?? "");
        if (h == IntPtr.Zero) return list;
        try
        {
            var n = pack_files_count(h);
            for (var i = 0; i < n; i++) { var s = Ptr(pack_files_get(h, i)); if (s != null) list.Add(s); }
        }
        finally { pack_files_free(h); }
        return list;
    }

    public static bool Pack(string dir, string outFile, string? hmacKey) => pkg_pack(dir, outFile, hmacKey ?? "") == 1;

    // ── Audit + SBOM (gmb_pkg_advisories) ────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_audit", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr pkg_audit(int n, string[] names, string[] versions, string advisoriesJson);
    [DllImport(Lib, EntryPoint = "gmb_pkg_audit_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int audit_count(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_audit_item", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int audit_item(IntPtr a, int i, [In, Out] byte[] name, UIntPtr nc, [In, Out] byte[] ver, UIntPtr vc,
                                         [In, Out] byte[] id, UIntPtr ic, [In, Out] byte[] sev, UIntPtr sc,
                                         [In, Out] byte[] sum, UIntPtr smc, [In, Out] byte[] fix, UIntPtr fc);
    [DllImport(Lib, EntryPoint = "gmb_pkg_audit_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void audit_free(IntPtr a);
    [DllImport(Lib, EntryPoint = "gmb_pkg_sbom", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int pkg_sbom(int n, string[] names, string[] versions, string[] installDirs, [In, Out] byte[] outv, UIntPtr cap);

    public sealed record AuditHit(string Name, string Version, string Id, string Severity, string Summary, string Fixed);

    public static List<AuditHit> Audit(string[] names, string[] versions, string advisoriesJson)
    {
        var list = new List<AuditHit>();
        var a = pkg_audit(names.Length, names, versions, advisoriesJson);
        if (a == IntPtr.Zero) return list;
        try
        {
            var n = audit_count(a);
            for (var i = 0; i < n; i++)
            {
                var nb = new byte[256]; var vb = new byte[64]; var ib = new byte[64];
                var sb = new byte[64]; var mb = new byte[1024]; var fb = new byte[64];
                if (audit_item(a, i, nb, (UIntPtr)nb.Length, vb, (UIntPtr)vb.Length, ib, (UIntPtr)ib.Length,
                        sb, (UIntPtr)sb.Length, mb, (UIntPtr)mb.Length, fb, (UIntPtr)fb.Length) == 1)
                    list.Add(new AuditHit(CStr(nb), CStr(vb), CStr(ib), CStr(sb), CStr(mb), CStr(fb)));
            }
        }
        finally { audit_free(a); }
        return list;
    }

    public static string Sbom(string[] names, string[] versions, string[] installDirs)
    {
        var buf = new byte[1 << 20];
        return pkg_sbom(names.Length, names, versions, installDirs, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "{}";
    }

    // ── Ed25519 (gmb_pkg_ed25519, OpenSSL) ───────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_ed25519_sign", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ed_sign(string priv, byte[] data, UIntPtr len, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_ed25519_public", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ed_public(string priv, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_ed25519_verify", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ed_verify(string pub, byte[] data, UIntPtr len, string sig);

    public static string? EdSign(string priv, byte[] data)
    {
        var b = new byte[128];
        return ed_sign(priv, data, (UIntPtr)data.Length, b, (UIntPtr)b.Length) == 1 ? CStr(b) : null;
    }
    public static string? EdPublic(string priv)
    {
        var b = new byte[64];
        return ed_public(priv, b, (UIntPtr)b.Length) == 1 ? CStr(b) : null;
    }
    [DllImport(Lib, EntryPoint = "gmb_pkg_ed25519_keygen", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ed_keygen([In, Out] byte[] priv, UIntPtr pcap, [In, Out] byte[] pub, UIntPtr ucap);
    public static bool EdVerify(string pub, byte[] data, string sig)
        => ed_verify(pub, data, (UIntPtr)data.Length, sig) == 1;

    public static (string PrivateKey, string PublicKey)? EdKeygen()
    {
        var pv = new byte[64]; var pb = new byte[64];
        return ed_keygen(pv, (UIntPtr)pv.Length, pb, (UIntPtr)pb.Length) == 1
            ? (CStr(pv), CStr(pb)) : null;
    }

    // ── ECDSA P-256 PEM (gmb_pkg_ecdsa, legacy) ──────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_ecdsa_sign_pem", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ecdsa_sign(string priv, byte[] data, UIntPtr len, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_ecdsa_public_pem", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int ecdsa_public(string priv, [In, Out] byte[] outv, UIntPtr cap);

    public static string? EcdsaSign(string priv, byte[] data)
    {
        var b = new byte[256];
        return ecdsa_sign(priv, data, (UIntPtr)data.Length, b, (UIntPtr)b.Length) == 1 ? CStr(b) : null;
    }
    public static string? EcdsaPublic(string priv)
    {
        var b = new byte[4096];
        return ecdsa_public(priv, b, (UIntPtr)b.Length) == 1 ? CStr(b) : null;
    }

    // ── Util (gmb_pkg_util) ──────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_compat_check", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int compat_check(string json, string edition, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_safe_dir", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int safe_dir(string name, [In, Out] byte[] outv, UIntPtr cap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_platform_key", CallingConvention = CallingConvention.Cdecl)]
    private static extern int platform_key([In, Out] byte[] outv, UIntPtr cap);

    /// <summary>Gate di compatibilità: "" se ok, altrimenti il messaggio di errore (dal C).</summary>
    public static string CompatCheck(string json, string edition)
    {
        var b = new byte[512];
        return compat_check(json, edition, b, (UIntPtr)b.Length) == 1 ? "" : CStr(b);
    }
    public static string SafeDir(string name)
    {
        var b = new byte[512];
        return safe_dir(name, b, (UIntPtr)b.Length) == 1 ? CStr(b) : "";
    }
    public static string PlatformKey()
    {
        var b = new byte[64];
        return platform_key(b, (UIntPtr)b.Length) == 1 ? CStr(b) : "any-any";
    }

    // ── Grafo dipendenze + peer (gmb_pkg_graph) ──────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_pkg_dep_tree", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr dep_tree(int n, string[] names, string[] installDirs);
    [DllImport(Lib, EntryPoint = "gmb_pkg_tree_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int tree_count(IntPtr t);
    [DllImport(Lib, EntryPoint = "gmb_pkg_tree_row", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int tree_row(IntPtr t, int i, [In, Out] byte[] parent, UIntPtr pcap, [In, Out] byte[] children, UIntPtr ccap);
    [DllImport(Lib, EntryPoint = "gmb_pkg_tree_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void tree_free(IntPtr t);
    [DllImport(Lib, EntryPoint = "gmb_pkg_check_peers", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int check_peers(int n, string[] names, string[] installDirs, [In, Out] byte[] outv, UIntPtr cap);

    public static Dictionary<string, string[]> DepTree(string[] names, string[] installDirs)
    {
        var d = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase);
        var t = dep_tree(names.Length, names, installDirs);
        if (t == IntPtr.Zero) return d;
        try
        {
            var n = tree_count(t);
            for (var i = 0; i < n; i++)
            {
                var pb = new byte[512]; var cb = new byte[8192];
                if (tree_row(t, i, pb, (UIntPtr)pb.Length, cb, (UIntPtr)cb.Length) == 1)
                {
                    var csv = CStr(cb);
                    d[CStr(pb)] = csv.Length == 0 ? [] : csv.Split(',', StringSplitOptions.RemoveEmptyEntries);
                }
            }
        }
        finally { tree_free(t); }
        return d;
    }

    public static List<string> CheckPeersList(string[] names, string[] installDirs)
    {
        var buf = new byte[1 << 16];
        check_peers(names.Length, names, installDirs, buf, (UIntPtr)buf.Length);
        var s = CStr(buf);
        return s.Length == 0 ? [] : s.Split('\n', StringSplitOptions.RemoveEmptyEntries).ToList();
    }

    [DllImport(Lib, EntryPoint = "gmb_pkg_c99_link_info", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int c99_link_info(int n, string[] names, string[] installDirs, string backend, string platform,
                                            [In, Out] byte[] inc, UIntPtr ic, [In, Out] byte[] libdirs, UIntPtr lc, [In, Out] byte[] libs, UIntPtr bc);

    public static (string[] Includes, string[] LibDirs, string[] Libs) C99LinkInfo(string[] names, string[] installDirs, string backend, string platform)
    {
        var ib = new byte[1 << 16]; var db = new byte[1 << 16]; var bb = new byte[1 << 16];
        c99_link_info(names.Length, names, installDirs, backend, platform, ib, (UIntPtr)ib.Length, db, (UIntPtr)db.Length, bb, (UIntPtr)bb.Length);
        static string[] Split(string s) => s.Length == 0 ? [] : s.Split(',', StringSplitOptions.RemoveEmptyEntries);
        return (Split(CStr(ib)), Split(CStr(db)), Split(CStr(bb)));
    }

    public static bool EnsureSource(string dir, string platform) => ensure_source(dir, platform) == 1;
    public static bool CopySamples(string dir, string projectRoot) => copy_samples(dir, projectRoot) == 1;
    public static int EnsureBuild(string dir, string platform) => ensure_build(dir, platform);
    public static bool RunBuild(string dir, string platform) => run_build(dir, platform) == 1;

    public sealed record ResolveItem(string Name, string Version, string InstallDir, string LockEntry);

    public static (List<ResolveItem> Items, string? Error) Resolve(
        string cacheRoot, string? vendorDir, string backend, string platform, string edition, bool offline, string? registryUrl,
        IEnumerable<(string Name, string Range)> reqs,
        IEnumerable<(string Name, string Range)> overrides,
        IEnumerable<string> features,
        IEnumerable<(string Name, string Version)> locked,
        IEnumerable<(string Name, string Dir)> local,
        IEnumerable<(string Scope, string Url)> registries,
        string? localPackagesDir = null)
    {
        var c = resolve_ctx_new();
        try
        {
            resolve_ctx_set(c, cacheRoot, vendorDir ?? "", backend, platform, edition, offline ? 1 : 0);
            // INSTALLAZIONE LOCALE dell'app (<dir dell'eseguibile>/packages): ha la PRECEDENZA
            // sulla cache (come node_modules), ma i download nuovi vanno sempre in cacheRoot.
            if (!string.IsNullOrWhiteSpace(localPackagesDir))
                resolve_ctx_add_cache_root(c, localPackagesDir!);
            resolve_ctx_set_registry(c, registryUrl ?? "");
            foreach (var (n, r) in reqs) resolve_ctx_add_req(c, n, r);
            foreach (var (n, r) in overrides) resolve_ctx_add_override(c, n, r);
            foreach (var f in features) resolve_ctx_add_feature(c, f);
            foreach (var (n, v) in locked) resolve_ctx_add_locked(c, n, v);
            foreach (var (n, d) in local) resolve_ctx_add_local(c, n, d);
            foreach (var (s, u) in registries) resolve_ctx_add_registry_scope(c, s, u);

            var res = pkg_resolve(c);
            var items = new List<ResolveItem>();
            var cnt = result_count(res);
            for (var i = 0; i < cnt; i++)
            {
                var nb = new byte[512]; var vb = new byte[128]; var ib = new byte[8192]; var lb = new byte[1 << 20];
                if (result_item(res, i, nb, (UIntPtr)nb.Length, vb, (UIntPtr)vb.Length, ib, (UIntPtr)ib.Length, lb, (UIntPtr)lb.Length) == 1)
                    items.Add(new ResolveItem(CStr(nb), CStr(vb), CStr(ib), CStr(lb)));
            }
            var errPtr = result_error(res);
            var error = errPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(errPtr) : null;
            result_free(res);
            return (items, error);
        }
        finally { resolve_ctx_free(c); }
    }
}
