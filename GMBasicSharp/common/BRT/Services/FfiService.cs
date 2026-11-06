// FILE: Basic.Runtime.Services.FfiService.cs

using System.Collections.Concurrent;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace Basic.Runtime.Services;

public class FfiService : IFfiService
{
    private readonly Dictionary<IntPtr, string> _libraries = new();
    private readonly Dictionary<(IntPtr, int), Delegate> _delegateCache = new();

    /// <summary>Struct annidate (2026-09-29): legge la FOGLIA scalare indicata da un path
    /// punteggiato (es. "I.X") navigando ricorsivamente un'istanza UDT PCodeVM
    /// (<see cref="Dictionary{TKey,TValue}"/> chiavato per nome, con campi UDT-annidati che
    /// sono essi stessi tali dictionary — vedi MakeDefaultUdtInstance/SyntheticUdtDescriptor in
    /// PCodeProcessor.Helpers.cs). Ritorna null se un segmento intermedio manca o non è un
    /// dictionary (istanza mai popolata a quel livello).</summary>
    private static object? GetFfiLeafValue(System.Collections.IDictionary? dict, string path)
    {
        object? cur = dict;
        foreach (var seg in path.Split('.'))
        {
            if (cur is not System.Collections.IDictionary d || !d.Contains(seg)) return null;
            cur = d[seg];
        }
        return cur;
    }

    /// <summary>Struct annidate (2026-09-29): scrive la FOGLIA scalare indicata da un path
    /// punteggiato (<paramref name="namePath"/>) dentro un'istanza UDT PCodeVM in costruzione
    /// (il ritorno di una FFI a UDT annidato), creando i dictionary intermedi mancanti man mano
    /// che scende. Ad ogni livello intermedio nuovo scrive ANCHE la chiave "__field_&lt;SymbolId&gt;"
    /// (fallback già usato altrove nel motore, vedi PCodeProcessor.cs PCodeFieldNameFromOffset):
    /// un livello ricostruito qui non ha un vero __class__/FieldNames, quindi OP_LOAD_MEMBER per
    /// `r.I` (il livello intermedio, non solo la foglia finale `r.I.X`) deve risolvere tramite
    /// questo fallback per offset — <paramref name="symbolIdPath"/> porta il SymbolId di OGNI
    /// segmento, stesso ordine/lunghezza di <paramref name="namePath"/>.Split('.').</summary>
    private static void SetFfiLeafValue(Dictionary<string, object?> root, string namePath, int[] symbolIdPath, double value)
        => SetFfiLeafValue(root, namePath, symbolIdPath, (object?)value);

    private static void SetFfiLeafValue(Dictionary<string, object?> root, string namePath, int[] symbolIdPath, object? value)
    {
        var names = namePath.Split('.');
        var cur = root;
        for (var i = 0; i < names.Length - 1; i++)
        {
            if (cur.TryGetValue(names[i], out var sub) && sub is Dictionary<string, object?> subDict)
                cur = subDict;
            else
            {
                var next = new Dictionary<string, object?>();
                cur[names[i]] = next;
                if (i < symbolIdPath.Length) cur["__field_" + symbolIdPath[i]] = next;
                cur = next;
            }
        }
        cur[names[^1]] = value;
        if (symbolIdPath.Length > 0) cur["__field_" + symbolIdPath[^1]] = value;
    }

    // Preload statico delle librerie FFI dei pacchetti REQUIRE risolti: condiviso tra TUTTE
    // le istanze (il PCodeVm/IL creano un host con un FfiService fresco a ogni run). La
    // chiave è il basename (senza prefisso lib- né estensione, case-insensitive): così
    // DECLARE ... LIB "nome" risolve la lib di pacchetto anche se il file reale è
    // lib<nome>.dylib in un path assoluto di cache (match per basename in LoadLibrary).
    private static readonly ConcurrentDictionary<string, IntPtr> PreloadHandles = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, string> Preloads = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Carica una libreria di pacchetto (path assoluto) e la registra per il match
    /// per basename. Idempotente.</summary>
    public static IntPtr Preload(string absPath)
    {
        if (PreloadHandles.TryGetValue(absPath, out var existing)) return existing;
        if (NativeLibrary.TryLoad(absPath, out var handle))
        {
            PreloadHandles[absPath] = handle;
            Preloads[BasenameKey(absPath)] = absPath;
            return handle;
        }
        throw new FileNotFoundException($"Library not found: {absPath}");
    }

    private static string BasenameKey(string path)
    {
        var name = Path.GetFileName(path);
        if (name.StartsWith("lib", StringComparison.OrdinalIgnoreCase)) name = name[3..];
        var dot = name.IndexOf('.');
        if (dot >= 0) name = name[..dot];
        return name;
    }

    public IntPtr LoadLibrary(string path)
    {
        if (NativeLibrary.TryLoad(path, out var handle))
        {
            _libraries[handle] = path;
            return handle;
        }
        foreach (var ext in new[] { ".dylib", ".so", ".dll" })
        {
            if (NativeLibrary.TryLoad(path + ext, out handle))
            { _libraries[handle] = path + ext; return handle; }
        }
        // Resolve relative to assembly
        var asmPath = Assembly.GetExecutingAssembly().Location;
        var baseDir = Path.GetDirectoryName(asmPath)!;
        for (int i = 0; i < 5; i++)
        {
            var fullPath = Path.Combine(baseDir, path);
            if (NativeLibrary.TryLoad(fullPath, out handle))
            { _libraries[handle] = fullPath; return handle; }
            foreach (var ext in new[] { ".dylib", ".so", ".dll" })
            {
                if (NativeLibrary.TryLoad(fullPath + ext, out handle))
                { _libraries[handle] = fullPath + ext; return handle; }
            }
            baseDir = Path.GetDirectoryName(baseDir);
            if (string.IsNullOrEmpty(baseDir)) break;
        }

        // Auto-discovery `libs/` (drop-in FFI, 2026-09-22): una libreria messa in <dir>/libs/
        // accanto al programma è risolta SENZA pacchetto REQUIRE. Walk-up dal CWD (il percorso
        // del programma, §9.1), nearest-first; pattern <name><ext> e lib<base><ext>. Kill-switch
        // GMB_FFI_NO_LIBS_SCAN=1 per i gate legacy. Parità: stessa logica in gmb_ffi_dl.c (C).
        if (!string.Equals(Basic.Providers.GmbEnv.Get("GMB_FFI_NO_LIBS_SCAN"), "1", StringComparison.Ordinal))
        {
            var libsPath = FindInLibsFolders(path);
            if (libsPath != null)
            {
                var libsHandle = Preload(libsPath);   // idempotente + registra per basename
                _libraries[libsHandle] = libsPath;
                return libsHandle;
            }
        }

        // Auto-discovery cache pacchetti GMB (~/.gmbasic/packages): un pacchetto REQUIRE che espone
        // una lib FFI (ffi.libs) diventa risolvibile senza preload esplicito. Override cache
        // GMB_PKG_CACHE. Kill-switch GMB_FFI_NO_PKGCACHE_SCAN=1. Parità: gmb_ffi_dl.c (C).
        var pkgPath = FindInPackageCache(path);
        if (pkgPath != null)
        {
            var pkgHandle = Preload(pkgPath);
            _libraries[pkgHandle] = pkgPath;
            return pkgHandle;
        }

        // Librerie pre-caricate dei pacchetti REQUIRE: match per basename (DECLARE LIB "nome"
        // risolve la lib di pacchetto anche se sta in un path assoluto di cache).
        var key = BasenameKey(path);
        if (Preloads.TryGetValue(key, out var preloaded) && PreloadHandles.TryGetValue(preloaded, out var preloadedHandle))
        {
            _libraries[preloadedHandle] = preloaded;
            return preloadedHandle;
        }

        // (4) DOWNLOAD ON-DEMAND (parità col runtime C: `gmb_ffi_dl.c` → `gmb_libs_autoensure`).
        // Le nostre lib FFI (gruppo `ffi` nell'indice: libgmbamqp, libgmbgrpc, …, libgmbmedia)
        // NON stanno nel repo: sono su c-vision/GMBasicModules, un asset per file, e si scaricano
        // nella cache (`~/.gmbasic/libs/<rid>`) la prima volta che un programma le chiede. Senza
        // questo passo un `DECLARE … LIB "gmbamqp"` su una macchina senza cache falliva con
        // "Library not found" (il C invece le scaricava già: divergenza di comportamento).
        // Rete SOLO se il file compare nell'indice per questo RID: per le lib di sistema o di
        // pacchetto non si tenta niente.
        var ffiName = Path.GetFileName(path);
        var ffiBase = ffiName.StartsWith("lib", StringComparison.OrdinalIgnoreCase) ? ffiName[3..] : ffiName;
        if (TryDownloadFromManifest(ffiName, ffiBase))
        {
            var downloaded = FindInLibsFolders(path);
            if (downloaded != null)
            {
                var dlHandle = Preload(downloaded);
                _libraries[dlHandle] = downloaded;
                return dlHandle;
            }
        }

        throw new FileNotFoundException($"Library not found: {path}");
    }

    /// <summary>RID della piattaforma corrente (osx-arm64/osx-x64/win-x64/linux-*).</summary>
    private static string PlatformRid()
        => RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
            ? (RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64")
            : RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win-x64"
            : RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";

    // ── download on-demand delle NOSTRE lib FFI (indice per-file) ───────────────────────────────
    // Parità col runtime C (`gmb_libs_autoensure`): la lib si scarica dalla release
    // (c-vision/GMBasicModules, gruppo `ffi`) nella cache `~/.gmbasic/libs/<rid>/`. Il nome-file
    // deve stare nell'indice per il RID: così non si fa rete per le lib di sistema (libc, …).
    private static readonly object ManifestFilesLock = new();
    private static HashSet<string>? _manifestFiles;

    /// <summary>Nomi-file dell'indice per il RID corrente (una sola lettura riuscita). null quando
    /// l'indice non è leggibile: in quel caso non si tenta nessun download (comportamento di prima).</summary>
    private static HashSet<string>? ManifestFileNames()
    {
        lock (ManifestFilesLock)
        {
            if (_manifestFiles != null) return _manifestFiles;
            try
            {
                var m = CommonLib.LibsResolver.LoadManifestAsync().GetAwaiter().GetResult();
                var rid = PlatformRid();
                if (m.Rids != null && m.Rids.TryGetValue(rid, out var r) && r.Files != null && r.Files.Count > 0)
                    _manifestFiles = new HashSet<string>(r.Files.Keys, StringComparer.Ordinal);
            }
            catch { /* indice assente/illeggibile: nessun download, la ricerca locale resta */ }
            return _manifestFiles;
        }
    }

    /// <summary>Scarica (se serve) la lib FFI <paramref name="baseName"/> dalla release per-file.
    /// Ritorna true se il file è ora in cache (o lo era già con sha corretto).</summary>
    private static bool TryDownloadFromManifest(string name, string baseName)
    {
        var files = ManifestFileNames();
        if (files == null || files.Count == 0) return false;
        var rid = PlatformRid();
        foreach (var ext in new[] { ".dylib", ".so", ".dll" })
        {
            foreach (var cand in new[] { "lib" + baseName + ext, name + ext })
            {
                if (!files.Contains(cand)) continue;   // non è una lib distribuita: nessuna rete
                try
                {
                    CommonLib.LibsResolver.EnsureAsync(cand, rid).GetAwaiter().GetResult();
                    return true;
                }
                catch { /* rete/sha: si prova il nome successivo, poi si prosegue col locale */ }
            }
        }
        return false;
    }

    /// <summary>Cerca una lib in una cartella `libs/`: piatto (&lt;libs&gt;/&lt;name&gt;) e una live sotto
    /// (&lt;libs&gt;/&lt;nome&gt;/&lt;name&gt;). Ritorna il primo match o null.</summary>
    private static string? TryFindLibIn(string libsRoot, string name, string baseName)
    {
        if (string.IsNullOrEmpty(libsRoot) || !Directory.Exists(libsRoot)) return null;
        foreach (var ext in new[] { ".dylib", ".so", ".dll" })
        {
            var cand = Path.Combine(libsRoot, name + ext);
            if (File.Exists(cand)) return cand;
            var candLib = Path.Combine(libsRoot, "lib" + baseName + ext);
            if (!string.Equals(candLib, cand, StringComparison.Ordinal) && File.Exists(candLib)) return candLib;
        }
        foreach (var sub in Directory.EnumerateDirectories(libsRoot))
        {
            foreach (var ext in new[] { ".dylib", ".so", ".dll" })
            {
                var cand = Path.Combine(sub, name + ext);
                if (File.Exists(cand)) return cand;
                var candLib = Path.Combine(sub, "lib" + baseName + ext);
                if (!string.Equals(candLib, cand, StringComparison.Ordinal) && File.Exists(candLib)) return candLib;
            }
        }
        return null;
    }

    /// <summary>Auto-discovery librerie: cerca `<libs>/<name>` (piatto) o `<libs>/<nome>/<name>`
    /// (1) risalendo dal CWD (nearest-first), poi (2) in ASSOLUTO dall'install GMBasic
    /// (`&lt;root&gt;/libs` = nostre lib interne, `&lt;root&gt;/Resources/libs/&lt;rid&gt;` = builtin) —
    /// così qualunque programma BASIC, ovunque creato, trova le librerie. Usato da
    /// <see cref="LoadLibrary"/>.</summary>
    private static string? FindInLibsFolders(string path)
    {
        var name = Path.GetFileName(path);
        if (string.IsNullOrEmpty(name)) return null;
        var baseName = name.StartsWith("lib", StringComparison.OrdinalIgnoreCase) ? name[3..] : name;
        // (0) BUNDLE del compilatore (locale-prima, parità con gmb_ffi_dl.c): la lib copiata
        // da `--libs` sta in <exe>/libs/<rid>/ e deve vincere su CWD/cache.
        {
            var exeDir = AppContext.BaseDirectory;
            var rid = PlatformRid();
            var local = TryFindLibIn(Path.Combine(exeDir, "libs", rid), name, baseName)
                        ?? TryFindLibIn(Path.Combine(exeDir, "libs"), name, baseName)
                        ?? TryFindLibIn(exeDir, name, baseName)
                        ?? TryFindLibIn(Path.Combine(exeDir, "..", "libs", rid), name, baseName);
            if (local != null) return local;
        }
        // (1) risalendo dal CWD (nearest-first)
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        for (int i = 0; i < 5 && dir != null; i++)
        {
            var cand = TryFindLibIn(Path.Combine(dir.FullName, "libs"), name, baseName)
                       ?? TryFindLibIn(Path.Combine(dir.FullName, "shared", "libs"), name, baseName);
            if (cand != null) return cand;
            dir = dir.Parent;
        }
        // (2) ASSOLUTO dall'install GMBasic (indipendente dalla posizione del programma)
        var fromInstall = TryFindLibIn(CommonLib.GmbiPaths.RepoLibsDir, name, baseName)
            ?? TryFindLibIn(CommonLib.GmbiPaths.LibsCacheDir(PlatformRid()), name, baseName);
        if (fromInstall != null) return fromInstall;
        // (3) FALLBACK: lib PIATTA nella cartella del programma (CWD): mio.bas + libfoo.dylib.
        var cwdNow = Directory.GetCurrentDirectory();
        foreach (var ext in new[] { ".dylib", ".so", ".dll" })
        {
            var cand = Path.Combine(cwdNow, name + ext);
            if (File.Exists(cand)) return cand;
            var candLib = Path.Combine(cwdNow, "lib" + baseName + ext);
            if (!string.Equals(candLib, cand, StringComparison.Ordinal) && File.Exists(candLib)) return candLib;
        }
        return null;
    }

    /// <summary>Cerca la lib di un pacchetto nella cache GMB
    /// (<c>&lt;root&gt;/packages/&lt;pkg&gt;/&lt;ver&gt;/lib/&lt;platform&gt;/&lt;name&gt;&lt;ext&gt;</c> o
    /// <c>lib&lt;base&gt;&lt;ext&gt;</c>). Default cache: GmbiPaths.PackagesDir (&lt;root&gt;/packages).
    /// Override cache: GMB_PKG_CACHE. Kill-switch: GMB_FFI_NO_PKGCACHE_SCAN=1.</summary>
    private static string? FindInPackageCache(string path)
    {
        if (string.Equals(Basic.Providers.GmbEnv.Get("GMB_FFI_NO_PKGCACHE_SCAN"), "1", StringComparison.Ordinal)) return null;
        var name = Path.GetFileName(path);
        if (string.IsNullOrEmpty(name)) return null;
        var baseName = name.StartsWith("lib", StringComparison.OrdinalIgnoreCase) ? name[3..] : name;
        /* ORDINE (direttiva utente): installazione LOCALE dell'app (`<dir dell'eseguibile>/packages`,
         * modello node_modules) PRIMA, poi la cache (GMB_PKG_CACHE o <checkout|utente>/packages).
         * Stessa regola del compilatore e del loader C: si compila con un .inc e si carica la
         * dylib DELLO STESSO pacchetto. */
        var roots = new List<string>();
        var envCache = Basic.Providers.GmbEnv.Get("GMB_PKG_CACHE");
        var local = CommonLib.GmbiPaths.LocalPackagesDir;
        if (Directory.Exists(local)) roots.Add(local);
        roots.Add(string.IsNullOrEmpty(envCache) ? CommonLib.GmbiPaths.PackagesDir : envCache);
        var plat = RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
            ? (RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64")
            : RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win-x64"
            : RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
        foreach (var cache in roots)
        {
            if (!Directory.Exists(cache)) continue;
            foreach (var pkgDir in Directory.EnumerateDirectories(cache))
            {
                foreach (var verDir in Directory.EnumerateDirectories(pkgDir))
                {
                    var libDir = Path.Combine(verDir, "lib", plat);
                    if (!Directory.Exists(libDir)) continue;
                    foreach (var ext in new[] { ".dylib", ".so", ".dll" })
                    {
                        var cand = Path.Combine(libDir, name + ext);
                        if (File.Exists(cand)) return cand;
                        var candLib = Path.Combine(libDir, "lib" + baseName + ext);
                        if (File.Exists(candLib)) return candLib;
                    }
                }
            }
        }
        return null;
    }

    public void FreeLibrary(IntPtr handle)
    {
        _libraries.Remove(handle);
        NativeLibrary.Free(handle);
    }

    public IntPtr GetFunction(IntPtr library, string name)
    {
        if (NativeLibrary.TryGetExport(library, name, out var ptr))
            return ptr;
        throw new EntryPointNotFoundException($"Function '{name}' not found in library.");
    }

    public object? CallFunction(IntPtr function, object?[] args)
    {
        if (function == IntPtr.Zero) return null;
        var argCount = args.Length;
        var key = (function, argCount);
        if (!_delegateCache.TryGetValue(key, out var del))
        {
            del = CreateDelegate(function, argCount);
            _delegateCache[key] = del;
        }
        var dArgs = new object?[argCount];
        for (int i = 0; i < argCount; i++)
            dArgs[i] = Convert.ToDouble(args[i] ?? 0);
        return del.DynamicInvoke(dArgs);
    }

    private readonly Dictionary<(IntPtr, int, byte), Delegate> _typedDelegateCache = new();

    /// <summary>Chiamata FFI tipizzata (OP_CALL_FFI_T): kind omogeneo per TUTTI i
    /// parametri e il ritorno — 1 = int64 nativo (LONG), 0 = double. Il delegate CLR
    /// viene costruito con la firma reale (int64/double) così l'ABI corrisponde al
    /// simbolo C exportato (`long vec_add(long,long)`): il contratto all-double di
    /// CallFunction leggeva registri sbagliati (vec_add(21,21)→21 su ARM64).</summary>
    public object? CallFunctionTyped(IntPtr function, object?[] args, byte kind)
    {
        if (function == IntPtr.Zero) return null;
        var key = (function, args.Length, kind);
        if (!_typedDelegateCache.TryGetValue(key, out var del))
        {
            var paramTypes = Enumerable.Repeat(kind == 1 ? typeof(long) : typeof(double), args.Length).ToArray();
            del = CreateDelegate(function, kind == 1 ? typeof(long) : typeof(double), paramTypes);
            _typedDelegateCache[key] = del;
        }
        var dArgs = new object?[args.Length];
        for (int i = 0; i < args.Length; i++)
        {
            // Cast verso object ESPLICITO: il ternario long/double senza il cast unifica
            // il tipo a double (promozione numerica C#) → DynamicInvoke falliva con
            // "Object of type 'System.Double' cannot be converted to 'System.Int64'".
            dArgs[i] = kind == 1 ? (object)Convert.ToInt64(args[i] ?? 0L) : Convert.ToDouble(args[i] ?? 0.0);
        }
        var result = del.DynamicInvoke(dArgs);
        // BASIC numerico uniforme: il valore (anche int64) torna alla gamba BASIC come double
        return kind == 1 ? Convert.ToDouble(result ?? 0L) : result;
    }

    private readonly Dictionary<(IntPtr, int, int, long, long, long, long, string), Delegate> _typedArrDelegateCache = new();
    // Struct/UDT (2026-09-28): cache dei VALUE TYPE CLR costruiti a runtime per una "forma"
    // UDT (kind + numero di campi) — riusata sia per parametri sia per ritorni, e tra chiamate
    // diverse con la stessa forma (es. due UDT distinti con lo stesso numero di campi/kind
    // condividono il tipo — irrilevante, sono blittable puri, nessun nome semantico coinvolto).
    private readonly Dictionary<string, Type> _udtShapeTypeCache = new();
    /// <summary>Kind misto (2026-09-29): <paramref name="fieldIsInt"/> è il kind PER CAMPO (non
    /// più un unico <c>kind</c> uniforme) — un parametro passa sempre un array uniforme (stesso
    /// valore ripetuto, garantito dal guard di compilazione), un RITORNO può passare un mix. La
    /// chiave di cache è la SEQUENZA dei kind (es. "ID" per double poi intero), non solo il
    /// conteggio — due forme con lo stesso numero di campi ma ordine diverso sono TIPI CLR
    /// diversi (il .NET marshaler li tratta diversamente in fase di P/Invoke).</summary>
    private Type GetOrBuildUdtShapeType(bool[] fieldIsInt)
        => GetOrBuildUdtShapeType(null, fieldIsInt, null);

    /// <summary>Costruisce il VALUE TYPE CLR che rappresenta uno struct C per libffi/P-Invoke:
    /// layout ESPLICITO con gli offset calcolati dal compilatore (parità col layout C) e tipi
    /// reali per campo (2026-11-06, S1). Senza <paramref name="types"/> ricade sulla forma
    /// storica a 8 byte (tutti double/int64 consecutivi), usata dai chiamanti vecchi.</summary>
    private Type GetOrBuildUdtShapeType(byte[]? types, bool[] fieldIsInt, int[]? offsets)
    {
        var shapeKey = types != null
            ? "T" + string.Join(",", types) + "|O" + (offsets != null ? string.Join(",", offsets) : "")
            : "K" + string.Concat(fieldIsInt.Select(b => b ? 'I' : 'D'));
        if (_udtShapeTypeCache.TryGetValue(shapeKey, out var cached)) return cached;
        var asmName = new AssemblyName("FfiUdtShapes");
        var asmBuilder = AssemblyBuilder.DefineDynamicAssembly(asmName, AssemblyBuilderAccess.Run);
        var modBuilder = asmBuilder.DefineDynamicModule("Main");
        var typeBuilder = modBuilder.DefineType(
            $"FfiUdtShape_{(types != null ? "L" : "K")}{Math.Abs(shapeKey.GetHashCode())}",
            TypeAttributes.Public | TypeAttributes.ExplicitLayout | TypeAttributes.Sealed, typeof(ValueType));
        var n = types?.Length ?? fieldIsInt.Length;
        for (var i = 0; i < n; i++)
        {
            var clr = types != null ? ClrTypeOfFfiCode(types[i]) : (fieldIsInt[i] ? typeof(long) : typeof(double));
            var fb = typeBuilder.DefineField($"F{i}", clr, FieldAttributes.Public);
            if (types != null && offsets != null && i < offsets.Length)
                fb.SetCustomAttribute(new CustomAttributeBuilder(
                    typeof(System.Runtime.InteropServices.FieldOffsetAttribute).GetConstructor([typeof(int)])!,
                    new object[] { offsets[i] }));
        }
        var built = typeBuilder.CreateType()!;
        _udtShapeTypeCache[shapeKey] = built;
        return built;
    }

    /// <summary>Converte un valore grezzo (double/long/string) nel tipo CLR del campo struct C
    /// indicato dal codice tipo FFI.</summary>
    private static object ConvertFfiValue(object? raw, byte code)
    {
        switch (code)
        {
            case 1:  return Convert.ToInt64(raw ?? 0L);
            case 2:  return Convert.ToSByte(raw ?? 0);
            case 3:  return Convert.ToByte(raw ?? 0);
            case 4:  return Convert.ToInt16(raw ?? 0);
            case 5:  return Convert.ToUInt16(raw ?? 0);
            case 6:  return Convert.ToInt32(raw ?? 0);
            case 7:  return Convert.ToUInt32(raw ?? 0);
            case 8:  return Convert.ToSingle(raw ?? 0.0);
            case 9:  return IntPtr.Zero;
            case 10: return Convert.ToUInt64(raw ?? 0UL);
            default: return Convert.ToDouble(raw ?? 0.0);
        }
    }

    /// <summary>Forma di uno struct UDT serializzata dal compilatore IL:
    /// <c>size|nome:tipo:offset;nome:tipo:offset;…</c> (nome = path punteggiato per i campi
    /// annidati). Serve al backend IL, dove il tipo CLR del dialetto (INTEGER/LONG a 64 bit)
    /// non coincide col layout C: il runtime costruisce un tipo-ombra col layout giusto e
    /// converte campo per campo (2026-11-06, S1).</summary>
    private static (int Size, (string Path, byte Code, int Off)[] Fields) ParseUdtShape(string s)
    {
        var bar = s.IndexOf('|');
        var size = int.Parse(bar < 0 ? s : s[..bar]);
        var rest = bar < 0 ? "" : s[(bar + 1)..];
        if (rest.Length == 0) return (size, Array.Empty<(string, byte, int)>());
        var parts = rest.Split(';', StringSplitOptions.RemoveEmptyEntries);
        var fields = new (string, byte, int)[parts.Length];
        for (var i = 0; i < parts.Length; i++)
        {
            var c = parts[i].Split(':');
            fields[i] = (c[0], byte.Parse(c[1]), int.Parse(c[2]));
        }
        return (size, fields);
    }

    private static FfiUdtShape ShapeFromString(string s)
    {
        var (size, flds) = ParseUdtShape(s);
        var names = new string[flds.Length];
        var types = new byte[flds.Length];
        var offs = new int[flds.Length];
        var isInt = new bool[flds.Length];
        var sym = new int[flds.Length][];
        for (var i = 0; i < flds.Length; i++)
        {
            names[i] = flds[i].Path; types[i] = flds[i].Code; offs[i] = flds[i].Off;
            isInt[i] = types[i] != 0 && types[i] != 8;
            sym[i] = Array.Empty<int>();
        }
        return new FfiUdtShape(names, sym, isInt, types, offs, size);
    }

    private static Type BuildFfiTypeFromShape((string Path, byte Code, int Off)[] fields)
    {
        var types = new byte[fields.Length];
        var offs = new int[fields.Length];
        for (var i = 0; i < fields.Length; i++) { types[i] = fields[i].Code; offs[i] = fields[i].Off; }
        var isInt = new bool[fields.Length];
        for (var i = 0; i < fields.Length; i++) isInt[i] = types[i] != 0 && types[i] != 8;
        return GetOrBuildUdtShapeTypeStatic(types, isInt, offs);
    }

    private static readonly Dictionary<string, Type> _staticShapeCache = new();
    private static Type GetOrBuildUdtShapeTypeStatic(byte[] types, bool[] fieldIsInt, int[] offsets)
    {
        var key = "T" + string.Join(",", types) + "|O" + string.Join(",", offsets);
        if (_staticShapeCache.TryGetValue(key, out var cached)) return cached;
        var asmBuilder = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("FfiUdtShapesS"), AssemblyBuilderAccess.Run);
        var modBuilder = asmBuilder.DefineDynamicModule("M");
        var tb = modBuilder.DefineType("FfiUdtShape_" + Math.Abs(key.GetHashCode()),
            TypeAttributes.Public | TypeAttributes.ExplicitLayout | TypeAttributes.Sealed, typeof(ValueType));
        for (var i = 0; i < types.Length; i++)
        {
            var fb = tb.DefineField($"F{i}", ClrTypeOfFfiCode(types[i]), FieldAttributes.Public);
            fb.SetCustomAttribute(new CustomAttributeBuilder(
                typeof(System.Runtime.InteropServices.FieldOffsetAttribute).GetConstructor([typeof(int)])!,
                new object[] { offsets[i] }));
        }
        var built = tb.CreateType()!;
        _staticShapeCache[key] = built;
        return built;
    }

    private static object? GetPathValue(object? src, string path)
    {
        foreach (var seg in path.Split('.'))
        {
            if (src is null) return null;
            var f = src.GetType().GetField(seg, System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
            if (f is null) return null;
            src = f.GetValue(src);
        }
        return src;
    }

    private static void SetPathValue(object root, string[] segs, int idx, object? value)
    {
        var f = root.GetType().GetField(segs[idx], System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance)!;
        if (idx == segs.Length - 1)
        {
            var ft = f.FieldType;
            object? v = ft == typeof(string) ? (value as string ?? Convert.ToString(value)) : Convert.ChangeType(value ?? 0, ft);
            f.SetValue(root, v);
            return;
        }
        var child = f.GetValue(root)!;
        SetPathValue(child, segs, idx + 1, value);
        f.SetValue(root, child);
    }

    private static object ConvertToFfiStruct(object src, (string Path, byte Code, int Off)[] fields, Type ffiType, List<IntPtr> strPtrs)
    {
        var dst = Activator.CreateInstance(ffiType)!;
        for (var i = 0; i < fields.Length; i++)
        {
            var raw = GetPathValue(src, fields[i].Path);
            object? val;
            if (fields[i].Code == 9)
            {
                var sp = Marshal.StringToCoTaskMemUTF8(Convert.ToString(raw) ?? "");
                strPtrs.Add(sp);
                val = sp;
            }
            else val = ConvertFfiValue(raw, fields[i].Code);
            ffiType.GetField($"F{i}")!.SetValue(dst, val);
        }
        return dst;
    }

    private static object ConvertFromFfiStruct(object ffi, (string Path, byte Code, int Off)[] fields, Type basicType)
    {
        var dst = Activator.CreateInstance(basicType)!;
        for (var i = 0; i < fields.Length; i++)
        {
            var raw = ffi.GetType().GetField($"F{i}")!.GetValue(ffi);
            object? val = fields[i].Code == 9
                ? (raw is IntPtr ip ? (Marshal.PtrToStringUTF8(ip) ?? "") : (raw as string ?? ""))
                : Convert.ToDouble(raw ?? 0.0);
            SetPathValue(dst, fields[i].Path.Split('.'), 0, val);
        }
        return dst;
    }

    /// <summary>Scrive un elemento di array-di-UDT in memoria nativa, campo per campo, secondo i
    /// codici tipo del layout C (S3, 2026-11-06). Evita del tutto il marshalling di struct a
    /// layout esplicito (che causava SIGBUS passando un array gestito pinnato a libffi).</summary>
    private static void WriteFfiElem(IntPtr dst, FfiUdtShape shape, object? src, List<IntPtr> strPtrs)
    {
        var d = src as System.Collections.IDictionary;
        for (var i = 0; i < shape.FieldNames.Length; i++)
        {
            object? raw = d != null ? GetFfiLeafValue(d, shape.FieldNames[i]) : GetPathValue(src, shape.FieldNames[i]);
            var code = shape.FieldTypes![i];
            var p = dst + shape.FieldOffsets![i];
            switch (code)
            {
                case 0: Marshal.WriteInt64(p, BitConverter.DoubleToInt64Bits(Convert.ToDouble(raw ?? 0.0))); break;
                case 1:
                case 10: Marshal.WriteInt64(p, Convert.ToInt64(raw ?? 0L)); break;
                case 2: Marshal.WriteByte(p, (byte)Convert.ToSByte(raw ?? 0)); break;
                case 3: Marshal.WriteByte(p, Convert.ToByte(raw ?? 0)); break;
                case 4:
                case 5: Marshal.WriteInt16(p, Convert.ToInt16(raw ?? 0)); break;
                case 6:
                case 7: Marshal.WriteInt32(p, Convert.ToInt32(raw ?? 0)); break;
                case 8: Marshal.WriteInt32(p, BitConverter.SingleToInt32Bits(Convert.ToSingle(raw ?? 0.0))); break;
                case 9:
                    var sp = Marshal.StringToCoTaskMemUTF8(Convert.ToString(raw) ?? "");
                    strPtrs.Add(sp);
                    Marshal.WriteIntPtr(p, sp);
                    break;
            }
        }
    }

    private static object?[] ReadFfiElem(IntPtr src, FfiUdtShape shape)
    {
        var vals = new object?[shape.FieldNames.Length];
        for (var i = 0; i < vals.Length; i++)
        {
            var code = shape.FieldTypes![i];
            var p = src + shape.FieldOffsets![i];
            vals[i] = code switch
            {
                0 => BitConverter.Int64BitsToDouble(Marshal.ReadInt64(p)),
                1 or 10 => Marshal.ReadInt64(p),
                2 or 3 => (double)Marshal.ReadByte(p),
                4 or 5 => (double)Marshal.ReadInt16(p),
                6 or 7 => (double)Marshal.ReadInt32(p),
                8 => (double)BitConverter.Int32BitsToSingle(Marshal.ReadInt32(p)),
                9 => Marshal.PtrToStringUTF8(Marshal.ReadIntPtr(p)) ?? "",
                _ => 0.0,
            };
        }
        return vals;
    }

    /// <summary>Costruisce un elemento struct-ombra (layout C) da una struct del dialetto (IL) o
    /// da un Dictionary (PCode), per gli array di UDT (S3, 2026-11-06).</summary>
    private static object BuildFfiElem(object? src, FfiUdtShape shape, Type ffiElem, List<IntPtr> strPtrs)
    {
        var dst = Activator.CreateInstance(ffiElem)!;
        var d = src as System.Collections.IDictionary;
        for (var i = 0; i < shape.FieldNames.Length; i++)
        {
            object? raw = d != null ? GetFfiLeafValue(d, shape.FieldNames[i]) : GetPathValue(src, shape.FieldNames[i]);
            object? val;
            var code = shape.FieldTypes![i];
            if (code == 9)
            {
                var sp = Marshal.StringToCoTaskMemUTF8(Convert.ToString(raw) ?? "");
                strPtrs.Add(sp);
                val = sp;
            }
            else val = ConvertFfiValue(raw, code);
            ffiElem.GetField($"F{i}")!.SetValue(dst, val);
        }
        return dst;
    }

    /// <summary>Tipo CLR che rappresenta un campo struct C con lo stesso layout (codici tipo
    /// condivisi col bytecode: 0=double 1=int64 2=int8 3=uint8 4=int16 5=uint16 6=int32
    /// 7=uint32 8=float 9=pointer 10=uint64).</summary>
    private static Type ClrTypeOfFfiCode(byte code) => code switch
    {
        1 => typeof(long), 2 => typeof(sbyte), 3 => typeof(byte),
        4 => typeof(short), 5 => typeof(ushort), 6 => typeof(int), 7 => typeof(uint),
        8 => typeof(float), 9 => typeof(IntPtr), 10 => typeof(ulong), _ => typeof(double),
    };

    /// <summary>Come <see cref="CallFunctionTyped"/>, per firme FFI con almeno un parametro
    /// array BYREF (2026-09-28, AiWiki Bugs/(open) - FFI-Marshalling-Limitato...md punto 1):
    /// <paramref name="arrayMask"/> bit i settato → <c>args[i]</c> è un array da passare come
    /// puntatore. Due casi, a seconda di COME il chiamante rappresenta gli array BASIC:
    /// <list type="bullet">
    /// <item>IL backend: <c>args[i]</c> è già un <c>double[]</c>/<c>long[]</c> CLR primitivo
    /// (blittable) → PIN diretto (GCHandle, zero-copy: il native scrive nel buffer gestito,
    /// visibile subito a BASIC).</item>
    /// <item>PCodeVM: gli array DIM'd sono <c>object[]</c> (elementi boxati — OP_REDIM,
    /// PCodeProcessor.cs, <c>new object?[len]</c>) — NON blittable, pinnarli direttamente
    /// lancerebbe ArgumentException (silenziosamente inghiottita dal try/catch del chiamante,
    /// bug osservato 2026-09-28: il native veniva chiamato con un puntatore mai costruito,
    /// nessun errore visibile, array del chiamante invariato). Serve un marshalling copy-in/
    /// copy-out via scratch buffer double[]/long[], esattamente come il path VM/C
    /// (gmb_vm_core.c) per lo stesso motivo (storage a elementi taggati, non un buffer nativo
    /// contiguo). Il tipo dello scratch (int64 vs double) viene da
    /// <paramref name="arrayElemIntMask"/> (compile-time) — NON dedotto dal primo elemento a
    /// runtime: un `DIM v() AS LONG` boxa comunque i suoi elementi come `double` in PCodeVM,
    /// quindi il peek a runtime sceglieva sistematicamente il tipo sbagliato (bug osservato
    /// 2026-09-28: `1.0` scritto nello scratch, letto dal native come `int64_t` → bit pattern
    /// reinterpretato, ~4.6e18 invece di 1).</item>
    /// </list>
    /// Gli altri slot restano scalari <c>kind</c>-omogenei come in CallFunctionTyped.
    /// <para>STRING (2026-09-28): <paramref name="stringMask"/> bit i settato → <c>args[i]</c> è
    /// una <c>string</c> CLR, convertita in un buffer UTF-8 non gestito
    /// (<see cref="Marshal.StringToCoTaskMemUTF8"/>) e passata come puntatore — valido SOLO
    /// durante la chiamata, liberato nel <c>finally</c> come i GCHandle degli array (stesso
    /// contratto "il nativo copia se vuole trattenerla" già documentato per gli array). Se
    /// <paramref name="retIsString"/>, il valore int64 restituito dalla chiamata è in realtà un
    /// puntatore nativo (spesso un buffer statico/riusato) — copiato IMMEDIATAMENTE in una nuova
    /// <c>string</c> CLR (<see cref="Marshal.PtrToStringUTF8(IntPtr)"/>) e ritornato come tale
    /// (non convertito a double) — mai tenuto vivo oltre la chiamata.</para></summary>
    /// <summary>Numero di elementi "piatti" di un array: per una jagged somma le righe,
    /// per una matrice rettangolare e' il prodotto delle dimensioni (Array.Length).</summary>
    private static int FlatLength(Array a)
        => a is Array[] ja ? ja.Sum(r => r?.Length ?? 0) : a.Length;

    /// <summary>Enumerazione row-major degli elementi: per una jagged concatena le righe,
    /// per una matrice rettangolare `foreach` su un Array multidimensionale e' gia' row-major.</summary>
    private static IEnumerable<object?> FlatValues(Array a)
    {
        if (a is Array[] ja)
        {
            foreach (var row in ja)
                if (row is not null)
                    foreach (var v in row) yield return v;
            yield break;
        }
        foreach (var v in a) yield return v;
    }

    /// <summary>Flatten ricorsivo per array annidati (PCode usa object[] di object[]).</summary>
    private static IEnumerable<object?> FlatValuesAny(object? a)
    {
        if (a is Array arr)
        {
            foreach (var v in arr)
                foreach (var x in FlatValuesAny(v)) yield return x;
            yield break;
        }
        yield return a;
    }

    /// <summary>Copy-out ricorsivo verso array annidati (PCode object[] di object[]).</summary>
    private static void SetFlatValuesAny(object? target, Array src, ref int k)
    {
        if (target is Array arr)
        {
            for (var i = 0; i < arr.Length; i++)
            {
                if (arr.GetValue(i) is Array child) SetFlatValuesAny(child, src, ref k);
                else arr.SetValue(src.GetValue(k++), i);
            }
        }
    }

    public object? CallFunctionTypedArr(IntPtr function, object?[] args, int kind, long arrayMask, long arrayElemIntMask,
        long stringMask, bool retIsString, long byrefScalarMask = 0, long byrefScalarElemIntMask = 0,
        FfiUdtShape?[]? udtParams = null, FfiUdtShape? udtReturn = null,
        Type?[]? udtParamClrTypes = null, Type? udtReturnClrType = null, long scalarKindMask = 0,
        string[]? udtShapeStrs = null, string? udtReturnShapeStr = null)
    {
        if (function == IntPtr.Zero) return null;
        var n = args.Length;
        // Struct/UDT (2026-09-28): la shape (numero di campi per posizione + ritorno, oppure —
        // backend IL — il Type CLR reale) entra nella chiave di cache del delegate — un
        // delegate costruito per una firma diversa non è riusabile.
        // Kind misto (2026-09-29): la chiave include il PATTERN di kind per campo (es. "DI"),
        // non solo il conteggio — "DI" e "ID" hanno lo stesso numero di campi ma sono TIPI CLR
        // diversi (vedi GetOrBuildUdtShapeType).
        static string ShapeSig(FfiUdtShape? s) => s == null ? "" :
            (s.FieldTypes != null
                ? string.Join(",", s.FieldTypes) + "#" + string.Join(",", s.FieldOffsets ?? Array.Empty<int>()) + "#" + s.Size
                : string.Concat(s.FieldIsInt.Select(b => b ? 'I' : 'D')));
        var udtShapeKey = udtParams == null && udtReturn == null && udtParamClrTypes == null && udtReturnClrType == null
            && udtShapeStrs == null && udtReturnShapeStr == null ? "" :
            string.Join(",", (udtParams ?? Array.Empty<FfiUdtShape?>()).Select(ShapeSig)) + "|" + ShapeSig(udtReturn) +
            "|" + string.Join(",", (udtParamClrTypes ?? Array.Empty<Type?>()).Select(t => t?.FullName ?? "")) + "|" + (udtReturnClrType?.FullName ?? "") +
            "|" + string.Join(",", udtShapeStrs ?? Array.Empty<string>()) + "|" + (udtReturnShapeStr ?? "");
        var key = (function, n, kind, arrayMask, stringMask, byrefScalarMask, scalarKindMask, udtShapeKey);
        if (!_typedArrDelegateCache.TryGetValue(key, out var del))
        {
            var paramTypes = new Type[n];
            for (var i = 0; i < n; i++)
                paramTypes[i] = udtShapeStrs != null && i < udtShapeStrs.Length && udtShapeStrs[i] != null
                    && (arrayMask & (1L << i)) == 0
                    // Backend IL con layout C reale (S1, 2026-11-06): tipo-ombra costruito dalla
                    // forma (larghezze C vere) — args[i] viene convertito campo per campo sotto.
                    ? BuildFfiTypeFromShape(ParseUdtShape(udtShapeStrs[i]).Fields)
                    : udtParamClrTypes != null && i < udtParamClrTypes.Length && udtParamClrTypes[i] is { } clrT
                    // Struct/UDT per valore, backend IL (2026-09-29): il tipo CLR REALE del
                    // UDT, già costruito a compile-time (TypeBuilder) — nessuna forma-ombra da
                    // costruire, args[i] è già un'istanza boxed di ESATTAMENTE questo tipo.
                    ? clrT
                    : udtParams != null && i < udtParams.Length && udtParams[i] is { } udtP && (arrayMask & (1L << i)) == 0
                    // Struct/UDT per valore, backend PCodeVM (2026-09-28): un vero VALUE TYPE CLR
                    // costruito a runtime (campi kind-omogenei, layout sequenziale) — .NET
                    // marshalla il passaggio per valore da solo (stessa ABI di un parametro
                    // struct P/Invoke nativo, nessuna classificazione manuale dei registri).
                    // Vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §2.
                    ? GetOrBuildUdtShapeType(udtP.FieldTypes, udtP.FieldIsInt, udtP.FieldOffsets)
                    : (byrefScalarMask & (1L << i)) != 0
                    // BYREF scalare (2026-09-28): un parametro CLR ref BLITTABLE — .NET
                    // marshalla un ref double/ref long verso P/Invoke passando direttamente
                    // l'indirizzo (stesso meccanismo di un vero parametro C `double*`/`int64_t*`
                    // nativo, nessuna copia manuale servita qui). DynamicInvoke scrive il valore
                    // OUT dentro lo stesso slot dArgs[i] dopo la chiamata — vedi sotto.
                    ? ((byrefScalarElemIntMask & (1 << i)) != 0 ? typeof(long) : typeof(double)).MakeByRefType()
                    : (arrayMask & (1L << i)) != 0 || (stringMask & (1L << i)) != 0
                        ? typeof(IntPtr)
                        // Firma mista (2026-10): uno scalare SEMPLICE segue scalarKindMask (per
                        // posizione), non più il 'kind' uniforme — nel caso omogeneo legacy
                        // coincide sempre con 'kind' (il chiamante lo garantisce), zero cambio.
                        : ((scalarKindMask & (1L << i)) != 0 ? typeof(long) : typeof(double));
            var returnType = udtReturnShapeStr != null
                ? BuildFfiTypeFromShape(ParseUdtShape(udtReturnShapeStr).Fields)
                : udtReturnClrType ?? (udtReturn != null
                    ? GetOrBuildUdtShapeType(udtReturn.FieldTypes, udtReturn.FieldIsInt, udtReturn.FieldOffsets)
                    : kind == 1 ? typeof(long) : typeof(double));
            del = CreateDelegate(function, returnType, paramTypes);
            _typedArrDelegateCache[key] = del;
        }
        var dArgs = new object?[n];
        var handles = new GCHandle[n];
        var stringPtrs = new IntPtr[n];
        // Campi STRING di uno struct FFI (2026-11-06, S1): buffer UTF-8 da liberare a fine
        // chiamata (il nativo non li trattiene).
        List<IntPtr>? udtStrPtrs = null;
        // Per gli slot object[] (PCodeVM): scratch buffer copy-in + riferimento all'array
        // originale per il copy-out dopo la chiamata.
        var scratchIsInt = new bool[n];
        var scratchBuf = new Array?[n];
        var scratchOrig = new object[n][];
        // Array CLR TIPIZZATO (double[]/long[]/ulong[]) con kind dell'elemento DIVERSO da
        // quello dichiarato dal parametro FFI: copy-in/copy-out convertito (2026-11-04, AiWiki
        // Bugs/(solved) - FFI-Array-Tipizzato-LONG-IL.md).
        var typedScratch = new Array?[n];
        var typedOrig = new Array?[n];
        // Array BYTE/SBYTE (2026-11-06): il puntatore FFI viaggia a 8 byte per elemento, quindi
        // un byte[] CLR NON puo' essere pinnato direttamente (l'elemento e' largo 1). Si usa uno
        // scratch long[] con i valori 0..255 e il copy-back dopo la chiamata.
        // BYTE/SBYTE: elemento largo 1 byte nella VM ma 8 byte nello scratch FFI
        // (il nativo riceve `int64_t*` con valori 0..255); copy-in/copy-back qui.
        var byteScratch = new long[]?[n];
        // Array di UDT (S3, 2026-11-06): buffer CLR di struct-ombra (layout C) + array
        // originale per il copy-out; i puntatori delle stringhe vivono fino a fine chiamata.
        var udtArrScratch = new Array?[n];
        var udtArrOrig = new Array?[n];
        var udtArrShapes = new FfiUdtShape?[n];
        var udtArrRaw = new IntPtr[n];
        var udtArrElemSize = new int[n];
        var udtArrElemType = new Type?[n];
        var byteOrig = new Array?[n];
        try
        {
            for (var i = 0; i < n; i++)
            {
                if (udtShapeStrs != null && i < udtShapeStrs.Length && udtShapeStrs[i] != null
                    && (arrayMask & (1L << i)) == 0)
                {
                    // Backend IL con layout C reale (S1, 2026-11-06): args[i] è la struct del
                    // dialetto (64-bit ovunque); si converte campo per campo nel tipo-ombra col
                    // layout C e si passa quello.
                    var shapeIn = ParseUdtShape(udtShapeStrs[i]);
                    var ffiTypeIn = BuildFfiTypeFromShape(shapeIn.Fields);
                    dArgs[i] = ConvertToFfiStruct(args[i]!, shapeIn.Fields, ffiTypeIn, udtStrPtrs ??= new List<IntPtr>());
                    continue;
                }
                if (udtParamClrTypes != null && i < udtParamClrTypes.Length && udtParamClrTypes[i] != null)
                {
                    // Struct/UDT per valore, IN, backend IL (2026-09-29): args[i] è GIA'
                    // un'istanza boxed del tipo CLR reale (EmitObjectBoxedArgument l'ha
                    // costruita/boxata a compile-time) — passa inalterata, nessuna copia.
                    dArgs[i] = args[i];
                    continue;
                }
                if ((arrayMask & (1L << i)) != 0
                    && ((udtParams != null && i < udtParams.Length && udtParams[i] is { } s0) 
                        || (udtShapeStrs != null && i < udtShapeStrs.Length && udtShapeStrs[i] != null)))
                {
                    var udtArrShape = (udtParams != null && i < udtParams.Length && udtParams[i] is { } s1)
                        ? s1
                        : ShapeFromString(udtShapeStrs![i]);
                    udtArrShapes[i] = udtArrShape;
                    // Array di UDT (S3, 2026-11-06): n struct contigue nel layout C. La sorgente
                    // può essere un array di struct del dialetto (IL) o di Dictionary (PCode).
                    var ffiElem = GetOrBuildUdtShapeType(udtArrShape.FieldTypes, udtArrShape.FieldIsInt, udtArrShape.FieldOffsets);
                    var srcArr = args[i] as Array;
                    var na = srcArr?.Length ?? 0;
                    // Buffer NON gestito: un array gestito di struct a layout esplicito non è
                    // affidabile da pinnare (SIGBUS in ffi_call, 2026-11-06). Si scrive campo per
                    // campo in memoria nativa e si rilegge dopo la chiamata.
                    var elemSize = udtArrShape.Size > 0 ? udtArrShape.Size : Marshal.SizeOf(ffiElem);
                    var raw = Marshal.AllocHGlobal(Math.Max(1, na * elemSize));
                    var strPtrs = udtStrPtrs ??= new List<IntPtr>();
                    for (var j = 0; j < na; j++)
                        WriteFfiElem(raw + j * elemSize, udtArrShape, srcArr!.GetValue(j), strPtrs);
                    udtArrScratch[i] = null;
                    udtArrOrig[i] = srcArr;
                    udtArrElemSize[i] = elemSize;
                    udtArrRaw[i] = raw;
                    udtArrElemType[i] = ffiElem;
                    dArgs[i] = raw;
                    continue;
                }
                if (udtParams != null && i < udtParams.Length && udtParams[i] is { } udtShapeIn)
                {
                    // Struct/UDT per valore, IN (2026-09-28): args[i] è l'istanza PCodeVM
                    // (Dictionary<string,object?> chiavato per nome campo, vedi
                    // OP_NEW_UDT_INSTANCE). Si costruisce un'istanza BOXED del value type CLR
                    // dinamico e la si popola campo per campo — .NET marshalla il passaggio per
                    // valore da qui in poi, nessuno scratch/pin servito.
                    var structTypeIn = GetOrBuildUdtShapeType(udtShapeIn.FieldTypes, udtShapeIn.FieldIsInt, udtShapeIn.FieldOffsets);
                    var boxedIn = Activator.CreateInstance(structTypeIn)!;
                    var srcDict = args[i] as System.Collections.IDictionary;
                    for (var fi = 0; fi < udtShapeIn.FieldNames.Length; fi++)
                    {
                        var raw = GetFfiLeafValue(srcDict, udtShapeIn.FieldNames[fi]);
                        object fieldVal;
                        if (udtShapeIn.FieldTypes != null)
                        {
                            var code = udtShapeIn.FieldTypes[fi];
                            if (code == 9)
                            {
                                var sp = Marshal.StringToCoTaskMemUTF8(Convert.ToString(raw) ?? "");
                                (udtStrPtrs ??= new List<IntPtr>()).Add(sp);
                                fieldVal = sp;
                            }
                            else fieldVal = ConvertFfiValue(raw, code);
                        }
                        else fieldVal = udtShapeIn.FieldIsInt[fi] ? (object)Convert.ToInt64(raw ?? 0L) : Convert.ToDouble(raw ?? 0.0);
                        structTypeIn.GetField($"F{fi}")!.SetValue(boxedIn, fieldVal);
                    }
                    dArgs[i] = boxedIn;
                    continue;
                }
                if ((stringMask & (1L << i)) != 0)
                {
                    stringPtrs[i] = Marshal.StringToCoTaskMemUTF8((string?)args[i] ?? "");
                    dArgs[i] = stringPtrs[i];
                    continue;
                }
                if ((byrefScalarMask & (1L << i)) != 0)
                {
                    // BYREF scalare: il valore IN va nel box che DynamicInvoke aggiorna in place
                    // per un parametro CLR ref — il tipo (int64 vs double) segue
                    // byrefScalarElemIntMask, non "kind" (kind qui è sempre 1/Int64 perché un
                    // qualunque parametro puntatore forza la firma intera — il BYTE indica solo
                    // cosa c'è DENTRO il puntatore).
                    dArgs[i] = (byrefScalarElemIntMask & (1 << i)) != 0
                        ? (object)Convert.ToInt64(args[i] ?? 0L)
                        : Convert.ToDouble(args[i] ?? 0.0);
                    continue;
                }
                if ((arrayMask & (1L << i)) == 0)
                {
                    // Firma mista (2026-10): scalare semplice, segue scalarKindMask per
                    // posizione (coincide con 'kind' nel caso omogeneo legacy).
                    dArgs[i] = (scalarKindMask & (1L << i)) != 0 ? (object)Convert.ToInt64(args[i] ?? 0L) : Convert.ToDouble(args[i] ?? 0.0);
                    continue;
                }
                switch (args[i])
                {
                    // Solo i tipi CLR a 8 byte (double[]/long[]/ulong[]) — l'unico set che il
                    // guard compile-time ammette come elemento-array FFI (Compiler.Expressions.cs
                    // IsFfiArrayElemType, fix 2026-09-28: INTEGER/BYTE/SBYTE/UINTEGER/SINGLE
                    // mappano a CLR byte[]/short[]/float[]/... — larghezza < 8 byte, pinnarli
                    // direttamente e passarli come gmb_int_t*/double* a 8 byte darebbe garbage).
                    case double[] or long[] or ulong[]
                        or double[,] or long[,] or ulong[,]
                        or double[][] or long[][]:
                    {
                        var arr = (Array)args[i]!;
                        /* KIND dell'elemento (2026-11-04): `arrayElemIntMask` dice se il
                         * parametro nativo è `int64_t*` o `double*`. Un array BASIC non
                         * tipizzato (`DIM a() = {...}`) è SEMPRE `double[]` anche quando il
                         * parametro dichiarato è `AS LONG`: pinnarlo direttamente passava i bit
                         * IEEE-754 al nativo, che li rileggeva come int64 (bug: SAT → -1,
                         * ALLDIFF → 0 su IL, mentre C99/PCode/VM-C convertivano). Stessa cosa
                         * al contrario per un `DIM c(n) AS LONG` verso un parametro double.
                         * Qui si converte in uno scratch del tipo GIUSTO e — nel copy-out —
                         * si riportano le eventuali scritture del nativo nell'array originale. */
                        var wantInt = (arrayElemIntMask & (1L << i)) != 0;
                        var arrIsInt64 = arr is long[] or ulong[] or long[,] or ulong[,] or long[][];
                        /* Matrici (2026-11-06): una matrice CLR rettangolare (`double[,]`) e'
                         * contigua row-major, quindi si pinna direttamente come buffer piatto.
                         * Una jagged (`double[][]`) non e' contigua: va appiattita in scratch.
                         * Il nativo riceve `double*`/`int64_t*` con rows*cols elementi; le
                         * dimensioni restano parametri espliciti (l'ABI C non le passa). */
                        var isJagged = arr is Array[];
                        if (!isJagged && wantInt == arrIsInt64)
                        {
                            handles[i] = GCHandle.Alloc(arr, GCHandleType.Pinned);
                            dArgs[i] = handles[i].AddrOfPinnedObject();
                            break;
                        }
                        {
                            var flatCount = FlatLength(arr);
                            var buf = wantInt
                                ? (Array)new long[flatCount]
                                : new double[flatCount];
                            var j = 0;
                            foreach (var raw in FlatValues(arr))
                            {
                                if (wantInt) ((long[])buf)[j] = Convert.ToInt64(raw ?? 0L);
                                else ((double[])buf)[j] = Convert.ToDouble(raw ?? 0.0);
                                j++;
                            }
                            typedScratch[i] = buf;
                            typedOrig[i] = arr;
                            handles[i] = GCHandle.Alloc(buf, GCHandleType.Pinned);
                            dArgs[i] = handles[i].AddrOfPinnedObject();
                            break;
                        }
                    }
                    case byte[] or sbyte[]:
                    {
                        var srcArr = (Array)args[i]!;
                        var buf = new long[srcArr.Length];
                        for (var j = 0; j < srcArr.Length; j++)
                            buf[j] = Convert.ToInt64(srcArr.GetValue(j) ?? 0L);
                        byteScratch[i] = buf;
                        byteOrig[i] = srcArr;
                        handles[i] = GCHandle.Alloc(buf, GCHandleType.Pinned);
                        dArgs[i] = handles[i].AddrOfPinnedObject();
                        break;
                    }
                    /* PCode (dinamicamente tipizzato) consegna le matrici come `object[,]`
                     * (o jagged `object[][]`) con valori boxati: si appiattiscono in uno
                     * scratch tipizzato come per gli array CLR tipizzati, e il copy-out
                     * riscrive i valori convertiti nella matrice originale. */
                    case object[,] or object[][] or object[,,]:
                    {
                        var arr = (Array)args[i]!;
                        var wantInt2 = (arrayElemIntMask & (1L << i)) != 0;
                        var flatCount2 = FlatLength(arr);
                        var buf2 = wantInt2 ? (Array)new long[flatCount2] : (Array)new double[flatCount2];
                        var j2 = 0;
                        foreach (var raw2 in FlatValues(arr))
                        {
                            if (wantInt2) ((long[])buf2)[j2] = Convert.ToInt64(raw2 ?? 0L);
                            else ((double[])buf2)[j2] = Convert.ToDouble(raw2 ?? 0.0);
                            j2++;
                        }
                        typedScratch[i] = buf2;
                        typedOrig[i] = arr;
                        handles[i] = GCHandle.Alloc(buf2, GCHandleType.Pinned);
                        dArgs[i] = handles[i].AddrOfPinnedObject();
                        break;
                    }
                    /* Rango >= 3 (2026-11-06, S2): una matrice CLR rettangolare di qualunque
                     * rango è contigua row-major come una 2-D. Se l'elemento è a 8 byte e il
                     * kind combacia col parametro nativo si pinna direttamente (zero-copy);
                     * altrimenti si appiattisce in uno scratch tipizzato, con copy-out. */
                    case Array arect when arect.Rank >= 2:
                    {
                        var wantIntR = (arrayElemIntMask & (1L << i)) != 0;
                        var etR = arect.GetType().GetElementType();
                        var isInt64R = etR == typeof(long) || etR == typeof(ulong);
                        var flatCountR = FlatLength(arect);
                        if ((etR == typeof(double) || isInt64R) && wantIntR == isInt64R)
                        {
                            handles[i] = GCHandle.Alloc(arect, GCHandleType.Pinned);
                            dArgs[i] = handles[i].AddrOfPinnedObject();
                            break;
                        }
                        var bufR = wantIntR ? (Array)new long[flatCountR] : new double[flatCountR];
                        var jR = 0;
                        foreach (var rawR in FlatValues(arect))
                        {
                            if (wantIntR) ((long[])bufR)[jR] = Convert.ToInt64(rawR ?? 0L);
                            else ((double[])bufR)[jR] = Convert.ToDouble(rawR ?? 0.0);
                            jR++;
                        }
                        typedScratch[i] = bufR;
                        typedOrig[i] = arect;
                        handles[i] = GCHandle.Alloc(bufR, GCHandleType.Pinned);
                        dArgs[i] = handles[i].AddrOfPinnedObject();
                        break;
                    }
                    case object[] boxed:
                    {
                        /* PCode: una jagged e' object[] di object[] (non object[][] tipizzato).
                         * Appiattiscila nello scratch tipizzato e ricopia ricorsivamente. */
                        if (boxed.Length > 0 && boxed.Any(e => e is Array))
                        {
                            var wantIntN = (arrayElemIntMask & (1L << i)) != 0;
                            var flatCountN = FlatValuesAny(boxed).Count();
                            var bufN = wantIntN ? (Array)new long[flatCountN] : (Array)new double[flatCountN];
                            var jN = 0;
                            foreach (var rawN in FlatValuesAny(boxed))
                            {
                                if (wantIntN) ((long[])bufN)[jN] = Convert.ToInt64(rawN ?? 0L);
                                else ((double[])bufN)[jN] = Convert.ToDouble(rawN ?? 0.0);
                                jN++;
                            }
                            typedScratch[i] = bufN;
                            typedOrig[i] = boxed;
                            handles[i] = GCHandle.Alloc(bufN, GCHandleType.Pinned);
                            dArgs[i] = handles[i].AddrOfPinnedObject();
                            break;
                        }
                        var isInt = (arrayElemIntMask & (1L << i)) != 0;
                        scratchIsInt[i] = isInt;
                        scratchOrig[i] = boxed;
                        var buf = isInt ? (Array)new long[boxed.Length] : new double[boxed.Length];
                        for (var j = 0; j < boxed.Length; j++)
                        {
                            if (isInt) ((long[])buf)[j] = Convert.ToInt64(boxed[j] ?? 0L);
                            else ((double[])buf)[j] = Convert.ToDouble(boxed[j] ?? 0.0);
                        }
                        scratchBuf[i] = buf;
                        handles[i] = GCHandle.Alloc(buf, GCHandleType.Pinned);
                        dArgs[i] = handles[i].AddrOfPinnedObject();
                        break;
                    }
                    default:
                        throw new InvalidOperationException(
                            $"FFI: argomento {i} atteso array (arrayMask), ricevuto {(args[i]?.GetType().Name ?? "null")}.");
                }
            }
            var result = del.DynamicInvoke(dArgs);
            // copy-out: ricopia lo scratch (eventualmente scritto dal native) dentro gli
            // elementi boxati originali — copre sia input che output senza distinguerli.
            for (var i = 0; i < n; i++)
            {
                if (scratchBuf[i] is { } buf)
                {
                    var orig = scratchOrig[i];
                    for (var j = 0; j < orig.Length; j++)
                        orig[j] = scratchIsInt[i] ? ((long[])buf).GetValue(j) : ((double[])buf).GetValue(j);
                }
                /* Copy-out degli array CLR tipizzati passati via scratch convertito: il nativo
                 * può aver scritto (array BYREF di output) — riporta i valori nell'array del
                 * chiamante, che è lo stesso oggetto visto da BASIC. */
                if (byteScratch[i] is { } bbuf)
                {
                    // Copy-back con saturazione a BYTE/SBYTE (il nativo puo' aver scritto 0..255).
                    switch (byteOrig[i])
                    {
                        case byte[] bdst:
                            for (var j = 0; j < bdst.Length; j++)
                                bdst[j] = (byte)Math.Clamp(bbuf[j], 0L, 255L);
                            break;
                        case sbyte[] sdest:
                            for (var j = 0; j < sdest.Length; j++)
                                sdest[j] = (sbyte)Math.Clamp(bbuf[j], -128L, 127L);
                            break;
                    }
                }
                if (typedScratch[i] is { } tbuf)
                {
                    switch (typedOrig[i])
                    {
                        case double[] dorig:
                            for (var j = 0; j < dorig.Length; j++) dorig[j] = Convert.ToDouble(tbuf.GetValue(j));
                            break;
                        case long[] lorig:
                            for (var j = 0; j < lorig.Length; j++) lorig[j] = Convert.ToInt64(tbuf.GetValue(j));
                            break;
                        case ulong[] uorig:
                            for (var j = 0; j < uorig.Length; j++) uorig[j] = Convert.ToUInt64(tbuf.GetValue(j));
                            break;
                        case double[,] dmat:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < dmat.GetLength(0); r0++)
                                for (var c0 = 0; c0 < dmat.GetLength(1); c0++)
                                    dmat[r0, c0] = Convert.ToDouble(tbuf.GetValue(k++));
                            break;
                        }
                        case long[,] lmat:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < lmat.GetLength(0); r0++)
                                for (var c0 = 0; c0 < lmat.GetLength(1); c0++)
                                    lmat[r0, c0] = Convert.ToInt64(tbuf.GetValue(k++));
                            break;
                        }
                        case ulong[,] umat:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < umat.GetLength(0); r0++)
                                for (var c0 = 0; c0 < umat.GetLength(1); c0++)
                                    umat[r0, c0] = Convert.ToUInt64(tbuf.GetValue(k++));
                            break;
                        }
                        case double[][] djag:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < djag.Length; r0++)
                                for (var c0 = 0; c0 < djag[r0].Length; c0++)
                                    djag[r0][c0] = Convert.ToDouble(tbuf.GetValue(k++));
                            break;
                        }
                        case long[][] ljag:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < ljag.Length; r0++)
                                for (var c0 = 0; c0 < ljag[r0].Length; c0++)
                                    ljag[r0][c0] = Convert.ToInt64(tbuf.GetValue(k++));
                            break;
                        }
                        case object[,] omat:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < omat.GetLength(0); r0++)
                                for (var c0 = 0; c0 < omat.GetLength(1); c0++)
                                    omat[r0, c0] = tbuf.GetValue(k++);
                            break;
                        }
                        case object[][] ojag:
                        {
                            var k = 0;
                            for (var r0 = 0; r0 < ojag.Length; r0++)
                                for (var c0 = 0; c0 < ojag[r0].Length; c0++)
                                    ojag[r0][c0] = tbuf.GetValue(k++);
                            break;
                        }
                        case object[] onest:
                        {
                            var k = 0;
                            SetFlatValuesAny(onest, tbuf, ref k);
                            break;
                        }
                        default:
                        {
                            /* Rango >= 3 (2026-11-06, S2): copy-out generico su matrice
                             * rettangolare — gli indici si ricostruiscono dalla posizione
                             * lineare row-major, il valore si converte sul tipo dell'elemento
                             * (lo scratch può essere int64 mentre la matrice è double e
                             * viceversa). */
                            if (typedOrig[i] is Array oarr && oarr.Rank >= 2)
                            {
                                var etO = oarr.GetType().GetElementType();
                                var dimsO = new int[oarr.Rank];
                                for (var d0 = 0; d0 < oarr.Rank; d0++) dimsO[d0] = oarr.GetLength(d0);
                                var idxO = new int[oarr.Rank];
                                for (var kO = 0; kO < oarr.Length; kO++)
                                {
                                    var remO = kO;
                                    for (var d0 = oarr.Rank - 1; d0 >= 0; d0--) { idxO[d0] = remO % dimsO[d0]; remO /= dimsO[d0]; }
                                    object vO = tbuf.GetValue(kO) ?? 0;
                                    if (etO == typeof(double)) vO = Convert.ToDouble(vO);
                                    else if (etO == typeof(long)) vO = Convert.ToInt64(vO);
                                    else if (etO == typeof(ulong)) vO = Convert.ToUInt64(vO);
                                    oarr.SetValue(vO, idxO);
                                }
                            }
                            break;
                        }
                    }
                }
            }
            // BYREF scalare: DynamicInvoke ha già aggiornato dArgs[i] con il valore OUT (per un
            // parametro CLR ref, .NET scrive il risultato nello stesso slot dell'array passato) —
            // lo si riscrive nell'array ORIGINALE del chiamante (args, non dArgs): il chiamante
            // (PCodeProcessor.cs/IL) rilegge args[i] dopo questa chiamata per propagare il
            // valore alla variabile BASIC (ByRefBox.Value / stloc). Contratto documentato in
            // IFfiService.cs.
            if (byrefScalarMask != 0)
                for (var i = 0; i < n; i++)
                    if ((byrefScalarMask & (1L << i)) != 0) args[i] = dArgs[i];
            if (retIsString)
            {
                var ptr = (IntPtr)Convert.ToInt64(result ?? 0L);
                return ptr == IntPtr.Zero ? "" : (Marshal.PtrToStringUTF8(ptr) ?? "");
            }
            // Copy-out degli array di UDT (S3): la funzione nativa può aver modificato il buffer.
            for (var i = 0; i < n; i++)
            {
                if (udtArrRaw[i] == IntPtr.Zero || udtArrOrig[i] is not { } srcArr) continue;
                var shape = udtArrShapes[i]!;
                var esz = udtArrElemSize[i];
                for (var j = 0; j < srcArr.Length; j++)
                {
                    var vals = ReadFfiElem(udtArrRaw[i] + j * esz, shape);
                    var target = srcArr.GetValue(j);
                    for (var f = 0; f < shape.FieldNames.Length; f++)
                    {
                        object? v = vals[f];
                        var symbolPath = shape.FieldSymbolIdPaths[f];
                        if (target is Dictionary<string, object?> td)
                            SetFfiLeafValue(td, shape.FieldNames[f], symbolPath, v);
                        else if (target != null)
                            SetPathValue(target, shape.FieldNames[f].Split('.'), 0, v);
                    }
                    if (target is not null and not Dictionary<string, object?>) srcArr.SetValue(target, j);
                }
            }
            if (udtReturnShapeStr != null)
            {
                // Ritorno UDT con layout C (S1): il delegate ritorna la struct-ombra, si
                // riconverte nella struct del dialetto (tipo noto al chiamante IL).
                var retShape = ParseUdtShape(udtReturnShapeStr);
                if (result == null) return null;
                return udtReturnClrType != null
                    ? ConvertFromFfiStruct(result, retShape.Fields, udtReturnClrType)
                    : result;
            }
            if (udtReturnClrType != null)
            {
                // Struct/UDT per valore, OUT, backend IL (2026-09-29): 'result' è già
                // un'istanza BOXED del tipo CLR reale del UDT (lo stesso Type passato come
                // returnType del delegate) — torna COSÌ COM'È, il chiamante (EmitFfiCall)
                // la unboxa con Unbox_Any sul tipo noto a compile-time.
                return result;
            }
            if (udtReturn != null)
            {
                // Struct/UDT per valore, OUT (2026-09-28): 'result' è un'istanza BOXED del
                // value type CLR dinamico — si ricostruisce una NUOVA istanza PCodeVM
                // (Dictionary<string,object?>), con __class__/FieldNames chiavato per
                // SymbolId (quello che OP_LOAD_MEMBER usa per risolvere un accesso a campo,
                // vedi Compiler.UserTypes.cs) così un successivo `pt.X` funziona come su
                // qualunque altra istanza UDT. Valori numerici SEMPRE come double (convenzione
                // PCodeVM: un campo AS LONG è comunque boxato come double, come per gli array —
                // vedi DefaultValueForFieldType).
                var structTypeOut = GetOrBuildUdtShapeType(udtReturn.FieldTypes, udtReturn.FieldIsInt, udtReturn.FieldOffsets);
                var instance = new Dictionary<string, object?>();
                // __class__.FieldNames sul livello RADICE (2026-09-29, struct annidate): mappa
                // il SymbolId del PRIMO segmento di ogni foglia al suo nome — sufficiente per
                // risolvere `r.I`/`r.Y` (i campi diretti del ritorno) come su qualunque altra
                // istanza UDT. I livelli PIÙ interni (dentro un campo annidato) non hanno un
                // __class__ completo — SetFfiLeafValue scrive comunque il fallback
                // "__field_<SymbolId>" ad OGNI livello, che OP_LOAD_MEMBER usa quando
                // __class__/FieldNames non risolve l'offset (vedi PCodeProcessor.cs).
                var fieldNamesBySymbolId = new Dictionary<int, string>();
                for (var fi = 0; fi < udtReturn.FieldNames.Length; fi++)
                {
                    var raw = structTypeOut.GetField($"F{fi}")!.GetValue(result);
                    var symbolIdPath = udtReturn.FieldSymbolIdPaths[fi];
                    object? value;
                    if (udtReturn.FieldTypes != null && udtReturn.FieldTypes[fi] == 9)
                        value = raw is IntPtr ip ? (Marshal.PtrToStringUTF8(ip) ?? "") : (raw as string ?? "");
                    else
                        value = Convert.ToDouble(raw ?? 0.0);
                    SetFfiLeafValue(instance, udtReturn.FieldNames[fi], symbolIdPath, value);
                    if (symbolIdPath.Length > 0) fieldNamesBySymbolId[symbolIdPath[0]] = udtReturn.FieldNames[fi].Split('.')[0];
                }
                instance["__class__"] = new Dictionary<string, object?> { ["FieldNames"] = fieldNamesBySymbolId };
                return instance;
            }
            return kind == 1 ? Convert.ToDouble(result ?? 0L) : result;
        }
        finally
        {
            for (var i = 0; i < n; i++)
            {
                if (handles[i].IsAllocated) handles[i].Free();
                if (stringPtrs[i] != IntPtr.Zero) Marshal.FreeCoTaskMem(stringPtrs[i]);
            }
            if (udtStrPtrs != null)
            {
                foreach (var sp in udtStrPtrs) Marshal.FreeCoTaskMem(sp);
            }
            for (var i = 0; i < n; i++)
                if (udtArrRaw[i] != IntPtr.Zero) { Marshal.FreeHGlobal(udtArrRaw[i]); udtArrRaw[i] = IntPtr.Zero; }
        }
    }

    private static Delegate CreateDelegate(IntPtr funcPtr, Type returnType, Type[] paramTypes)
    {
        var delegateType = CreateDelegateType(returnType, paramTypes);
        return Marshal.GetDelegateForFunctionPointer(funcPtr, delegateType);
    }

    private static Delegate CreateDelegate(IntPtr funcPtr, int argCount)
    {
        var returnType = typeof(double);
        var paramTypes = Enumerable.Repeat(typeof(double), argCount).ToArray();
        var delegateType = CreateDelegateType(returnType, paramTypes);
        return Marshal.GetDelegateForFunctionPointer(funcPtr, delegateType);
    }

    private static Type CreateDelegateType(Type returnType, Type[] paramTypes)
    {
        var asmName = new AssemblyName("FfiDelegates");
        var asmBuilder = AssemblyBuilder.DefineDynamicAssembly(asmName, AssemblyBuilderAccess.Run);
        var modBuilder = asmBuilder.DefineDynamicModule("Main");
        var typeBuilder = modBuilder.DefineType("FfiDelegate_" + Guid.NewGuid().ToString("N")[..8],
            TypeAttributes.Public | TypeAttributes.Sealed, typeof(MulticastDelegate));
        
        var ctorBuilder = typeBuilder.DefineConstructor(
            MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.SpecialName | MethodAttributes.RTSpecialName,
            CallingConventions.Standard, new[] { typeof(object), typeof(IntPtr) });
        ctorBuilder.SetImplementationFlags(MethodImplAttributes.Runtime | MethodImplAttributes.Managed);
        
        var invokeBuilder = typeBuilder.DefineMethod("Invoke",
            MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.NewSlot | MethodAttributes.Virtual,
            returnType, paramTypes);
        invokeBuilder.SetImplementationFlags(MethodImplAttributes.Runtime | MethodImplAttributes.Managed);

        return typeBuilder.CreateType()!;
    }
}
