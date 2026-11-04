// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Segnala a OP_CALL_BUILTIN che il risultato verra' depositato sullo stack dal
    /// completamento asincrono prima del wake del PCB. Non e' un valore BASIC osservabile.</summary>
    private static readonly object ParkedBuiltinResult = new();

    private object ParkHttpBuiltin(PCodeTask task, string method, object?[] args)
    {
        var options = new HttpOptions
        {
            Method = method,
            Body = args.Length > 1 ? args[1]?.ToString() ?? "" : ""
        };
        if (args.Length > 2 && !string.IsNullOrEmpty(args[2]?.ToString()))
            options.Headers.ContentType = args[2]!.ToString()!;

        var url = args.Length > 0 ? args[0]?.ToString() ?? "" : "";
        ParkAsync(task, PCodeWaitKind.Http,
            ct => new ValueTask<FetchResult>(
                _runtime.HttpClientHelper.PerformRequestAsync(url, options, ct)),
            result => task.PushRef(result.Body ?? ""));
        return ParkedBuiltinResult;
    }

private static double[] ToDoubleArr(object? arg) => Basic.IlBackend.RuntimeFlatten.ToDoubles(arg);
    private static double[][] ToDoubleMatrix(object? value)
    {
        if (value is not System.Collections.IEnumerable rows || value is string) return [];
        var result = new List<double[]>();
        foreach (var row in rows) result.Add(ToDoubleArr(row));
        return result.ToArray();
    }
    private static double[][] ToJaggedArr(object? value)
    {
        // Matrice RETTANGOLARE rank-2 (DIM M(3,1) AS DOUBLE → double[,] o object[,]): l'IL la
        // converte con RectToJagged, ma il dispatch PCode intercetta la builtin con l'argomento
        // grezzo (spesso object[,] con elementi boxed).
        if (value is Array arr2 && arr2.Rank == 2)
        {
            var r2 = arr2.GetLength(0);
            var c2 = arr2.GetLength(1);
            var res2 = new double[r2][];
            for (var i2 = 0; i2 < r2; i2++)
            {
                res2[i2] = new double[c2];
                for (var j2 = 0; j2 < c2; j2++) res2[i2][j2] = SafeDouble(arr2.GetValue(i2, j2));
            }
            return res2;
        }
        if (value is not System.Collections.IEnumerable rows || value is string) return Array.Empty<double[]>();
        var result = new System.Collections.Generic.List<double[]>();
        foreach (var row in rows) result.Add(ToDoubleArr(row));
        return result.ToArray();
    }
    private static double[][]? ToRectArr(object? value)
    {
        if (value == null) return null;
        var jag = ToJaggedArr(value);
        return jag.Length == 0 ? null : jag;
    }
    private static double[][] RectToJagged(double[,] matrix)
    {
        var result = new double[matrix.GetLength(0)][];
        for (var row = 0; row < result.Length; row++)
        {
            result[row] = new double[matrix.GetLength(1)];
            for (var column = 0; column < result[row].Length; column++)
                result[row][column] = matrix[row, column];
        }
        return result;
    }
    private static Dictionary<string, object?> HypothesisResult(CommonLib.StatisticalTestResult result) => new()
    {
        ["Statistic"] = result.Statistic,
        ["PValue"] = result.PValue,
        ["DegreesOfFreedom"] = result.DegreesOfFreedom
    };

    /// <summary>
    /// JSON.EXTRACT su un documento JSON GREZZO (stringa non ancora parseata): naviga un
    /// path a segmenti ("status", "hits[0]", "hits[0].sourceId", "a.b[1].c") su un
    /// JsonElement System.Text.Json. Implementazione PCode-nativa, indipendente da
    /// IlRuntime.ExtractFromElement (stesso algoritmo, mai condiviso -- REGOLA DI
    /// SEPARAZIONE dei due backend/compilatori: parità di comportamento, non di codice).
    /// </summary>
    private static object? PCodeJsonExtractPathFromElement(System.Text.Json.JsonElement el, string path)
    {
        if (string.IsNullOrEmpty(path)) return null;
        var remaining = path;
        var current = el;
        while (remaining.Length > 0)
        {
            int dot = remaining.IndexOf('.');
            int bracket = remaining.IndexOf('[');
            var segEnd = (dot >= 0 && (bracket < 0 || dot < bracket)) ? dot : (bracket >= 0 ? bracket : remaining.Length);
            var seg = remaining[..segEnd];
            if (seg.Length > 0)
            {
                if (!current.TryGetProperty(seg, out current)) return null;
            }
            if (bracket >= 0 && bracket < (dot >= 0 ? dot : int.MaxValue))
            {
                int close = remaining.IndexOf(']', bracket);
                if (close < 0) return null;
                var idxStr = remaining[(bracket + 1)..close];
                if (!int.TryParse(idxStr, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var idx) || idx < 0)
                    return null;
                if (current.ValueKind != System.Text.Json.JsonValueKind.Array || idx >= current.GetArrayLength())
                    return null;
                current = current[idx];
                remaining = close + 1 < remaining.Length ? remaining[(close + 1)..] : "";
                if (remaining.StartsWith('.')) remaining = remaining[1..];
                continue;
            }
            remaining = segEnd < remaining.Length ? remaining[(segEnd + 1)..] : "";
        }
        return current.ValueKind switch
        {
            System.Text.Json.JsonValueKind.String => current.GetString(),
            System.Text.Json.JsonValueKind.Number => current.GetDouble(),
            System.Text.Json.JsonValueKind.True => -1.0,
            System.Text.Json.JsonValueKind.False => 0.0,
            System.Text.Json.JsonValueKind.Null => null,
            _ => current.GetRawText()
        };
    }

    /// <summary>
    /// JSON.EXTRACT su un valore GIÀ PARSEATO (Dictionary/List annidati, es. il risultato
    /// di JSON.Parse): stesso path a segmenti di PCodeJsonExtractPathFromElement, ma
    /// naviga IDictionary/IList invece di JsonElement. Implementazione PCode-nativa,
    /// indipendente da IlRuntime.ExtractFromObject (stessa REGOLA DI SEPARAZIONE).
    /// </summary>
    private static object? PCodeJsonExtractPathFromObject(object? obj, string path)
    {
        if (obj is null || string.IsNullOrEmpty(path)) return null;
        var remaining = path;
        object? current = obj;
        while (remaining.Length > 0 && current != null)
        {
            int dot = remaining.IndexOf('.');
            int bracket = remaining.IndexOf('[');
            var segEnd = (dot >= 0 && (bracket < 0 || dot < bracket)) ? dot : (bracket >= 0 ? bracket : remaining.Length);
            var seg = remaining[..segEnd];
            if (seg.Length > 0)
            {
                if (current is System.Collections.IDictionary d)
                {
                    if (!d.Contains(seg)) return null;
                    current = d[seg];
                }
                else return null;
            }
            if (bracket >= 0 && bracket < (dot >= 0 ? dot : int.MaxValue))
            {
                int close = remaining.IndexOf(']', bracket);
                if (close < 0) return null;
                var idxStr = remaining[(bracket + 1)..close];
                if (!int.TryParse(idxStr, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var idx) || idx < 0)
                    return null;
                current = current is System.Collections.IList list && idx < list.Count ? list[idx] : null;
                if (current is null) return null;
                remaining = close + 1 < remaining.Length ? remaining[(close + 1)..] : "";
                if (remaining.StartsWith('.')) remaining = remaining[1..];
                continue;
            }
            remaining = segEnd < remaining.Length ? remaining[(segEnd + 1)..] : "";
        }
        return current;
    }
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _CoreDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> CoreDispatch
        => _CoreDispatch ??= BuildCoreDispatch();

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildCoreDispatch() => new()
    {
        [BuiltInFunction.CREATE_ARRAY] = (task, args) =>
        {
            {
                // Un elemento di un array di UDT deve avere il proprio dizionario indipendente
                // taggato __class__ — senza, resta null e ogni lettura/scrittura di campo
                // (OP_LOAD_MEMBER/OP_STORE_MEMBER) fallisce in silenzio (vedi 02-Knowledge/
                // GMBasic-PCODE-UDT-Field-Access-Bug.md). Ogni campo pre-popolato con un
                // valore di default TIPIZZATO (via FieldTypes/DefaultValueForFieldType — stessa
                // logica di OP_NEW_UDT_INSTANCE sopra) invece di lasciarlo assente dal
                // dizionario.
                // (Uses class-level MakeDefaultUdtInstance)
                System.Collections.IDictionary? udtDesc = null;
                int dimStart = 0;
        
                if (args.Length > 1 && args[0] is double cdIdx && cdIdx >= 0 && cdIdx < _constants.Count && _constants[(int)cdIdx] is System.Collections.IDictionary cd && cd.Contains("FieldNames"))
                {
                    udtDesc = cd;
                    dimStart = 1;
                }
        
                bool hasBounds = false;
                if (args.Length - dimStart >= 2 && args[dimStart] is double dimCount && dimCount > 0 && dimCount <= 10)
                {
                    // The first arg after UDT descriptor is the dimension count prefix
                    int numDims = (int)dimCount;
                    // Check if remaining args form TO bounds or plain dims
                    int remaining = args.Length - dimStart - 1; // -1 for dimCount prefix
        
                    if (remaining == numDims * 2)
                    {
                        // TO bounds: each dim has [lower, upper]
                        var sizes = new int[numDims];
                        var lowers = new int[numDims];
                        for (int d = 0; d < numDims; d++)
                        {
                            int lo = Convert.ToInt32(args[dimStart + 1 + d * 2]);
                            int hi = Convert.ToInt32(args[dimStart + 1 + d * 2 + 1]);
                            sizes[d] = hi - lo + 1;
                            lowers[d] = lo;
                        }
                        if (sizes.Any(s => s < 0)) return null;
                        var toArr = Array.CreateInstance(typeof(object), sizes, lowers);
                        // Elementi UDT: una istanza DISTINTA per elemento. SetValue(object,int)
                        // (indice singolo) NON è legale su un array multi-D ("Array was not a
                        // one-dimensional array") → FillFieldArray decompone l'indice lineare.
                        if (udtDesc != null)
                            FillFieldArray(toArr, sizes, () => MakeDefaultUdtInstance(udtDesc));
                        return toArr;
                    }
                    else if (remaining == numDims)
                    {
                        // Plain dimensions: each arg is the upper bound (0-based)
                        var sizes = new int[numDims];
                        for (int d = 0; d < numDims; d++)
                        {
                            sizes[d] = Convert.ToInt32(args[dimStart + 1 + d]) + 1;
                        }
                        if (sizes.Any(s => s < 0)) return null;
                        var arr = Array.CreateInstance(typeof(object), sizes);
                        if (udtDesc != null)
                            FillFieldArray(arr, sizes, () => MakeDefaultUdtInstance(udtDesc));
                        return arr;
                    }
                    // Fallback to old logic for 1D without prefix
                }
        
                // Legacy path (no dimension count prefix): 1D array
                int lower = 0, upper = 0;
                hasBounds = false;
                dimStart = 0;
        
                if (args.Length > 1 && args[0] is double cdIdx2 && cdIdx2 >= 0 && cdIdx2 < _constants.Count && _constants[(int)cdIdx2] is System.Collections.IDictionary cd2 && cd2.Contains("FieldNames"))
                {
                    udtDesc = cd2;
                    dimStart = 1;
                }
        
                int remaining2 = args.Length - dimStart;
                if (remaining2 == 0)
                {
                    // DIM x() senza dimensioni (array dinamico): DEVE creare un array VUOTO
                    // (0 elementi) — prima il fallback "size = upper + 1" con upper=0 creava
                    // object?[1] con 1 slot null, e AADD(AADD) accumulava un elemento nullo
                    // in testa ("Collection was of a fixed size" / Nomi$(0) vuoto).
                    return new object?[0];
                }
                if (remaining2 == 2)
                {
                    // [lower, upper] — TO bounds
                    lower = Convert.ToInt32(args[dimStart]);
                    upper = Convert.ToInt32(args[dimStart + 1]);
                    hasBounds = true;
                }
                else if (remaining2 == 1)
                {
                    // Simple dimension
                    upper = Convert.ToInt32(args[dimStart]);
                }
        
                Array legacyArr;
                if (hasBounds)
                {
                    int size = upper - lower + 1;
                    if (size < 0) size = 0;
                    legacyArr = Array.CreateInstance(typeof(object), new int[] { size }, new int[] { lower });
                }
                else
                {
                    int size = upper + 1;
                    if (size < 0) size = 0;
                    legacyArr = new object?[size];
                }
        
                if (udtDesc != null && legacyArr.Length > 0)
                {
                    int start = legacyArr.GetLowerBound(0);
                    int end = legacyArr.GetUpperBound(0);
                    for (int i = start; i <= end; i++)
                        legacyArr.SetValue(MakeDefaultUdtInstance(udtDesc), i);
                }
                else if (legacyArr.Length > 0 && !hasBounds)
                {
                    legacyArr.SetValue(null, legacyArr.GetLowerBound(0));
                }
                return legacyArr;
            }
        },
        [BuiltInFunction.UBOUND] = (task, args) =>
        {
            {
                var dim = args.Length > 1 ? Convert.ToInt32(args[1] ?? 1) : 1;
                if (args.Length > 0 && args[0] is Array mdArray)
                {
                    if (mdArray.Rank > 1 || dim > 1)
                        return (double)(mdArray.GetUpperBound(dim - 1));
                    return (double)(mdArray.GetUpperBound(0));
                }
                if (dim == 1)
                {
                    if (args.Length > 0 && args[0] is System.Collections.IList list)
                        return (double)(list.Count - 1);
                    if (args.Length > 0 && args[0] is string str)
                        return (double)(str.Length - 1);
                    if (args.Length > 0 && args[0] is System.Collections.IDictionary dict)
                        return (double)(dict.Count - 1);
                }
                return 0.0;
            }
        },
        [BuiltInFunction.LBOUND] = (task, args) =>
        {
            {
                var dim = args.Length > 1 ? Convert.ToInt32(args[1] ?? 1) : 1;
                if (args.Length > 0 && args[0] is Array mdArray)
                    return (double)(mdArray.GetLowerBound(dim - 1));
                if (args.Length > 0 && args[0] is string)
                    return 0.0; // String: 0-based
                if (args.Length > 0 && args[0] is System.Collections.IDictionary)
                    return 0.0; // Dictionary: 0-based
                // IList (List<object>, es. DIM x = [] + .Add(), o il risultato di AFILTER/
                // AMAP/ecc.) mancava qui — cadeva nel fallback "1.0" mentre UBOUND per lo
                // stesso IList restituisce sempre list.Count-1 (0-based, vedi sopra). Un FOR
                // EACH su una List parte quindi da ctr=1 invece che 0, saltando SEMPRE il
                // primo elemento — invisibile su un singolo ciclo se nessuno nota l'elemento
                // mancante, ma con cicli FOR EACH annidati sulla stessa List (es.
                // TypesForeachAdd.bas: 4 elementi attesi, 3 effettivi per lato → 9 invece di
                // 16 prodotti) il conteggio sbagliato si nota subito.
                if (args.Length > 0 && args[0] is System.Collections.IList)
                    return 0.0; // List: 0-based, come UBOUND
                return 1.0; // Default 1-based (vero Array VB-style dichiarato senza LBOUND esplicito)
            }
        },
        [BuiltInFunction.EMPTY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): gestisce anche numeri/collection — unica impl.
                return args.Length > 0 && Basic.IlBackend.IlRuntime.Empty(args[0]) ? -1.0 : 0.0;
        },
        [BuiltInFunction.UDT_PACKED] = (task, args) =>
        {
                // Runtime comune (IlRuntime): registro field-count, O(1) — parità IL/C99.
                return Basic.IlBackend.IlRuntime.UdtPacked(args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.UDT_ALIGNED] = (task, args) =>
        {
if (args.Length > 0) { try { var sizes = new List<int>(); foreach (var a in args) { var arr = a as System.Collections.IList; if (arr != null) { for (int i = 0; i < arr.Count; i++) sizes.Add((int)SafeDouble(arr[i])); } else sizes.Add((int)SafeDouble(a)); } return (double)CommonLib.UdtUtils.UdtAligned(sizes.ToArray()); } catch { } } return 1.0;
            // --- Crypto ---
            // MD5/SHA1/SHA256/SHA512 (Fase 10, docs/StdLib-Runtime-Plan.md): prima passavano per
            // _runtime.Crypto.HashX (servizio host, stringa hex→Convert.FromHexString — bug di
            // tipo pre-esistente, "la VM stampava 0"), un'implementazione DUPLICATA e divergente
            // da CommonLib.CryptoUtils.CoreHashData già usata da IlBackend/IlRuntime.cs. Ora
            // entrambi i motori chiamano la stessa Basic.StdLibRuntime.CryptographyFunctions.
        },
        [BuiltInFunction.MD5] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Md5(ArgOr(args, 0));
        },
        [BuiltInFunction.SHA1] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Sha1(ArgOr(args, 0));
        },
        [BuiltInFunction.SHA256] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Sha256(ArgOr(args, 0));
        },
        [BuiltInFunction.SHA512] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Sha512(ArgOr(args, 0));
            // AESENCRYPT/AESDECRYPT: erano su _runtime.Crypto.EncryptAES/DecryptAES(string,string) —
            // .ToString() su un array BASIC (data) produceva "System.Object[]" invece dei byte reali,
            // rompendo qualunque round-trip su dati binari. Portate su CommonLib.CryptoUtils.CoreAesEncrypt/
            // CoreAesDecrypt (byte[]-based, AES-256-GCM con salt+nonce+tag), la stessa coppia già usata
            // dal backend IL nativo — mantenute appaiate qui per non rompere di nuovo il round-trip.
        },
        [BuiltInFunction.AESENCRYPT] = (task, args) =>
        {
            {
                var data = ToBytes(ArgOr(args, 0));
                var key = args.Length > 1 ? args[1]?.ToString() ?? "" : "";
                try { return CommonLib.CryptoUtils.CoreAesEncrypt(data, key); } catch { return Array.Empty<byte>(); }
            }
        },
        [BuiltInFunction.AESDECRYPT] = (task, args) =>
        {
            {
                var data = ToBytes(ArgOr(args, 0));
                var key = args.Length > 1 ? args[1]?.ToString() ?? "" : "";
                try { return CommonLib.CryptoUtils.CoreAesDecrypt(data, key); } catch { return Array.Empty<byte>(); }
            }
        },
        [BuiltInFunction.CHACHA20POLY1305ENCRYPT] = (task, args) =>
        {
            {
                try
                {
                    var pt = ToBytes(ArgOr(args, 0));
                    var key = ToBytes(ArgOr(args, 1));
                    var nonce = ToBytes(ArgOr(args, 2));
                    var aad = args.Length > 3 ? ToBytes(args[3]) : null;
                    var (ct, tag) = Basic.StdLibRuntime.CryptographyFunctions.ChaCha20Poly1305Encrypt(pt, key, nonce, aad);
                    return new System.Collections.Generic.Dictionary<string, object?>
                    {
                        ["ciphertext"] = ct,
                        ["tag"] = tag
                    };
                }
                catch { return new System.Collections.Generic.Dictionary<string, object?>(); }
            }
        },
        [BuiltInFunction.CHACHA20POLY1305DECRYPT] = (task, args) =>
        {
            {
                try
                {
                    var ct = ToBytes(ArgOr(args, 0));
                    var tag = ToBytes(ArgOr(args, 1));
                    var key = ToBytes(ArgOr(args, 2));
                    var nonce = ToBytes(ArgOr(args, 3));
                    var aad = args.Length > 4 ? ToBytes(args[4]) : null;
                    var pt = Basic.StdLibRuntime.CryptographyFunctions.ChaCha20Poly1305Decrypt(ct, tag, key, nonce, aad);
                    return pt ?? Array.Empty<byte>();
                }
                catch { return Array.Empty<byte>(); }
            }
        },
        [BuiltInFunction.PASSWORDHASH] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.PasswordHash(args.Length > 0 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.PASSWORDVERIFY] = (task, args) =>
        {
                return args.Length >= 2 && Basic.IlBackend.IlRuntime.PasswordVerify(args[0]?.ToString() ?? "", args[1]?.ToString() ?? "") ? -1.0 : 0.0;
        },
        [BuiltInFunction.RANDOMCRYPTOBYTES] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.RandomCryptoBytes(args.Length > 0 ? Convert.ToInt32(args[0] ?? 32) : 32);
        },
        [BuiltInFunction.HASHFILE] = (task, args) =>
        {
                /* HASHFILE(path, algorithm) → byte[] (StdLib byteArrayType). */
                if (args.Length >= 1) return Basic.IlBackend.IlRuntime.HashFile(
                    args[0]?.ToString() ?? "",
                    args.Length > 1 ? args[1]?.ToString()?.ToUpperInvariant() ?? "SHA256" : "SHA256");
                return Array.Empty<byte>();
        },
        [BuiltInFunction.ENCRYPTASYM] = (task, args) =>
        {
                // Runtime comune (IlRuntime): registry handle unico (§0bis).
                return Basic.IlBackend.IlRuntime.EncryptAsym(ArgOr(args, 0), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.DECRYPTASYM] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DecryptAsym(ArgOr(args, 0), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.INKEY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): Inkey(timeout) gestisce coda/grafica/ON KEY
                // (DispatchKeyEvent + ProcessKeyForActiveWindow) — unica implementazione.
                return Basic.IlBackend.IlRuntime.Inkey(args.Length > 0 ? (int)SafeDouble(args[0]) : 0);
        },
        [BuiltInFunction.FILEINPUT] = (task, args) =>
        {
                // Firma canonica FINPUT(charCount, fileNumber).
                if (args.Length >= 1)
                {
                    var finCount = args.Length >= 2 ? (long)SafeDouble(args[0]) : 1;
                    var finNum = (long)SafeDouble(args.Length >= 2 ? args[1] : args[0]);
                    // Un file aperto con `OPEN … AS #n` vive nel FileService della PCodeVM (task.FileHandles),
                    // NON nel registro handle di IlRuntime (FOPEN): passare il file number a IlRuntime.FileInput
                    // dava "Handle file non valido" (IO.FINPUT(10, 1) dopo OPEN … AS #1).
                    if (task.FileHandles.TryGetValue((int)finNum, out var finHandle)
                        && _runtime.File.GetStream(finHandle) is { } finStream)
                    {
                        var finBuf = new byte[Math.Max(0, (int)finCount)];
                        var finGot = finStream.Read(finBuf, 0, finBuf.Length);
                        return System.Text.Encoding.UTF8.GetString(finBuf, 0, Math.Max(0, finGot));
                    }
                    return Basic.IlBackend.IlRuntime.FileInput(finCount, finNum);
                }
                return "";
        },
        [BuiltInFunction.FREEFILE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): primo numero di file libero. Prima delegava a
                // CommonLib.FileReadWriteUtils.FreeFile(), che ritornava la COSTANTE 1 (un
                // numero già occupato con un file aperto) → divergeva da IL/C99/C99Native.
                // Trovato da MiniTests/io/io_freefile.bas.
                return (double)Basic.IlBackend.IlRuntime.FreeFile();
        },
        [BuiltInFunction.SIZEOF] = (task, args) =>
        {
                // CANONE: SIZEOF = dimensione in byte del TIPO STATICO, calcolata da TypeLayout
                // (unica sorgente). Il compilatore risolve a compile-time gli argomenti con tipo
                // noto (fold in Compiler.SizeOf.cs) e, per un NOME DI TIPO, spinge il descrittore
                // della classe che porta la chiave "Size" già calcolata (Compiler.UserTypes.cs):
                // qui arrivano solo i valori senza tipo statico. La tabella delle larghezze che
                // stava in questo handler è ELIMINATA — era una copia divergente (SINGLE 8,
                // LONG/QUAD 4, nessuna consapevolezza dei PACKED TYPE).
                static bool DescriptorSize(System.Collections.IDictionary d, out double bytes)
                {
                    bytes = 0;
                    // "Size" vale SOLO se il dict è un descrittore/istanza di classe (un DICT
                    // utente con una chiave omonima non deve essere scambiato per un tipo).
                    var isClassDict = d.Contains("FieldNames") || d.Contains("FieldTypes") || d.Contains("__class__");
                    if (isClassDict && d.Contains("Size") && d["Size"] is IConvertible)
                    { bytes = Convert.ToDouble(d["Size"]); return true; }
                    // Istanza: il descrittore è appeso a __class__.
                    if (d.Contains("__class__") && d["__class__"] is System.Collections.IDictionary cls)
                        return DescriptorSize(cls, out bytes);
                    return false;
                }
                if (args.Length == 0 || args[0] is null) return -1.0;
                if (args[0] is System.Collections.IDictionary szDict)
                    return DescriptorSize(szDict, out var dictBytes) ? dictBytes : -1.0;
                // stringa dinamica e array non hanno dimensione statica (il canone è -1).
                if (args[0] is string || args[0] is object?[]) return -1.0;
                // Valore boxed: la tabella CLR di IlRuntime.SizeOf coincide col canone
                // (double→8, Int16→2, decimal→16, …). 0/-1 dal runtime → -1.
                var scalarBytes = (double)Basic.IlBackend.IlRuntime.SizeOf(args[0]);
                return scalarBytes > 0 ? scalarBytes : -1.0;
            // --- Finance ---
        },
        [BuiltInFunction.PMT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Pmt(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0);
            // === FINANCE varianti (2026-08-06) ===
        },
        [BuiltInFunction.YIELD] = (task, args) =>
        {
            {
                // TaskYield() = Gosched() di Go (0 argomenti): cede il controllo al worker (come
                // gmb_vm_core.c case 677 argc==0 → force_yield). Prima qui si calcolava il bond
                // yield finanziario anche per 0 argomenti (divergenza dalla VM/C). Il flag è
                // consumato da ExecuteTaskTick (yield a metà tick → task riaccodato).
                if (args.Length == 0)
                {
                    task.YieldRequested = true;
                    return 0.0;
                }
                var settlement = DateTime.FromOADate(SafeDouble(args.Length > 0 ? args[0] : 0));
                var maturity = DateTime.FromOADate(SafeDouble(args.Length > 1 ? args[1] : 0));
                var price = args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0;
                var redemption = args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0;
                var rate = args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0;
                var frequency = args.Length > 5 ? (int)SafeDouble(args[5] ?? 0) : 1;
                var basis = args.Length > 6 ? (int)SafeDouble(args[6] ?? 0) : 0;
                try { return CommonLib.Numerics.Yield(settlement, maturity, price, redemption, rate, frequency, basis); } catch { return 0.0; }
            }
        
        },
        [BuiltInFunction.MESSAGE] = (task, args) =>
        {
                try
                {
                    if (args.Length > 0)
                    {
                        var txt = args[0]?.ToString() ?? "";
                        ClipperLib.Clipper.ClipperSet.Message(txt);
                        // Rendering: disegna il messaggio nel buffer alla riga di SET MESSAGE.
                        var stM = ClipperLib.Clipper.ClipperGlobal.State;
                        if (stM.MessageRow.HasValue && stM.RenderHook != null)
                        {
                            ClipperLib.Clipper.ClipperScrn.Prompt(stM.MessageRow.Value, 0, txt);
                            stM.RenderHook(stM.Buffer);
                        }
                        return txt;
                    }
                    return ClipperLib.Clipper.ClipperSet.Message();
                }
                catch { return ""; }
        },
        [BuiltInFunction.PRICESIMPLE] = (task, args) =>
        {
            {
                var settlement = DateTime.FromOADate(SafeDouble(args.Length > 0 ? args[0] : 0));
                var maturity = DateTime.FromOADate(SafeDouble(args.Length > 1 ? args[1] : 0));
                var rate = args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0;
                var yld = args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0;
                var redemption = args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0;
                var frequency = args.Length > 5 ? (int)SafeDouble(args[5] ?? 0) : 1;
                try { return CommonLib.Numerics.PriceSimple(settlement, maturity, rate, yld, redemption, frequency); } catch { return 0.0; }
            }
        
        },
        [BuiltInFunction.FV] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Fv(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0);
        },
        [BuiltInFunction.PV] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Pv(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0);
        },
        [BuiltInFunction.NPER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Nper(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0);
        },
        [BuiltInFunction.RATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Rate(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0,
                    args.Length > 5 ? SafeDouble(args[5] ?? 0.1) : 0.1);
        },
        [BuiltInFunction.IRR] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Irr(
                    ToDoubleArr(args.Length > 0 ? args[0] : null),
                    args.Length > 1 ? SafeDouble(args[1] ?? 0.1) : 0.1);
        },
        [BuiltInFunction.NPV] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Npv(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    ToDoubleArr(args.Length > 1 ? args[1] : null));
        },
        [BuiltInFunction.XNPV] = (task, args) =>
        {
            {
                // XNPV(rate, values, dates): le DATE venivano scartate (si chiamava Npv senza
                // pesatura sulle date) — risultato diverso da IL/StdLib.
                var xrate = args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0;
                var xvalues = ToDoubleArr(args.Length > 1 ? args[1] : null);
                var xdates = new List<System.DateTime>();
                if (args.Length > 2 && args[2] is System.Collections.IEnumerable dateEnum)
                    foreach (var d in dateEnum) xdates.Add(ToDateTime(d));
                return Basic.IlBackend.IlRuntime.Xnpv(xrate, xvalues, xdates.ToArray());
            }
        },
        [BuiltInFunction.MIRR] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Mirr(
                    ToDoubleArr(args.Length > 0 ? args[0] : null),
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0);
        },
        [BuiltInFunction.SLN] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Sln(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 1) : 1);
        },
        [BuiltInFunction.SYD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Syd(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 1) : 1,
                    args.Length > 3 ? SafeDouble(args[3] ?? 1) : 1);
        },
        [BuiltInFunction.DDB] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Ddb(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 1) : 1,
                    args.Length > 3 ? SafeDouble(args[3] ?? 1) : 1,
                    args.Length > 4 ? SafeDouble(args[4] ?? 2) : 2);
            // Conversioni numeriche
        },
        [BuiltInFunction.CINT] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return Math.Round(SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.CLNG] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return (double)(long)Math.Round(SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.CSNG] = (task, args) =>
        {
                /* CSNG = IDENTITÀ numerica (double): le gambe C non hanno un float32 e
                 * `DIM s AS SINGLE : s = 1.23456789` stampa 1.23456789. Prima qui si faceva
                 * `(float)` → 1.23456788 su PCode (e TYPENAME diceva SINGLE) mentre
                 * IL/VM-C/C99 davano 1.23456789/DOUBLE (parità chiusa 2026-11-01; il linker
                 * normalizza già la chiamata a CDbl, ma il wrapper del letterale su un target
                 * SINGLE viene creato DOPO la risoluzione → arrivava qui come CSNG). */
                if (args.Length > 0 && args[0] != null) return SafeDouble(args[0]);
                return 0.0;
        },
        [BuiltInFunction.CBYTE] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return (double)(byte)Math.Clamp((int)SafeDouble(args[0]), 0, 255);
                return 0.0;
        },
        [BuiltInFunction.CHOOSE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CHOOSE(index, options...) 1-based — unica impl.
                // Il chunk impacchetta le opzioni in array (a volte annidati: { { opt1, ... } }):
                // appiattisce ricorsivamente come fa il codegen IL.
                if (args.Length >= 2)
                {
                    System.Collections.Generic.List<object?>? Collect(object? e)
                    {
                        if (e is System.Collections.IEnumerable en2 && e is not string
                            && e is not System.Collections.IDictionary)
                        {
                            var flat = new System.Collections.Generic.List<object?>();
                            foreach (var x in en2) { var sub = Collect(x); if (sub != null) flat.AddRange(sub); }
                            return flat;
                        }
                        return new System.Collections.Generic.List<object?> { e };
                    }
                    System.Collections.Generic.List<object?> opts;
                    if (args.Length == 2) { opts = Collect(args[1]) ?? []; }
                    else { opts = new System.Collections.Generic.List<object?>(args.Skip(1)); }
                    var res = Basic.IlBackend.IlRuntime.Choose(SafeDouble(args[0]), opts.ToArray());
                    if (res != null) return res;
                }
                return 0.0;
        },
        [BuiltInFunction.CDBL] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return SafeDouble(args[0]);
                return 0.0;
        },
        [BuiltInFunction.VAL] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.ConversionUtils): parser prefisso
                // numerico — unica implementazione. BIT/BOOL → -1/0 (convenzione VMBASIC):
                // `VAL(TRUE).ToString()` darebbe "True" (0).
                if (args.Length > 0 && args[0] is bool vb) return vb ? -1.0 : 0.0;
                if (args.Length > 0 && args[0] != null) return StringHelpers.Val(args[0]?.ToString());
                return 0.0;
        },
        [BuiltInFunction.CCUR] = (task, args) =>
        {
                // BIT/BOOL → -1/0 (convenzione VMBASIC, cfr. VAL qui sopra): Convert.ToDecimal(bool)
                // dà 1, e il backend IL non ha una conversione bool→decimal implicita.
                if (args.Length > 0 && args[0] is bool vb) return vb ? -1m : 0m;
                if (args.Length > 0 && args[0] != null)
                    return Convert.ToDecimal(args[0], System.Globalization.CultureInfo.InvariantCulture);
                return 0m;
        },
        [BuiltInFunction.CSTR] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null)
                {
                    string cs;
                    if (args[0] is double d) cs = d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture);
                    else if (args[0] is float f) cs = f.ToString(System.Globalization.CultureInfo.InvariantCulture);
                    /* decimal (CURRENCY) senza ramo dedicato cadeva su ToString() cultura-corrente →
                     * separatore decimale VIRGOLA con locale it-IT ("12,34" invece di "12.34",
                     * divergenza IL vs PCode). Riferimento: ConversionUtils.CStr (InvariantCulture). */
                    else if (args[0] is decimal dec) cs = dec.ToString(System.Globalization.CultureInfo.InvariantCulture);
                    else cs = args[0]?.ToString() ?? "";
                    /* Spazio iniziale per NUMERI non negativi (parità ConversionUtils.CStr C#/IL):
                     * mancavano decimal (CURRENCY) e i tipi interi lunghi → Saldo/RECSIZE senza
                     * lo spazio (divergenza IL vs PCode). */
                    if (args[0] is double or float or int or decimal or long or short or byte or sbyte
                        or uint or ulong or ushort && !cs.StartsWith("-")) cs = " " + cs;
                    return cs;
                }
                return "";
        },
        [BuiltInFunction.TYPENAME] = (task, args) =>
        {
                // TYPENAME = nome del tipo (parità IlRuntime.TypeName/IL/golden C99): una
                // collezione (DIM AS DICT/LIST) è "DICTIONARY`2"/"LIST`1", un array "ARRAY", una
                // istanza UDT il nome BASIC. Gli scalari restano sul mapping condiviso TypeInfo
                // (es. INTEGER boxato a 16 bit → "DOUBLE", come PCode/VM-C). Prima usava solo
                // TypeInfo → dict "DICT", list "LIST OF X" (divergenza da IL/golden).
                var v = args.Length > 0 ? args[0] : null;
                if (v is System.Array) return "ARRAY";
                if (v is System.Collections.IDictionary dn)
                {
                    if (dn.Contains("__class__")) return Basic.IlBackend.IlRuntime.TypeInfo(v!);
                    // SortedDictionary → nome CLR (parità IlRuntime.TypeName/IL/golden C99).
                    return v!.GetType().Name.StartsWith("SortedDictionary", System.StringComparison.Ordinal)
                        ? "SORTEDDICTIONARY`2" : "DICTIONARY`2";
                }
                if (v is System.Collections.IList) return "LIST`1";
                // Collezioni PCode con classi CLR dedicate: IL le materializza come
                // SortedDictionary/List → normalizza al canone IL/golden.
                if (v is not null)
                {
                    switch (v.GetType().Name)
                    {
                        case "RedBlackTree`2": case "BPlusTree`2": case "BinaryIndexedTree`1":
                            return "SORTEDDICTIONARY`2";
                        case "BinaryHeap`1": case "MaxHeap`1":
                            return "LIST`1";
                    }
                }
                return Basic.IlBackend.IlRuntime.TypeInfo(v!);
        },
        [BuiltInFunction.TYPEINFO] = (task, args) =>
        {
                // Collezioni PCode con classi CLR dedicate: IL le materializza come
                // SortedDictionary (TYPEINFO "DICT") o List ("LIST OF <elem>").
                if (args.Length > 0 && args[0] is { } v0)
                {
                    switch (v0.GetType().Name)
                    {
                        case "RedBlackTree`2": case "BPlusTree`2": case "BinaryIndexedTree`1":
                            return "DICT";
                        case "BinaryHeap`1": case "MaxHeap`1":
                        {
                            object? first = null; var has = false;
                            if (v0 is System.Collections.IEnumerable en0)
                                foreach (var x0 in en0) { first = x0; has = true; break; }
                            return has ? "LIST OF " + Basic.IlBackend.IlRuntime.TypeInfo(first!) : "LIST OF ANY";
                        }
                    }
                }
                return Basic.IlBackend.IlRuntime.TypeInfo(args.Length > 0 ? args[0]! : null!);
        },
        [BuiltInFunction.TYPEINFOHAS] = (task, args) =>
        {
                return args.Length > 0 && Basic.IlBackend.IlRuntime.TypeInfoHas(args[0]!) != 0 ? 1.0 : 0.0;
        },
        [BuiltInFunction.NEXT] = (task, args) =>
        {
            {
                // NEXT(generatore): riprende un generatore sospeso — sia un'espressione
                // generatore lazy (PCodeGeneratorExprState, materializzata in anticipo, vedi
                // OP_NEW_GENERATOR_EXPR) sia un vero GENERATOR PROCEDURE con YIELD
                // (PCodeGeneratorInstance, guidato passo-passo, vedi DriveGeneratorTask).
                if (args.Length != 1)
                    throw new InvalidOperationException("NEXT() richiede un singolo argomento generatore.");
                if (args[0] is PCodeGeneratorExprState genExprState)
                {
                    if (genExprState.NextPosition >= genExprState.Count)
                        return StopIterationSignal.Instance;
                    return genExprState[genExprState.NextPosition++];
                }
                if (args[0] is PCodeGeneratorInstance genInstance)
                {
                    if (genInstance.Done)
                        return StopIterationSignal.Instance;
                    var result = DriveGeneratorTask(genInstance.Task);
                    if (result is StopIterationSignal)
                        genInstance.Done = true;
                    return result;
                }
                throw new InvalidOperationException(
                    "NEXT() richiede un'istanza di un generatore (GENERATOR PROCEDURE o espressione generatore).");
            }
        },
        [BuiltInFunction.CTYPE] = (task, args) =>
        {
                // Fallback generico per CTYPE(value, typeName) su valori primitivi — la
                // conversione tramite OPERATOR CTYPE definito dall'utente su un'istanza
                // UDT/CLASS NON passa di qui: viene compilata come dispatch diretto via
                // OP_CALL_MEMBER (vedi Compiler.Expressions.cs::VisitBuiltinFunctionCallExpression).
                if (args.Length >= 2 && args[1] is string ctypeTargetName)
                {
                    var ctypeSrc = args[0];
                    switch (ctypeTargetName.ToUpperInvariant())
                    {
                        case "STRING": return ctypeSrc?.ToString() ?? "";
                        case "INTEGER": case "INT": case "LONG": return (double)(long)SafeDouble(ctypeSrc ?? 0);
                        case "DOUBLE": case "SINGLE": case "FLOAT": return SafeDouble(ctypeSrc ?? 0);
                        case "BOOLEAN": case "BIT": return SafeDouble(ctypeSrc ?? 0) != 0.0 ? -1.0 : 0.0;
                    }
                }
                return args.Length > 0 ? args[0] : null;
        },
        [BuiltInFunction.FIX] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0 && args[0] != null) return Basic.IlBackend.IlRuntime.Fix(SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.ROUND] = (task, args) =>
        {
            {
                if (args.Length >= 2 && args[0] != null && args[1] != null)
                    return (double)Basic.IlBackend.IlRuntime.RoundSafe(SafeDouble(args[0]), Convert.ToInt32(args[1]));
                if (args.Length >= 1 && args[0] != null)
                    return (double)Basic.IlBackend.IlRuntime.RoundSafe(SafeDouble(args[0]), 0);
                return 0.0;
            }
        },
        [BuiltInFunction.TRUNC] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return (double)Basic.IlBackend.IlRuntime.TruncSafe(SafeDouble(args[0]), 0);
                return 0.0;
        
            // Funzioni matematiche
        },
        [BuiltInFunction.ABS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Abs(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SGN] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return Basic.IlBackend.IlRuntime.Sgn(SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.PI] = (task, args) =>
        {
                return Math.PI;
        },
        [BuiltInFunction.NOW] = (task, args) =>
        {
                // Runtime comune (IlRuntime): DateTime — unica implementazione.
                return Basic.IlBackend.IlRuntime.Now();
        },
        [BuiltInFunction.TODAY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Today();
        },
        [BuiltInFunction.TIME_FUNC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Time();
        },
        [BuiltInFunction.TIMEDOLLAR] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null)
                {
                    try { return DateTime.FromOADate(SafeDouble(args[0])).ToString("HH:mm:ss"); }
                    catch { return "00:00:00"; }
                }
                return Basic.IlBackend.IlRuntime.StrTime(); // STRTIME()/TIME$ a 0 arg: "HH:MM:SS" UTC (canone)
        },
        [BuiltInFunction.TIMESERIAL] = (task, args) =>
        {
                if (args.Length >= 3) return Basic.IlBackend.IlRuntime.TimeSerial(
                    (short)SafeDouble(args[0]), (short)SafeDouble(args[1]), (short)SafeDouble(args[2])).ToOADate();
                return Basic.IlBackend.IlRuntime.TimeSerial(0, 0, 0).ToOADate();
        },
        [BuiltInFunction.YEAR] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Year(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.MONTH] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Month(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.DAY] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Day(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.HOUR] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Hour(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.MINUTE] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Minute(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.SECOND] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Second(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.WEEKDAY] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Weekday(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.DOW] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.Dow(SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.CDOW] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.Cdow(SafeDouble(args[0]));
                return "";
        },
        [BuiltInFunction.CMONTH] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.Cmonth(SafeDouble(args[0]));
                return "";
        },
        [BuiltInFunction.CTOD] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.CtOD(args[0]?.ToString() ?? "");
                return 0.0;
        },
        [BuiltInFunction.DTOC] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.Dtoc(SafeDouble(args[0]));
                return "";
        },
        [BuiltInFunction.DTOS] = (task, args) =>
        {
                if (args.Length > 0) { try {
                    // Una data JSON arriva come STRINGA ISO ("YYYY-MM-DD") (campo di un report
                    // LEDGER): va PARSA (DateValue), non SafeDouble (che dà 0 → 18991230).
                    var a0 = args[0];
                    if (a0 is string ds && ds.Length >= 8 && (ds.Contains('-') || ds.Contains('/')))
                        return Basic.IlBackend.IlRuntime.Dtos(Basic.IlBackend.IlRuntime.DateValue(ds));
                    return Basic.IlBackend.IlRuntime.Dtos(DateTime.FromOADate(SafeDouble(a0)));
                } catch { return ""; } }
                return "";
        },
        [BuiltInFunction.SECONDS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Seconds();
        },
        [BuiltInFunction.UNIXTIMESTAMP] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.UnixTimestamp(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return Basic.IlBackend.IlRuntime.UnixTimestamp(DateTime.UtcNow);
        },
        [BuiltInFunction.DAYS360] = (task, args) =>
        {
                if (args.Length >= 2) { try { return Basic.IlBackend.IlRuntime.Days360(DateTime.FromOADate(SafeDouble(args[0])), DateTime.FromOADate(SafeDouble(args[1])), false); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.YEARFRAC] = (task, args) =>
        {
                if (args.Length >= 2) { try { return Basic.IlBackend.IlRuntime.YearFrac(DateTime.FromOADate(SafeDouble(args[0])), DateTime.FromOADate(SafeDouble(args[1])), args.Length > 2 ? SafeDouble(args[2]) : 0); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.APPPATHFUNC] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.AppPath();
        },
        [BuiltInFunction.CWDFUNC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Cwd();
        },
        [BuiltInFunction.FILEEXISTSFUNC] = (task, args) =>
        {
                if (args.Length > 0) return System.IO.File.Exists(args[0]?.ToString()) ? -1.0 : 0.0;
                return 0.0;
        },
        [BuiltInFunction.DIREXISTSFUNC] = (task, args) =>
        {
                if (args.Length > 0) return System.IO.Directory.Exists(args[0]?.ToString()) ? -1.0 : 0.0;
                return 0.0;
        },
        [BuiltInFunction.DIRFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.Dir(args[0]?.ToString() ?? "."); } catch { return Array.Empty<string>(); } }
                return Array.Empty<string>();
        },
        [BuiltInFunction.COUNTLINESFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return (double)System.IO.File.ReadAllLines(args[0]?.ToString() ?? "").Length; } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.GETTEMPDIRFUNC] = (task, args) =>
        {
                return System.IO.Path.GetTempPath();
        },
        [BuiltInFunction.GETTEMPFILEFUNC] = (task, args) =>
        {
                return System.IO.Path.GetTempFileName();
        },
        [BuiltInFunction.GETFOLDERPATHFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.GetFolderPath(args[0]?.ToString() ?? "", args.Length > 1 ? SafeDouble(args[1]) : 10); } catch { return ""; } }
                return "";
        },
        [BuiltInFunction.PARENTDIRFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { var p = System.IO.Path.GetDirectoryName(args[0]?.ToString() ?? "."); return p ?? ""; } catch { return ""; } }
                return "";
        },
        [BuiltInFunction.FOLDERSIZEFUNC] = (task, args) =>
        {
                // Sorgente gestita unica: IlRuntime.FolderSize (file → FileInfo.Length,
                // directory → somma ricorsiva). Il vecchio GetFolderSize usava
                // Directory.GetFiles SU UN FILE → eccezione ingoiata → 0 (divergenza
                // IL-vs-PCode del sample HttpFetch: "Dimensione: 0" vs 5969).
                if (args.Length > 0) { try { return (double)Basic.IlBackend.IlRuntime.FolderSize(args[0]?.ToString() ?? "."); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.ENDSWITH] = (task, args) =>
        {
                // Runtime comune (StringHelpers): case-insensitive — unica implementazione.
                if (args.Length >= 2) return StringHelpers.EndsWith(args[0]?.ToString(), args[1]?.ToString()) ? -1.0 : 0.0;
                return 0.0;
        },
        [BuiltInFunction.IIF] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 3) return Basic.IlBackend.IlRuntime.Iif(SafeDouble(args[0]), args[1], args[2]);
                return null;
        },
        [BuiltInFunction.NORMALIZEPATHFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return System.IO.Path.GetFullPath(args[0]?.ToString() ?? "."); } catch { return args[0]?.ToString() ?? ""; } }
                return "";
        },
        [BuiltInFunction.RESOLVEFILEPATHFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.ResolveFilePath(args[0]?.ToString() ?? ""); } catch { return ""; } }
                return "";
        },
        [BuiltInFunction.BINPATHFUNC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BinPath();
        },
        [BuiltInFunction.PROJECTFOLDERFUNC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ProjectFolder();
        },
        [BuiltInFunction.READBYTESFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return System.IO.File.ReadAllBytes(args[0]?.ToString() ?? ""); } catch { return Array.Empty<byte>(); } }
                return Array.Empty<byte>();
        },
        [BuiltInFunction.WRITEBYTESFUNC] = (task, args) =>
        {
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.WriteBytes(args[0]?.ToString() ?? "", args[1]);
                return -1L;
        },
        // --- FONTS: TrueType/OpenType (stessa dylib del runtime, nessuna seconda implementazione) ---
        [BuiltInFunction.TTRENDERFUNC] = (task, args) =>
                args.Length >= 3
                    ? Basic.IlBackend.IlRuntime.TtRender(args[0]?.ToString() ?? "",
                          Convert.ToDouble(args[1]), Convert.ToInt64(args[2]))
                    : Array.Empty<byte>(),
        [BuiltInFunction.TTMETRICSFUNC] = (task, args) =>
                args.Length >= 3
                    ? Basic.IlBackend.IlRuntime.TtMetrics(args[0]?.ToString() ?? "",
                          Convert.ToDouble(args[1]), Convert.ToInt64(args[2]))
                    : Array.Empty<double>(),
        [BuiltInFunction.TTGLYPHSFUNC] = (task, args) =>
                args.Length >= 1
                    ? Basic.IlBackend.IlRuntime.TtGlyphs(args[0]?.ToString() ?? "")
                    : Array.Empty<double>(),
        [BuiltInFunction.READLINESFUNC] = (task, args) =>
        {
                if (args.Length > 0) { try { return System.IO.File.ReadAllLines(args[0]?.ToString() ?? ""); } catch { return Array.Empty<string>(); } }
                return Array.Empty<string>();
        },
        [BuiltInFunction.READFILEFUNC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ReadFile(args.Length > 0 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.JSONTOOBJECT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): converte in Dictionary (JsonElementToObject),
                // come il backend IL — unica implementazione.
                return Basic.IlBackend.IlRuntime.JsonToObject(
                    args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.PETSCII] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0 && args[0] is string ps) return Basic.IlBackend.IlRuntime.Petscii(ps);
                return "";
        },
        [BuiltInFunction.TAB] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Tab();
        },
        [BuiltInFunction.TISTRING] = (task, args) =>
        {
                // STRTTI/TI$: funzione SENZA argomenti (IlRuntime.StrTi = CoreTiString).
                // Prima ritornava args[0]?.ToString() → stringa vuota su `DATETIME.STRTI()`.
                return Basic.IlBackend.IlRuntime.StrTi();
        },
        [BuiltInFunction.STRPTR] = (task, args) =>
        {
return (double)CommonLib.UdtUtils.VarPtr(args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.VARPTR] = (task, args) =>
        {
return (double)CommonLib.UdtUtils.VarPtr(args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.RGB] = (task, args) =>
        {
            {
                // RGB(r, g, b [, a]) -> RgbaColor dict {Red,Green,Blue,Alpha} (canone StdLib
                // RgbaColorType; stessa rappresentazione di IL e VM/C gmb_rgb).
                var rr = SafeDouble(ArgOr(args, 0));
                var gg = SafeDouble(ArgOr(args, 1));
                var bb = SafeDouble(ArgOr(args, 2));
                var aa = args.Length > 3 ? SafeDouble(args[3]) : 255.0;
                return new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["Red"] = Math.Clamp(rr, 0, 255), ["Green"] = Math.Clamp(gg, 0, 255),
                    ["Blue"] = Math.Clamp(bb, 0, 255), ["Alpha"] = Math.Clamp(aa, 0, 255)
                };
            }
        },
        [BuiltInFunction.VALTYPE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0)
                {
                    // Collezioni PCode con classi CLR dedicate: IL le materializza come
                    // SortedDictionary/List (ICollection → "A"); normalizza al canone IL.
                    if (args[0] is { } v0)
                    {
                        switch (v0.GetType().Name)
                        {
                            case "RedBlackTree`2": case "BPlusTree`2": case "BinaryIndexedTree`1":
                            case "BinaryHeap`1": case "MaxHeap`1":
                                return "A";
                        }
                    }
                    return Basic.IlBackend.IlRuntime.ValType(args[0]);
                }
                return "U";
        },
        [BuiltInFunction.POINT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): indice colore del pixel via
                // GraphicsManager.PointValue — unica implementazione.
                if (args.Length >= 2)
                    return Basic.IlBackend.IlRuntime.Point(SafeDouble(args[0]), SafeDouble(args[1]));
                return -1.0;
        },
        [BuiltInFunction.SCREENINFO] = (task, args) =>
        {
                // ScreenInfo() -> DisplayMode dict {ModeID,Width,Height,Bpp} (canone StdLib).
                return new System.Collections.Generic.Dictionary<string, object?>
                {
                ["ModeID"] = 0.0, ["Width"] = 80.0, ["Height"] = 25.0, ["Bpp"] = 0.0,
                ["Colors"] = 16.0, ["Active"] = -1.0, ["Description"] = "Text 80x25"
                };
        },
        [BuiltInFunction.SCREENS] = (task, args) =>
        {
                return 1.0;
        },
        [BuiltInFunction.SCREEN] = (task, args) =>
        {
                // SCREEN() come funzione: la modalità grafica corrente (0 = testo/console).
                return (double)PCodeGraphics.Instance.CurrentScreenMode;
        },
        [BuiltInFunction.COLLIDE] = (task, args) =>
        {
                // COLLIDE(id1, id2) → -1/0 (AABB) oppure COLLIDE(id) → int[] di ID collisi.
                // Sincrono: senza SCREEN o sprite mancanti restituisce false/array vuoto
                // (comportamento atteso del sample SpriteCollision.bas senza finestra).
                if (args.Length >= 2)
                {
                    var cId1 = (int)SafeDouble(args[0]);
                    var cId2 = (int)SafeDouble(args[1]);
                    return PCodeGraphics.Instance.Collide(cId1, cId2) ? -1L : 0L;
                }
                if (args.Length == 1)
                {
                    var cId = (int)SafeDouble(args[0]);
                    var hits = PCodeGraphics.Instance.CollideAll(cId);
                    return hits.Select(h => (object)(long)h).ToList();
                }
                return 0L;
        },
        [BuiltInFunction.COLLIDEBATCH] = (task, args) =>
        {
            {
                // COLLIDEBATCH(X(), Y(), w, h) → int[] coppie (i,j) in collisione AABB.
                // Broad+narrow phase con spatial grid su ARRAY (non sul sistema sprite):
                // O(n) medio, zero allocazioni per frame (coppie in un buffer riusato).
                if (args.Length >= 4)
                {
                    var xs = args[0] is System.Collections.IEnumerable xe ? xe.Cast<object?>().Select(o => (float)SafeDouble(o)).ToArray() : null;
                    var ys = args[1] is System.Collections.IEnumerable ye ? ye.Cast<object?>().Select(o => (float)SafeDouble(o)).ToArray() : null;
                    if (xs != null && ys != null && xs.Length == ys.Length)
                    {
                        var bw = (float)SafeDouble(args[2]);
                        var bh = (float)SafeDouble(args[3]);
                        return PCodeGraphics.Instance.CollideBatch(xs, ys, bw, bh);
                    }
                }
                return new int[0];
            }
        },
        [BuiltInFunction.FOPEN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): firma canonica FOpen(fileName) — unica impl.
                if (args.Length >= 1) return (double)Basic.IlBackend.IlRuntime.FOpen(args[0]?.ToString() ?? "");
                return -1.0;
        },
        [BuiltInFunction.FCLOSE] = (task, args) =>
        {
                if (args.Length > 0) return (double)Basic.IlBackend.IlRuntime.FClose((long)SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.FREAD] = (task, args) =>
        {
                // FUNZIONE PURA: ritorna i dati letti (una riga se count omesso, altrimenti count
                // byte). La scrittura in un buffer e' il COMANDO `FREAD handle INTO buffer`.
                if (args.Length < 1) return "";
                var fh = (long)SafeDouble(args[0]);
                var fc = args.Length >= 2 ? (long)SafeDouble(args[1]) : 0L;
                return Basic.IlBackend.IlRuntime.FRead(fh, fc);
        },
        [BuiltInFunction.FWRITE] = (task, args) =>
        {
                if (args.Length >= 2) return (double)Basic.IlBackend.IlRuntime.FWrite((long)SafeDouble(args[0]), args[1]);
                return 0.0;
        },
        [BuiltInFunction.FSEEK] = (task, args) =>
        {
                // Runtime comune (IlRuntime): FSeek(h, pos) o Seek(fileNumber) — unica
                // implementazione. Con UN argomento e' IO.SEEK(fileNumber): il numero va
                // risolto sui file legacy (`OPEN ... AS #n`), non solo sugli handle di
                // FOpen, altrimenti lancia "Handle file non valido"
                // (§ trovato da MiniTests/io/io_misc.bas).
                if (args.Length >= 2) return (double)Basic.IlBackend.IlRuntime.FSeek((long)SafeDouble(args[0]), (long)SafeDouble(args[1]));
                if (args.Length >= 1)
                {
                    // Con UN argomento e' IO.SEEK(fileNumber): il numero puo' essere un file
                    // LEGACY (`OPEN ... AS #n`), che in PCode vive in task.FileHandles (come in
                    // EOFFUNC), oppure un handle di FOpen. Prima si chiamava sempre
                    // Seek(handle) e con un numero legacy lanciava "Handle file non valido"
                    // (§ trovato da MiniTests/io/io_misc.bas).
                    var seekNum = (long)SafeDouble(args[0]);
                    if (task.FileHandles.TryGetValue((int)seekNum, out var seekH))
                        return (double)_runtime.File.GetPosition(seekH);
                    return (double)Basic.IlBackend.IlRuntime.FileSeekNumber(seekNum);
                }
                return 0.0;
        },
        [BuiltInFunction.FERROR] = (task, args) =>
        {
return (double)CommonLib.FileReadWriteUtils.FileError;
        },
        [BuiltInFunction.STACKPUSH] = (task, args) =>
        {
                // Con [istanza, valore]: push sul Stack BASIC (dall'istanza). Il primo arg
                // è l'istanza Stack (il Linker la antepone), il secondo il valore.
                if (args.Length >= 2 && args[0] is System.Collections.Generic.Stack<object?> sPush)
                { sPush.Push(args[1]); return 0.0; }
                if (args.Length >= 2 && args[0] is System.Collections.IList sPushL)
                { sPushL.Add(args[1]); return 0.0; }
                if (args.Length > 0) { _vmStack ??= new(); _vmStack.Push(args[0]); }
                return 0.0;
        },
        [BuiltInFunction.STACKPOP] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.Generic.Stack<object?> sPop)
                    return sPop.Count > 0 ? sPop.Pop() : null;
                if (args.Length >= 1 && args[0] is System.Collections.IList sPopL && sPopL.Count > 0)
                { var pv = sPopL[sPopL.Count - 1]; sPopL.RemoveAt(sPopL.Count - 1); return pv; }
                return _vmStack?.Count > 0 ? _vmStack.Pop() : null;
        },
        [BuiltInFunction.CELL] = (task, args) =>
        {
            // CELL(row,col,mode): attributo headless — FG=7 / BG=0, parità IL IlRuntime.CellValue.
            return (args.Length >= 3 && SafeDouble(args[2]) >= 1) ? 0.0 : 7.0;
        },
        [BuiltInFunction.CELLSTRING] = (task, args) =>
        {
if (args.Length >= 2) { var row = (int)SafeDouble(args[0]); var col = (int)SafeDouble(args[1]); try { var left = _runtime.Console.CursorLeft; var top = _runtime.Console.CursorTop; _runtime.Console.SetCursorPosition(col, row); _runtime.Console.SetCursorPosition(left, top); } catch { } } return "";
        },
        [BuiltInFunction.DEQUEUE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0 && args[0] is System.Collections.IList q && q.Count > 0) return Basic.IlBackend.IlRuntime.Dequeue(q);
                return null;
        },
        [BuiltInFunction.ENQUEUE] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList q2) { Basic.IlBackend.IlRuntime.Enqueue(q2, args[1]!); }
                return 0.0;
        },
        [BuiltInFunction.FCREATE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0) return (double)Basic.IlBackend.IlRuntime.FCreate(args[0]?.ToString() ?? "");
                return -1.0;
        },
        [BuiltInFunction.FETCH] = (task, args) =>
        {
            {
                var url = args.Length > 0 ? args[0]?.ToString() ?? "" : "";
                var options = ConvertPCodeHttpOptions(args.Length > 1 ? args[1] : null);
                ParkAsync(task, PCodeWaitKind.Http,
                    ct => new ValueTask<CommonLib.Http.FetchResult>(
                        _runtime.HttpClientHelper.PerformRequestAsync(url, options, ct)),
                    task.PushRef);
                return ParkedBuiltinResult;
            }
            // --- HTTP: delegato a ExecuteBuiltInHttp (PCodeProcessor.Builtins.Http.cs) ---
        },
        [BuiltInFunction.HOST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Host();
        },
        [BuiltInFunction.LOCALPORT] = (task, args) =>
        {
                // Handle BASIC = ID numerico (long), non oggetto → risolvi via servizio.
                if (args.Length > 0 && args[0] != null && _runtime.Network.GetNetworkHandle(Convert.ToInt64(args[0])) is { } nhLP)
                    return (double)CommonLib.Network.NetworkUtils.GetLocalPort(nhLP);
                return 0.0;
        },
        [BuiltInFunction.PORT] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null && _runtime.Network.GetNetworkHandle(Convert.ToInt64(args[0])) is { } nhP)
                    return (double)CommonLib.Network.NetworkUtils.GetRemotePort(nhP);
                return 0.0;
        },
        [BuiltInFunction.REMOTE] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null && _runtime.Network.GetNetworkHandle(Convert.ToInt64(args[0])) is { } nhR)
                    return CommonLib.Network.NetworkUtils.GetRemoteAddress(nhR);
                return "";
        },
        [BuiltInFunction.RECV] = (task, args) =>
        {
                // RECV(handle) — byte attualmente disponibili sul socket (stesso contratto del
                // deprecated Gpm.Network.Functions.EvaluateRecvFromStack). Prima il mapping
                // TokenKind.Recv puntava a RECNO (record number) e il case 'is NetworkHandle'
                // falliva perché il compilatore passa l'ID numerico (long) → RECV()=0 sempre.
                if (args.Length > 0 && args[0] != null && _runtime.Network.GetNetworkHandle(Convert.ToInt64(args[0])) is { } recvH)
                {
                    switch (recvH)
                    {
                        case CommonLib.Network.TcpClientHandle tcpCh:
                            return (double)tcpCh.ClientSocket.Available;
                        case CommonLib.Network.AsyncTcpClientHandle ach:
                            return (double)ach.Client.Client.Available;
                        case CommonLib.Network.UdpHandle uh:
                            return (double)uh.UdpSocket.Available;
                    }
                }
                return 0.0;
        },
        [BuiltInFunction.GETHOSTIP] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.GetHostIp(args[0]?.ToString() ?? "");
                return "";
        },
        [BuiltInFunction.NETERR] = (task, args) =>
        {
return (double)CommonLib.Network.NetworkUtils.NetErr;
        },
        [BuiltInFunction.READSECTION] = (task, args) =>
        {
if (args.Length >= 2) { var file = args[0]?.ToString() ?? ""; var section = args[1]?.ToString() ?? ""; return CommonLib.FileReadWriteUtils.ReadSectionObject(file, section); } if (args.Length == 1) return CommonLib.FileReadWriteUtils.ReadSectionObject(Path.Combine(Directory.GetCurrentDirectory(), "app.settings.json"), args[0]?.ToString() ?? ""); return new System.Collections.Generic.Dictionary<string, object?>();
        },
        [BuiltInFunction.PIXELBUFFER] = (task, args) =>
        {
                // PIXELBUFFER(w, h, array): disegna un buffer RGB (row-major, pack 0x00RRGGBB)
                // come texture a schermo intero (GraphicsManager.EnqueuePixelBuffer → main
                // thread, in ordine con le altre primitive). Ritorna null (azione).
                if (args.Length >= 3 && PCodeGraphics.Instance.IsScreenOn)
                {
                    var w = (int)SafeDouble(args[0]); var h = (int)SafeDouble(args[1]);
                    var pixels = ToDoubleArr(args[2]);
                    if (w > 0 && h > 0 && pixels.Length > 0)
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
                return null;
        },
        [BuiltInFunction.DIVIDENDPAYOUTRATIO] = (task, args) =>
        {
                if (args.Length >= 2) { try { return CommonLib.Trading.General.DividendPayoutRatio(SafeDouble(args[0]), SafeDouble(args[1])); } catch { } } return 0.0;
        },
        [BuiltInFunction.PBRATIO] = (task, args) =>
        {
                if (args.Length >= 2) { try { return CommonLib.Trading.General.PbRatio(SafeDouble(args[0]), SafeDouble(args[1])); } catch { } } return 0.0;
        },
        [BuiltInFunction.PERATIO] = (task, args) =>
        {
                if (args.Length >= 2) { try { return CommonLib.Trading.General.PeRatio(SafeDouble(args[0]), SafeDouble(args[1])); } catch { } } return 0.0;
        },
        [BuiltInFunction.FVEXT] = (task, args) =>
        {
                if (args.Length >= 1) return (double)CommonLib.Numerics.FvExt((decimal)SafeDouble(args[0]), (decimal)SafeDouble(ArgOr(args,1)), (decimal)SafeDouble(ArgOr(args,2)), (decimal)SafeDouble(ArgOr(args,3)), (int)SafeDouble(ArgOr(args,4)));
                return 0.0;
        },
        [BuiltInFunction.PVEXT] = (task, args) =>
        {
                if (args.Length >= 1) return (double)CommonLib.Numerics.PvExt((decimal)SafeDouble(args[0]), (decimal)SafeDouble(ArgOr(args,1)), (decimal)SafeDouble(ArgOr(args,2)), (decimal)SafeDouble(ArgOr(args,3)), (int)SafeDouble(ArgOr(args,4)));
                return 0.0;
        },
        [BuiltInFunction.PMTEXT] = (task, args) =>
        {
                if (args.Length >= 1) return (double)CommonLib.Numerics.PmtExt((decimal)SafeDouble(args[0]), (decimal)SafeDouble(ArgOr(args,1)), (decimal)SafeDouble(ArgOr(args,2)), (decimal)SafeDouble(ArgOr(args,3)), (int)SafeDouble(ArgOr(args,4)));
                return 0.0;
        },
        [BuiltInFunction.NPEREXT] = (task, args) =>
        {
                if (args.Length >= 1) return CommonLib.Numerics.NperExt(SafeDouble(args[0]), SafeDouble(ArgOr(args,1)), SafeDouble(ArgOr(args,2)), SafeDouble(ArgOr(args,3)), (int)SafeDouble(ArgOr(args,4)));
                return 0.0;
        },
        [BuiltInFunction.MNPV] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa implementazione — unica soluzione.
                if (args.Length >= 2 && args[1] is System.Collections.IList cf2) return Basic.IlBackend.IlRuntime.Mnpv(SafeDouble(args[0]), ToDoubleArr(cf2));
                return 0.0;
        },
        [BuiltInFunction.CONN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.Conn(args[0]);
                return 0.0;
        },
        [BuiltInFunction.LOF] = (task, args) =>
        {
                // LOF(n): file number legacy (USING/OPEN ... AS #n) → handle dal registro
                // del task (task.FileHandles, popolato dall'OPEN della VM) o dal registro
                // globale (IlRuntime.FileNumberToHandle, percorso IL); altrimenti il numero
                // È già un handle IO. Prima: Lof() diretto su un file number → errore/0.
                if (args.Length > 0)
                {
                    var lofNum = (long)SafeDouble(args[0]);
                    // Il file number della VM mappa un handle dell'IFileService del task
                    // (NON il registry interno di IlRuntime: sono due registri distinti).
                    if (task.FileHandles.TryGetValue((int)lofNum, out var lofH))
                        return (double)_runtime.File.GetLength(lofH);
                    return (double)Basic.IlBackend.IlRuntime.FileLofNumber(lofNum);
                }
                return 0.0;
        },
        [BuiltInFunction.EOFFUNC] = (task, args) =>
        {
            {
                // EOF condiviso tra FILE, DBF e ISAM. DBF.EOF()/ISAM.EOF() cadevano sempre nel
                // ramo file → ritornavano -1 (true) anche a inizio file → i loop
                // "DO WHILE NOT DBF.EOF()" non iteravano MAI. Prima: area DBF/ISAM per nome
                // alias o area corrente; poi il file sequenziale (IO.EOF(n)).
                if (args.Length > 0 && args[0] is string eofAlias)
                {
                    // SPEED.EOF(alias): cursore dell'area SPEED (stesso registro alias di
                    // SPEED GET/PUT). Si legge il record corrente dal DB (come GET): il
                    // flag `_eof` del driver può essere stale (GO TOP non lo risincronizza).
                    // Prima cadeva sul ramo FILE → sempre true.
                    if (_dataSession.FindAlias(eofAlias) is Basic.Databases.Rdd.SpeedDriver spEof)
                        return (spEof._db?.GetCurrent(spEof._tableId) == null) ? -1.0 : 0.0;
                    var eofEngine = ResolveDbfEngine(task, eofAlias);
                    if (eofEngine != null) return eofEngine.Eof ? -1.0 : 0.0;
                    // Driver RDD NON-DBF (CSV/DELIM/SDF/ARRAY): ResolveDbfEngine copre solo
                    // DbfDriver → null; navigazione via l'interfaccia IDatabaseDriver (come IL,
                    // che registra l'area RDD in _dbfHandles e legge Eof dall'engine). Senza,
                    // DBF.EOF() cadeva sul ramo FILE → -1 (sempre vero) → i loop
                    // DO WHILE NOT DBF.EOF() su area CSV non iteravano (CustomerOrdersReport).
                    var eofDrv = ResolveDbfDriver(task, eofAlias);
                    if (eofDrv != null) return eofDrv.Eof ? -1.0 : 0.0;
                    var eofWa = ResolveIsamArea(task, eofAlias);
                    if (eofWa?.Engine != null) return eofWa.Engine.IsEof() ? -1.0 : 0.0;
                }
                if (args.Length == 0)
                {
                    var curEngine = ResolveDbfEngine(task, null);
                    if (curEngine != null) return curEngine.Eof ? -1.0 : 0.0;
                    var curDrv = ResolveDbfDriver(task, null);
                    if (curDrv != null) return curDrv.Eof ? -1.0 : 0.0;
                    var curWa = ResolveIsamArea(task, null);
                    if (curWa?.Engine != null) return curWa.Engine.IsEof() ? -1.0 : 0.0;
                }
                // File sequenziale (IO.EOF(n)): true se la posizione corrente è alla fine.
                // BIT true = -1 (NOT -1 = 0 in BASIC bitwise), altrimenti `NOT EOF` non esce
                // e il WHILE NOT EOF di FilesIO gira all'infinito (porting da GMBasic).
                if (args.Length > 0)
                {
                    var eofFileNum = (int)SafeDouble(ArgOr(args, 0));
                    if (task.FileHandles.TryGetValue(eofFileNum, out var eofH))
                    {
                        // IsEof (non GetPosition>=GetLength): con un reader di riga bufferizzato
                        // la POSIZIONE e' a fine file gia' dopo la prima ReadLine → EOF troppo
                        // presto (bug: `DO WHILE NOT EOF(F)` leggeva una sola riga).
                        try { return _runtime.File.IsEof(eofH) ? -1.0 : 0.0; }
                        catch { return -1.0; }
                    }
                    // Handle di FOpen (non un numero di file legacy): stessa implementazione
                    // del backend IL (IlRuntime.Eof: Position >= Length) — unica sorgente di
                    // verita' (§0bis). Prima si cadeva nel `return -1.0` finale, quindi
                    // IO.EOF(handle) era SEMPRE true e divergeva da IL: trovato da
                    // MiniTests/io/io_handles.bas (il file aveva ancora una riga da leggere).
                    try { return Basic.IlBackend.IlRuntime.Eof(eofFileNum) ? -1.0 : 0.0; }
                    catch { return -1.0; }
                }
                return -1.0;
            }
        },
        [BuiltInFunction.BOF] = (task, args) =>
        {
                // BOF (non-qualificato): inizio del file per l'area ISAM/DBF.
                // (ISAM.BOF qualificato va al syscall ISAM_BOF — questo copre la forma diretta.)
                if (args.Length > 0)
                {
                    // LEDGER.BOF(h): handle del journal → cursore via DatabaseService (come IL).
                    if (args[0] is double || args[0] is int)
                    {
                        var bh = args[0] is double bd ? (long)bd : (long)(int)args[0]!;
                        if (ResolveLedgerCtx((int)bh) != null)
                            return Basic.IlBackend.IlRuntime.LedgerBofByHandle(bh);
                    }
                    var wa = ResolveIsamArea(task, args[0]);
                    return wa?.Engine != null && wa.Engine.IsBof() ? -1.0 : 0.0;
                }
                return 0.0;
            // --- TEAM: delegato a ExecuteBuiltInTeam (PCodeProcessor.Builtins.Team.cs) ---
        },
        [BuiltInFunction.LUPDATE] = (task, args) =>
        {
                // LUPDATE() è una DATA dell'area DBF: la sessione è CONDIVISA con IlRuntime,
                // quindi si delega alla stessa implementazione (prima questo handler leggeva
                // un PATH passato come argomento e rispondeva 0 → IL/PCode divergevano,
                // § MiniTests/dbf/zz_a).
                return Basic.IlBackend.IlRuntime.DbfLupdate(null);
        },
        [BuiltInFunction.MAXRETURN] = (task, args) =>
        {
                // MAXRETURN(rendimenti) del namespace TRADING.PORTFOLIO → PortfolioResult dict
                // (parità VM/C case 193 e StdLib PortfolioResultType). Prima ritornava il
                // double numerics: incoerente col tipo dichiarato.
                if (args.Length > 0 && args[0] is System.Collections.IList mrx) return Basic.IlBackend.IlRuntime.PortfolioMaxReturn(ToDoubleArray(mrx));
                return new Dictionary<string, object?>();
        },
        [BuiltInFunction.MINSEMIVARIANCE] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is System.Collections.IList mvx) return CommonLib.Numerics.MinSemiVariance(ToDoubleArray(mvx), SafeDouble(ArgOr(args, 1)));
                return 0.0;
        },
        [BuiltInFunction.OUTLIERS] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is System.Collections.IList olx) return CommonLib.Numerics.Outliers(ToDoubleArray(olx), SafeDouble(ArgOr(args, 1, 3.0)));
                return null;
        },
        [BuiltInFunction.REGRESS] = (task, args) =>
        {
                // Ritorna un Dictionary invece di un double[] posizionale: RegressionResultType
                // (dichiarato come tipo di ritorno in StdLib.Analysis.cs) fa già type-check a
                // compile-time contro DIM RegRes AS Analysis.RegressionResult — a runtime,
                // OP_LOAD_FIELD (PCodeProcessor.cs) ha già un case generico per IDictionary che
                // risolve .Slope/.Intercept/.R2 per nome, nessun nuovo plumbing necessario (stesso
                // fix di GMBasic, TODO AnalysisTests-ADFTEST-ARIMA-KMEANS bug #3).
                if (args.Length >= 2 && args[0] is System.Collections.IList rx && args[1] is System.Collections.IList ry) {
                    var (slope, intercept, r2) = CommonLib.Numerics.LinearRegression(ToDoubleArray(rx), ToDoubleArray(ry));
                    return new Dictionary<string, object?> { ["Slope"] = slope, ["Intercept"] = intercept, ["R2"] = r2 };
                }
                return null;
        },
        [BuiltInFunction.ROLLINGREGRESSION] = (task, args) =>
        {
                if (args.Length >= 3 && args[0] is System.Collections.IList rrx && args[1] is System.Collections.IList rry) {
                    // Parità IL (IlRuntime.RollingRegression): matrice [finestre][2] = [slope,intercept].
                    return CommonLib.Numerics.RollingRegressionPairs(ToDoubleArray(rrx), ToDoubleArray(rry), (int)SafeDouble(args[2]))
                        .Select(item => new[] { item.slope, item.intercept }).ToArray();
                }
                return null;
        },
        [BuiltInFunction.ROLLINGREGRESSIONSINGLE] = (task, args) =>
        {
                return CommonLib.Numerics.RollingRegression(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)))
                    .Select(item => new[] { item.slope, item.intercept }).ToArray();
        },
        [BuiltInFunction.YULEWALKER] = (task, args) =>
        {
                return CommonLib.Numerics.YuleWalker(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.MACOEFFICIENTS] = (task, args) =>
        {
                return CommonLib.Numerics.MaCoefficients(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.EUCLIDEANDISTANCESQUARED] = (task, args) =>
        {
                return CommonLib.Numerics.EuclideanDistanceSquared(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
        },
        [BuiltInFunction.PCA] = (task, args) =>
        {
                double[,] mat;
                var pcaArg = ArgOr(args, 0);
                if (pcaArg is double[,] rect)
                {
                    // Matrice RETTANGOLARE DIM d(m-1,n-1) (double[,]) → PCA direttamente.
                    mat = rect;
                }
                else if (pcaArg is System.Array arr2 && arr2.Rank == 2)
                {
                    // DIM matrice PCode = object[,] (multidimensionale), NON double[,] e NON
                    // IList-di-righe: va letto con GetLength/GetValue, altrimenti si iterano i
                    // singoli elementi (flat) e la matrice risulta vuota → PCA null → 0
                    // (bug Analysis.Pca: PCode 0,0 vs IL 8.3333,0).
                    int rr = arr2.GetLength(0), cc = arr2.GetLength(1);
                    mat = new double[rr, cc];
                    for (int i = 0; i < rr; i++) for (int j = 0; j < cc; j++) mat[i, j] = SafeDouble(arr2.GetValue(i, j));
                }
                else if (pcaArg is System.Collections.IList pcaRows)
                {
                    var pcaData = new List<double[]>();
                    foreach (var row in pcaRows)
                        if (row is System.Collections.IList rl2) pcaData.Add(ToDoubleArray(rl2));
                    if (pcaData.Count == 0) return null;
                    int cols = pcaData[0].Length;
                    mat = new double[pcaData.Count, cols];
                    for (int i = 0; i < pcaData.Count; i++) for (int j = 0; j < cols; j++) mat[i, j] = pcaData[i][j];
                }
                else return null;
                object RunPca()
                {
                    var (ev, evec) = CommonLib.Numerics.PCA(mat);
                    // Canone Analysis.PcaResult (IlRuntime.Pca): DICT {EigenValues, EigenVectors}
                    // — prima era una List posizionale [ev, evec] e i membri .EigenValues/.EigenVectors
                    // restavano vuoti (PCode stampava 0/0 vs IL 1.6266/0.0525).
                    return new Dictionary<string, object?> { ["EigenValues"] = ev, ["EigenVectors"] = evec };
                }
                return mat.GetLength(0) * mat.GetLength(1) >= 100_000
                    ? ParkCpuBuiltin(task, RunPca)
                    : RunPca();
        },
        [BuiltInFunction.ANOVA] = (task, args) =>
        {
                // ANALYSIS.ANOVA (token ANOVA, Analysis.AnovaResult): dict ESTESO
                // {FValue, PValue, DFBetween, DFWithin, SSBetween, SSWithin, MSBetween,
                // MSWithin} — IlRuntime.Anova. IlRuntime.StatsAnova (HypothesisResult con
                // Statistic/PValue/DegreesOfFreedom) è SOLO per STATS.ANOVA (token STATSANOVA,
                // handler in Builtins.Stats.cs): qui produceva un dict ridotto e i membri
                // FValue/DF* restavano vuoti (PCode stampava solo PValue vs IL completo).
                if (args.Length > 0 && args[0] is System.Collections.IList anovaGroups)
                {
                    var groups = new List<double[]>();
                    foreach (var g in anovaGroups)
                        if (g is System.Collections.IList gl2) groups.Add(ToDoubleArray(gl2));
                    return Basic.IlBackend.IlRuntime.Anova(groups.ToArray());
                }
                return null;
        },
        [BuiltInFunction.READVALUE] = (task, args) =>
        {
if (args.Length >= 3) { var file = args[0]?.ToString() ?? ""; var section = args[1]?.ToString() ?? ""; var key = args[2]?.ToString() ?? ""; var def = args.Length > 3 ? args[3]?.ToString() ?? "" : ""; return CommonLib.FileReadWriteUtils.ReadValue(file, section, key, def); } if (args.Length == 1) return CommonLib.FileReadWriteUtils.ReadValue(Path.Combine(Directory.GetCurrentDirectory(), "app.settings.json"), "", args[0]?.ToString() ?? "", ""); return "";
        
        },
        [BuiltInFunction.IFFTCOMPLEX] = (task, args) =>
        {
                // object[,] n×2 (CommonLib.Numerics.Ifft), come IFFT reale — il DIM matrice
                // PCode materializza object[,], NON double[][] (IlRuntime.IfftComplex) → vuoto.
                if (args.Length >= 2)
                {
                    var real = ToDoubleArr(ArgOr(args, 0));
                    var imag = ToDoubleArr(ArgOr(args, 1));
                    return real.Length >= 4096
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Ifft(real, imag))
                        : CommonLib.Numerics.Ifft(real, imag);
                }
                var ifftComplexMatrix = ToDoubleMatrix(ArgOr(args, 0));
                return ifftComplexMatrix.Length >= 4096
                    ? ParkCpuBuiltin(task, () => Basic.IlBackend.IlRuntime.Ifft(ifftComplexMatrix))
                    : Basic.IlBackend.IlRuntime.Ifft(ifftComplexMatrix);
        },
        [BuiltInFunction.ALERT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): ALERT("msg",...) e CONFIRM("msg",...) delegano a
                // IlRuntime.Confirm (stessa logica AlertBox con pulsanti). buttons=0 → solo OK
                // (ALERT); >100 → etichette %OKCANCEL/%YESNO/... (CONFIRM).
            {
                double ArgN(int i, double def) => args.Length > i && args[i] != null ? SafeDouble(args[i]) : def;
                return Basic.IlBackend.IlRuntime.Confirm(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgN(1, 0), ArgN(2, 1), ArgN(3, -1), ArgN(4, -1), ArgN(5, 1), ArgN(6, 0));
            }
        
        
            // --- Optimize: minimizzazione/root-finding multivariata. Firma del parametro
            // funzione CORRETTA in StdLib.Optimize.cs da scalare a vettoriale (era
            // FunctionPointerType([Double],Double), non poteva rappresentare una funzione di più
            // variabili — nessuna di queste era mai dispatchata, nessuna firma preesistente da
            // rompere). L'array BASIC passato alla closure è un List<object?> di double boxed,
            // stessa convenzione di ritorno usata da GETRANGE/TOLIST.
        },
        [BuiltInFunction.CONFIRM] = (task, args) =>
        {
                // Runtime comune (IlRuntime): ALERT("msg",...) e CONFIRM("msg",...) delegano a
                // IlRuntime.Confirm (stessa logica AlertBox con pulsanti). buttons=0 → solo OK
                // (ALERT); >100 → etichette %OKCANCEL/%YESNO/... (CONFIRM).
            {
                double ArgN(int i, double def) => args.Length > i && args[i] != null ? SafeDouble(args[i]) : def;
                return Basic.IlBackend.IlRuntime.Confirm(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgN(1, 0), ArgN(2, 1), ArgN(3, -1), ArgN(4, -1), ArgN(5, 1), ArgN(6, 0));
            }
        
        
            // --- Optimize: minimizzazione/root-finding multivariata. Firma del parametro
            // funzione CORRETTA in StdLib.Optimize.cs da scalare a vettoriale (era
            // FunctionPointerType([Double],Double), non poteva rappresentare una funzione di più
            // variabili — nessuna di queste era mai dispatchata, nessuna firma preesistente da
            // rompere). L'array BASIC passato alla closure è un List<object?> di double boxed,
            // stessa convenzione di ritorno usata da GETRANGE/TOLIST.
        },
        [BuiltInFunction.IFFT] = (task, args) =>
        {
                // FFT/IFFT ritornano object[n,2] (colonna 0 = reale, colonna 1 = immaginaria —
                // vedi CommonLib/Numerics/FFT.cs). Se IFFT riceve DIRETTAMENTE l'output di FFT
                // (il round-trip FFT→IFFT usato da AnalysisTests.bas), va spacchettato nelle due
                // colonne e passato all'overload Ifft(real,imag): appiattire la matrice con
                // ToDoubleArray (il vecchio comportamento) la trattava come un singolo array 1D di
                // 2n valori concatenati riga per riga — un segnale nel dominio del tempo senza
                // senso, che non ricostruiva mai il segnale originale.
                if (args.Length > 0)
                {
                    if (args[0] is object[,] ifftMatrix && ifftMatrix.GetLength(1) >= 2)
                    {
                        var n = ifftMatrix.GetLength(0);
                        var real = new double[n];
                        var imag = new double[n];
                        for (var i = 0; i < n; i++) { real[i] = SafeDouble(ifftMatrix[i, 0]); imag[i] = SafeDouble(ifftMatrix[i, 1]); }
                        return real.Length >= 4096
                            ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Ifft(real, imag))
                            : CommonLib.Numerics.Ifft(real, imag);
                    }
                    if (args[0] is System.Collections.IList ifftIn)
                    {
                        var input = ToDoubleArray(ifftIn);
                        return input.Length >= 4096
                            ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Ifft(input))
                            : CommonLib.Numerics.Ifft(input);
                    }
                }
                return null;
        },
        [BuiltInFunction.SELECT] = (task, args) =>
        {
{ var arx = (int)(args.Length > 0 ? SafeDouble(args[0]) : 0); return arx > 0 && _runtime.SqlDataSession.GetWorkArea(arx) != null ? 1.0 : 0.0; }
        },
        [BuiltInFunction.FOUND] = (task, args) =>
        {
            {
                // ISAM.FOUND/DBF.FOUND: TryEmitDatabaseSyscall esclude esplicitamente FOUND dal
                // dispatch SysCall dedicato (a differenza di EOF/BOF), quindi arriva qui come
                // BuiltInFunction generico — mancava questo case, quindi tornava sempre falsy
                // (verificato: SEEK su un record esistente comunque riportava FOUND=0).
                var foundFileRef = args.Length > 0 ? args[0] : null;
                if (foundFileRef == null)
                {
                    // Nessun alias esplicito (es. "DBF.FOUND()" senza argomenti): ResolveIsamDriver
                    // risolve SOLO aree ISAM (via task.CurrentIsamArea), quindi per un programma
                    // puramente DBF restituiva sempre null → FOUND sempre falso, indipendentemente
                    // dall'esito reale dell'ultima SEEK (verificato: DBF SEEK su una chiave
                    // esistente posizionava correttamente il cursore ma DBF.FOUND() restituiva
                    // comunque 0). task.LastIsamFound è il flag condiviso già impostato da
                    // DBF_SEEK/ISAM_SEEK/SPEED_SEEK — usarlo qui copre tutti e tre i motori quando
                    // non è specificato un alias esplicito.
                    return task.LastIsamFound ? -1.0 : 0.0;
                }
                var foundDriver = ResolveIsamDriver(task, foundFileRef);
                return foundDriver != null && foundDriver.Found ? -1.0 : 0.0;
            }
        },
        [BuiltInFunction.LASTREC] = (task, args) =>
        {
            {
                /* LEDGER.LASTREC(h) usa il token CONDIVISO Lastrec: se l'argomento è un handle
                 * di ledger si ritorna il numero di transazioni (prima cadeva nel ramo ISAM →
                 * 0). § MiniTests/ledger/ledger_ops.bas. */
                if (args.Length >= 1)
                {
                    var lrh = args[0] is double lrd ? (int)lrd : args[0] is int lri ? lri : -1;
                    var lrctx = ResolveLedgerCtx(lrh);
                    if (lrctx != null) return (double)lrctx.TransactionIndex.Count;
                }
                // ISAM.LASTREC(alias): stesso buco — nessun case, tornava sempre 0 (verificato:
                // "LASTREC(Clienti) dopo aggiunte" stampava 0 invece di 4 dopo 4 APPEND).
                var lastRecFileRef = args.Length > 0 ? args[0] : null;
                var lastRecDriver = ResolveIsamDriver(task, lastRecFileRef);
                return lastRecDriver != null ? (double)lastRecDriver.RecCount : 0.0;
            }
        },
        [BuiltInFunction.LEDGER_EOF] = (task, args) =>
        {
                // LEDGER.EOF(h): handle esplicito → cursore del journal via DatabaseService
                // (LedgerEofByHandle). Il vecchio IlRuntime.LedgerEof(alias) usava _ledgerHandles,
                // che la PCodeVM non popola (il suo LEDGER.OPEN usa un dizionario proprio) →
                // ritornava sempre 0 mentre IL dà -1. § probe LEDGER.
                if (args.Length >= 1 && (args[0] is double || args[0] is int))
                {
                    var eh = args[0] is double ed ? (long)ed : (long)(int)args[0]!;
                    if (ResolveLedgerCtx((int)eh) != null)
                        return Basic.IlBackend.IlRuntime.LedgerEofByHandle(eh);
                }
                return Basic.IlBackend.IlRuntime.LedgerEof(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DIFFERENTIATE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): differenza centrale O(h^2) via Func<double,double>
                // adattato dalla closure PCode (CallClosureSync) — unica implementazione.
                if (args.Length >= 2 && args[0] is ClosureData diffF)
                {
                    try
                    {
                        Func<double, double> dfn = x => SafeDouble(CallClosureSync(task, diffF, x));
                        return Basic.IlBackend.IlRuntime.Differentiate(dfn, SafeDouble(args[1]));
                    }
                    catch { }
                }
                return 0.0;
        },
        [BuiltInFunction.ZOMMA] = (task, args) =>
        {
                if (args.Length >= 5) { try { return CommonLib.Trading.Options.Zomma(SafeDouble(args[0]), SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]), SafeDouble(args[4])); } catch { } } return 0.0;
        },
        [BuiltInFunction.HOLTWINTERS] = (task, args) =>
        {
                // BASIC: HOLTWINTERS(series, alpha, beta, gamma, period) — args[1..3]=alpha/beta/
                // gamma, args[4]=period; il case leggeva args[1] come seasonalPeriod invece di
                // alpha (seasonalPeriod=0 -> forecasts vuoto -> HwRes.Forecast(0) fuori range).
                // Ritorna un Dictionary (stesso motivo di REGRESS sopra), stesso fix di GMBasic
                // (TODO AnalysisTests-ADFTEST-ARIMA-KMEANS bug #2+#3).
                if (args.Length >= 2 && args[0] is System.Collections.IList hwIn) {
                    var hwD = ToDoubleArray(hwIn);
                    var hwR = CommonLib.Numerics.HoltWintersFit(hwD, (int)SafeDouble(ArgOr(args,4,4)), SafeDouble(ArgOr(args,1,0.3)), SafeDouble(ArgOr(args,2,0.1)), SafeDouble(ArgOr(args,3,0.1)));
                    return new Dictionary<string, object?> { ["Forecast"] = hwR.fitted, ["Mse"] = hwR.mse };
                }
                return null;
        
            // ARIMA/ADFTEST/KMEANS erano raggruppati nel case INTERPOLATECLAMP (arità e semantica
            // incompatibili) — cadevano silenziosamente nel ramo `return 0.0`, nessuna implementazione
            // reale sotto (CommonLib.Numerics non aveva ADF/ARIMA/KMeans — stesso bug di GMBasic,
            // TODO AnalysisTests-ADFTEST-ARIMA-KMEANS bug #1). Implementati in
            // CommonLib/Numerics/Analysis.cs (identico a GMBasic); ritornano Dictionary per lo
            // stesso motivo di REGRESS/HOLTWINTERS sopra.
        },
        [BuiltInFunction.ARIMA] = (task, args) =>
        {
                if (args.Length >= 4 && args[0] is System.Collections.IList arimaData) {
                    try {
                        var (arC, maC, aic, fitted) = CommonLib.Numerics.ArimaFit(ToDoubleArray(arimaData), (int)SafeDouble(args[1]), (int)SafeDouble(args[2]), (int)SafeDouble(args[3]));
                        return new Dictionary<string, object?> { ["ArCoeffs"] = arC, ["MaCoeffs"] = maC, ["AIC"] = aic, ["Fitted"] = fitted };
                    } catch { }
                }
                return null;
        },
        [BuiltInFunction.ADFTEST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 1 && args[0] is System.Collections.IList adfData) return Basic.IlBackend.IlRuntime.AdfTest(ToDoubleArray(adfData));
                return null;
        },
        [BuiltInFunction.KMEANS] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList kmData) {
                    try {
                        var input = ToDoubleArray(kmData);
                        var clusters = (int)SafeDouble(args[1]);
                        object RunKMeans()
                        {
                            var centroids = CommonLib.Numerics.KMeansCluster(input, clusters);
                            return new Dictionary<string, object?> { ["Centroids"] = centroids };
                        }
                        return input.Length >= 100_000
                            ? ParkCpuBuiltin(task, RunKMeans)
                            : RunKMeans();
                    } catch { }
                }
                return null;
        },
        [BuiltInFunction.INTEGRATE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): quad adattivo Gauss-Kronrod via Func<double,double>
                // adattato dalla closure PCode (CallClosureSync) — unica implementazione.
                if (args.Length >= 3 && args[0] is ClosureData intF)
                {
                    try
                    {
                        Func<double, double> ifn = x => SafeDouble(CallClosureSync(task, intF, x));
                        return Basic.IlBackend.IlRuntime.Integrate(ifn, SafeDouble(args[1]), SafeDouble(args[2]));
                    }
                    catch { }
                }
                return 0.0;
        },
        [BuiltInFunction.SIMPSON] = (task, args) =>
        {
                if (args.Length >= 4 && args[0] is ClosureData simF)
                {
                    try
                    {
                        Func<double, double> sfn = x => SafeDouble(CallClosureSync(task, simF, x));
                        return Basic.IlBackend.IlRuntime.Simpson(sfn, SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]));
                    }
                    catch { }
                }
                return 0.0;
        },
        [BuiltInFunction.TRAPEZOID] = (task, args) =>
        {
                if (args.Length >= 2)
                    return Basic.IlBackend.IlRuntime.Trapezoid(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
                return 0.0;
        },
        [BuiltInFunction.CUMTRAPEZOID] = (task, args) =>
        {
                if (args.Length >= 2)
                    return Basic.IlBackend.IlRuntime.CumTrapezoid(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
                return Array.Empty<double>();
        },
        [BuiltInFunction.CUBICSPLINE] = (task, args) =>
        {
                if (args.Length >= 3)
                    return Basic.IlBackend.IlRuntime.CubicSpline(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(args[2]));
                return 0.0;
        },
        [BuiltInFunction.IFFTSHIFT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IfftShift(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.MAXSHARPE] = (task, args) =>
        {
            {
                // MAXSHARPE(rendimenti, covarianza, riskFree) -> PortfolioResult (dictionary
                // coi campi BASIC ExpectedReturn/Volatility/SharpeRatio/Weights) via
                // CommonLib.Portfolio.OptimizeMeanVariance("MAXSHARPE").
                if (args.Length >= 2)
                {
                    var r = ToDoubleArray(args[0]);
                    var cov = ToDoubleMatrix2D(args[1]);
                    var rf = args.Length > 2 ? SafeDouble(args[2]) : 0.0;
                    if (r.Length > 0 && cov != null)
                    {
                        // Solver closed-form condiviso (IlRuntime.MaxSharpe) — parità IL/VM-C.
                        int rows = cov.GetLength(0);
                        var jag = new double[rows][];
                        for (int i = 0; i < rows; i++) { jag[i] = new double[cov.GetLength(1)]; for (int j = 0; j < cov.GetLength(1); j++) jag[i][j] = cov[i, j]; }
                        return Basic.IlBackend.IlRuntime.MaxSharpe(r, jag, rf);
                    }
                }
                return null;
            }
        
        },
        [BuiltInFunction.ISDATE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return args.Length > 0 && Basic.IlBackend.IlRuntime.IsDate(args[0]?.ToString() ?? "") ? -1.0 : 0.0;
        },
        [BuiltInFunction.ISNUMERIC] = (task, args) =>
        {
                return args.Length > 0 && Basic.IlBackend.IlRuntime.IsNumericValue(args[0]) ? -1.0 : 0.0;
        },
        [BuiltInFunction.DAYSINMONTH] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.DaysInMonth(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.ISLEAPYEAR] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.IsLeapYear(DateTime.FromOADate(SafeDouble(args[0]))) ? -1.0 : 0.0; } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.ISOWEEKYEAR] = (task, args) =>
        {
                if (args.Length > 0) { try { return (double)System.Globalization.ISOWeek.GetWeekOfYear(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.FIRSTDAYOFMONTH] = (task, args) =>
        {
                if (args.Length > 0) { try { return Basic.IlBackend.IlRuntime.FirstDayOfMonth(DateTime.FromOADate(SafeDouble(args[0]))); } catch { return 0.0; } }
                return 0.0;
        },
        [BuiltInFunction.EOMONTH] = (task, args) =>
        {
                if (args.Length >= 1) return Basic.IlBackend.IlRuntime.Eomonth(
                    DateTime.FromOADate(SafeDouble(args[0])), args.Length >= 2 ? (short)SafeDouble(args[1]) : (short)0);
                return 0.0;
        },
        [BuiltInFunction.MEAN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): GPU per array grandi — unica implementazione.
                // ParamArray: appiattisce TUTTI gli argomenti (es. MEAN(1,2,3,4) arriva come
                // 4 scalari separati, MEAN(v) come lista) — come MIN/MAX (SelectMany).
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Mean(vals);
                }
        },
        [BuiltInFunction.MEDIAN] = (task, args) =>
        {
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Median(vals);
                }
        },
        [BuiltInFunction.VARIANCE] = (task, args) =>
        {
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Variance(vals);
                }
        },
        [BuiltInFunction.STDDEV] = (task, args) =>
        {
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.StdDev(vals);
                }
        },
        [BuiltInFunction.RND] = (task, args) =>
        {
                /* RND non accetta argomenti: genera un numero in [0,1). Ignorarli in silenzio
                 * accettava codice non valido (RND(1) e RND() indistinguibili, senza avviso):
                 * RND(n) -> 1..n e' semantica Clipper, non GMBasic. Ora errore esplicito,
                 * come su IL e VM-C. */
                if (args.Length > 0)
                        throw new InvalidOperationException("RND non accetta argomenti.");
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Rnd();
        },
        [BuiltInFunction.FACT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Fact(args.Length > 0 ? Convert.ToInt32(args[0] ?? 0) : 0);
        },
        [BuiltInFunction.PERM] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Perm(
                    args.Length > 0 ? Convert.ToInt32(args[0] ?? 0) : 0,
                    args.Length > 1 ? Convert.ToInt32(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.COMB] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Comb(
                    args.Length > 0 ? Convert.ToInt32(args[0] ?? 0) : 0,
                    args.Length > 1 ? Convert.ToInt32(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.HYPOT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Hypot(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.PYTHAGORAS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Pythagoras(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.DIST2D] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Dist2d(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0);
        },
        [BuiltInFunction.DIST3D] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Dist3d(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0,
                    args.Length > 4 ? SafeDouble(args[4] ?? 0) : 0,
                    args.Length > 5 ? SafeDouble(args[5] ?? 0) : 0);
        },
        [BuiltInFunction.DEG2RAD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.Deg2Rad(SafeDouble(args[0] ?? 0)) : 0.0;
        },
        [BuiltInFunction.RAD2DEG] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.Rad2Deg(SafeDouble(args[0] ?? 0)) : 0.0;
        },
        [BuiltInFunction.CLAMP] = (task, args) =>
        {
                // Runtime comune (IlRuntime): Math.Clamp — unica implementazione.
                return Basic.IlBackend.IlRuntime.Clamp(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0);
        },
        [BuiltInFunction.POLARX] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.PolarX(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.POLARY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.PolarY(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0);
        },
        [BuiltInFunction.ANGLE2D] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Angle2d(
                    args.Length > 0 ? SafeDouble(args[0] ?? 0) : 0,
                    args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0,
                    args.Length > 2 ? SafeDouble(args[2] ?? 0) : 0,
                    args.Length > 3 ? SafeDouble(args[3] ?? 0) : 0);
        },
        [BuiltInFunction.MAD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Mad(vals);
                }
        },
        [BuiltInFunction.SQR] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Sqr(SafeDouble(args[0])) : 0.0;
            // Nucleo scipy.special (SPECIAL namespace)
            // --- SPECIAL: delegato a ExecuteBuiltInSpecial (PCodeProcessor.Builtins.Special.cs) ---
        },
        [BuiltInFunction.SIN] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Sin(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.COS] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Cos(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.TAN] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Tan(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.ATN] = (task, args) =>
        {
                // ATN(y, x) = atan2(y, x) (2° argomento opzionale, parity IL/VM-C/C99Native)
                if (args.Length >= 2 && args[0] != null && args[1] != null)
                    return Basic.IlBackend.IlRuntime.Atn2(SafeDouble(args[0]), SafeDouble(args[1]));
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Atn(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.ASIN] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Asin(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.ACOS] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Acos(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SINH] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Sinh(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.COSH] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Cosh(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.TANH] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Tanh(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.LOG] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Log(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.EXP] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Exp(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.LOG10] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Log10(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.FLOOR] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Floor(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.CEILING] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Ceiling(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.INT] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Floor(SafeDouble(args[0])) : 0.0;
        
        
        },
        [BuiltInFunction.MIN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): appiattisce array/valori multipli — unica impl.
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    if (vals.Length == 0) return 0.0;
                    return Basic.IlBackend.IlRuntime.Min(vals);
                }
        },
        [BuiltInFunction.MAX] = (task, args) =>
        {
                // Runtime comune (IlRuntime): appiattisce array/valori multipli — unica impl.
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    if (vals.Length == 0) return 0.0;
                    return Basic.IlBackend.IlRuntime.Max(vals);
                }
        
            // Stringhe
        },
        [BuiltInFunction.LEN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): gestisce string/collection/array — unica impl.
                return (double)Basic.IlBackend.IlRuntime.Len(args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.LENB] = (task, args) =>
        {
                // LENB(s): BYTE UTF-8 (LEN conta i CARATTERI). Stessa impl della gamba IL:
                // argomento non stringa → -1 (parità dei parametri).
                return (double)Basic.IlBackend.IlRuntime.LenBytes(args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.ASC] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Asc(args[0]?.ToString());
                return 0.0;
        },
        [BuiltInFunction.CHR] = (task, args) =>
        {
                if (args.Length > 0 && args[0] != null) return StringHelpers.Chr(Convert.ToInt32(args[0]));
                return "";
        },
        [BuiltInFunction.STR] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.StringUtils): spazio per i positivi.
                // NIL (arg null) → SafeDouble(null)=0 → " 0" (parità IL/VM-C/C99; prima "").
                if (args.Length > 0) return StringHelpers.Str(SafeDouble(args[0]));
                return "";
        },
        [BuiltInFunction.FORMAT] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.FormatUtils): gestisce boolean/
                // DateTime/OADate/numeric — unica implementazione (canone IL).
                if (args.Length >= 2) return StringHelpers.Format(args[0], args[1]?.ToString() ?? "");
                return args.Length > 0 ? args[0]?.ToString() ?? "" : "";
        },
        [BuiltInFunction.FGRGB] = (task, args) =>
        {
            {
                if (args.Length < 4) return args.Length > 0 ? args[0]?.ToString() ?? "" : "";
                var t = args[0]?.ToString() ?? "";
                var rr = (byte)Math.Clamp(SafeDouble(args[1] ?? 0), 0, 255);
                var gg = (byte)Math.Clamp(SafeDouble(args[2] ?? 0), 0, 255);
                var bb = (byte)Math.Clamp(SafeDouble(args[3] ?? 0), 0, 255);
                if (System.Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1" || Console.IsOutputRedirected) return t;
                return "\x1b[38;2;" + rr + ";" + gg + ";" + bb + "m" + t + "\x1b[0m";
            }
        },
        [BuiltInFunction.BGRGB] = (task, args) =>
        {
            {
                if (args.Length < 4) return args.Length > 0 ? args[0]?.ToString() ?? "" : "";
                var t2 = args[0]?.ToString() ?? "";
                var r2 = (byte)Math.Clamp(SafeDouble(args[1] ?? 0), 0, 255);
                var g2 = (byte)Math.Clamp(SafeDouble(args[2] ?? 0), 0, 255);
                var b2 = (byte)Math.Clamp(SafeDouble(args[3] ?? 0), 0, 255);
                if (System.Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1" || Console.IsOutputRedirected) return t2;
                return "\x1b[48;2;" + r2 + ";" + g2 + ";" + b2 + "m" + t2 + "\x1b[0m";
            }
        
            // === Comandi CONSOLE ibridi (2026-08-06): TUI/console ===
        },
        [BuiltInFunction.SETWINDOWTITLE] = (task, args) =>
        {
            {
                var t = args.Length > 0 ? args[0]?.ToString() ?? "" : "";
                try { _runtime.Console.Title = t; } catch { }
                return null;
            }
        },
        [BuiltInFunction.GETCURSORPOS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.GetCursorPos();
        },
        [BuiltInFunction.GETTEXTMETRICS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.GetTextMetrics();
        },
        [BuiltInFunction.GETPALETTECOLOR] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.GetPaletteColor(args.Length > 0 ? SafeDouble(args[0]) : 0);
        },
        [BuiltInFunction.DUP] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] != null && args[1] != null)
                {
                    var count = Convert.ToInt32(args[0]);
                    if (count < 0) count = 0;
                    if (args[1] is string dupStr && dupStr.Length > 0)
                        return new string(dupStr[0], count);
                    if (args[1] is double or int)
                    {
                        var c = Convert.ToInt32(args[1]);
                        if (c >= 0 && c <= 0xFFFF) return new string((char)c, count);
                    }
                }
                return "";
        },
        [BuiltInFunction.LEFT] = (task, args) =>
        {
                // Runtime comune (CommonLib.StringUtils): gestione edge robusta — unica impl.
                if (args.Length >= 2) return CommonLib.StringUtils.Left(args[0]?.ToString(), Convert.ToInt32(args[1]));
                return "";
        },
        [BuiltInFunction.RIGHT] = (task, args) =>
        {
                if (args.Length >= 2) return CommonLib.StringUtils.Right(args[0]?.ToString(), Convert.ToInt32(args[1]));
                return "";
        },
        [BuiltInFunction.MID] = (task, args) =>
        {
                if (args.Length >= 2) return CommonLib.StringUtils.Mid(
                    args[0]?.ToString(), Convert.ToInt32(args[1]),
                    args.Length >= 3 ? Convert.ToInt32(args[2]) : null);
                return "";
        },
        [BuiltInFunction.INSTR] = (task, args) =>
        {
                // Runtime comune (CommonLib.StringUtils.Instr): case-insensitive, come la
                // PCodeVM (OrdinalIgnoreCase) — unica implementazione.
                if (args.Length >= 3 && args[0] is not string && args[1] is string inh && args[2] is string ins)
                    return (double)CommonLib.StringUtils.Instr(inh, ins, Convert.ToInt32(args[0]));
                if (args.Length >= 2 && args[0] is string istr && args[1] is string isub)
                    return (double)CommonLib.StringUtils.Instr(istr, isub, args.Length >= 3 ? Convert.ToInt32(args[2]) : 1);
                return 0.0;
        
        },
        [BuiltInFunction.UPPER] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string us) return us.ToUpperInvariant();
                return "";
        },
        [BuiltInFunction.LOWER] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string lowerStr) return lowerStr.ToLowerInvariant();
                return "";
        },
        [BuiltInFunction.PASCAL] = (task, args) =>
        {
                // Runtime comune (StringHelpers→StringUtils): stessa logica — unica impl.
                if (args.Length > 0) return StringHelpers.Pascal(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.CAMEL] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Camel(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.SNAKE] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Snake(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.KEBAB] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length > 0) return StringHelpers.Kebab(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.TITLE] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.StringUtils): stessa logica.
                if (args.Length > 0) return StringHelpers.Title(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.SENTENCE] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Sentence(args[0]?.ToString());
                return "";
        },
        [BuiltInFunction.TRIM] = (task, args) =>
        {
                return args.Length > 0 ? (Basic.IlBackend.IlRuntime.StringTrim(args[0]?.ToString()) ?? "") : "";
        },
        [BuiltInFunction.LTRIM] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string ltr) return ltr.TrimStart();
                return "";
        },
        [BuiltInFunction.RTRIM] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string rtr) return rtr.TrimEnd();
                return "";
        },
        [BuiltInFunction.REPLACE] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length >= 3) return StringHelpers.Replace(args[0]?.ToString(), args[1]?.ToString(), args[2]?.ToString());
                return "";
        },
        [BuiltInFunction.REPEATFUNC] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.StringUtils.Repeat): stessa logica.
                if (args.Length >= 2 && args[0] is string rpt8) return StringHelpers.Repeat(rpt8, Convert.ToInt32(args[1]), args.Length >= 3 ? args[2]?.ToString() : null);
                return "";
        },
        [BuiltInFunction.PADL] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length >= 2 && args[0] is string pdl7) return StringHelpers.PadL(pdl7, Convert.ToInt32(args[1]), args.Length >= 3 ? args[2]?.ToString() : " ");
                return "";
        },
        [BuiltInFunction.PADR] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is string pdr) return StringHelpers.PadR(pdr, Convert.ToInt32(args[1]), args.Length >= 3 ? args[2]?.ToString() : " ");
                return "";
        },
        [BuiltInFunction.PADC] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is string pdc2) return StringHelpers.PadC(pdc2, Convert.ToInt32(args[1]), args.Length >= 3 ? args[2]?.ToString() : " ");
                return "";
        },
        [BuiltInFunction.REVERSE] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica per stringhe — unica impl.
                if (args.Length > 0 && args[0] is string revs8) return StringHelpers.Reverse(revs8);
                if (args.Length >= 1 && args[0] is System.Collections.IList rrl)
                {
                    // Assegnazione per indice, non Clear()+Add(): un LIST OF x inizializzato con
                    // un literal ({"a","b"}) è un array a dimensione fissa (string[]/object[]) —
                    // IList.Add() ci lancia NotSupportedException ("fixed size"), che il chiamante
                    // (REPL) inghiottiva silenziosamente terminando il task senza output.
                    var arr = rrl.Cast<object?>().ToArray(); Array.Reverse(arr);
                    for (var ri = 0; ri < arr.Length; ri++) rrl[ri] = arr[ri];
                    return rrl;
                }
                if (args.Length >= 1) return args[0]; // pass-through for FOR EACH on generic lists
                return null;
        },
        [BuiltInFunction.RANGE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stesso unwrap ParamArray + loop — unica impl.
                return Basic.IlBackend.IlRuntime.Range(args);
        },
        [BuiltInFunction.SEARCH] = (task, args) =>
        {
                // Runtime comune (StringHelpers): case-insensitive — unica implementazione.
                if (args.Length >= 2 && args[0] is string src10) return StringHelpers.Search(src10, args[1]?.ToString() ?? "") ? -1.0 : 0.0;
                return 0.0;
        },
        [BuiltInFunction.SPLIT] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length >= 1 && args[0] is string spl) return StringHelpers.Split(spl, args.Length >= 2 ? args[1]?.ToString() ?? "," : ",");
                return null;
        },
        [BuiltInFunction.WRAP] = (task, args) =>
        {
                // Runtime comune (CommonLib.StringUtils.Wrap): wrap per PAROLE con
                // allineamento (0=sinistra, 1=destra, 2=giustificato) — firma canonica
                // StdLib WRAP(text, lineLength, [alignment]). Il vecchio PCode usava il 3°
                // argomento come SEPARATORE (firma errata rispetto al canone) — corretto.
                if (args.Length >= 2 && args[0] is string wrp)
                    return CommonLib.StringUtils.Wrap(wrp, Convert.ToInt32(args[1]), args.Length >= 3 ? Convert.ToInt32(args[2]) : 0);
                return "";
        },
        [BuiltInFunction.HEX] = (task, args) =>
        {
                // Runtime comune (StringHelpers→CommonLib.ConversionUtils): unica implementazione.
                if (args.Length > 0) return StringHelpers.Hex((long)SafeDouble(args[0]));
                return "0";
        },
        [BuiltInFunction.OCT] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Oct((long)SafeDouble(args[0]));
                return "0";
        },
        [BuiltInFunction.BIN] = (task, args) =>
        {
                if (args.Length > 0) return StringHelpers.Bin((long)SafeDouble(args[0]));
                return "0";
        },
        [BuiltInFunction.SPACE] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length > 0) return StringHelpers.Space(Convert.ToInt32(args[0]));
                return "";
        
            // === Trading ===
        },
        [BuiltInFunction.LOAD] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string loadPath)
                {
                    try
                    {
                        var loadText = File.ReadAllText(loadPath);
                        return JsonElementToObject(System.Text.Json.JsonDocument.Parse(loadText).RootElement);
                    }
                    catch { return new Dictionary<string, object?>(); }
                }
                return new Dictionary<string, object?>();
        },
        [BuiltInFunction.PARSE] = (task, args) =>
        {
                if (args.Length > 0 && args[0] is string parseJson)
                {
                    try { return JsonElementToObject(System.Text.Json.JsonDocument.Parse(parseJson).RootElement); }
                    catch { return new Dictionary<string, object?>(); }
                }
                return new Dictionary<string, object?>();
        },
        [BuiltInFunction.OBJECTTOJSON] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Stringify(
                    args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.STRINGIFY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Stringify(
                    args.Length > 0 ? args[0] : null);
        },
        [BuiltInFunction.UNIQUECOUNT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): gestisce dict/collection — unica implementazione.
                if (args.Length >= 1) return Basic.IlBackend.IlRuntime.UniqueCount(args[0]!);
                return 0.0;
        },
        [BuiltInFunction.COUNT] = (task, args) =>
        {
                /* LEDGER.COUNT(h): token condiviso Count → stesso caso di LASTREC sopra. */
                if (args.Length >= 1)
                {
                    var cdh = args[0] is double cd ? (int)cd : args[0] is int cdi ? cdi : -1;
                    var crctx = ResolveLedgerCtx(cdh);
                    if (crctx != null) return (double)crctx.TransactionIndex.Count;
                }
                if (args.Length >= 2 && args[0] is string cts) { var cwhat = args[1]?.ToString() ?? ""; if (string.IsNullOrEmpty(cwhat)) return 0.0; int ci = 0, cidx = 0; while ((cidx = cts.IndexOf(cwhat, cidx, System.StringComparison.Ordinal)) >= 0) { ci++; cidx += cwhat.Length; } return (double)ci; }
                // BAG: rappresentato come Dictionary<object,long> (conteggi) — la semantica
                // CommonLib è: Count(item) = molteplicità dell'elemento; Count() senza argomento
                // = somma dei conteggi (elementi totali). PRIMA del check ICollection (il
                // Dictionary è ICollection → altrimenti restituirebbe le chiavi).
                if (args.Length >= 2 && args[0] is System.Collections.IDictionary bagCnt2 &&
                    bagCnt2.GetType().IsGenericType &&
                    bagCnt2.GetType().GetGenericArguments().Length == 2 &&
                    bagCnt2.GetType().GetGenericArguments()[1] == typeof(long))
                {
                    var key = args[1];
                    if (bagCnt2.Contains(key!))
                    {
                        var cntVal = bagCnt2[key!];
                        if (cntVal is long cl) return (double)cl;
                        if (cntVal is double cddv) return cddv;
                    }
                    return 0.0;
                }
                if (args.Length >= 1 && args[0] is System.Collections.IDictionary bagCnt &&
                    bagCnt.GetType().IsGenericType &&
                    bagCnt.GetType().GetGenericArguments().Length == 2 &&
                    bagCnt.GetType().GetGenericArguments()[1] == typeof(long))
                {
                    long sum = 0;
                    foreach (var v in bagCnt.Values) { if (v is long l) sum += l; else if (v is double dd) sum += (long)dd; }
                    return (double)sum;
                }
                if (args.Length >= 1 && args[0] is System.Collections.ICollection ccol) return (double)ccol.Count;
                // ICollection<T> generica (HashSet<T>/Queue<T>/Stack<T> implementano la
                // variante generica, NON quella non-generica → cadeva nel fallback 0).
                if (args.Length >= 1 && args[0] is not null)
                {
                    var ctype = args[0]!.GetType();
                    if (ctype.GetInterfaces().Any(i => i.IsGenericType && i.GetGenericTypeDefinition() == typeof(System.Collections.Generic.ICollection<>)))
                    {
                        var cntProp = ctype.GetProperty("Count");
                        if (cntProp != null && cntProp.GetValue(args[0]) is int hsc) return (double)hsc;
                    }
                }
                if (args.Length >= 1 && args[0] is System.Collections.IList clst) return (double)clst.Count;
                if (args.Length >= 1 && args[0] is object?[] carr) return (double)((Array)carr).Length;
                // Collezioni avanzate CommonLib (Graph/Timeline/Heap/...): hanno una
                // proprietà Count/NodeCount pubblica ma non implementano ICollection.
                if (args.Length >= 1 && args[0] is not null)
                {
                    var ctype = args[0]!.GetType();
                    if (ctype.Namespace?.StartsWith("CommonLib.Collections") == true)
                    {
                        var cntProp = ctype.GetProperty("Count") ?? ctype.GetProperty("NodeCount");
                        if (cntProp != null)
                        {
                            var cv = cntProp.GetValue(args[0]);
                            if (cv is int cvi) return (double)cvi;
                            if (cv is long cvl) return (double)cvl;
                            if (cv is double cvd) return cvd;
                        }
                    }
                }
                return 0.0;
        },
        [BuiltInFunction.TOARRAY] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.ICollection ccc) { return ccc.Cast<object?>().ToArray(); }
                if (args.Length >= 1 && args[0] is System.Collections.IEnumerable en) { return en.Cast<object?>().ToArray(); }
                // RedBlackTree non è IEnumerable — usa Entries() (ordinato).
                // I KeyValuePair non sono serializzabili da JSON.Stringify (restituiva []):
                // si convertono in dict {Key:..., Value:...} come faceva il vecchio wrapper.
                if (args.Length >= 1 && args[0] is CommonLib.Collections.IKeyValueIndexable kvi3)
                {
                    var result = new System.Collections.Generic.List<object?>();
                    foreach (var (k, v) in kvi3.Entries())
                        result.Add(new System.Collections.Generic.Dictionary<object, object?> { ["Key"] = k, ["Value"] = v });
                    return result.ToArray();
                }
                return null;
        },
        [BuiltInFunction.ADDRANGE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 2 && args[0] is System.Collections.IList tgt && args[1] is System.Collections.IEnumerable src) { Basic.IlBackend.IlRuntime.AddRange(tgt, src); return null; }
                return null;
        },
        [BuiltInFunction.PEEK] = (task, args) =>
        {
                // NET.PEEK(handle, numBytes): 2 argomenti numerici → peek dal socket (parità IL/C99).
                if (args.Length == 2 && args[0] is double or long or int)
                    return Basic.IlBackend.IlRuntime.TcpPeek((long)SafeDouble(args[0]), (int)SafeDouble(args[1]));
                // Runtime comune (IlRuntime): ritorna il PRIMO elemento (canone IL) — prima
                // il PCode ritornava l'ultimo (divergenza corretta verso il canone).
                if (args.Length >= 1 && args[0] is System.Collections.IList pl && pl.Count > 0) return Basic.IlBackend.IlRuntime.Peek(pl);
                return null;
        },
        [BuiltInFunction.CONTAINS] = (task, args) =>
        {
                if (args.Length >= 2) { return args[0] is string s0 && args[1] is string s1 ? (StringHelpers.Contains(s0, s1) ? -1.0 : 0.0) : args[0] is System.Collections.IList list ? (list.Contains(args[1]) ? -1.0 : 0.0) : args[0] is System.Collections.IDictionary dict ? (dict.Contains(args[1] ?? "") ? -1.0 : 0.0) : args[0] is System.Collections.Generic.ICollection<object?> gc ? (gc.Contains(args[1]) ? -1.0 : 0.0) : 0.0; }
                return null;
        },
        [BuiltInFunction.SORT] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList ssl) { var arr2 = ssl.Cast<object?>().ToArray(); Array.Sort(arr2); ssl.Clear(); foreach (var it2 in arr2) ssl.Add(it2); return null; }
                return null;
        },
        [BuiltInFunction.INSERT] = (task, args) =>
        {
                if (args.Length >= 3 && args[0] is System.Collections.IList il) { var idx = Convert.ToInt32(args[1]); if (idx >= 0 && idx <= il.Count) il.Insert(idx, args[2]); return null; }
                return null;
        },
        [BuiltInFunction.TRYGETVALUE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 3 && args[0] is System.Collections.IDictionary td && args[2] is System.Collections.IList outList) return Basic.IlBackend.IlRuntime.TryGetValue(td, args[1] ?? "", outList);
                return 0.0;
        },
        [BuiltInFunction.FIRST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 1 && args[0] is System.Collections.IList fl && fl.Count > 0) return Basic.IlBackend.IlRuntime.First(fl);
                return null;
        },
        [BuiltInFunction.CLEAR] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList cll) { cll.Clear(); return null; }
                if (args.Length >= 1 && args[0] is System.Collections.IDictionary cdd) { cdd.Clear(); return null; }
                return null;
        },
        [BuiltInFunction.REMOVEAT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 2 && args[0] is System.Collections.IList ral) return Basic.IlBackend.IlRuntime.RemoveAt(ral, Convert.ToInt32(args[1]));
                return null;
        },
        [BuiltInFunction.LAST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 1 && args[0] is System.Collections.IList lst && lst.Count > 0) return Basic.IlBackend.IlRuntime.Last(lst);
                return null;
        },
        [BuiltInFunction.INDEXOF] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList iol) { return Basic.IlBackend.IlRuntime.IndexOf(iol, args[1]!); }
                return null;
        },
        [BuiltInFunction.GETRANGE] = (task, args) =>
        {
                if (args.Length >= 3 && args[0] is System.Collections.IList grl) return Basic.IlBackend.IlRuntime.GetRange(grl, Convert.ToInt32(args[1]), Convert.ToInt32(args[2]));
                return null;
        },
        [BuiltInFunction.STARTSWITH] = (task, args) =>
        {
                // Runtime comune (StringHelpers): case-insensitive — unica implementazione.
                if (args.Length >= 2) return StringHelpers.StartsWith(args[0]?.ToString(), args[1]?.ToString()) ? -1.0 : 0.0;
                return 0.0;
        },
        [BuiltInFunction.JOIN] = (task, args) =>
        {
                // Runtime comune (StringHelpers): stessa logica — unica implementazione.
                if (args.Length >= 1 && args[0] is System.Collections.IEnumerable je) return StringHelpers.Join(je, args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
                return null;
        },
        [BuiltInFunction.SUM] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                // ParamArray: appiattisce TUTTI gli argomenti (separati o lista).
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Sum(vals);
                }
        },
        [BuiltInFunction.GET] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IDictionary gd) return gd[args[1] ?? ""];
                if (args.Length >= 2 && args[0] is System.Collections.IList gl) { var gi = Convert.ToInt32(args[1]); if (gi >= 0 && gi < gl.Count) return gl[gi]; }
                return null;
        },
        [BuiltInFunction.SET] = (task, args) =>
        {
                if (args.Length >= 3 && args[0] is System.Collections.IDictionary sd) { sd[args[1] ?? ""] = args[2]; return null; }
                if (args.Length >= 3 && args[0] is System.Collections.IList ssl2) { var si = Convert.ToInt32(args[1]); if (si >= 0 && si < ssl2.Count) ssl2[si] = args[2]; return null; }
                return null;
        },
        [BuiltInFunction.EXTRACT] = (task, args) =>
        {
                // BUG REALE trovato 2026-09-07 ("extract FAIL" in ForEachJsonEdgeCases.bas,
                // PCode): questo case gestiva SOLO una chiave piatta (TryGetProperty/dict.Contains
                // diretto su jsonKey) -- mai la sintassi path con indice array usata da
                // JSON.EXTRACT("roles[1]")/"a.b[0].c" (verificata funzionante sul backend IL,
                // IlRuntime.Extract/ExtractFromElement/ExtractFromObject). Un jsonKey letterale
                // "roles[1]" non è mai una property/chiave reale -> TryGetProperty/dict.Contains
                // fallivano sempre, ritornando stringa vuota. Fix: naviga il path a segmenti
                // (.campo / [indice]) con PCodeJsonExtractPathFromElement/FromObject, sotto --
                // implementazione indipendente da IlRuntime (mondo PCode, mai condivisa con IL:
                // stessa REGOLA DI SEPARAZIONE dei due backend, parità di comportamento non di
                // codice), stesso algoritmo verificato sull'altro mondo. Se jsonKey non contiene
                // '.'/'[' il comportamento per una chiave semplice resta identico a prima (lookup
                // diretto), nessuna regressione sui casi già funzionanti.
                if (args.Length >= 2 && args[1] is string jsonKey)
                {
                    if (args[0] is string jsonStr)
                    {
                        try
                        {
                            using var doc = System.Text.Json.JsonDocument.Parse(jsonStr);
                            return PCodeJsonExtractPathFromElement(doc.RootElement, jsonKey);
                        }
                        catch { return ""; }
                    }
                    if (args[0] is System.Collections.IDictionary or System.Collections.IList)
                        return PCodeJsonExtractPathFromObject(args[0], jsonKey);
                }
                if (args.Length >= 1 && args[0] is System.Collections.IList el && el.Count > 0) { var item = el[0]; el.RemoveAt(0); return item; }
                return null;
        },
        [BuiltInFunction.REMOVE] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is string rms99) return rms99.Replace(args[1]?.ToString() ?? "", "");
                if (args.Length >= 2 && args[0] is System.Collections.IList rl) { rl.Remove(args[1]); return null; }
                if (args.Length >= 2 && args[0] is System.Collections.IDictionary rd) { rd.Remove(args[1] ?? ""); return null; }
                return null;
        },
        [BuiltInFunction.TOLIST] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IEnumerable en2) { return new System.Collections.Generic.List<object?>(en2.Cast<object?>()); }
                return null;
        },
        [BuiltInFunction.TODICT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 1) return Basic.IlBackend.IlRuntime.ToDict(args[0]);
                return null;
        },
        [BuiltInFunction.RUN] = (task, args) =>
        {
            {
                // Metriche dal runtime comune (IlRuntime.BacktestRun): gestisce array
                // rettangolare double[,] (rappresentazione del linguaggio) e jagged, evita la
                // doppia implementazione divergente.
                var metrics = Basic.IlBackend.IlRuntime.BacktestRun(
                    args.Length > 0 ? args[0] : null,
                    args.Length > 1 ? args[1] : null,
                    null);
                var totalRet = SafeDouble(metrics["TotalReturn"]);
                var annRet = SafeDouble(metrics["AnnualizedReturn"]);
                var maxDD = SafeDouble(metrics["MaxDrawdown"]);
                var sharpe = SafeDouble(metrics["SharpeRatio"]);
                var trades = SafeDouble(metrics["TradesCount"]);
                var eq = metrics["EquityCurve"];
                var dict = new System.Collections.Generic.Dictionary<string, object?>(System.StringComparer.OrdinalIgnoreCase)
                {
                    ["TotalReturn"] = totalRet,
                    ["AnnualizedReturn"] = annRet,
                    ["MaxDrawdown"] = maxDD,
                    ["SharpeRatio"] = sharpe,
                    ["TradesCount"] = trades,
                    ["Turnover"] = 0.0,
                    ["EquityCurve"] = eq,
                };
                // Build class descriptor for field access
                var fieldNames = new System.Collections.Generic.Dictionary<int, string> { {0,"TotalReturn"}, {1,"AnnualizedReturn"}, {2,"MaxDrawdown"}, {3,"SharpeRatio"}, {4,"TradesCount"}, {5,"Turnover"}, {6,"EquityCurve"} };
                var cd = new System.Collections.Generic.Dictionary<string, object?> { ["FieldNames"] = fieldNames };
                dict["__class__"] = cd;
                // Also add __field_N for fallback (no enumeration, direct assignment)
                dict["__field_0"] = totalRet;
                dict["__field_1"] = annRet;
                dict["__field_2"] = maxDD;
                dict["__field_3"] = sharpe;
                dict["__field_4"] = trades;
                dict["__field_5"] = 0.0;
                dict["__field_6"] = eq;
                return dict;
            }
        },
        [BuiltInFunction.MINVOLATILITY] = (task, args) =>
        {
            {
                // MINVOLATILITY(covarianza) -> PortfolioResult via OptimizeMeanVariance("MINVOLATILITY").
                var cov = args.Length > 0 ? ToDoubleMatrix2D(args[0]) : null;
                if (cov != null)
                {
                    var n = cov.GetLength(0);
                    var r = new double[n];
                    for (int i = 0; i < n; i++) r[i] = 0;
                    try { return OptimizePortfolioToDict(CommonLib.Trading.Portfolio.OptimizeMeanVariance(r, cov, "MINVOLATILITY", 0, null)); }
                    catch { return null; }
                }
                return null;
            }
        },
        [BuiltInFunction.EQUALLYWEIGHTED] = (task, args) =>
        {
            {
                // EQUALLYWEIGHTED(numeroAsset) -> PortfolioResult via CommonLib.
                if (args.Length > 0)
                {
                    try { return OptimizePortfolioToDict(CommonLib.Trading.Portfolio.EquallyWeighted((int)SafeDouble(args[0]))); }
                    catch { return null; }
                }
                return null;
            }
        },
        [BuiltInFunction.RISKPARITY] = (task, args) =>
        {
            {
                // RISKPARITY(covarianza) -> PortfolioResult via CommonLib.
                var cov = args.Length > 0 ? ToDoubleMatrix2D(args[0]) : null;
                if (cov != null)
                {
                    try { return OptimizePortfolioToDict(CommonLib.Trading.Portfolio.RiskParity(cov)); }
                    catch { return null; }
                }
                return null;
            }
        },
        [BuiltInFunction.MEANVARIANCE] = (task, args) =>
        {
            {
                // MEANVARIANCE(rendimenti, covarianza [, vincoli]) -> PortfolioResult via
                // OptimizeMeanVariance (max Sharpe con rf 0 per il caso senza vincoli).
                if (args.Length >= 2)
                {
                    var r = ToDoubleArray(args[0]);
                    var cov = ToDoubleMatrix2D(args[1]);
                    if (r.Length > 0 && cov != null)
                    {
                        // Solver closed-form condiviso (IlRuntime.MeanVariance) — parità IL/VM-C.
                        int rows = cov.GetLength(0);
                        var jag = new double[rows][];
                        for (int i = 0; i < rows; i++) { jag[i] = new double[cov.GetLength(1)]; for (int j = 0; j < cov.GetLength(1); j++) jag[i][j] = cov[i, j]; }
                        return Basic.IlBackend.IlRuntime.MeanVariance(r, jag);
                    }
                }
                return null;
            }
        },
        [BuiltInFunction.BLACKLITTERMAN] = (task, args) =>
        {
            {
                // BLACKLITTERMAN(marketCaps, cov, riskAversion, tau, P, Q, omega, riskFree)
                // -> PortfolioResult via CommonLib (firma allineata alla dichiarazione).
                if (args.Length >= 8)
                {
                    var caps = ToDoubleArray(args[0]);
                    var cov = ToDoubleMatrix2D(args[1]);
                    var riskAversion = SafeDouble(args[2]);
                    var tau = SafeDouble(args[3]);
                    var P = ToDoubleMatrix2D(args[4]);
                    var Q = ToDoubleArray(args[5]);
                    var omega = ToDoubleMatrix2D(args[6]);
                    var rf = SafeDouble(args[7]);
                    if (caps.Length > 0 && cov != null && P != null && omega != null)
                    {
                        try { return OptimizePortfolioToDict(CommonLib.Trading.Portfolio.BlackLitterman(caps, cov, riskAversion, tau, P, Q, omega, rf)); }
                        catch { return null; }
                    }
                }
                return null;
            }
        },
        [BuiltInFunction.EVOLVE] = (task, args) =>
        {
                // Parità IlRuntime.PortfolioEvolve(rendimentiAttesi, matriceCovarianza, parametriGA):
                // prima il handler usava la vecchia firma (popolazione, closure fitness, generazioni)
                // → `EVOLVE(r, cov, NULL)` ritornava sempre null/0 (bug PORTFOLIO_EVOLVE).
                if (args.Length >= 2 && args[0] is System.Collections.IList)
                {
                    try
                    {
                        var evr = ToDoubleArray(args[0]);
                        var evc = ToDoubleMatrix(args[1]);
                        var evres = Basic.IlBackend.IlRuntime.PortfolioEvolve(evr, evc, args.Length > 2 ? args[2] : null) as CommonLib.Trading.PortfolioResult;
                        return evres is null ? null : OptimizePortfolioToDict(evres);
                    }
                    catch { return null; }
                }
                return null;
        },
        [BuiltInFunction.ISLONGONLY] = (task, args) =>
        {
if (args.Length > 0 && args[0] is System.Collections.IList wts) { var w = new double[wts.Count]; for (int i = 0; i < wts.Count; i++) w[i] = SafeDouble(wts[i]); return CommonLib.Numerics.IsLongOnly(w) ? -1.0 : 0.0; } return 0.0;
        },
        [BuiltInFunction.TOTALRETURN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (frazione, non x100).
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.TotalReturn(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.ANNUALIZEDRETURN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (frazione, non x100).
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.AnnualizedReturn(
                    ToDoubleArr(args[0]), args.Length > 1 ? (long)SafeDouble(args[1] ?? 12) : 12L);
                return 0.0;
        },
        [BuiltInFunction.MAXDRAWDOWN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (frazione, es. -0.25
                // per -25% — il PCode ritornava percentuale x100, unità diversa). Unica impl.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.MaxDrawdown(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.SHARPERATIO] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa formula (senza riskfree) — unica impl.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.SharpeRatio(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.SORTINORATIO] = (task, args) =>
        {
                // Firma canonica (StdLib): (values, riskfree, periodi) → IlRuntime.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.SortinoRatio(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1]) : 0.0,
                    args.Length > 2 ? (long)SafeDouble(args[2]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.CALMARRATIO] = (task, args) =>
        {
                // Runtime comune (IlRuntime): AnnualizedReturn/|MaxDrawdown| — unica impl.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.CalmarRatio(
                    ToDoubleArr(args[0]), args.Length > 1 ? (long)SafeDouble(args[1]) : 12L);
                return 0.0;
        },
        [BuiltInFunction.VALUEATRISK] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (percentile, senza x100
                // — il PCode ritornava -val*100). Unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.ValueAtRisk(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1] ?? 0.95) : 0.95);
                return 0.0;
        },
        [BuiltInFunction.ANNUALIZEDVOLATILITY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (frazione, non x100).
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.AnnualizedVolatility(
                    ToDoubleArr(args[0]), args.Length > 1 ? (long)SafeDouble(args[1]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.DOWNSIDEDEVIATION] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics (Average, senza x100 —
                // il PCode divideva per n-1 e moltiplicava per 100). Unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.DownsideDeviation(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1] ?? 0) : 0.0);
                return 0.0;
        },
        [BuiltInFunction.BETA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics — unica implementazione.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.Beta(ToDoubleArr(args[0]), ToDoubleArr(args[1]));
                return 0.0;
        },
        [BuiltInFunction.ALPHA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): firma canonica (asset, benchmark, riskfree, periodi)
                // — il PCode prima calcolava senza riskfree (semplificazione incompleta).
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.Alpha(
                    ToDoubleArr(args[0]), ToDoubleArr(args[1]),
                    args.Length > 2 ? SafeDouble(args[2]) : 0.0,
                    args.Length > 3 ? (long)SafeDouble(args[3]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.SKEWNESS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Skewness(vals);
                }
        },
        [BuiltInFunction.KURTOSIS] = (task, args) =>
        {
                {
                    var vals = args.SelectMany(a => ToDoubleArr(a)).ToArray();
                    return Basic.IlBackend.IlRuntime.Kurtosis(vals);
                }
        },
        [BuiltInFunction.CORRELATION] = (task, args) =>
        {
                // Runtime comune (IlRuntime): GPU per array grandi — unica implementazione.
                return Basic.IlBackend.IlRuntime.Correlation(
                    ToDoubleArr(args.Length > 0 ? args[0] : null),
                    ToDoubleArr(args.Length > 1 ? args[1] : null));
        },
        [BuiltInFunction.PERCENTILE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Numerics — unica implementazione.
                return Basic.IlBackend.IlRuntime.Percentile(
                    ToDoubleArr(args.Length > 0 ? args[0] : null),
                    args.Length > 1 ? SafeDouble(args[1] ?? 50) : 50);
        },
        [BuiltInFunction.TREYNORRATIO] = (task, args) =>
        {
                // Firma canonica (StdLib): (asset, benchmark, riskfree, periodi) → IlRuntime.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.TreynorRatio(
                    ToDoubleArr(args[0]), ToDoubleArr(args[1]),
                    args.Length > 2 ? SafeDouble(args[2]) : 0.0,
                    args.Length > 3 ? (long)SafeDouble(args[3]) : 252L);
                // Legacy 1-arg (semplificato, senza benchmark/riskfree): mantiene il comportamento.
                if (args.Length > 0 && args[0] is System.Collections.IList trRets) { var r = ToDoubleArray(trRets); if (r.Length < 2) return 0.0; var avg = r.Average(); var std = Math.Sqrt(r.Sum(x => Math.Pow(x - avg, 2)) / (r.Length - 1)); return std > 0 ? (avg / std) * Math.Sqrt(252) : 0; } return 0.0;
        },
        [BuiltInFunction.OMEGARATIO] = (task, args) =>
        {
                // Runtime comune (IlRuntime): TradingFunctions — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.OmegaRatio(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1]) : 0.0);
                return 0.0;
        },
        [BuiltInFunction.ULCERINDEX] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.UlcerIndex(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.INFORMATIONRATIO] = (task, args) =>
        {
                // Firma canonica (StdLib): (asset, benchmark, periodi) → IlRuntime.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.InformationRatio(
                    ToDoubleArr(args[0]), ToDoubleArr(args[1]),
                    args.Length > 2 ? (long)SafeDouble(args[2]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.KELLYCRITERION] = (task, args) =>
        {
                // KELLYCRITERION(rendimenti, riskFree) → PortfolioResult dict (parità VM/C
                // case 189 e StdLib PortfolioResultType). Prima ritornava il double.
                if (args.Length > 0)
                    return args.Length > 1
                        ? Basic.IlBackend.IlRuntime.PortfolioKellyCriterion(ToDoubleArr(args[0]), SafeDouble(args[1]))
                        : Basic.IlBackend.IlRuntime.PortfolioKellyCriterion(ToDoubleArr(args[0]), 0.0);
                return new Dictionary<string, object?>();
        },
        [BuiltInFunction.JARQUEBERATEST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): TradingFunctions.JarqueBeraTest — unica impl.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.JarqueBeraTest(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.ANNUALIZEDTRACKINGERROR] = (task, args) =>
        {
                // Runtime comune (IlRuntime): firma (asset, benchmark, periodi), frazione.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.AnnualizedTrackingError(
                    ToDoubleArr(args[0]), ToDoubleArr(args[1]), args.Length > 2 ? (long)SafeDouble(args[2]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.ANNUALIZEDDOWNSIDEDEVIATION] = (task, args) =>
        {
                // Runtime comune (IlRuntime): firma (rets, threshold, periodi), frazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.AnnualizedDownsideDeviation(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1]) : 0.0,
                    args.Length > 2 ? (long)SafeDouble(args[2]) : 252L);
                return 0.0;
        },
        [BuiltInFunction.RETURNKURTOSIS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): TradingFunctions — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.ReturnsKurtosis(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.RETURNSSKEWNESS] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.ReturnsSkewness(ToDoubleArr(args[0]));
                return 0.0;
        },
        [BuiltInFunction.DIVIDENDYIELD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica (*100) — unica implementazione.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.DividendYield(SafeDouble(args[0]), SafeDouble(args[1]));
                return 0.0;
        },
        [BuiltInFunction.CURRENTYIELD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): Bonds.CurrentYield (frazione, non x100) — unica impl.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.CurrentYield(SafeDouble(args[0]), SafeDouble(args[1]));
                return 0.0;
        },
        [BuiltInFunction.PORTFOLIOEXPECTEDRETURN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 2) return Basic.IlBackend.IlRuntime.PortfolioExpectedReturn(args[0]!, args[1]!);
                return 0.0;
        },
        [BuiltInFunction.PORTFOLIOVAR] = (task, args) =>
        {
                // PVAR(storici, pesi, conf) — 3 arg (StdLib): conf = args[2] (i pesi sono
                // ignorati, come IL/VM-C). Forma a 2 arg: conf = args[1].
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.PortfolioVar(
                    args[0]!, args.Length >= 3 ? SafeDouble(args[2]) : (args.Length > 1 ? SafeDouble(args[1]) : 0.95));
                return 0.0;
        },
        [BuiltInFunction.PORTFOLIOCVAR] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.PortfolioCVar(
                    args[0]!, args.Length >= 3 ? SafeDouble(args[2]) : (args.Length > 1 ? SafeDouble(args[1]) : 0.95));
                return 0.0;
        },
        [BuiltInFunction.PORTFOLIOMAD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): supporta matrice 2D via MadSolver — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.PortfolioMad(
                    args[0]!, args.Length > 1 ? SafeDouble(args[1]) : 0);
                return 0.0;
        },
        [BuiltInFunction.PSRATIO] = (task, args) =>
        {
                if (args.Length >= 2) { try { return CommonLib.Trading.General.PsRatio(SafeDouble(args[0]), SafeDouble(args[1])); } catch { } } return 0.0;
        },
        [BuiltInFunction.PORTFOLIOSEMIMAD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): supporta matrice 2D via MadSolver — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.PortfolioSemiMad(
                    args[0]!, args.Length > 1 ? SafeDouble(args[1]) : 0);
                return 0.0;
        },
        [BuiltInFunction.CONDITIONALVALUEATRISK] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CommonLib.Trading.Metrics — unica impl.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.ConditionalValueAtRisk(
                    ToDoubleArr(args[0]), args.Length > 1 ? SafeDouble(args[1]) : 0.95);
                return 0.0;
        
        },
        [BuiltInFunction.AADD] = (task, args) =>
        {
                // AADD array, valore — statement in-place (il compiler fa OP_POP, il risultato
                // non è riassegnato). PushTargetByRef passa un RefToVar: se l'array è un
                // System.Array fisso (es. DIM Nomi$() = new object?[0]) l'Add diretto lancia
                // "Collection was of a fixed size" → converti a List, aggiungi e SCRIVI nel
                // RefToVar (così la variabile punta al nuovo List dinamico).
                if (args.Length >= 2) {
                    object? target = args[0];
                    if (target is RefToVar rvA)
                    {
                        var srcA = rvA.Array[rvA.Index];
                        if (srcA is System.Collections.IList slA && slA is not System.Array)
                        {
                            slA.Add(args[1]);
                            return null;
                        }
                        // AADD su un ARRAY nativo: mantieni la rappresentazione ad ARRAY
                        // (cresciuto) come IL (Array.Resize su T[]) invece di convertire a
                        // List — così TYPENAME resta "ARRAY" (prima diventava "LIST`1").
                        if (srcA is System.Array arrA)
                        {
                            var grown = new object?[arrA.Length + 1];
                            for (var ai = 0; ai < arrA.Length; ai++) grown[ai] = arrA.GetValue(ai);
                            grown[arrA.Length] = args[1];
                            rvA.Array[rvA.Index] = grown;
                            return null;
                        }
                        var resA = new System.Collections.Generic.List<object?>();
                        if (srcA is System.Collections.IEnumerable enA)
                            foreach (var e in enA) resA.Add(e);
                        resA.Add(args[1]);
                        rvA.Array[rvA.Index] = resA;
                        return null;
                    }
                    if (target is System.Collections.IList tlA && tlA is not System.Array) { tlA.Add(args[1]); }
                }
                return null;
        
        },
        [BuiltInFunction.AEVAL] = (task, args) =>
        {
                if (args.Length >= 2) {
                    if (IsCallable(args[1])) {
                        // Unwrap RefToVar to get the actual collection
                        object? evalTarget = args[0];
                        if (evalTarget is RefToVar rv) evalTarget = rv.Array[rv.Index];
                        // Clipper AEVAL: il block riceve (elemento, indice) — parità IL
                        // (ArrayHelpers.AEvalIndexed) e C99* (gmb_array_aeval). L'ARITÀ del
                        // block arriva come 3° argomento dal compilatore: il prologo lega gli
                        // argomenti in ordine INVERSO, quindi a un block a 1 parametro va
                        // passato SOLO l'elemento (altrimenti l'indice finirebbe nel primo
                        // parametro — regressione arrays18).
                        var evalArity = args.Length >= 3 && args[2] is double ad ? (int)ad : 2;
                        void EvalCall(object? item, int index)
                        {
                            if (evalArity >= 2) CallFnSync(task, args[1], item, (double)index);
                            else CallFnSync(task, args[1], item);
                        }
                        if (evalTarget is System.Collections.IList evalList) {
                            for (var ei = 0; ei < evalList.Count; ei++)
                                EvalCall(evalList[ei], ei);
                        } else if (evalTarget is System.Array evalArr) {
                            for (var ei = 0; ei < evalArr.Length; ei++)
                                EvalCall(evalArr.GetValue(ei), ei);
                        } else if (evalTarget is System.Collections.ICollection evalCol) {
                            var ei = 0;
                            foreach (var evalItem in evalCol)
                                EvalCall(evalItem, ei++);
                        }
                    }
                }
                return null;
        
        },
        [BuiltInFunction.AFILTER] = (task, args) =>
        {
                if (args.Length >= 2 && IsCallable(args[1])) {
                    object? filterTarget = args[0];
                    if (filterTarget is RefToVar rv1) filterTarget = rv1.Array[rv1.Index];
                    if (filterTarget is System.Collections.IList srcList) {
                        var result = new System.Collections.Generic.List<object?>();
                        foreach (var item in srcList) {
                            var res = CallFnSync(task, args[1], item);
                            if (SafeDouble(res) != 0.0) result.Add(item);
                        }
                        return result;
                    }
                }
                return null;
        
        },
        [BuiltInFunction.AMAP] = (task, args) =>
        {
                if (args.Length >= 2 && IsCallable(args[1])) {
                    object? mapTarget = args[0];
                    if (mapTarget is RefToVar rv2) mapTarget = rv2.Array[rv2.Index];
                    if (mapTarget is System.Collections.IList srcList2) {
                        var result2 = new System.Collections.Generic.List<object?>();
                        foreach (var item in srcList2) {
                            var res2 = CallFnSync(task, args[1], item);
                            result2.Add(res2);
                        }
                        return result2;
                    }
                }
                return null;
        
        },
        [BuiltInFunction.AREDUCE] = (task, args) =>
        {
                // Parità IL (ArrayHelpers.AReduce/AReduceNoInitial): il 3° argomento
                // initialValue (OPTIONAL in StdLib) è ONORATO — acc = initialValue e fold
                // su tutti gli elementi. Senza initialValue: acc = items[0], fold da 1.
                // Lista vuota: con init → initialValue invariato; senza init → default(T)
                // (0 per l'accumulatore numerico), non più null come prima.
                if (args.Length >= 2 && IsCallable(args[1])) {
                    object? reduceTarget = args[0];
                    if (reduceTarget is RefToVar rv3) reduceTarget = rv3.Array[rv3.Index];
                    if (reduceTarget is System.Collections.IList reduceList) {
                        bool hasInit = args.Length >= 3;
                        if (reduceList.Count == 0) return hasInit ? args[2] : 0.0;
                        object? acc;
                        int startIdx;
                        if (hasInit) { acc = args[2]; startIdx = 0; }
                        else { acc = reduceList[0]; startIdx = 1; }
                        for (int ri = startIdx; ri < reduceList.Count; ri++) {
                            var reduced = CallFnSync(task, args[1], acc, reduceList[ri]);
                            if (reduced != null) acc = reduced;
                        }
                        return acc;
                    }
                }
                return null;
        
        },
        [BuiltInFunction.ASCAN] = (task, args) =>
        {
                // Indice 0-based, -1 se non trovato — stessa convenzione del tree-walking
                // Interpreter (Gpm.Arrays.Functions.Step.cs, ExecuteAScanStep) e del sample
                // Executor/Samples/Clipper/ClipperArrays.bas, con cui il codice BASIC
                // esistente (es. ArraysComplexProcessing.bas: `IF idx > -1 THEN`) è scritto.
                // La vecchia convenzione 1-based/0-non-trovato rendeva quel controllo un
                // tautologia sempre vera (0 e ogni indice positivo sono entrambi > -1),
                // facendo entrare SEMPRE nel ramo "trovato" anche quando ASCAN non trovava
                // nulla.
                if (args.Length >= 2 && IsCallable(args[1])) {
                    object? scanTarget = args[0];
                    if (scanTarget is RefToVar rv4) scanTarget = rv4.Array[rv4.Index];
                    if (scanTarget is System.Collections.IList scanList) {
                        for (int si = 0; si < scanList.Count; si++) {
                            var scanRes = CallFnSync(task, args[1], scanList[si]);
                            if (SafeDouble(scanRes) != 0.0) return (double)si;
                        }
                        return -1.0;
                    }
                }
                if (args.Length >= 2) {
                    object? plainTarget = args[0];
                    if (plainTarget is RefToVar rv5) plainTarget = rv5.Array[rv5.Index];
                    if (plainTarget is System.Collections.IList plainList) {
                        for (int sk = 0; sk < plainList.Count; sk++) {
                            if (Equals(plainList[sk], args[1])) return (double)sk;
                        }
                        return -1.0;
                    }
                }
                return -1.0;
        
        },
        [BuiltInFunction.AFLAT] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList flatList) {
                    var flat = new System.Collections.Generic.List<object?>();
                    void FRec(System.Collections.IEnumerable en) { foreach (var it in en) { if (it is System.Collections.IEnumerable sub && it is not string) FRec(sub); else flat.Add(it); } }
                    FRec(flatList); return flat;
                }
                return 0.0;
        
        },
        [BuiltInFunction.AUNIQUE] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList uniqList)
                    return uniqList.Cast<object?>().Distinct().ToList();
                return 0.0;
        
        },
        [BuiltInFunction.ASORT] = (task, args) =>
        {
                // ASORT(array, [compareFunction], [order$]) — args[1]/args[2] venivano
                // completamente ignorati: qualunque ASORT con una funzione di confronto
                // (necessaria per ordinare array di UDT, mai IComparable) non riordinava
                // affatto l'array, e "D" (discendente) non veniva mai applicato. args[0] è
                // un RefToVar (PushTargetByRef, non il valore diretto) — va spacchettato
                // come fanno già AEVAL/AFILTER/AMAP/ecc., altrimenti l'intero branch non
                // veniva mai raggiunto.
                {
                    object? sortArgTarget = args.Length >= 1 ? args[0] : null;
                    if (sortArgTarget is RefToVar sortRv) sortArgTarget = sortRv.Array[sortRv.Index];
                    if (sortArgTarget is System.Collections.IList sortList) {
                        var arr = sortList.Cast<object?>().ToArray();
                        var descending = args.Length >= 3 && args[2] is string ord &&
                            ord.Equals("D", StringComparison.OrdinalIgnoreCase);
                        if (args.Length >= 2 && IsCallable(args[1])) {
                            // Insertion sort STABILE (identico a ArrayHelpers.ASortWithComparer
                            // e alle gmb_array_sort_*_cmp C): sposta a sinistra se
                            // compareFunc(key, prev) < 0. Gestisce SIA il 3-vie (segno) SIA il
                            // predicato booleano BASIC (TRUE = -1 = "prima"; FALSE = 0 = "non
                            // prima"). Array.Sort trattava FALSE (0) come "uguale" → ASORT
                            // parziale/no-op. `bool` → -1/0 (SafeDouble(bool) darebbe 0).
                            for (var i = 1; i < arr.Length; i++) {
                                var key = arr[i];
                                var j = i - 1;
                                while (j >= 0) {
                                    var raw = CallFnSync(task, args[1], key, arr[j]);
                                    var cmp = raw is bool bv ? (bv ? -1.0 : 0.0) : SafeDouble(raw);
                                    if (!(cmp < 0)) break;
                                    arr[j + 1] = arr[j];
                                    j--;
                                }
                                arr[j + 1] = key;
                            }
                        } else {
                            // Comparer SafeDouble: gli elementi numerici nel P-CODE sono boxed
                            // come double/long/int — il comparer default di Array.Sort su
                            // object?[] lancia "Failed to compare two elements" quando trova
                            // tipi misti o null (es. ASORT su array con null residui).
                            Array.Sort(arr, (x, y) =>
                            {
                                if (x is null && y is null) return 0;
                                if (x is null) return -1;
                                if (y is null) return 1;
                                var xd = SafeDouble(x);
                                var yd = SafeDouble(y);
                                return xd < yd ? -1 : xd > yd ? 1 : 0;
                            });
                        }
                        if (descending) Array.Reverse(arr);
                        for (var wi = 0; wi < arr.Length; wi++) sortList[wi] = arr[wi];
                        return new System.Collections.Generic.List<object?>(arr);
                    }
                }
                return 0.0;
        
        },
        [BuiltInFunction.AREVERSE] = (task, args) =>
        {
                // Parità IL (IlProgramCompiler.Builtins.cs → EmitArrayUtilsCloneThenMutate +
                // ArrayHelpers: AREVERSE ritorna UNA NUOVA lista rovesciata e NON muta mai
                // l'array sorgente — il risultato si usa SOLO dal valore di ritorno, come
                // AMAP/AFILTER/ACLONE. In passato qui si faceva il write-back indice-per-
                // indice sulla sorgente (semantica "aReverse in-place" di Clipper), che è
                // la divergenza da IL documentata in ArrayHelpers.cs:695-703.
                {
                    object? revArgTarget = args.Length >= 1 ? args[0] : null;
                    if (revArgTarget is RefToVar revRv) revArgTarget = revRv.Array[revRv.Index];
                    if (revArgTarget is System.Collections.IList revList) {
                        var arr = revList.Cast<object?>().ToArray();
                        Array.Reverse(arr);
                        return new System.Collections.Generic.List<object?>(arr);
                    }
                }
                return 0.0;
        
        },
        [BuiltInFunction.ACOUNT] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList countList && IsCallable(args[1])) {
                    double cnt = 0;
                    foreach (var item in countList) {
                        if (SafeDouble(CallFnSync(task, args[1], item)) != 0.0) cnt++;
                    }
                    return cnt;
                }
                return 0.0;
        
        },
        [BuiltInFunction.ACLONE] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList srcClone)
                    return new System.Collections.Generic.List<object?>(srcClone.Cast<object?>());
                return null;
        
        },
        [BuiltInFunction.ASHUFFLE] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList srcShuf) {
                    var arr = srcShuf.Cast<object?>().ToArray();
                    var rng = new System.Random();
                    for (int i = arr.Length - 1; i > 0; i--) {
                        int j = rng.Next(i + 1);
                        (arr[i], arr[j]) = (arr[j], arr[i]);
                    }
                    return new System.Collections.Generic.List<object?>(arr);
                }
                return null;
        
        },
        [BuiltInFunction.ABINARYSEARCH] = (task, args) =>
        {
                // Parità IL (CommonLib.ArrayUtils.BinarySearch, ArrayHelpers): VERA ricerca
                // binaria — richiede un array GIÀ ORDINATO per il comparer; indice 0-based,
                // -1 se non trovato. Prima era una scan lineare con Equals (divergenza da IL
                // documentata). I numerici sono normalizzati a double: i valori boxed misti
                // (int vs double) fanno esplodere Comparer<object>.Default.
                if (args.Length >= 2 && args[0] is System.Collections.IList bsList) {
                    var bsTarget = args[1];
                    int lo = 0, hi = bsList.Count - 1;
                    while (lo <= hi) {
                        int mid = lo + (hi - lo) / 2;
                        var bsItem = bsList[mid];
                        int c;
                        if (bsItem is null && bsTarget is null) c = 0;
                        else if (bsItem is null) c = -1;
                        else if (bsTarget is null) c = 1;
                        else if (IsNumeric(bsItem) && IsNumeric(bsTarget))
                            c = SafeDouble(bsItem).CompareTo(SafeDouble(bsTarget));
                        else c = System.Collections.Generic.Comparer<object>.Default.Compare(bsItem, bsTarget);
                        if (c == 0) return (double)mid;
                        if (c < 0) lo = mid + 1;
                        else hi = mid - 1;
                    }
                    return -1.0;
                }
                return -1.0;
        
        },
        [BuiltInFunction.AJOIN] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is System.Collections.IList joinList) {
                    var sep = args.Length >= 2 ? args[1]?.ToString() ?? "" : ",";
                    return string.Join(sep, joinList.Cast<object?>().Select(a => a?.ToString() ?? ""));
                }
                return "";
        
        },
        [BuiltInFunction.ACHUNK] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList chunkList && SafeDouble(args[1]) is double chunkSize && chunkSize > 0) {
                    int cs = (int)chunkSize;
                    var chunkResult = new System.Collections.Generic.List<object?>();
                    for (int ci = 0; ci < chunkList.Count; ci += cs) {
                        var chunk = new System.Collections.Generic.List<object?>();
                        for (int cj = ci; cj < ci + cs && cj < chunkList.Count; cj++)
                            chunk.Add(chunkList[cj]);
                        chunkResult.Add(chunk);
                    }
                    return chunkResult;
                }
                return null;
        
        },
        [BuiltInFunction.APAD] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList padList) {
                    int padNewLen = (int)SafeDouble(args[1]);
                    var padResult = new System.Collections.Generic.List<object?>(padList.Cast<object?>());
                    while (padResult.Count < padNewLen)
                        padResult.Add(args.Length >= 3 ? args[2] : null);
                    if (padResult.Count > padNewLen)
                        padResult = new System.Collections.Generic.List<object?>(padResult.Take(padNewLen));
                    return padResult;
                }
                return null;
        
        },
        [BuiltInFunction.AZIP] = (task, args) =>
        {
                if (args.Length >= 2 && args[0] is System.Collections.IList zip1 && args[1] is System.Collections.IList zip2) {
                    var zipResult = new System.Collections.Generic.List<object?>();
                    int zipLen = System.Math.Min(zip1.Count, zip2.Count);
                    for (int zi = 0; zi < zipLen; zi++) {
                        var pair = new System.Collections.Generic.List<object?> { zip1[zi], zip2[zi] };
                        zipResult.Add(pair);
                    }
                    return zipResult;
                }
                return null;
        
        },
        [BuiltInFunction.ATOKENS] = (task, args) =>
        {
                if (args.Length >= 1 && args[0] is string tokStr) {
                    var tokDelims = args.Length >= 2 ? args[1]?.ToString() ?? " " : " ";
                    return new System.Collections.Generic.List<object?>(tokStr.Split(tokDelims.ToCharArray(), StringSplitOptions.RemoveEmptyEntries));
                }
                if (args.Length >= 1 && args[0] is double tokDbl) {
                    var tokDelims2 = args.Length >= 2 ? args[1]?.ToString() ?? " " : " ";
                    return new System.Collections.Generic.List<object?>(tokDbl.ToString().Split(tokDelims2.ToCharArray(), StringSplitOptions.RemoveEmptyEntries));
                }
                return new System.Collections.Generic.List<object?>();
        
        },
        [BuiltInFunction.AINS] = (task, args) =>
        {
                // AINS array, pos[, val] — statement in-place: scrive nel RefToVar (il compiler
                // fa OP_POP, il valore di ritorno non è riassegnato). Prima ritornava un nuovo
                // List che veniva scartato → la variabile non cambiava mai.
                if (args.Length >= 2) {
                    object? target = args[0];
                    if (target is RefToVar rvI)
                    {
                        object? srcI = rvI.Array[rvI.Index];
                        var insResult = new System.Collections.Generic.List<object?>();
                        if (srcI is System.Collections.IEnumerable enI)
                            foreach (var e in enI) insResult.Add(e);
                        int insPos = (int)SafeDouble(args[1]);
                        // Parità IL (EmitAInsStatement: newarr è zero-inizializzato): lo slot
                        // nuovo è il DEFAULT DEL TIPO elemento — 0 per array numerici, null per
                        // stringhe/oggetti. Prima inseriva sempre null → su un array numerico
                        // PCode stampava vuoto dove IL stampa 0 (ArraysMiniTest.bas: 10 0 20 30).
                        object? insVal = args.Length >= 3
                            ? args[2]
                            : (insResult.Count > 0 && IsNumeric(insResult[0]!) ? 0.0 : null);
                        if (insPos >= 0 && insPos <= insResult.Count)
                            insResult.Insert(insPos, insVal);
                        rvI.Array[rvI.Index] = insResult;
                        return null;
                    }
                }
                return null;
        
        },
        [BuiltInFunction.ADEL] = (task, args) =>
        {
                // ADEL array, pos — statement in-place: scrive nel RefToVar.
                if (args.Length >= 2) {
                    object? target = args[0];
                    if (target is RefToVar rvD)
                    {
                        object? srcD = rvD.Array[rvD.Index];
                        var delResult = new System.Collections.Generic.List<object?>();
                        if (srcD is System.Collections.IEnumerable enD)
                            foreach (var e in enD) delResult.Add(e);
                        int delPos = (int)SafeDouble(args[1]);
                        if (delPos >= 0 && delPos < delResult.Count)
                            delResult.RemoveAt(delPos);
                        rvD.Array[rvD.Index] = delResult;
                        return null;
                    }
                }
                return null;
        
        },
        [BuiltInFunction.AFILL] = (task, args) =>
        {
                // args[0] è un RefToVar nella forma statement (PushTargetByRef) — va
                // spacchettato come fanno ASORT/ADEL, altrimenti lo statement è un no-op.
                object? fillTarget = args.Length >= 1 ? args[0] : null;
                if (fillTarget is RefToVar fillRv) fillTarget = fillRv.Array[fillRv.Index];
                if (fillTarget is System.Array fillArr && fillArr.Rank == 1) {
                    // Rank-1 qualsiasi (inclusi lower bound != 0, es. DIM A(1 TO 3) che crea
                    // object[*] NON IList): riempie per indice piatto 0..Length-1 — SetValue
                    // piatto vale per qualunque rank-1. Prima gli array con lower != 0
                    // cadevano in tutti i rami (element object non numerico + non IList) e
                    // la builtin ritornava null: il chiamante (DIM+AFILL) faceva OP_POP sul
                    // nulla → crash "Index was outside the bounds" (bug 2026-09-30).
                    var fillEt = fillArr.GetType().GetElementType();
                    int fillStart = (args.Length >= 3 && args[2] != null) ? (int)SafeDouble(args[2]) : 0;
                    int fillCount = (args.Length >= 4 && args[3] != null) ? (int)SafeDouble(args[3]) : fillArr.Length - fillStart;
                    int fillBase = fillArr.GetLowerBound(0);
                    for (int i = fillStart; i < fillStart + fillCount && i < fillArr.Length; i++)
                    {
                        object? fillVal = SafeDouble(args[1]);
                        if (fillEt != null && fillEt != typeof(object))
                            fillVal = System.Convert.ChangeType(fillVal, fillEt,
                                System.Globalization.CultureInfo.InvariantCulture);
                        fillArr.SetValue(fillVal, i + fillBase);
                    }
                    return fillArr;
                }
                if (fillTarget is System.Array fillArrN && fillArrN.Rank >= 2) {
                    // Matrice N-D (DIM m(r,c)): object[,] nasce null → una riga mai assegnata
                    // si stampa vuota (IL dà 0). SetValue(object,int) NON è legale su array
                    // multi-D ("Array was not a one-dimensional array") → indice multi-array.
                    var fillEtN = fillArrN.GetType().GetElementType();
                    object? fillValN = SafeDouble(args[1]);
                    if (fillEtN != null && fillEtN != typeof(object))
                        fillValN = System.Convert.ChangeType(fillValN, fillEtN,
                            System.Globalization.CultureInfo.InvariantCulture);
                    int rk = fillArrN.Rank;
                    var dims = new int[rk];
                    var idx = new int[rk];
                    for (int d = 0; d < rk; d++) dims[d] = fillArrN.GetLength(d);
                    for (int flat = 0; flat < fillArrN.Length; flat++)
                    {
                        int rem = flat;
                        for (int d = rk - 1; d >= 0; d--) { idx[d] = rem % dims[d]; rem /= dims[d]; }
                        fillArrN.SetValue(fillValN, idx);
                    }
                    return fillArrN;
                }
                if (args.Length >= 2 && fillTarget is System.Collections.IList fillList) {
                    var fillVal = args[1];
                    int fillStart = (args.Length >= 3 && args[2] != null) ? (int)SafeDouble(args[2]) : 0;
                    int fillCount = (args.Length >= 4 && args[3] != null) ? (int)SafeDouble(args[3]) : fillList.Count - fillStart;
                    for (int i = fillStart; i < fillStart + fillCount && i < fillList.Count; i++)
                        fillList[i] = fillVal;
                    return fillList;
                }
                return null;
        },
        [BuiltInFunction.ACOPY] = (task, args) =>
        {
                // Forma statement: sorgente by-value (LoadVariable), destinazione RefToVar
                // (PushTargetByRef) — spacchettare entrambe come ASORT/ADEL.
                object? copySrc = args.Length >= 1 ? args[0] : null;
                if (copySrc is RefToVar srcRv) copySrc = srcRv.Array[srcRv.Index];
                object? copyDst = args.Length >= 2 ? args[1] : null;
                if (copyDst is RefToVar dstRv) copyDst = dstRv.Array[dstRv.Index];
                if (copySrc is System.Array srcArr && copyDst is System.Array dstArr)
                {
                    var srcEt = srcArr.GetType().GetElementType();
                    if ((srcEt == typeof(double) || srcEt == typeof(float) || srcEt == typeof(int)
                         || srcEt == typeof(long) || srcEt == typeof(short) || srcEt == typeof(byte))
                        && dstArr.GetType().GetElementType() == srcEt)
                    {
                        int srcStart = (args.Length >= 3 && args[2] != null) ? (int)SafeDouble(args[2]) : 0;
                        int copyCount = (args.Length >= 4 && args[3] != null) ? (int)SafeDouble(args[3]) : srcArr.Length - srcStart;
                        int dstStart = (args.Length >= 5 && args[4] != null) ? (int)SafeDouble(args[4]) : 0;
                        int eff = System.Math.Min(srcArr.Length - srcStart, System.Math.Min(dstArr.Length - dstStart, copyCount));
                        System.Array.Copy(srcArr, srcStart, dstArr, dstStart, System.Math.Max(0, eff));
                        return srcArr;
                    }
                }
                if (copySrc is System.Collections.IList srcCopy && copyDst is System.Collections.IList dstCopy) {
                    int srcStart = (args.Length >= 3 && args[2] != null) ? (int)SafeDouble(args[2]) : 0;
                    int copyCount = (args.Length >= 4 && args[3] != null) ? (int)SafeDouble(args[3]) : srcCopy.Count - srcStart;
                    int dstStart = (args.Length >= 5 && args[4] != null) ? (int)SafeDouble(args[4]) : 0;
                    for (int i = 0; i < copyCount && srcStart + i < srcCopy.Count && dstStart + i < dstCopy.Count; i++)
                        dstCopy[dstStart + i] = srcCopy[srcStart + i];
                    return srcCopy;
                }
                return null;
        
        },
        [BuiltInFunction.SAFECAST] = (task, args) =>
        {
                // Safe cast (AS?): try to convert value to target type, return NIL on failure
                if (args.Length >= 1)
                {
                    var scVal = args[0];
                    // Try numeric: if the value can be parsed as a number, return it
                    if (scVal is string scStr)
                    {
                        if (double.TryParse(scStr, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var scNum))
                            return scNum;
                        return null; // conversion failed → NIL
                    }
                    // Already a number: return as-is
                    if (scVal is double or int or long or short or float or decimal)
                        return SafeDouble(scVal ?? 0);
                    return scVal; // pass through for compatible types
                }
                return null;
        
        },
        [BuiltInFunction.AUTOCORRELATION] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.Autocorrelation(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.AVG] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Avg(
                    args.Length == 1 && (args[0] is double[] or object?[] or System.Collections.IList)
                        ? ToDoubleArr(args[0])
                        : args.Select(SafeDouble).ToArray());
        
            // --- Distribuzioni di probabilita' (PDF/CDF continuo) ---
        },
        [BuiltInFunction.BASE32DECODE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Base32Decode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.BASE32ENCODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Base32Encode(ArgOr(args, 0));
        },
        [BuiltInFunction.BASE58DECODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Base58Decode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.BASE58ENCODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Base58Encode(ArgOr(args, 0));
        },
        [BuiltInFunction.BERNOULLIPMF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.BernoulliPmf(SafeDouble(args.Length > 1 ? args[1] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.BETACDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.BetaCdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.BETAPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.BetaPdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.BIDASKSPREAD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BidAskSpread(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.BINOMIALOPTION] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BinomialOption(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), (int)SafeDouble(ArgOr(args, 5)), (ArgOr(args, 6)?.ToString() ?? "CALL").Equals("PUT", StringComparison.OrdinalIgnoreCase));
        },
        [BuiltInFunction.BINOMIALPMF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.BinomialPmf(SafeDouble(args.Length > 2 ? args[2] : null), (int)SafeDouble(args.Length > 1 ? args[1] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.BLAKE2B256] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.Blake2b256(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.BLOCKDIFFICULTY] = (task, args) =>
        {
            {
                var header = ToBytes(ArgOr(args, 0));
                var bits = (int)SafeDouble(ArgOr(args, 1));
                return double.Parse(CommonLib.BlockchainUtils.BlockDifficulty(header, bits).ToString(), System.Globalization.CultureInfo.InvariantCulture);
            }
        },
        [BuiltInFunction.BONDEQUIVALENTYIELD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BondEquivalentYield(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.BONDPRICE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BondPrice(ToDateTime(ArgOr(args, 0)), ToDateTime(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), (int)SafeDouble(ArgOr(args, 5)), args.Length > 6 ? (int)SafeDouble(ArgOr(args, 6)) : 0);
        },
        [BuiltInFunction.BONDYIELD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BondYield(ToDateTime(ArgOr(args, 0)), ToDateTime(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), (int)SafeDouble(ArgOr(args, 5)), args.Length > 6 ? (int)SafeDouble(ArgOr(args, 6)) : 0);
        },
        [BuiltInFunction.BOOKVALUEPERSHARE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.BookValuePerShare(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.BSCALL] = (task, args) =>
        {
            {
                var dy = SafeDouble(ArgOr(args, 5));
                return dy == 0 ? Basic.StdLibRuntime.TradingFunctions.BsCall(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4))) : Basic.StdLibRuntime.TradingFunctions.MertonCall(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), dy, SafeDouble(ArgOr(args, 4)));
            }
        },
        [BuiltInFunction.BSPUT] = (task, args) =>
        {
            {
                var dy = SafeDouble(ArgOr(args, 5));
                return dy == 0 ? Basic.StdLibRuntime.TradingFunctions.BsPut(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4))) : Basic.StdLibRuntime.TradingFunctions.MertonPut(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), dy, SafeDouble(ArgOr(args, 4)));
            }
        },
        [BuiltInFunction.BTCADDRESSFROMPUB] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                // Il flag testnet può arrivare come bool (TRUE/FALSE): SafeDouble(bool) vale 0
                // ("True" non è parsabile) → il testnet veniva ignorato. Gestisci il bool.
                var testnet = args.Length > 1 && (ArgOr(args, 1) is bool bt ? bt : SafeDouble(args[1]) != 0.0);
                return Basic.IlBackend.IlRuntime.BtcAddressFromPub(ArgOr(args, 0), testnet);
        },
        [BuiltInFunction.CAPITALGAINSYIELD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.CapitalGainsYield(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.CAPM] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.Capm(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.CHARM] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Charm(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), ArgOr(args, 5)?.ToString() ?? "CALL");
        },
        [BuiltInFunction.CHICDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiCdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.CHIPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiPdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.CHISQUAREDCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiSquaredCdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.CHISQUAREDPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiSquaredPdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
            // --- STATS: delegato a ExecuteBuiltInStats (PCodeProcessor.Builtins.Stats.cs) ---
        },
        [BuiltInFunction.COMMAND] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Command((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.COMMANDCOUNT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.CommandCount();
        },
        [BuiltInFunction.ENVGET] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.EnvGet(ArgOr(args, 0)?.ToString());
        },
        [BuiltInFunction.ENVHAS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.EnvHas(ArgOr(args, 0)?.ToString());
        },
        [BuiltInFunction.CUMIPMT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Cumipmt(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), SafeDouble(ArgOr(args, 5)));
        },
        [BuiltInFunction.CUMPRINC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Cumprinc(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), SafeDouble(ArgOr(args, 5)));
        },
        [BuiltInFunction.DB] = (task, args) =>
        {
                return (double)Basic.StdLibRuntime.FinanceFunctions.Db((decimal)SafeDouble(ArgOr(args, 0)), (decimal)SafeDouble(ArgOr(args, 1)), (decimal)SafeDouble(ArgOr(args, 2)), (decimal)SafeDouble(ArgOr(args, 3)), (int)SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.DECOMPOSE] = (task, args) =>
        {
            {
                var mx = Basic.StdLibRuntime.TimeSeriesFunctions.Decompose(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
                var result = new List<object?>();
                foreach (var row in mx) { var rowList = new List<object?>(); foreach (var v in row) rowList.Add(v); result.Add(rowList); }
                return result;
            }
        },
        [BuiltInFunction.DELTA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Delta(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), ArgOr(args, 5)?.ToString() ?? "CALL");
        },
        [BuiltInFunction.DELTASPEED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DeltaSpeed(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.DESEASONALIZE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.Deseasonalize(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.DIFFERENCE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.Difference(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.DILUTEDEPS] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.DilutedEps(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.DPS] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.Dps(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.DURATION] = (task, args) =>
        {
                return Basic.StdLibRuntime.FinanceFunctions.Duration(ToDateTime(ArgOr(args, 0)), ToDateTime(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), (int)SafeDouble(ArgOr(args, 4)), (int)SafeDouble(ArgOr(args, 5)));
        },
        [BuiltInFunction.ECDSASECP256K1SIGN] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.EcdsaSecp256k1Sign(ToBytes(ArgOr(args, 0)), ToBytes(ArgOr(args, 1)));
        },
        [BuiltInFunction.GENERATEMNEMONIC] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.GenerateMnemonic(args.Length > 0 ? SafeDouble(ArgOr(args, 0)) : 12);
        },
        [BuiltInFunction.MNEMONICTOSEED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MnemonicToSeed(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.DERIVEKEY] = (task, args) =>
        {
            {
                var seed = ToBytes(ArgOr(args, 0));
                var path = ArgOr(args, 1)?.ToString() ?? "m";
                var dict = CommonLib.BlockchainUtils.DeriveKey(seed, path);
                return new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["privateKey"] = dict["privateKey"],
                    ["publicKey"] = dict["publicKey"],
                    ["chainCode"] = dict["chainCode"]
                };
            }
        },
        [BuiltInFunction.GETPUBLICKEY] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.GetPublicKey(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.CONTRACT] = (task, args) =>
        {
                // CONTRACT(address, interfaceName, rpcUrl?, privateKey?) — restituisce un dict proxy.
                return new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["address"] = ArgOr(args, 0)?.ToString() ?? "",
                    ["interface"] = ArgOr(args, 1)?.ToString() ?? "",
                    ["rpcUrl"] = args.Length > 2 ? ArgOr(args, 2)?.ToString() : "",
                    ["privateKey"] = args.Length > 3 ? ArgOr(args, 3)?.ToString() : ""
                };
        },
        [BuiltInFunction.EMA] = (task, args) =>
        {
            {
                var data = ToDoubleArr(ArgOr(args, 0));
                var period = SafeDouble(ArgOr(args, 1));
                return Basic.StdLibRuntime.TimeSeriesFunctions.Ema(data, period);
            }
            // --- MOUSE: delegato a ExecuteBuiltInMouse (PCodeProcessor.Builtins.Mouse.cs) ---
        },
        [BuiltInFunction.AIPERMISSION] = (task, args) =>
        {
                // AI PERMISSION DEFAULT lvl (1) | nome, lvl (2) | nome, TOOL tool, policy (3)
            {
                if ((args?.Length ?? 0) == 1)
                {
                    var lvl = ArgOr(args, 0)?.ToString() ?? "";
                    return Basic.IlBackend.IlRuntime.AiPermissionDefault(lvl) != 0 ? -1.0 : 0.0;
                }
                if ((args?.Length ?? 0) == 3)
                    return Basic.IlBackend.IlRuntime.AiPermissionTool(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "") != 0 ? -1.0 : 0.0;
                return Basic.IlBackend.IlRuntime.AiPermissionLevel(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "") != 0 ? -1.0 : 0.0;
            }
        },
        [BuiltInFunction.AIANALYZEIMAGE] = (task, args) =>
        {
                // Vision (lettura file + inferenza, può durare) → blocking pool, mai sul worker.
            {
                var path = ArgOr(args, 0)?.ToString() ?? "";
                var prompt = ArgOr(args, 1)?.ToString() ?? "";
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiAnalyzeImage(path, prompt),
                    result => task.PushRef(result));
                return ParkedBuiltinResult;
            }
            // --- AIAGENT: delegato a ExecuteBuiltInAiagent (PCodeProcessor.Builtins.Aiagent.cs) ---
        },
        [BuiltInFunction.PROCESS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ProcessDocument(
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.EPS] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.Eps(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.EQUITYMULTIPLIER] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.EquityMultiplier(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ETHADDRESSCHECKSUM] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.EthAddressChecksum(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.ETHADDRESSFROMPUB] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.EthAddressFromPub(ArgOr(args, 0));
        },
        [BuiltInFunction.EXPONENTIALCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ExponentialCdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.EXPONENTIALPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ExponentialPdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.FCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.FCdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.FPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.FPdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.FROMBASE64] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.FromBase64(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.FROMHEX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.FromHex(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GAMMA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Gamma(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.GAMMACOLOR] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GammaColor(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        
            // --- Obbligazioni (Bonds) ---
        },
        [BuiltInFunction.GEOMETRICMEANRETURN] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.GeometricMeanReturn(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.GEOMETRICPMF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.GeometricPmf(SafeDouble(args.Length > 1 ? args[1] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.GROWTHRATE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.GrowthRate(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.HASHFILESTRING] = (task, args) =>
        {
                return ToHexStr(CommonLib.CryptoUtils.CoreHashFile(ArgOr(args, 1)?.ToString() ?? "SHA256", ArgOr(args, 0)?.ToString() ?? ""));
        },
        [BuiltInFunction.HOLDINGPERIODRETURN] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.HoldingPeriodReturn(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.HYPERGEOMETRICPMF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.HypergeometricPmf((int)SafeDouble(args.Length > 1 ? args[1] : null), (int)SafeDouble(args.Length > 2 ? args[2] : null), (int)SafeDouble(args.Length > 3 ? args[3] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
            // --- INTERPOLATE: delegato a ExecuteBuiltInInterpolate (PCodeProcessor.Builtins.Interpolate.cs) ---
        },
        [BuiltInFunction.ABIENCODE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): Keccak selector + ABI words, e l'appiattimento
                // RICORSIVO del tail variadico (che può arrivare impacchettato dal ParamArray o
                // essere un array a runtime). Vive SOLO lì (una sola implementazione, condivisa
                // con la gamba IL): prima era duplicato qui.
                if (args.Length >= 1)
                {
                    var tail = new object[args.Length - 1];
                    for (int i = 1; i < args.Length; i++) tail[i - 1] = ArgOr(args, i)!;
                    return Basic.IlBackend.IlRuntime.AbiEncode(ArgOr(args, 0)?.ToString() ?? "", tail);
                }
                return "0x";
        },
        [BuiltInFunction.ABIDECODE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica ABI (bool/string/uint) — unica impl.
                if (args.Length >= 2)
                {
                    var typeList = new List<string>();
                    if (ArgOr(args, 1) is System.Collections.IList tl)
                        foreach (var t in tl) typeList.Add(t?.ToString() ?? "uint256");
                    else if (ArgOr(args, 1) is string ts) typeList.Add(ts);
                    return Basic.IlBackend.IlRuntime.AbiDecode(
                        ArgOr(args, 0)?.ToString() ?? "", typeList.ToArray());
                }
                return Array.Empty<object?>();
        },
        [BuiltInFunction.KEYEXPORT] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.KeyExport((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.KEYIMPORT] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.KeyImport(ArgOr(args, 0)?.ToString() ?? "", 0);
                return 0.0;
        },
        [BuiltInFunction.KEYGEN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione (genera
                // una coppia di chiavi e registra i due handle nel registry). IlRuntime.
                return Basic.IlBackend.IlRuntime.KeyGen(
                    ArgOr(args, 0)?.ToString() ?? "RSA",
                    (long)SafeDouble(ArgOr(args, 1, 2048)));
        },
        [BuiltInFunction.KEY_CLOSE] = (task, args) =>
        {
                if (args.Length > 0) Basic.Runtime.Crypto.CryptoKeyRegistry.Close((long)SafeDouble(args[0]));
                return null;
        },
        [BuiltInFunction.KEY_MAKE] = (task, args) =>
        {
            {
                // KEY.MAKE(tasto, modificatori) → intero (parità IL IlRuntime.KeyMake e
                // StdLib LongType). ON KEY accetta sia l'intero sia la KeyCombination.
                if (args.Length >= 2) return (double)(int)Convert.ToInt64(args[0]);
                return 0.0;
            }
        },
        [BuiltInFunction.LOGISTICPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.LogisticPdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.LOGNORMALCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.LognormalCdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.LOGNORMALPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.LognormalPdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.MD5STRING] = (task, args) =>
        {
                return Basic.StdLibRuntime.CryptographyFunctions.StrMd5(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.MERTONCALL] = (task, args) =>
        {
                // Firma BASIC (StdLib.Trading.Options.cs): [S,K,T,r,v,q] — args[4]=volatilità,
                // args[5]=dividend yield. TradingFunctions.MertonCall(s,k,t,r,q,v) vuole l'ordine
                // POSIZIONALE inverso (q prima di v): senza lo scambio qui, args[4]/args[5]
                // venivano legati ai parametri sbagliati — MERTONCALL(100,100,1,0.05,0.2,0.03)
                // restituiva ~1.4E-07 invece del prezzo Black-Scholes-Merton atteso (~8), perché
                // volatilità 0.2 e dividend yield 0.03 finivano scambiati. Stesso bug in MERTONPUT.
                // Confrontare con il case BSCALL/BSPUT sopra, che già passa "dy" nella posizione
                // corretta.
                return Basic.StdLibRuntime.TradingFunctions.MertonCall(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 5)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.MERTONPUT] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.MertonPut(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 5)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.MOVINGAVERAGE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.MovingAverage(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TIMESERIESCOVARIANCEMATRIX] = (task, args) =>
        {
                return RectToJagged(CommonLib.Numerics.CovarianceMatrix(ToDoubleMatrix(ArgOr(args, 0))));
        },
        [BuiltInFunction.TIMESERIESMINMAX] = (task, args) =>
        {
            {
                var (min, max) = CommonLib.Numerics.MinMax(ToDoubleArr(ArgOr(args, 0)));
                return new[] { min, max };
            }
        },
        [BuiltInFunction.ROLLINGMEAN] = (task, args) =>
        {
                return CommonLib.Numerics.RollingMean(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGSKEWNESS] = (task, args) =>
        {
                return CommonLib.Numerics.RollingSkewness(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGKURTOSIS] = (task, args) =>
        {
                return CommonLib.Numerics.RollingKurtosis(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGMEDIAN] = (task, args) =>
        {
                return CommonLib.Numerics.RollingMedian(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGQUANTILE] = (task, args) =>
        {
                return CommonLib.Numerics.RollingQuantile(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ROLLINGSHARPE] = (task, args) =>
        {
                return CommonLib.Numerics.RollingSharpe(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ROLLINGSORTINO] = (task, args) =>
        {
                return CommonLib.Numerics.RollingSortino(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
        },
        [BuiltInFunction.ROLLINGMINMAX] = (task, args) =>
        {
                return CommonLib.Numerics.RollingMinMax(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1))).Select(item => new[] { item.min, item.max }).ToArray();
        },
        [BuiltInFunction.NAV] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.Nav(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.NEGATIVEBINOMIALPMF] = (task, args) =>
        {
                // Bug reale trovato in Fase 2 (StdLib.Runtime, 2026-08-04): questo call site
                // passava (r=args[2], p=args[1]) — INVERTITI rispetto all'ordine dichiarato
                // [x/k, r, p] in StdLib/StdLib.Stats.cs (args[1]=r, args[2]=p). Prima del fix a
                // CommonLib.Numerics.NegativeBinomialPmf (che internamente scambiava a sua volta
                // r/p chiamando MathNet), i due bug si annullavano per coincidenza, dando un
                // risultato accidentalmente corretto. Con CommonLib corretto, questo call site
                // invertito falliva sempre con "Parametri non validi" (r=p_value non sempre >0,
                // p=r_value spesso >1 e fuori da [0,1]). Corretto qui l'ordine, verificato
                // NegativeBinomialPmf(k=3,r=5,p=0.5) = 0.13671875 via test_pcode.
                return Basic.StdLibRuntime.MathFunctions.NegativeBinomialPmf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.NORMALCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.NormalCdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
            // Inverse CDF (ppf) e ISF — BASIC (p, params) → C# (params..., p)
        },
        [BuiltInFunction.NORMALPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.NormalPpf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.NORMALISF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.NormalIsf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.LOGNORMALPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.LognormalPpf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.CHISQUAREDPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiSquaredPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.CHISQUAREDISF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiSquaredIsf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.CHIPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ChiPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.BETAPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.BetaPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.FPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.FPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.FISF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.FIsf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.STUDENTTPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.StudentTPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.STUDENTTISF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.StudentTIsf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.EXPONENTIALPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ExponentialPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.EXPONENTIALISF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ExponentialIsf(SafeDouble(ArgOr(args, 1)), SafeDouble(args[0]));
        },
        [BuiltInFunction.WEIBULLPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.WeibullPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.LOGISTICPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.LogisticPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.UNIFORMPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.UniformPpf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.PARETOPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ParetoPpf(SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(args[0]));
        },
        [BuiltInFunction.TRIANGULARPPF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.TriangularPpf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 0.5), SafeDouble(args.Length > 3 ? args[3] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.NORMALPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.NormalPdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.PARETOPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.ParetoPdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.POISSONPMF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.PoissonPmf(SafeDouble(args.Length > 1 ? args[1] : null), (int)SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.PREFERREDSTOCKVALUE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.PreferredStockValue(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.PRICE] = (task, args) =>
        {
                return Basic.StdLibRuntime.FinanceFunctions.Price(ToDateTime(ArgOr(args, 0)), ToDateTime(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), (int)SafeDouble(ArgOr(args, 5)), (int)SafeDouble(ArgOr(args, 6)));
        },
        [BuiltInFunction.RANDOMBYTES] = (task, args) =>
        {
                return CommonLib.CryptoUtils.CoreGetRandomBytes((int)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.RANDOMDOUBLE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.RandomDouble(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.RANDOMINT] = (task, args) =>
        {
                return (double)Basic.IlBackend.IlRuntime.RandomInt((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.RANDOMNORMAL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RandomNormal(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.RETURNS] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.Returns(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.RHO] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Rho(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), ArgOr(args, 5)?.ToString() ?? "CALL");
        },
        [BuiltInFunction.RISKPREMIUM] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.RiskPremium(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.RLPENCODE] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.RlpEncode(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.ROLLINGAUTOCORRELATION] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingAutocorrelation(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)), (int)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ROLLINGCORRELATION] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingCorrelation(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), (int)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ROLLINGCOVARIANCE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingCovariance(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), (int)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ROLLINGDELTA] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingDelta(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGMAD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingMad(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGSTDDEV] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingStdDev(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.ROLLINGZSCORE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TimeSeriesFunctions.RollingZScore(ToDoubleArr(ArgOr(args, 0)), (int)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.SHA1STRING] = (task, args) =>
        {
                return Basic.StdLibRuntime.CryptographyFunctions.StrSha1(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.SHA256STRING] = (task, args) =>
        {
                return Basic.StdLibRuntime.CryptographyFunctions.StrSha256(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.SHA512STRING] = (task, args) =>
        {
                return Basic.StdLibRuntime.CryptographyFunctions.StrSha512(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.SIGN] = (task, args) =>
        {
                // Runtime comune (IlRuntime): registry handle unico (§0bis).
                return Basic.IlBackend.IlRuntime.Sign(ArgOr(args, 0), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.SKEWEDNORMALPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.SkewedNormalPdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args.Length > 3 ? args[3] : 0.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.STOCKVALUECONSTGROWTH] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.StockValueConstGrowth(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.STOCKVALUEZEROGROWTH] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.StockValueZeroGrowth(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.STOD] = (task, args) =>
        {
                return new DateValue(CommonLib.DateUtils.CoreStod((ArgOr(args, 0) ?? "").ToString() ?? "").ToDateTime(TimeOnly.MinValue).ToOADate());
        },
        [BuiltInFunction.STUDENTTCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.StudentTCdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.STUDENTTPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.StudentTPdf(SafeDouble(args.Length > 1 ? args[1] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.SYS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stesso switch — unica implementazione.
                return Basic.IlBackend.IlRuntime.Sys(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.SHELL] = (task, args) =>
        {
                // Runtime comune (IlRuntime): SYSTEM.SHELL "cmd". GPM (§0ter): l'operazione
                // (Process.Start+Wait) gira fuori dal worker (Task.Run) e il task si parcheggia.
                ParkBlocking(task,
                    () => (object?)(double)Basic.IlBackend.IlRuntime.SystemShell(ArgOr(args, 0)?.ToString() ?? ""),
                    result => task.PushRef(result));
                return ParkedBuiltinResult;
        },
        [BuiltInFunction.SHELLOUT] = (task, args) =>
        {
                ParkBlocking(task,
                    () => (object?)Basic.IlBackend.IlRuntime.SystemExec(ArgOr(args, 0)?.ToString() ?? ""),
                    result => task.PushRef(result));
                return ParkedBuiltinResult;
        },
        [BuiltInFunction.SHELLCODE] = (task, args) =>
        {
                ParkBlocking(task,
                    () => (object?)(double)Basic.IlBackend.IlRuntime.SystemExecCode(ArgOr(args, 0)?.ToString() ?? ""),
                    result => task.PushRef(result));
                return ParkedBuiltinResult;
        },
        [BuiltInFunction.GOMAXPROCS] = (task, args) =>
        {
                // Parità Go runtime.GOMAXPROCS(n): riconfigura i P a runtime; con 0 argomenti
                // (o 0) legge il valore corrente. La VM ridimensiona i worker thread; lo
                // scheduler migra i task dei P rimossi alla coda globale (mai persi).
                if (args.Length == 0)
                    return _vm?.ProcessorCount ?? 1;
                var gmpTarget = Convert.ToInt32(SafeDouble(args[0]));
                if (gmpTarget <= 0)
                    return _vm?.ProcessorCount ?? 1;
                return _vm?.SetGoMaxProcs(gmpTarget) ?? 1;
        },
        [BuiltInFunction.NUMGOROUTINE] = (task, args) =>
        {
                // Parità Go runtime.NumGoroutine(): numero di goroutine attive (task non finiti).
                return (double)(_scheduler?.TaskCount ?? 0);
        },
        [BuiltInFunction.NUMCPU] = (task, args) =>
        {
                // Parità Go runtime.NumCPU(): numero di CPU logiche del processo.
                return (double)Environment.ProcessorCount;
        },
        [BuiltInFunction.SLEEP] = (task, args) =>
        {
            {
                // CONCURRENCY.SleepAsync(ms) — funzione, NON l'opcode OP_SLEEP. La GPM (§0ter)
                // impone il park del task verde, mai il blocco del worker: stessa meccanica di
                // OP_SLEEP (BeginWait Timer + ParkUntil + return ParkedBuiltinResult). Il task
                // RIPRENDE quando il timer scatta (o la cancellazione lo sveglia).
                var sleepMs = (long)SafeDouble(ArgOr(args, 0));
                if (sleepMs <= 0)
                {
                    // ms<=0: nessun park, semantica go-style (yield cooperativo richiesto).
                    task.YieldRequested = true;
                    return ParkedBuiltinResult;
                }
                var sleepWait = task.BeginWait(PCodeWaitKind.Timer, () => task.SleepUntilMs = 0);
                task.ParkUntil(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + sleepMs, sleepWait);
                return ParkedBuiltinResult;
            }
            // --- sync/atomic (parità Go sync/atomic) ---
            // --- ATOMIC: delegato a ExecuteBuiltInAtomic (PCodeProcessor.Builtins.Atomic.cs) ---
        },
        [BuiltInFunction.POOLGET] = (task, args) =>
        {
            {
                // GET(p): primo elemento disponibile o NULL (parità Go sync.Pool.Get).
                if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodePool poolGet) return null;
                return poolGet.Get();
            }
        },
        [BuiltInFunction.POOLPUT] = (task, args) =>
        {
            {
                // PUT(p, v): restituisce un elemento al pool (parità Go sync.Pool.Put).
                if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodePool poolPut) return 0.0;
                poolPut.Put(args[1]);
                return 0.0;
            }
            // --- SYNCMAP: delegato a ExecuteBuiltInSyncmap (PCodeProcessor.Builtins.Syncmap.cs) ---
        },
        [BuiltInFunction.MUTEXTRYLOCK] = (task, args) =>
        {
            {
                // TRYLOCK(mu): acquisisce se libero, altrimenti false SENZA parcheggiare (Go 1.18).
                if (args.Length < 1) return 0.0;
                return args[0] switch
                {
                    Basic.Runtime.Gpm.PCodeMutex tmu => tmu.TryLockNonBlocking() ? -1.0 : 0.0,
                    Basic.Runtime.Gpm.PCodeRwMutex trw => trw.TryLockNonBlocking() ? -1.0 : 0.0,
                    _ => 0.0
                };
            }
            // --- context.Context (parità Go) ---
            // --- CONTEXT: delegato a ExecuteBuiltInContext (PCodeProcessor.Builtins.Context.cs) ---
        },
        [BuiltInFunction.TAXEQUIVALENTYIELD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.TaxEquivalentYield(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        
            // --- Opzioni (Black-Scholes / Merton / greche) ---
        },
        [BuiltInFunction.THETA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Theta(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)), ArgOr(args, 5)?.ToString() ?? "CALL");
        },
        [BuiltInFunction.TI] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Ti();
        },
        [BuiltInFunction.TOBASE64] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.ToBase64(ArgOr(args, 0));
        },
        [BuiltInFunction.TOHEX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ToHex(ArgOr(args, 0));
        },
        [BuiltInFunction.HMACSHA256] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.HmacSha256(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.HMACSHA384] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HmacSha384(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.HMACSHA512] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HmacSha512(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.TOBASE64URL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ToBase64Url(ArgOr(args, 0));
        },
        [BuiltInFunction.FROMBASE64URL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.FromBase64Url(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.HTMLENCODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HtmlEncode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.URLENCODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.UrlEncode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.JSENCODE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JsEncode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.JWTCREATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JwtCreate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.JWTVERIFY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JwtVerify(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.JWTPAYLOAD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JwtPayload(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.HTTP_SAFEFILENAME] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HttpSafeFileName(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.HTTP_DETECTCONTENTTYPE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HttpDetectContentType(ArgOr(args, 0));
        },
        [BuiltInFunction.HTTP_SESSIONCREATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.HttpSessionCreate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.ISREQUIRED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IsValidRequired(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.ISEMAIL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IsValidEmail(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.ISURL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IsValidUrl(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.ISCREDITCARD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IsValidCreditCard(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.ISLENGTH] = (task, args) =>
        {
            {
                double _min = args.Length > 1 && ArgOr(args, 1) is double minD ? minD : 0;
                double _max = args.Length > 2 && ArgOr(args, 2) is double maxD ? maxD : 0;
                return Basic.IlBackend.IlRuntime.IsValidLength(ArgOr(args, 0)?.ToString() ?? "", _min, _max);
            }
        },
        [BuiltInFunction.TOTALSTOCKRETURN] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.TotalStockReturn(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TRIANGULARPDF] = (task, args) =>
        {
                // BASIC: TRIANGULARPDF(x, min, mode, max) — args[0] è x, va spostato per ultimo
                // per la firma StdLib.Runtime (min, mode, max, x), come ogni altro case PDF/CDF in
                // questo blocco. Stesso bug e stesso fix di GMBasic (scoperto verificando la
                // migrazione IlBackend→StdLib.Runtime, Fase 9 del piano): passava args[0..3] senza
                // riordino, mappando silenziosamente x→min, min→mode, mode→max, max→x —
                // parametrizzazione quasi sempre invalida, eccezione interna a Triangular.PDF che
                // terminava l'intero task VM senza alcun output, anche per statement precedenti.
                return Basic.StdLibRuntime.MathFunctions.TriangularPdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 0.5), SafeDouble(args.Length > 3 ? args[3] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.UNIFORMPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.UniformPdf(SafeDouble(args.Length > 1 ? args[1] : 0.0), SafeDouble(args.Length > 2 ? args[2] : 1.0), SafeDouble(args[0]));
        },
        [BuiltInFunction.VANNA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Vanna(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.VARINTDECODE] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.VarIntDecode(ToBytes(ArgOr(args, 0)));
        },
        [BuiltInFunction.VARINTENCODE] = (task, args) =>
        {
                return CommonLib.BlockchainUtils.VarIntEncode((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.VEGA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Vega(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.VERIFY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): registry handle unico (§0bis).
                return Basic.IlBackend.IlRuntime.Verify(ArgOr(args, 0), ArgOr(args, 1), (long)SafeDouble(ArgOr(args, 2))) ? -1.0 : 0.0;
        },
        [BuiltInFunction.VERIFYRAW] = (task, args) =>
        {
                // Runtime comune (IlRuntime): verifica Ed25519 con chiave pubblica raw (base64).
                return Basic.IlBackend.IlRuntime.VerifyRaw(ArgOr(args, 0), ArgOr(args, 1), args.Length > 2 ? (ArgOr(args, 2)?.ToString() ?? "") : "") ? -1.0 : 0.0;
        },
        [BuiltInFunction.VOLATILITY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Volatility(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.VOLGA] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.Volga(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)), SafeDouble(ArgOr(args, 4)));
        },
        [BuiltInFunction.WEIBULLCDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.WeibullCdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.WEIBULLPDF] = (task, args) =>
        {
                return Basic.StdLibRuntime.MathFunctions.WeibullPdf(SafeDouble(args.Length > 2 ? args[1] : null), SafeDouble(args.Length > 2 ? args[2] : null), SafeDouble(args.Length > 0 ? args[0] : null));
        },
        [BuiltInFunction.XIRR] = (task, args) =>
        {
            {
                var values = ToDoubleArr(ArgOr(args, 0));
                var dates = ToDateTimeArr(ArgOr(args, 1));
                var guess = SafeDouble(ArgOr(args, 2));
                return Basic.StdLibRuntime.FinanceFunctions.Xirr(values, dates, guess);
            }
        },
        [BuiltInFunction.YIELDTOMATURITY] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.YieldToMaturity(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
        },
        [BuiltInFunction.ZEROCOUPONBONDVALUE] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.ZeroCouponBondValue(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.ZEROCOUPONBONDYIELD] = (task, args) =>
        {
                return Basic.StdLibRuntime.TradingFunctions.ZeroCouponBondYield(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        
            // --- COMPRESSION: delegato a ExecuteBuiltInCompression (PCodeProcessor.Builtins.Compression.cs) ---
        },
        [BuiltInFunction.CONVDOC] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string cp)
                  return IlRuntime.ConvertToMd(cp);
              return ""; }
        },
        [BuiltInFunction.CONVHTML] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string hPath)
                  return IlRuntime.ConvertToHtml(hPath);
              return ""; }
        },
        [BuiltInFunction.CONVTEXT] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string tPath)
                  return IlRuntime.ConvertToText(tPath);
              return ""; }
        },
        [BuiltInFunction.CONVFMTS] = (task, args) =>
        {
                return ConversionLazy.ConvertFormats();
            // === DOCUMENT/PDF namespace (Fase 4): handle-based via NativeDocPdf ===
        },
        [BuiltInFunction.CONVERTEXTRACTTO] = (task, args) =>
        {
            { if (args.Length >= 2 && args[0] is string xsrc && args[1] is string xdst)
                { IlRuntime.ConvertExtractTo(xsrc, xdst); return 1.0; }
              return 0.0; }
        },
        [BuiltInFunction.CONVERTRENDERTO] = (task, args) =>
        {
            { if (args.Length >= 2 && args[0] is string rsrc && args[1] is string rdst)
                return IlRuntime.ConvertRenderTo(rsrc, rdst);
              return -1.0; }
        },
        [BuiltInFunction.MDPARSE] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string mt) return IlRuntime.MdParse(mt);
              return new Dictionary<string, object?>(); }
        },
        [BuiltInFunction.MDHEADINGS] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdHeadings(args[0]!, args.Length >= 2 && args[1] is double ml ? ml : 0);
              return new List<object?>(); }
        },
        [BuiltInFunction.MDLINKS] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdLinks(args[0]!); return new List<object?>(); }
        },
        [BuiltInFunction.MDIMAGES] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdImages(args[0]!); return new List<object?>(); }
        },
        [BuiltInFunction.MDTABLES] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdTables(args[0]!); return new List<object?>(); }
        },
        [BuiltInFunction.MDCODEBLOCKS] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdCodeBlocks(args[0]!); return new List<object?>(); }
        },
        [BuiltInFunction.MDTREE] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdTree(args[0]!); return ""; }
        },
        [BuiltInFunction.MDTOHTML] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdToHtml(args[0]!); return ""; }
        },
        [BuiltInFunction.MDTOTEXT] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdToText(args[0]!); return ""; }
        },
        [BuiltInFunction.MDVALIDATE] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdValidate(args[0]?.ToString() ?? ""); return ""; }
        },
        [BuiltInFunction.MDSTRIP] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdStrip(args[0]?.ToString() ?? ""); return ""; }
        },
        [BuiltInFunction.MDFORMAT] = (task, args) =>
        {
            { if (args.Length >= 1) return IlRuntime.MdFormat(args[0]!); return ""; }
        },
        [BuiltInFunction.MDQUERY] = (task, args) =>
        {
            { if (args.Length >= 2 && args[1] is string sel) return IlRuntime.MdQuery(args[0]!, sel);
              return new List<object?>(); }
            // --- DOC: delegato a ExecuteBuiltInDoc (PCodeProcessor.Builtins.Doc.cs) ---
        },
        [BuiltInFunction.CVTBATCH] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string bd) {
                var bf = args.Length >= 2 ? args[1]?.ToString() ?? "md" : "md";
                var br = args.Length >= 3 && args[2] is double d2 && d2 != 0;
                return ConversionLazy.ConvertBatch(bd, bf, br); }
              return ""; }
        },
        [BuiltInFunction.WEBTOMD] = (task, args) =>
        {
            { if (args.Length >= 1 && args[0] is string wu)
                return ConversionLazy.ConvertUrl(wu);
              return ""; }
            // --- AI: delegato a ExecuteBuiltInAi (PCodeProcessor.Builtins.Ai.cs) ---
        },
        [BuiltInFunction.USED] = (task, args) =>
        {
            {
                var usedDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                return usedDriver is { IsOpen: true } ? -1L : 0L;
            }
            // DBF.ALIAS([area]) → alias della work area.
        },
        [BuiltInFunction.ALIAS] = (task, args) =>
        {
            {
                var aliasDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                return aliasDriver?.Alias ?? "";
            }
            // DBF.FIELDNAME(fieldNum[, area]) → nome dell'n-esimo campo (1-based).
        },
        [BuiltInFunction.FIELDNAME] = (task, args) =>
        {
            // Runtime comune (IlRuntime): 1-based, campi dall'engine per i DBF — una sola
            // implementazione per IL e PCode (§0bis).
            return Basic.IlBackend.IlRuntime.DbfFieldName(
                args.Length > 0 ? args[0] : null,
                args.Length >= 2 ? args[1] : null);
            // DBF.RECSIZE([area]) → dimensione record (byte).
        },
        [BuiltInFunction.RECSIZE] = (task, args) =>
        {
            {
                var rsDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                if (rsDriver == null) return 0L;
                /* Lunghezza record in BYTE: l'engine ISAM la conosce (RecordLength). Sommare
                 * Fields.Length dava il NUMERO DI CAMPI (IsamDriver popola RddFieldInfo con
                 * Length=0) -> RECSIZE 6 invece di 87 (gamba PCode di DbIsamFullTests.bas). */
                if (rsDriver is Basic.Databases.Rdd.IsamDriver idrv && idrv.Engine != null)
                    return (long)idrv.Engine.RecordLength;
                return (long)rsDriver.Fields.Sum(f => Math.Max(1, f.Length));
            }
            // DBF.DELETED([area]) → -1 se il record corrente è marcato cancellato.
        },
        [BuiltInFunction.DELETED] = (task, args) =>
        {
            {
                var delDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                if (delDriver is Basic.Databases.Rdd.DbfDriver delDbf && delDbf._engine != null)
                    try { return delDbf._engine.IsCurrentRecordDeleted() ? -1L : 0L; } catch { }
                if (delDriver is Basic.Databases.Rdd.IsamDriver delIsam && delIsam._engine != null)
                    try { return delIsam._engine.IsDeleted() ? -1L : 0L; } catch { }
                return 0L;
            }
            // DBF.INDEXKEY([area]) → chiave dell'ordine corrente.
        },
        [BuiltInFunction.INDEXKEY] = (task, args) =>
        {
            {
                var ikDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                if (ikDriver == null) return "";
                try { return ikDriver.CurrentKey() ?? ""; } catch { return ""; }
            }
            // DBF.INDEXORD([area]) → numero dell'ordine corrente.
        },
        [BuiltInFunction.INDEXORD] = (task, args) =>
        {
            {
                var ioDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                if (ioDriver == null) return 0L;
                try { return (long)ioDriver.CurrentKeyNo(); } catch { return 0L; }
            }
            // DBF.DBFILTER([area]) → espressione del filtro attivo (stringa, vuota se nessuno).
        },
        [BuiltInFunction.DBFILTER] = (task, args) =>
        {
            /* L'entry era VUOTA (solo il commento sopra): la lettura cadeva nel fallback e
             * restituiva sempre "" → il filtro impostato con DBF SET FILTER TO non era mai
             * visibile (§ MiniTests/dbf/dbf_lock_filter.bas). Si legge il TESTO dal driver
             * (l'engine conserva solo l'oggetto espressione), come il backend IL. */
            var flDrv = _dataSession.GetWorkArea(_dataSession.CurrentArea)?.Driver
                        as Basic.Databases.Rdd.DbfDriver;
            if (flDrv != null) { try { return flDrv.FilterExpression ?? ""; } catch { } }
            var flAny = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
            if (flAny is Basic.Databases.Rdd.BaseDriver flBase2)
            { try { return flBase2.FilterExpression ?? ""; } catch { } }
            return "";
        },
        [BuiltInFunction.DBRELATION] = (task, args) =>
        {
            // Runtime comune (IlRuntime): ESPRESSIONE della relazione (stringa), non un flag.
            // Prima ritornava -1L/0L mentre la StdLib dichiara StringType → divergenza da IL e
            // dal contratto (§ MiniTests/dbf/dbf_state.bas).
            return Basic.IlBackend.IlRuntime.DbfRelation(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.FLOCK] = (task, args) =>
        {
            {
                var fkDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                if (fkDriver == null) return 0L;
                try { return fkDriver.FileLock() ? 1L : 0L; } catch { return 0L; }
            }
            // DBF.RLOCK([area]) → -1 se il record CORRENTE è stato bloccato
            // (TryLockCurrentRecord, come il deprecated — 1 solo argomento).
        },
        [BuiltInFunction.RLOCK] = (task, args) =>
        {
            {
                try
                {
                    var rkDriver = ResolveIsamDriver(task, args.Length > 0 ? args[0] : null);
                    if (rkDriver == null) return 0L;
                    if (rkDriver is Basic.Databases.Rdd.IsamDriver rkIsam && rkIsam._engine != null)
                        return rkIsam._engine.TryLockCurrentRecord() ? 1L : 0L;
                    if (rkDriver is Basic.Databases.Rdd.DbfDriver rkDbf && rkDbf._engine != null)
                        return rkDbf._engine.TryLockCurrentRecord() ? 1L : 0L;
                    return rkDriver.Lock(rkDriver.RecNo) ? 1L : 0L;
                }
                catch { return 0L; }
            }
        
            // ────────────────────────────────────────────────────────────────
            // JOY — joystick (JoyManager).
            // ────────────────────────────────────────────────────────────────
            // JOY(port) → stato bitmask della porta.
        },
        [BuiltInFunction.JOY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return (double)Basic.IlBackend.IlRuntime.Joy(args.Length > 0 ? (int)SafeDouble(args[0]) : 0);
            // JOYCOUNT() → numero di joystick rilevati.
        },
        [BuiltInFunction.JOYCOUNT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JoyCount();
            // JOYNAME(port) → nome del joystick.
        },
        [BuiltInFunction.JOYNAME] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JoyName(args.Length > 0 ? (int)SafeDouble(args[0]) : 0);
            // JOYPRESENT(port) → -1 se il joystick è presente.
        },
        [BuiltInFunction.JOYPRESENT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.JoyPresent(args.Length > 0 ? (int)SafeDouble(args[0]) : 0);
        
            // ==================================================================
            // GAP CHIUSI 2026-08-29 — 67 BuiltInFunction ora delegate a IlRuntime
            // (runtime condiviso con ILBackend, regola §0bis: una sola implementazione).
            // ==================================================================
        
            // ── AI CONFIG LOAD/SAVE ──────────────────────────────────────────
            // --- AI: delegato a ExecuteBuiltInAi (PCodeProcessor.Builtins.Ai.cs) ---
        },
        [BuiltInFunction.C2PX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.C2PX(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.C2PY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.C2PY(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.P2CX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.P2CX(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.P2CY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.P2CY(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        
            // ── RDD (driver-agnostico → IlRuntime, come il ILBackend) ────────
            // --- RDD: delegato a ExecuteBuiltInRdd (PCodeProcessor.Builtins.Rdd.cs) ---
        },

        [BuiltInFunction.CHACHA20ENCRYPT] = (task, args) =>
        {
            try
            {
                return Basic.IlBackend.IlRuntime.ChaCha20Encrypt(ArgOr(args, 0), ArgOr(args, 1), ArgOr(args, 2));
            }
            catch { return Array.Empty<byte>(); }
        },
        [BuiltInFunction.CHACHA20DECRYPT] = (task, args) =>
        {
            try
            {
                return Basic.IlBackend.IlRuntime.ChaCha20Decrypt(ArgOr(args, 0), ArgOr(args, 1), ArgOr(args, 2));
            }
            catch { return Array.Empty<byte>(); }
        },
        [BuiltInFunction.METHODINFO] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.MethodInfo(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() : null);
        },
        [BuiltInFunction.PROPERTYINFO] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.PropertyInfo(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() : null);
        },
        [BuiltInFunction.TEXTHEIGHT] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.TextHeight();
        },
        [BuiltInFunction.TEXTWIDTH] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.TextWidth(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PORTFOLIOSHARPERATIO] = (task, args) =>
        {
            // SHARPERATIO(rendimento, rischio, riskFree) — forma a 3 SCALARI (StdLib);
            // forma a 2 array (pesi, rendimenti) per la variante matriciale. Parità IL.
            if (args.Length == 3 && args[0] is not System.Collections.IList && args[1] is not System.Collections.IList)
                return Basic.IlBackend.IlRuntime.SharpeRatio3(SafeDouble(args[0]), SafeDouble(args[1]), SafeDouble(args[2]));
            if (args.Length >= 2)
                return Basic.IlBackend.IlRuntime.PortfolioSharpeRatio(args[0]!, args[1]!);
            return 0.0;
        },
        [BuiltInFunction.PORTFOLIOSTDEV] = (task, args) =>
        {
            if (args.Length >= 2)
                return Basic.IlBackend.IlRuntime.PortfolioStDev(args[0]!, args[1]!);
            return 0.0;
        },
        [BuiltInFunction.KECCAK256STRING] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.StrKeccak256(ArgOr(args, 0));
        },
        [BuiltInFunction.KECCAK256] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.Keccak256(ArgOr(args, 0));
        },
        [BuiltInFunction.RIPEMD160STRING] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.StrRipeMd160(ArgOr(args, 0));
        },
        [BuiltInFunction.RIPEMD160] = (task, args) =>
        {
            return Basic.IlBackend.IlRuntime.RipeMd160(ArgOr(args, 0));
        },
        [BuiltInFunction.MINCVAR] = (task, args) =>
        {
            if (args.Length >= 1 && args[0] is System.Collections.IList mmr)
            {
                var r = ToDoubleArray(mmr);
                if (r.Length == 0) return null;
                var conf = args.Length > 1 ? SafeDouble(args[1]) : 0.95;
                var result = new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["ExpectedReturn"] = MinCvar(r, conf),
                    ["Volatility"] = 0.0,
                    ["SharpeRatio"] = 0.0,
                    ["Weights"] = new double[] { 1.0 }
                };
                return result;
            }
            return null;
        },
        [BuiltInFunction.MINCDAR] = (task, args) =>
        {
            if (args.Length >= 1 && args[0] is System.Collections.IList mmr)
            {
                var r = ToDoubleArray(mmr);
                if (r.Length == 0) return null;
                var conf = args.Length > 1 ? SafeDouble(args[1]) : 0.95;
                var result = new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["ExpectedReturn"] = MinCdar(r, conf),
                    ["Volatility"] = 0.0,
                    ["SharpeRatio"] = 0.0,
                    ["Weights"] = new double[] { 1.0 }
                };
                return result;
            }
            return null;
        },

        [BuiltInFunction.RECNO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfRecno(args.Length > 0 ? args[0]?.ToString() : null);
        },
        // RDD.RECNO(): stesso runtime di RECNO sul PCode (il driver corrente); il codice distinto serve alla VM/C.
        [BuiltInFunction.RDDRECNO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfRecno(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.LUPDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfLupdate(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DBFSETDURABILITY] = (task, args) =>
        {
                // Come il path syscall (SysCall.DBF_SET_DURABILITY): engine di PCODE + -1 = ok.
                var sdMode = args.Length > 0 ? args[0]?.ToString() ?? "" : "";
                var sdEngine = ResolveDbfEngine(task, null);
                if (sdEngine == null) return 0.0;
                sdEngine.DurabilityMode = sdMode.Equals("Full", StringComparison.OrdinalIgnoreCase)
                    ? Basic.DBF.DbfDurabilityMode.Full : Basic.DBF.DbfDurabilityMode.Buffered;
                return -1.0;
        },
        [BuiltInFunction.DBFAPPENDMANY] = (task, args) =>
        {
                // La StdLib ha TRE forme: APPENDMANY(records), (fileNumber, records),
                // (alias, records). I RECORD sono sempre l'ULTIMO argomento, l'area il primo (se
                // c'è). Prima la forma a 1 argomento passava i record a null e l'array come
                // alias → conteggio sbagliato (§ MiniTests/dbf/zz_b: PCode 1, IL 2).
                return Basic.IlBackend.IlRuntime.DbfAppendMany(
                    args.Length > 0 ? args[args.Length - 1] : null,
                    args.Length > 1 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DBFSETREPLICATION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfSetReplication(
                    args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DBFAPPLYREPLICATION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfApplyReplication(
                    args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DBFFOLLOWREPLICATION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.DbfFollowReplication(
                    args.Length > 0 ? args[0]?.ToString() ?? "" : "", null);
        },
        [BuiltInFunction.DBFBEGINGROUPCOMMIT] = (task, args) =>
        {
                // Risoluzione dell'area di PCODE (DataSession), non i registri di IlRuntime:
                // delegando a IlRuntime la gamba PCode ritornava 0 (registri propri di IL).
                // -1 = successo, come il path syscall e come IL (§ dbf_lock_filter.bas).
                var bgEngine = ResolveDbfEngine(task, null);
                if (bgEngine != null) { bgEngine.BeginGroupCommit(); return -1.0; }
                return 0.0;
        },
        [BuiltInFunction.DBFENDGROUPCOMMIT] = (task, args) =>
        {
                var egEngine = ResolveDbfEngine(task, null);
                if (egEngine != null) { egEngine.EndGroupCommit(); return -1.0; }
                return 0.0;
        },
        [BuiltInFunction.ISAMINTEGRITY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IsamIntegrity(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.SPEEDSETMANY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SpeedSetMany(args.Length > 1 ? args[1] : null,
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.MEMORYMAP] = (task, args) =>
        {
                return (double)Basic.IlBackend.IlRuntime.MemoryMap(args.Length > 0 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.MEMORYMAPPEEK] = (task, args) =>
        {
                return (double)Basic.IlBackend.IlRuntime.MemoryPeek(
                    (long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.MEMORYMAPREADBYTES] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryReadBytes(
                    (long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)), (long)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.LEDGER_OPEN] = (task, args) =>
        {
                return (double)Basic.IlBackend.IlRuntime.LedgerOpen(args.Length > 0 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.LEDGER_EDIT] = (task, args) =>
        {
                throw new NotSupportedException("LEDGER.EDIT come funzione non è supportata — usa la forma statement (LEDGER EDIT ... INTO).");
        },
        [BuiltInFunction.LEDGER_REGISTER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LedgerRegister(
                    args.Length > 1 ? args[1]?.ToString() : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.LEDGER_BALANCE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LedgerBalance(
                    args.Length > 1 ? args[1]?.ToString() : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.LEDGER_ACCOUNTS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LedgerAccounts(
                    args.Length > 1 ? args[1]?.ToString() : null,
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.LEDGER_PLOT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LedgerPlot(
                    args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                    args.Length > 2 ? args[2]?.ToString() ?? "" : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.LEDGER_IMPORT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LedgerImport(
                    args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                    args.Length > 2 ? args[2]?.ToString() ?? "" : "",
                    args.Length > 0 ? args[0]?.ToString() : null);
        },

    };

    private static readonly HashSet<BuiltInFunction> CoreLinalgBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.CHOLESKY,
        BuiltInFunction.DETERMINANT,
        BuiltInFunction.DOTPRODUCT,
        BuiltInFunction.EVD,
        BuiltInFunction.LINALGCHOLESKYFUNC,
        BuiltInFunction.LINALGDECOMPGETFACTORFUNC,
        BuiltInFunction.LINALGEVDFUNC,
        BuiltInFunction.LINALGINVERSEFUNC,
        BuiltInFunction.LINALGLUFUNC,
        BuiltInFunction.LINALGMATRIXCREATEFUNC,
        BuiltInFunction.LINALGMULTIPLYFUNC,
        BuiltInFunction.LINALGQRFUNC,
        BuiltInFunction.LINALGSVDFUNC,
        BuiltInFunction.LINALGTRANSPOSEFUNC,
        BuiltInFunction.LINALGVECTORCREATEFUNC,
        BuiltInFunction.LU,
        BuiltInFunction.MATRIXABOVEMASK,
        BuiltInFunction.MATRIXABOVEMATRIX,
        BuiltInFunction.MATRIXADDCOLVECTOR,
        BuiltInFunction.MATRIXADDROWVECTOR,
        BuiltInFunction.MATRIXARGMAX,
        BuiltInFunction.MATRIXARGMIN,
        BuiltInFunction.MATRIXASCSV,
        BuiltInFunction.MATRIXASJSON,
        BuiltInFunction.MATRIXCLONE,
        BuiltInFunction.MATRIXCOLMEANS,
        BuiltInFunction.MATRIXCOLSUMS,
        BuiltInFunction.MATRIXCOLUMNCOUNT,
        BuiltInFunction.MATRIXCOLVARIANCES,
        BuiltInFunction.MATRIXCONCATCOLS,
        BuiltInFunction.MATRIXCONCATROWS,
        BuiltInFunction.MATRIXFROBENIUSNORM,
        BuiltInFunction.MATRIXINFINITYNORM,
        BuiltInFunction.MATRIXISLOWERTRIANGULAR,
        BuiltInFunction.MATRIXISORTHOGONAL,
        BuiltInFunction.MATRIXISSYMMETRIC,
        BuiltInFunction.MATRIXISUPPERTRIANGULAR,
        BuiltInFunction.MATRIXLOADCSV,
        BuiltInFunction.MATRIXLOADJSON,
        BuiltInFunction.MATRIXMAXVALUE,
        BuiltInFunction.MATRIXMINVALUE,
        BuiltInFunction.MATRIXMULTIPLYLEFTVECTOR,
        BuiltInFunction.MATRIXMULTIPLYVECTOR,
        BuiltInFunction.MATRIXNORM,
        BuiltInFunction.MATRIXONENORM,
        BuiltInFunction.MATRIXPSEUDOINVERSE,
        BuiltInFunction.MATRIXRANK,
        BuiltInFunction.MATRIXRESHAPE,
        BuiltInFunction.MATRIXROWCOUNT,
        BuiltInFunction.MATRIXROWMEANS,
        BuiltInFunction.MATRIXROWSUMS,
        BuiltInFunction.MATRIXROWVARIANCES,
        BuiltInFunction.MATRIXSLICE,
        BuiltInFunction.MATRIXTRACE,
        BuiltInFunction.MATRIX_BLOCKDIAG,
        BuiltInFunction.MATRIX_CIRCULANT,
        BuiltInFunction.MATRIX_COND,
        BuiltInFunction.MATRIX_COSM,
        BuiltInFunction.MATRIX_EIG,
        BuiltInFunction.MATRIX_EXPM,
        BuiltInFunction.MATRIX_FRACTIONALPOWER,
        BuiltInFunction.MATRIX_GETVALUE,
        BuiltInFunction.MATRIX_HANKEL,
        BuiltInFunction.MATRIX_HESSENBERG,
        BuiltInFunction.MATRIX_KRONECKER,
        BuiltInFunction.MATRIX_LOGM,
        BuiltInFunction.MATRIX_NUCLEARNORM,
        BuiltInFunction.MATRIX_NULLSPACE,
        BuiltInFunction.MATRIX_ORTH,
        BuiltInFunction.MATRIX_POLAR,
        BuiltInFunction.MATRIX_SCHUR,
        BuiltInFunction.MATRIX_SETVALUE,
        BuiltInFunction.MATRIX_SIGNM,
        BuiltInFunction.MATRIX_SINM,
        BuiltInFunction.MATRIX_SOLVETRIANGULAR,
        BuiltInFunction.MATRIX_SQRTM,
        BuiltInFunction.MATRIX_SUBSPACEANGLES,
        BuiltInFunction.MATRIX_TANM,
        BuiltInFunction.MATRIX_TOEPLITZ,
        BuiltInFunction.NORM,
        BuiltInFunction.QR,
        BuiltInFunction.SVD,
        BuiltInFunction.VECTORCHEBYSHEV,
        BuiltInFunction.VECTORCOSINEDISTANCE,
        BuiltInFunction.VECTORMANHATTAN,
        BuiltInFunction.VECTORMINKOWSKI,
    };

    private static readonly Lazy<(HashSet<BuiltInFunction> Set, Func<PCodeProcessor, PCodeTask, BuiltInFunction, object?[], object?> Exec)[]> _domainExecutors =
        new(() => new (HashSet<BuiltInFunction> Set, Func<PCodeProcessor, PCodeTask, BuiltInFunction, object?[], object?> Exec)[]
        {
        (AIBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAi(t, fn, a)),
        (AIAGENTBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAiagent(t, fn, a)),
        (AIPATHBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAipath(t, fn, a)),
        (AIPLUGINBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAiplugin(t, fn, a)),
        (AISKILLSBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAiskills(t, fn, a)),
        (AIVECTORBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAivector(t, fn, a)),
        (ATOMICBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInAtomic(t, fn, a)),
        (COMPRESSIONBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInCompression(t, fn, a)),
        (GREPBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInGrep(t, fn, a)),
        (MESSAGEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInMessage(t, fn, a)),
        (CONTEXTBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInContext(t, fn, a)),
        (DATEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInDate(t, fn, a)),
        (DBFBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInDbf(t, fn, a)),
        (DICTBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInDict(t, fn, a)),
        (DOCBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInDoc(t, fn, a)),
        (FFTBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInFft(t, fn, a)),
        (HTTPBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInHttp(t, fn, a)),
        (INTERPOLATEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInInterpolate(t, fn, a)),
        (ISAMBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInIsam(t, fn, a)),
        (KDTREEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInKdtree(t, fn, a)),
        (BITSBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInBits(t, fn, a)),
        (TRAYBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInTray(t, fn, a)),
        (MCPBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInMcp(t, fn, a)),
        (ConnectorBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInConnector(t, fn, a)),
        (MEMORYBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInMemory(t, fn, a)),
        (MOUSEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInMouse(t, fn, a)),
        (PROVIDERBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInProvider(t, fn, a)),
        (RDDBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInRdd(t, fn, a)),
        (SIGNALBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSignal(t, fn, a)),
        (SOLVEBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSolve(t, fn, a)),
        (SPATIALBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSpatial(t, fn, a)),
        (SPECIALBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSpecial(t, fn, a)),
        (SPEEDBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSpeed(t, fn, a)),
        (SQLBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSql(t, fn, a)),
        (STATSBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInStats(t, fn, a)),
        (SYNCMAPBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInSyncmap(t, fn, a)),
        (TEAMBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInTeam(t, fn, a)),
        (ORCHESTRATIONBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInOrchestration(t, fn, a)),
        (TimeseriesBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInTimeseries(t, fn, a)),
        (TorchBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInTorch(t, fn, a)),
        (VAULTBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInVault(t, fn, a)),
        (WINDOWBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInWindow(t, fn, a)),
        (WORKFLOWBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInWorkflow(t, fn, a)),
        (TOOLBuiltInFunctions, (self, t, fn, a) => self.ExecuteBuiltInTool(t, fn, a)),
        (CoreLinalgBuiltInFunctions, (self, t, fn, a) => self.ExecuteLinAlgBuiltIn(t, fn, a)),
        });

    /// <summary>Chiama <see cref="ExecuteBuiltIn"/> instradando un'eccezione del builtin nello
    /// schema a 3 livelli di <see cref="RaiseBasicError"/> (TRY/CATCH → ON ERROR GOTO/GOSUB →
    /// eccezione fatale), come THROW_ERROR. Prima l'eccezione sfuggiva dal dispatch e ON ERROR
    /// non veniva mai consultato (bug parità errori builtin: è il fix minimo del titolo).
    /// Se l'errore è gestito ritorna <see cref="ParkedBuiltinResult"/>: il chiamante non deve
    /// pushare il risultato (l'IP/unwind è già stato rediretto all'handler).</summary>
    private object? ExecuteBuiltInGuarded(PCodeTask task, int funcCode, object?[] args)
    {
        try
        {
            return ExecuteBuiltIn(task, funcCode, args);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            RaiseBasicError(task, 200.0, ex.Message);
            return ParkedBuiltinResult;
        }
    }

    private object? ExecuteBuiltIn(PCodeTask task, int funcCode, object?[] args)
    {
        var fn = (BuiltInFunction)funcCode;
        if (CoreDispatch.TryGetValue(fn, out var handler))
            return handler(task, args);
        var execs = _domainExecutors.Value;
        for (var i = 0; i < execs.Length; i++)
            if (execs[i].Set.Contains(fn))
                return execs[i].Exec(this, task, fn, args);
        var nm = Enum.IsDefined(typeof(BuiltInFunction), fn)
            ? Enum.GetName(typeof(BuiltInFunction), fn)
            : "non-BuiltInFunction";
        throw new InvalidOperationException(
            $"OP_CALL_BUILTIN: funcCode {funcCode} non gestito dalla VM ({nm}). "
            + "Aggiungere un case reale che delega a IlRuntime, mai un fallback numerico.");
    }

    /// <summary>
    /// Determina il nome del tipo runtime di un valore per TYPENAME. A differenza dell'AST
    /// interpreter (che prova prima il tipo statico dichiarato via <c>Gpm.TypeInfo.Functions.cs</c>),
    /// qui il P-CODE VM ha solo il valore boxato a runtime — coerente comunque per il caso d'uso
    /// principale (elementi di una `LIST OF ANY`, dove anche l'AST cade sull'inferenza a runtime).


}
