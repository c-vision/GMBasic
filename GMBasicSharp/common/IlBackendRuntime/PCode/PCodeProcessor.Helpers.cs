// FILE: Basic.IlBackend.PCode.PCodeProcessor.Helpers.cs
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
    /// <summary>Legge la stringa di errore dell'ultimo fallimento ML (mai il puntatore).</summary>
    private static string MarshalLastError()
    {
        var p = MlNativeEngine.gmb_ml_last_error();
        return p != System.IntPtr.Zero ? (System.Runtime.InteropServices.Marshal.PtrToStringUTF8(p) ?? "") : "";
    }

    private double PopNum(PCodeTask task) => NumOf(task.Sp > 0 ? task.PopRef() : null);

    /// <summary>Coercizione NUMERICA di un valore (senza toccare lo stack): unica
    /// implementazione per PopNum e per OP_ADD. OP_ADD aveva una copia inline che usava
    /// TryParse(v.ToString()) e quindi perdeva il BIT: `TRUE + 1` dava 1 (TryParse("True") →
    /// fallito → 0+1) mentre `TRUE * 3` dava -3 (via PopNum). Canone BASIC: TRUE = -1.</summary>
    private static double NumOf(object? v)
    {
        if (v is DateValue dv) return dv.OADate;
        if (v is DateOnly doD) return doD.ToDateTime(TimeOnly.MinValue).ToOADate();
        if (v is DateTime dt) return dt.ToOADate();
        if (v is Basic.Databases.Rdd.GbDate gd) return gd.Value.ToOADate();
        if (v is double d) return d;
        if (v is int i) return i;
        if (v is long l) return l;
        if (v is float f) return f;
        if (v is decimal dec) return (double)dec;
        // BIT: true = -1 (NOT -1 = 0 in BASIC bitwise; 'IF fn()' con true doveva
        // valutare -1, ma TryParse("True") -> 0 -> il ramo veniva saltato sempre
        // (es. comprehension con filtro IF IsPrimo(n) produceva liste vuote).
        if (v is bool b) return b ? -1.0 : 0.0;
        return double.TryParse(v?.ToString(), System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var r) ? r : 0.0;
    }

    /// <summary>OP_CMP_EQ/NE/GT/GTE/LT/LTE, ramo generico (non stringa/dictionary/null) usano
    /// `NumOf` — la STESSA coercizione di PopNum/aritmetica, non una seconda copia. Una copia
    /// separata (CoerceNumForCompare) era già divergita una volta: gestiva solo
    /// double/bool/stringa-numerica e qualunque altro tipo boxed (long, int, float, decimal,
    /// date — es. i valori grezzi letti da un record ISAM via GET_FILE, sempre long per i campi
    /// LONG) cadeva nel branch finale ": 0", quindi DUE long DIVERSI (1L e 2L) confrontavano
    /// 0 == 0, cioè SEMPRE uguali (ordineRec.ID_Cliente vs clienteRec.ID da ISAM GET: "2 = 1"
    /// risultava vero). Con una sola coercizione quel tipo di drift non è rappresentabile.</summary>
    /// <summary>Usata da OP_CMP_EQ/OP_CMP_NE quando almeno un operando è null: null==null resta
    /// vero (identità), ma null confronta uguale anche a "" (stringa vuota) — MAI a una stringa
    /// non vuota. Aggiunta per un campo SESSION/JSON/dict mai impostato (letto come null da
    /// OP_LOAD_ELEMENT su chiave assente) che il codice BSP confronta sempre con "" per testare
    /// "non impostato" (bug trovato dal vivo: SESSION("hits") non si inizializzava mai). Non
    /// riapre il bug di sicurezza originale (null che combacia con un valore stringa REALE, es.
    /// "password_segreta") perché il confronto resta stretto a "" specificamente.</summary>
    private static bool NullEqualsForCompare(object? a, object? b)
        => (a is null && b is null)
        || (a is null && b is string bs && bs.Length == 0)
        || (b is null && a is string asr && asr.Length == 0);

    /// <summary>Handle GPM (context/channel/waitgroup/...) confrontato con 0/NIL: il
    /// riferimento è NON null → "non zero" (parità IL/VM-C). Vedi context/done.bas `IF ch <> 0`.</summary>
    private static bool IsGpmHandleForCompare(object? v) => v is Basic.Runtime.Gpm.PCodeContext
        or Basic.Runtime.Gpm.ChannelInfo or Basic.Runtime.Gpm.PCodeWaitGroup or Basic.Runtime.Gpm.PCodeMutex
        or Basic.Runtime.Gpm.PCodeRwMutex or Basic.Runtime.Gpm.PCodeOnce or Basic.Runtime.Gpm.PCodeAtomic
        or Basic.Runtime.Gpm.PCodePool or Basic.Runtime.Gpm.PCodeCond or Basic.Runtime.Gpm.PCodeConcurrentMap
        or Basic.Runtime.Gpm.PCodeFutureValue;

    /// <summary>Zero numerico nel ramo "handle GPM confrontato con 0/NIL": stessa lista di
    /// tipi di NumOf, bool compreso — FALSE vale 0 anche qui (canone BASIC), altrimenti
    /// `IF ch = FALSE` cadeva nel ramo della coercizione e un handle (non numerico → parse
    /// fallito → 0) risultava uguale a FALSE.</summary>
    private static bool IsNumericZeroForCompare(object? v)
        => v is double d && d == 0.0 || v is int i && i == 0
        || v is long l && l == 0 || v is float f && f == 0f
        || v is decimal dec && dec == 0m || v is bool b && !b;

    /// <summary>Comparer per le collezioni K/V tipizzate: normalizza i numeri (int/double/
    /// long) a double prima del confronto — il Comparer<object>.Default esplode con chiavi
    /// boxed di tipi misti ("Object must be of type Double"). Le chiavi non numeriche
    /// sono confrontate per stringa.</summary>
    internal static System.Collections.Generic.IComparer<object> CompareMixedKeys() =>
        System.Collections.Generic.Comparer<object>.Create((a, b) =>
        {
            if (a is System.IConvertible ac && b is System.IConvertible bc && IsNumeric(a) && IsNumeric(b))
                return System.Convert.ToDouble(a, System.Globalization.CultureInfo.InvariantCulture)
                    .CompareTo(System.Convert.ToDouble(b, System.Globalization.CultureInfo.InvariantCulture));
            if (a is IComparable ia && b is not null && a.GetType() == b.GetType())
                return ia.CompareTo(b);
            return string.Compare(a?.ToString(), b?.ToString(), StringComparison.Ordinal);
        });

    private static bool IsNumeric(object o) =>
        o is sbyte or byte or short or ushort or int or uint or long or ulong or float or double or decimal;

    private static double SafeDouble(object? v) => v switch
    {
        double d => d,
        // BOOL/BIT: TRUE=-1, FALSE=0 (canone GMBasic, parità IL/VM-C/C99). Senza questo ramo
        // cadeva su `v.ToString()` → "True" → parse fail → 0 (STR(TRUE)=" 0" invece di "-1").
        bool b => b ? -1.0 : 0.0,
        // SINGLE: senza questo ramo cadeva sul parse di v.ToString() — con culture it-IT "1,25" (virgola) veniva
        // rifiutato dal parse InvariantCulture e STR$/CDBL/... di un SINGLE davano 0 (IL: 1.25).
        // Via testo "R" invariant (non cast): 0.1f -> 0.1 come l'IL, non 0.10000000149011612.
        float f => float.IsFinite(f) ? double.Parse(f.ToString("R", CultureInfo.InvariantCulture), CultureInfo.InvariantCulture) : (double)f,
        DateValue dv => dv.OADate,
        DateOnly doD => doD.ToDateTime(TimeOnly.MinValue).ToOADate(),
        DateTime dt => dt.ToOADate(),
        Basic.Databases.Rdd.GbDate gd => gd.Value.ToOADate(),
        decimal dec => (double)dec,
        // InvariantCulture: con culture IT il "." è separatore migliaia e "9.6"
        // parsava 96 (bug CINT("9.6")=96). Parità IL (ToDouble invariant).
        _ => double.TryParse(v?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out var r) ? r : 0.0
    };

    /// <summary>
    /// Valore di default per un campo UDT/CLASS appena allocato (OP_NEW_UDT_INSTANCE,
    /// CREATE_ARRAY per un array di UDT), in base al suo DataType dichiarato (da
    /// descriptor["FieldTypes"], vedi Compiler.UserTypes.cs) — stessa lista di tipi numerici
    /// usata dal Compiler per il default di un DIM scalare (Compiler.Assign.cs). Prima di
    /// questo fix ogni campo, di QUALUNQUE tipo, veniva inizializzato a "" (stringa vuota):
    /// un campo numerico/data mai assegnato si leggeva come vuoto invece che come 0.
    /// dataType è `object?` (non DataType) perché il valore arriva da un IDictionary
    /// generico costruito dal Compiler, senza garanzie di tipo statico qui.
    /// </summary>
    /// <summary>Alloca l'array per un campo-array di CLASS alla creazione dell'istanza
    /// (OP_NEW_CLASS_INSTANCE). <paramref name="dimPairsObj"/> è il double[] con coppie
    /// [lower0,upper0,...] registrato dal Compiler in FieldArrayDimensions; il tipo del campo
    /// (ArrayType) decide il default degli elementi: 0.0 numerici, null stringhe, istanza
    /// default per UDT/TYPE (una PER elemento, mai aliasata). Rappresentazione identica a
    /// CREATE_ARRAY (object[]/Array con lower bounds) così l'indicizzazione la trova.</summary>
    private static object? MakeFieldArray(object? dimPairsObj, object? fieldDataType)
    {
        if (dimPairsObj is not double[] dimPairs || dimPairs.Length < 2) return null;
        var nd = dimPairs.Length / 2;
        var sizes = new int[nd];
        var lowers = new int[nd];
        for (var d = 0; d < nd; d++)
        {
            lowers[d] = (int)dimPairs[d * 2];
            sizes[d] = (int)dimPairs[d * 2 + 1] - lowers[d] + 1;
        }
        for (var d = 0; d < nd; d++) if (sizes[d] < 0) return null;
        var arr = Array.CreateInstance(typeof(object), sizes, lowers);
        var elemT = (fieldDataType as ArrayType)?.ElementType;
        if (elemT is UserDefinedType udtE)
        {
            // Una istanza DISTINTA per ogni elemento (mai aliasata): una sola istanza
            // condivisa faceva sì che arr(0)/arr(1)/arr(2) fossero lo stesso oggetto.
            var desc = SyntheticUdtDescriptor(udtE);
            FillFieldArray(arr, sizes, () => MakeDefaultUdtInstance(desc));
        }
        else if (elemT is StringType)
        {
            // stringhe: null (= "" in lettura/stampa), come il DIM locale
        }
        else
        {
            FillFieldArray(arr, sizes, () => 0.0);
        }
        return arr;
    }

    /// <summary>Riempie ogni elemento di un array-campo (anche MULTIDIMENSIONALE) col valore
    /// prodotto da <paramref name="make"/> (invocato PER ELEMENTO — una factory, non un valore
    /// condiviso: gli elementi UDT devono essere istanze distinte). `Array.SetValue(v, i)` a
    /// indice singolo lancia ArgumentException ("Array was not a one-dimensional array") su un
    /// array a >1 dim (bug 2-D). Decompone l'indice lineare in coordinate 0-based.</summary>
    private static void FillFieldArray(Array arr, int[] sizes, Func<object?> make)
    {
        var nd = sizes.Length;
        if (nd == 1)
        {
            var lo0 = arr.GetLowerBound(0);
            for (var i = 0; i < arr.Length; i++) arr.SetValue(make(), lo0 + i);
            return;
        }
        var lo = new int[nd];
        for (var d = 0; d < nd; d++) lo[d] = arr.GetLowerBound(d);
        var idx = new int[nd];
        for (var i = 0; i < arr.Length; i++)
        {
            var rem = i;
            for (var d = nd - 1; d >= 0; d--) { idx[d] = lo[d] + (sizes[d] == 0 ? 0 : rem % sizes[d]); rem = sizes[d] == 0 ? 0 : rem / sizes[d]; }
            arr.SetValue(make(), idx);
        }
    }

    private static object? DefaultValueForFieldType(object? dataType) => dataType switch
    {
        SingleType or DoubleType or CurrencyType or IntegerType or LongType or ByteType
            or QuadType or BitType or DateType or TimeType or DateTimeType => 0.0,
        StringType => "",
        // UDT annidato (2026-09-29, bug reale — vedi AiWiki Bugs/(solved) - UDT-Campo-
        // Annidato-Scrittura-Encadenata-Non-Persiste.md): un campo il cui tipo è un ALTRO TYPE
        // cadeva nel fallback "" (stringa vuota) come qualunque tipo non riconosciuto — `o.I.X
        // = 3.5` (Outer.I AS Inner) falliva silenziosamente perché OP_STORE_MEMBER richiede
        // `obj is IDictionary` per scrivere il campo, e una stringa non lo è MAI. Costruisce
        // ricorsivamente un'istanza reale (MakeDefaultUdtInstance, già usata per REDIM di
        // array di UDT) a partire da un descriptor "sintetico" — FieldNames/FieldTypes presi
        // direttamente da udt.Fields (l'AST, non serve il descriptor compilato: un TYPE
        // annidato semplice non ha bisogno di Constructors/PropertyGetters qui, entrambi letti
        // con guardia sicura da OP_LOAD_MEMBER/OP_STORE_MEMBER anche se assenti).
        UserDefinedType udt => MakeDefaultUdtInstance(SyntheticUdtDescriptor(udt)),
        _ => "",
    };

    /// <summary>Descriptor minimale (solo FieldNames/FieldTypes, chiavati per SymbolId — stesso
    /// shape del descriptor "vero" costruito da Compiler.UserTypes.cs) per un TYPE annidato,
    /// derivato direttamente dall'AST (<see cref="UserDefinedType.Fields"/>) invece che da una
    /// costante del chunk — non serve cercare il descriptor "ufficiale" del tipo annidato,
    /// dato che qui basta poter costruire un'istanza di default e risolvere i suoi campi per
    /// nome/offset (vedi PCodeFieldNameFromOffset).</summary>
    private static System.Collections.IDictionary SyntheticUdtDescriptor(UserDefinedType udt) => new Dictionary<string, object?>
    {
        ["FieldNames"] = udt.Fields.ToDictionary(f => f.SymbolId, f => f.Name.Lexeme),
        ["FieldTypes"] = udt.Fields.ToDictionary(f => f.SymbolId, f => (DataType)f.DataType),
    };

    /// <summary>Legge un campo da una STRINGA JSON (parsata). Parità IlRuntime.JsonFieldGet
    /// (backend IL) per il caso string: usato da OP_LOAD_FIELD quando il valore è una stringa
    /// JSON (es. risultato di JSON.ToJson/ToObject su un UDT nel PCode). Ritorna null se non
    /// parsabile o campo assente.</summary>
    internal static object? PCodeJsonStringField(string json, string fieldName)
    {
        var t = json.TrimStart();
        if (t.Length == 0 || t[0] != '{') return null;
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind == System.Text.Json.JsonValueKind.Object &&
                doc.RootElement.TryGetProperty(fieldName, out var prop))
                return JsonElementToObject(prop);
        }
        catch { }
        return null;
    }

    private static object? JsonElementToObject(System.Text.Json.JsonElement element)
    {
        switch (element.ValueKind)
        {
            case System.Text.Json.JsonValueKind.Object:
                var obj = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                foreach (var prop in element.EnumerateObject())
                    obj[prop.Name] = JsonElementToObject(prop.Value);
                return obj;
            case System.Text.Json.JsonValueKind.Array:
                var list = new List<object?>();
                foreach (var item in element.EnumerateArray())
                    list.Add(JsonElementToObject(item));
                return list;
            case System.Text.Json.JsonValueKind.String:
                return element.GetString();
            case System.Text.Json.JsonValueKind.Number:
                return element.GetDouble();
            // BOOLEANO: bool, non -1/0 — come IlRuntime.JsonElementToObject (stessa
            // rappresentazione dell'assegnazione `o("k") = TRUE`), altrimenti il
            // round-trip parse -> stringify cambia il tipo nel JSON.
            case System.Text.Json.JsonValueKind.True:
                return true;
            case System.Text.Json.JsonValueKind.False:
                return false;
            case System.Text.Json.JsonValueKind.Null:
                return null;
            default:
                return null;
        }
    }

    /// <summary>Risolve il NOME campo dell'offset dato da un'istanza UDT (dict con __class__),
    /// accettando chiavi INT (UDT nativo) sia STRING (descrittore ricostruito da round-trip
    /// JSON ToJson→ToObject, dove le chiavi JSON sono stringhe "offset").</summary>
    internal static string? PCodeFieldNameFromOffset(System.Collections.IDictionary instance, int offset)
    {
        if (instance["__class__"] is not System.Collections.IDictionary classDesc) return null;
        if (classDesc["FieldNames"] is not System.Collections.IDictionary fn) return null;
        if (fn[offset] is string intKey) return intKey;
        if (fn[offset.ToString()] is string strKey) return strKey;
        return null;
    }

    /// <summary>Converte un Dictionary/List nidificato in JsonNode per serializzazione JSON.</summary>
    private static System.Text.Json.Nodes.JsonNode? ObjectToJsonNode(object? value)
    {
        switch (value)
        {
            case null: return null;
            case string s: return System.Text.Json.Nodes.JsonValue.Create(s);
            case double d: return System.Text.Json.Nodes.JsonValue.Create(d);
            case int i: return System.Text.Json.Nodes.JsonValue.Create(i);
            case long l: return System.Text.Json.Nodes.JsonValue.Create(l);
            case bool b: return System.Text.Json.Nodes.JsonValue.Create(b);
            case System.Collections.IDictionary dict:
                var jobj = new System.Text.Json.Nodes.JsonObject();
                foreach (System.Collections.DictionaryEntry kv in dict)
                    if (kv.Key is string k) jobj[k] = ObjectToJsonNode(kv.Value);
                return jobj;
            case System.Collections.IList list:
                var jarr = new System.Text.Json.Nodes.JsonArray();
                foreach (var item in list) jarr.Add(ObjectToJsonNode(item));
                return jarr;
            default: return System.Text.Json.Nodes.JsonValue.Create(value?.ToString() ?? "");
        }
    }

    private static System.Collections.Generic.Dictionary<string, object?> MakeDefaultUdtInstance(System.Collections.IDictionary desc)
    {
        var inst = new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase) { ["__class__"] = desc };
        var fieldTypes = desc.Contains("FieldTypes") ? desc["FieldTypes"] as System.Collections.IDictionary : null;
        if (desc["FieldNames"] is System.Collections.IDictionary fn)
            foreach (var k in fn.Keys)
                if (fn[k] is string fieldName)
                    inst[fieldName] = DefaultValueForFieldType(fieldTypes != null && fieldTypes.Contains(k) ? fieldTypes[k] : null);
        return inst;
    }

    /// <summary>
    /// Confronto per VALORE (`=`/`&lt;&gt;`) di due istanze UDT/CLASS: stesso __class__ e
    /// stesso valore per ogni campo dichiarato — MAI per riferimento. Prima di questo fix
    /// OP_CMP_EQ/OP_CMP_NE cadevano nel ramo numerico generico per due IDictionary (nessun
    /// campo è né string né double), che le convertiva entrambe a 0 (nessuno dei due `is
    /// double`/`is string` combacia) → 0==0 → sempre "uguali", indipendentemente dai campi
    /// (vedi 02-Knowledge/GMBasic-PCODE-UDT-Field-Access-Bug.md). Stessa semantica campo-
    /// per-campo di ExecuteComparisonOperation (Interpreter/Gpm.Expressions.Operators.cs) —
    /// solo `FieldNames` (nomi campo, esclusi __class__/__closure__, chiavi di bookkeeping)
    /// viene confrontato, non l'intero dizionario (che includerebbe anche il riferimento al
    /// descrittore, sempre uguale per istanze dello stesso tipo comunque). Se uno dei due
    /// argomenti non è un dizionario (es. confronto UDT con NIL), sono considerati diversi.
    /// </summary>
    private static bool UdtValueEquals(object? a, object? b)
    {
        if (a is not System.Collections.IDictionary da || b is not System.Collections.IDictionary db)
            return false;
        var classA = da.Contains("__class__") ? da["__class__"] : null;
        var classB = db.Contains("__class__") ? db["__class__"] : null;
        if (!ReferenceEquals(classA, classB)) return false;
        if (classA is System.Collections.IDictionary classDesc && classDesc["FieldNames"] is System.Collections.IDictionary fieldNames)
        {
            foreach (var k in fieldNames.Keys)
            {
                if (fieldNames[k] is not string fieldName) continue;
                var av = da.Contains(fieldName) ? da[fieldName] : null;
                var bv = db.Contains(fieldName) ? db[fieldName] : null;
                if (!Equals(av, bv)) return false;
            }
            return true;
        }
        // Nessun descrittore risolvibile (non dovrebbe succedere per un'istanza UDT/CLASS
        // valida): confronto conservativo per riferimento, mai per valore su dati ignoti.
        return ReferenceEquals(a, b);
    }

    /// <summary>
    /// Crea e accoda un nuovo PCodeTask per GO SUB/GO FUNCTION: estrae WaitGroup (opzionale,
    /// in cima) e argomenti dallo stack del CHIAMANTE, li ripusha sullo stack del NUOVO task
    /// (convenzione posizionale — il prologo compilato della procedura li consuma da lì via
    /// OP_STORE_LOCAL, stessa logica di OP_NEW_GENERATOR_CALL), registra col WaitGroup se
    /// presente, poi accoda sullo scheduler cooperativo (nessun thread reale — vedi PCodeVm.cs).
    /// Condivisa da OP_GO (fire-and-forget) e OP_GO_FUNCTION (che in più collega un future).
    /// </summary>
    private PCodeTask LaunchGoTask(PCodeTask task, int addr)
    {
        var goTask = _scheduler!.CreateTask(addr, task);
        goTask.Scheduler = _scheduler;
        goTask.Globals = task.Globals;
        // NOTA (2026-08-26, DEFER goroutine): la goroutine NON riceve un frame radice — il body
        // parte direttamente da `addr` con task.Frame == null, come sempre (un frame radice
        // rompeva i park di rete: locals frame vs task-level). I DEFER delle goroutine usano lo
        // storage task-level (task.DeferredIps), letti da OP_DEFER/BeginDeferredExecution quando
        // frame è null; al RETURN della root (frame null) i deferred vengono eseguiti e il task
        // completa (unwind.Frame == null → niente resume a ReturnOpcodeIp+1).
        // Stack: args (bottom) ... args (top-1) | wg (top)
        object? wg = null;
        if (task.Sp > 0)
        {
            task.Sp--;
            var e = task.Stack[task.Sp];
            wg = e.Type == 2 ? e.Ref : (object)e.Num;
        }
        var args = new System.Collections.Generic.List<object?>();
        while (task.Sp > 0)
        {
            task.Sp--;
            var e = task.Stack[task.Sp];
            args.Add(e.Type == 2 ? e.Ref : (object)e.Num);
        }
        for (var i = args.Count - 1; i >= 0; i--)
            goTask.PushRef(args[i]);
        if (wg is PCodeWaitGroup pwg)
        {
            pwg.Add(1);
            goTask.WaitGroup = pwg;
        }
        else
        {
            // GO SUB senza IN: il figlio si registra sul gruppo IMPLICITO del
            // genitore (creato se assente) — la WAIT nuda lo attende; il Done
            // implicito di fine task (HandlePostTick) decrementa (bug 2026-10-01).
            Basic.Runtime.Gpm.GpmImplicitChildren.RegisterChild(task, goTask);
        }
        _scheduler.Schedule(goTask);
        return goTask;
    }

    /// <summary>
    /// Sottoscrive una sola volta (per l'intera vita di questo PCodeProcessor) gli eventi
    /// WindowResized/WindowCloseRequested di GraphicsManager — un singleton di processo che
    /// vive più a lungo di un singolo run BASIC, per questo la sottoscrizione va fatta con
    /// guardia (vedi _windowEventsSubscribed) invece che nel costruttore. Nota: non esiste
    /// un hook di Dispose/Shutdown su PCodeProcessor per una disiscrizione esplicita a fine
    /// run — stesso limite architetturale già accettato altrove in questo file (nessuna
    /// pulizia esplicita), accettabile perché la sottoscrizione è al più UNA per run.
    /// </summary>
    private void EnsureWindowEventsSubscribed()
    {
        if (_windowEventsSubscribed) return;
        _windowEventsSubscribed = true;
        // ON WINDOW POLIMORFICO (TUI + GUI): un solo canale — SdlManager.WindowEvent riceve gli
        // eventi di QUALUNQUE finestra SDL (la console TUI e lo screen grafico). Se lo screen
        // grafico è attivo gli eventi arrivano da lì, altrimenti dalla finestra della console.
        // Le lambda registrate da RegisterWindow vengono invocate qui (spawn del task handler).
        try
        {
            var mgr = CommonGraphics.Consoles.SdlManager.Instance;
            mgr.WindowEvent += e =>
            {
                // WindowEventKind (Ast/Graphics.cs): Close=0, Resize=1, Quit=2, ...
                if (e.window.windowEvent == SDL2.SDL.SDL_WindowEventID.SDL_WINDOWEVENT_RESIZED)
                    CommonLib.App.EventDispatcher.Instance.DispatchWindow(1, 0); // Resize (handle 0 = finestra SDL)
                else if (e.window.windowEvent == SDL2.SDL.SDL_WindowEventID.SDL_WINDOWEVENT_CLOSE)
                    CommonLib.App.EventDispatcher.Instance.DispatchWindow(0, 0); // Close (handle 0 = finestra SDL)
            };
            mgr.TextInputEvent += e =>
            {
                // Paste (SDL_TEXTINPUT con testo — SDL genera SDL_TEXTINPUT per paste/IME).
                // Il paste incolla testo multi-carattere: segnala l'evento Paste (kind 8).
                string txt;
                unsafe { txt = System.Text.Encoding.UTF8.GetString(new ReadOnlySpan<byte>(e.text.text, 32)); }
                if (txt.TrimEnd('\0').Length > 1)
                    CommonLib.App.EventDispatcher.Instance.DispatchWindow(8, 0);
            };
        }
        catch { }
    }

    /// <summary>
    /// Invocato (da un thread SDL, non da un worker della VM) quando un evento finestra
    /// registrato via ON WINDOW ... CALL si verifica. Spawna un task INDIPENDENTE che parte
    /// dall'indirizzo dell'handler — stesso schema di LaunchGoTask/GO SUB, scelto perché
    /// CreateTask/Schedule sono già thread-safe per chiamanti esterni ai worker della VM
    /// (lock interni, vedi BRT/Gpm/GpmCore.cs).
    /// </summary>
    /// <summary>
    /// Spawna un task INDIPENDENTE all'indirizzo dell'handler ON KEY (stesso schema di
    /// FireWindowEvent). Chiamato dal EventDispatcher quando il tasto registrato viene letto.
    /// </summary>
    private void FireKeyEvent(int targetAddr, object?[]? globals)
    {
        var sched = _scheduler;
        if (sched == null) return;
        var newTask = sched.CreateTask(targetAddr);
        newTask.Scheduler = sched;
        newTask.Globals = globals;
        sched.Schedule(newTask);
    }

    /// <summary>
    /// Inoltra un tasto letto al EventDispatcher unificato (ON KEY). Da chiamare in ogni
    /// punto in cui la VM legge un tasto: INKEY/GET_KEY/INPUT e la READ Clipper.
    /// </summary>
    /// <summary>
    /// Spawna un task INDIPENDENTE all'indirizzo dell'handler ON MOUSE (stesso schema di
    /// FireKeyEvent). Chiamato dal EventDispatcher quando un evento mouse (TUI o GUI) scatta.
    /// </summary>
    private void FireMouseEvent(int targetAddr, object?[]? globals)
    {
        var sched = _scheduler;
        if (sched == null) return;
        var newTask = sched.CreateTask(targetAddr);
        newTask.Scheduler = sched;
        newTask.Globals = globals;
        sched.Schedule(newTask);
    }

    private void DispatchKeyEvent(CommonLib.Consoles.TerminalKeyInfo tki)
    {
        try
        {
            var mods = (tki.Ctrl ? ConsoleModifiers.Control : 0) | (tki.Alt ? ConsoleModifiers.Alt : 0) | (tki.Shift ? ConsoleModifiers.Shift : 0);
            var combo = new CommonLib.Keyboard.KeyCombination((ConsoleKey)tki.KeyCode, mods);
            CommonLib.App.EventDispatcher.Instance.DispatchKey(combo);
        }
        catch { }
    }

    private void FireWindowEvent(int targetAddr, object?[]? globals)
    {
        var sched = _scheduler;
        if (sched == null) return;
        var newTask = sched.CreateTask(targetAddr);
        newTask.Scheduler = sched;
        newTask.Globals = globals;
        sched.Schedule(newTask);
    }

    /// <summary>
    /// Propaga il fallimento di un future GO FUNCTION (task collegato terminato con
    /// un'eccezione non gestita, vedi PCodeVm.HandlePostTick/PCodeFutureValue.Fail) al task che
    /// lo sta leggendo — stesso schema a 3 livelli di SysCall.THROW_ERROR: prima il TRY/CATCH
    /// attivo se c'è, poi l'ON ERROR GOTO, infine termina con un'eccezione .NET chiara. Senza
    /// questo, un future fallito lasciava il lettore bloccato per sempre (IsResolved restava
    /// false) — nessun errore, nessun crash visibile, solo un hang silenzioso indistinguibile da
    /// un deadlock legittimo.
    /// </summary>
    private void PropagateFutureFailure(PCodeTask task, PCodeFutureValue future)
    {
        task.ErrorLineIp = task.CurrentInstructionIp;
        task.ErrorResumeNextIp = StatementResumeIp(task.CurrentInstructionIp, task.Ip);
        DiscardIncompletePrintStatement(task);
        // ExecuteTaskTick incorpora lo stack trace .NET dentro Message (per il debug di crash
        // interni non gestiti) — non appropriato da mostrare in un CATCH lato BASIC, prendiamo
        // solo la prima riga (il messaggio vero e proprio, prima del trace multi-riga).
        var rawMsg = future.FailureException?.Message ?? "unknown error";
        var firstLine = rawMsg.Split('\n')[0].TrimEnd('\r');
        var msg = "GO FUNCTION error: " + firstLine;
        SetErrGlobal(task, new System.Collections.Generic.Dictionary<string, object?>
        {
            ["Number"] = 200.0, // ErrorCode.InternalError
            ["Description"] = msg,
            ["LineNumber"] = (double)SourceLineForIp(task.CurrentInstructionIp),
            ["SourceLineText"] = string.Empty,
            ["StackTrace"] = string.Empty
        });
        var errInfo = new System.Collections.Generic.Dictionary<string, object?>
        {
            ["Number"] = 200.0, // ErrorCode.InternalError
            ["Description"] = msg
        };
        if (TryEnterExceptionHandler(task, errInfo))
            return;
        UnwindProcedureFramesToErrorHandler(task);
        if (task.ErrorHandlerIp is > 0)
        {
            /* NB: NON si pusha il messaggio. L'handler ON ERROR non ha un valore in cima allo
             * stack (il canone è la pseudo-variabile ERR, popolata sopra): il messaggio
             * rimaneva lì e, con RESUME NEXT dopo un errore in MEZZO a un'espressione, veniva
             * consumato dallo STORE che segue (es. `C = A / B` con B=0 → C = "Division by
             * zero" invece di 0, divergenza con IL/C99Native). */
            if (task.ErrorHandlerIsGosub)
            {
                task.IpStack ??= new Stack<int>();
                task.IpStack.Push(task.Ip);
            }
            task.Ip = task.ErrorHandlerIp.Value;
            return;
        }
        throw new InvalidOperationException("BASIC Runtime Error: " + msg, future.FailureException);
    }

    /// <summary>Schema a 3 livelli per un errore BASIC a runtime (TRY/CATCH attivo → ON ERROR
    /// GOTO/GOSUB → eccezione .NET chiara) — estratto da SysCall.THROW_ERROR e riusato da OP_READ
    /// (OUT OF DATA) e da qualunque altro opcode che debba sollevare un errore BASIC.
    /// Valore di ritorno: <c>true</c> = ON ERROR RESUME NEXT (l'esecuzione riprende
    /// dall'istruzione successiva, quindi l'opcode chiamante deve lasciare un risultato — 0 —
    /// sullo stack al posto dell'espressione fallita); <c>false</c> = l'errore è stato
    /// consegnato a TRY/CATCH o all'handler ON ERROR (l'Ip è già cambiato: NIENTE va pushato,
    /// altrimenti il valore finirebbe sopra quello che l'handler sta per consumare — è il bug
    /// per cui `CATCH e AS ERR` legava 0 invece del dizionario dell'errore).</summary>
    private bool RaiseBasicError(PCodeTask task, double errCode, string msg)
    {
        task.ErrorLineIp = task.CurrentInstructionIp;
        task.ErrorResumeNextIp = StatementResumeIp(task.CurrentInstructionIp, task.Ip);
        DiscardIncompletePrintStatement(task);
        // Pseudo-variabile ERR del linguaggio (StdLib: ERR.Number/Description/LineNumber/…):
        // va popolata PRIMA del dispatch, così l'handler ON ERROR e il CATCH la vedono.
        // Prima non veniva mai scritta → `ERR.Number` restava vuoto/0 (divergenza con IL/C99).
        if (string.IsNullOrEmpty(msg))
            msg = BasicException.GetDefaultMessage((ErrorCode)(int)errCode);
        // UN SOLO dizionario per entrambi i consumatori: la pseudo-variabile ERR e il valore
        // legato da `CATCH e AS ERR` (prima il CATCH riceveva un dict con soli Number/
        // Description: e.LineNumber restava vuoto).
        var errInfo = new System.Collections.Generic.Dictionary<string, object?>
        {
            ["Number"] = errCode,
            ["Description"] = msg,
            ["LineNumber"] = (double)SourceLineForIp(task.CurrentInstructionIp),
            ["SourceLineText"] = string.Empty,
            ["StackTrace"] = string.Empty
        };
        SetErrGlobal(task, errInfo);
        if (TryEnterExceptionHandler(task, errInfo))
            return false;
        // ON ERROR RESUME NEXT (OP_RESUME 0 → ErrorMode=ResumeNext, senza handler IP):
        // l'errore è CATTURATO e si riprende dall'istruzione successiva (parità IL).
        // Prima ErrorMode non era mai controllato → l'errore restava fatale (bug
        // AgentStructuredToolCalling: IL catturava, PCode no).
        if (task.ErrorMode == PCodeErrorMode.ResumeNext)
        {
            task.Ip = task.ErrorResumeNextIp;
            return true;   // il chiamante lascia un risultato (0) al posto dell'espressione fallita
        }
        UnwindProcedureFramesToErrorHandler(task);
        if (task.ErrorHandlerIp > 0)
        {
            /* NB: NON si pusha il messaggio (vedi PropagateFutureFailure sopra): l'handler
             * ON ERROR non riceve un valore sullo stack — il canone è la pseudo-variabile ERR.
             * Il valore rimasto finiva consumato dallo STORE che segue un errore in MEZZO a
             * un'espressione (`C = A / B`, B=0 → C = "Division by zero" invece di 0). */
            if (task.ErrorHandlerIsGosub)
            {
                task.IpStack ??= new Stack<int>();
                task.IpStack.Push(task.Ip);
            }
            task.Ip = task.ErrorHandlerIp.Value;
            return false;
        }
        throw new InvalidOperationException("BASIC Runtime Error: " + msg);
    }

    /// <summary>Intervalli [start,end) del bytecode emesso da ogni istruzione BASIC (per
    /// RESUME NEXT a granularità di ISTRUZIONE). Null per i chunk vecchi/senza metadata.</summary>
    private readonly (int Start, int End)[]? _debugStatementRanges;

    /// <summary>Slot locali numerici per indirizzo di procedura (default 0).</summary>
    private readonly Dictionary<int, bool[]>? _localNumericByAddr;

    /// <summary>
    /// Ip di ripresa per RESUME NEXT: l'END dell'istruzione più INTERNA che contiene
    /// <paramref name="instructionIp"/> (l'opcode fallito). Senza metadata o se l'ip non è in
    /// nessun intervallo si ricade su <paramref name="fallbackIp"/> (ip dopo l'opcode).
    /// </summary>
    private int StatementResumeIp(int instructionIp, int fallbackIp)
    {
        var ranges = _debugStatementRanges;
        if (ranges is null || instructionIp < 0) return fallbackIp;
        int best = -1, bestLen = int.MaxValue;
        foreach (var (st, en) in ranges)
        {
            if (instructionIp < st || instructionIp >= en) continue;
            var len = en - st;
            if (len < bestLen) { bestLen = len; best = en; }
        }
        return best >= 0 ? best : fallbackIp;
    }

    /// <summary>
    /// Riga sorgente (1-based, come il sorgente BASIC) dell'ip indicato. La mappa
    /// <c>Chunk.Lines</c> ha una voce per OGNI byte, ma gli opcode sintetici (prologo, store
    /// impliciti, fine corpo) portano -1: si cerca all'indietro (poi in avanti) la prima voce
    /// valida. Le voci sono l'INDICE 0-based della riga (`SourceLineIndex`, vedi il parser:
    /// `currentTokenLine - 1`), quindi la riga del sorgente è <c>voce + 1</c> — l'equivalente
    /// del `s->line` (1-based) che C99Native passa a gmb_raise_error.
    /// </summary>
    private int SourceLineForIp(int ip)
    {
        var lines = _debugSourceLines;
        if (lines is null || lines.Length == 0) return 0;
        static int ToLine(int raw) => raw >= 0 ? raw + 1 : 0;
        if (ip >= 0 && ip < lines.Length && lines[ip] > 0) return ToLine(lines[ip]);
        for (var d = 1; d < lines.Length; d++)
        {
            var back = ip - d;
            if (back >= 0 && back < lines.Length && lines[back] > 0) return ToLine(lines[back]);
            var fwd = ip + d;
            if (fwd >= 0 && fwd < lines.Length && lines[fwd] > 0) return ToLine(lines[fwd]);
        }
        return 0;
    }

    /// <summary>
    /// Popola la pseudo-variabile globale `ERR` (dichiarata in StdLib.System.cs come tipo
    /// `ErrType`: Number/Description/LineNumber/SourceLineText/StackTrace) con i dettagli
    /// dell'errore appena sollevato. Il simbolo è un globale con schema di tipo built-in: il
    /// suo accesso (`ERR.Number`) è name-based (`OP_LOAD_FIELD`), quindi il valore deve essere
    /// un dizionario con le chiavi dello schema — così le letture dinamiche lo risolvono come
    /// qualunque altro tipo di sistema. No-op se il programma non ha lo slot `ERR` (es. codice
    /// compilato senza StdLib), per non alterare le build ridotte.
    /// </summary>
    private void SetErrGlobal(PCodeTask task, System.Collections.Generic.Dictionary<string, object?> err)
    {
        var idx = _hostGlobalNames.Length == 0 ? -1 : Array.IndexOf(_hostGlobalNames, "ERR");
        if (idx < 0) return;
        task.EnsureGlobals(idx + 1);
        var globals = task.Globals;
        if (globals is null || idx >= globals.Length) return;
        if (globals[idx] is ByRefBox box) box.Value = err;
        else globals[idx] = err;
    }

    private static void EnterProcedureErrorScope(PCodeTask task, PCodeCallFrame frame)
    {
        frame.IsErrorScopeBoundary = true;
        frame.SavedErrorHandlerIp = task.ErrorHandlerIp;
        frame.SavedErrorHandlerIsGosub = task.ErrorHandlerIsGosub;
        frame.SavedErrorMode = task.ErrorMode;
        frame.SavedErrorLineIp = task.ErrorLineIp;
        frame.SavedErrorResumeNextIp = task.ErrorResumeNextIp;
        task.ErrorHandlerIp = null;
        task.ErrorHandlerIsGosub = false;
        task.ErrorMode = PCodeErrorMode.None;
        task.ErrorLineIp = -1;
        task.ErrorResumeNextIp = -1;
    }

    private static void DiscardIncompletePrintStatement(PCodeTask task)
    {
        // OP_PRINT_ITEM accumula gli elementi della riga qui e li promuove a PendingPrints
        // soltanto su OP_PRINT_LINE/NEWLINE. Se la valutazione di un argomento solleva un
        // errore (tipicamente implicit-await di un future fallito), lo statement non è mai
        // completato: il prefisso parziale non deve sopravvivere al salto verso l'handler.
        task.PrintLineStaging = null;
    }

    private static void RestoreCallerErrorScope(PCodeTask task, PCodeCallFrame frame)
    {
        if (!frame.IsErrorScopeBoundary)
            return;
        task.ErrorHandlerIp = frame.SavedErrorHandlerIp;
        task.ErrorHandlerIsGosub = frame.SavedErrorHandlerIsGosub;
        task.ErrorMode = frame.SavedErrorMode;
        task.ErrorLineIp = frame.SavedErrorLineIp;
        task.ErrorResumeNextIp = frame.SavedErrorResumeNextIp;
    }

    private void UnwindProcedureFramesToErrorHandler(PCodeTask task)
    {
        while (task.ErrorHandlerIp is null && task.Frame is { IsErrorScopeBoundary: true } frame)
        {
            task.Frame = frame.Previous;
            task.Sp = Math.Clamp(frame.SavedStackPointer, 0, task.Stack.Length);
            while (task.TryStack.Count > 0 && ReferenceEquals(task.TryStack.Peek().Frame, frame))
                task.TryStack.Pop();
            RestoreCallerErrorScope(task, frame);
            // Il punto di continuazione del chiamante è subito dopo CALL/CALL_N, non subito
            // dopo l'ERROR nella procedura ormai rimossa dalla frame chain.
            task.ErrorLineIp = Math.Max(0, frame.ReturnIp - 1);
            /* Il CALL del chiamante può stare dentro un'espressione (`x = F()`): la ripresa
             * va all'END di quell'istruzione, non all'opcode subito dopo il CALL. */
            task.ErrorResumeNextIp = StatementResumeIp(frame.ReturnIp - 1, frame.ReturnIp);
        }
    }

    /// <summary>Entra nel CATCH più interno ancora appartenente alla frame chain corrente.</summary>
    private bool TryEnterExceptionHandler(PCodeTask task, object? errorValue)
    {
        var superseded = task.PendingUnwind;
        var suppressed = new List<object?>();
        if (superseded != null)
        {
            suppressed.AddRange(superseded.SuppressedErrors);
            suppressed.Add(superseded.Reason switch
            {
                PCodeUnwindReason.Exception => superseded.ErrorValue,
                PCodeUnwindReason.Cancel => new OperationCanceledException("P-Code task cancellation suppressed by FINALLY error."),
                _ => $"{superseded.Reason} unwind from IP {superseded.OriginIp}"
            });
        }
        TryInfo? target = null;
        foreach (var tryInfo in task.TryStack)
        {
            if (!IsFrameInChain(task.Frame, tryInfo.Frame))
                continue;
            if (!tryInfo.HasCatch)
                continue;
            // Un errore nato nel FINALLY non può essere catturato dal CATCH della stessa
            // regione: quel protected statement è già in fase di uscita. Cerca un handler
            // esterno e lascia che ContinuePendingUnwind elimini il cleanup corrente.
            if (task.CurrentInstructionIp >= tryInfo.FinallyIp &&
                task.CurrentInstructionIp < tryInfo.EndIp)
                continue;
            target = tryInfo;
            break;
        }
        if (target == null)
        {
            /* Nessun CATCH attivo, ma ci possono essere FINALLY da attraversare (TRY … FINALLY
             * senza CATCH): avvia comunque l'unwind con TargetHandler = null, così i cleanup
             * eseguono PRIMA della propagazione. Prima il FINALLY veniva saltato del tutto
             * (divergenza con IL/C99Native: "FINALLY sempre eseguito"). */
            if (task.CleanupStack.Count > 0)
            {
                task.PendingUnwind = new PCodeUnwindState
                {
                    Reason = PCodeUnwindReason.Exception,
                    OriginIp = task.CurrentInstructionIp,
                    Frame = task.Frame,
                    TargetHandler = null,
                    ErrorValue = AttachSuppressedErrors(errorValue, suppressed),
                    SuppressedErrors = suppressed
                };
                ContinuePendingUnwind(task);
                return true;
            }
            if (superseded != null)
            {
                AttachSuppressedErrors(errorValue, suppressed);
                task.LastSuppressedErrors = suppressed;
                DiscardSupersededCleanup(task);
                task.PendingUnwind = null;
            }
            else
            {
                task.LastSuppressedErrors = Array.Empty<object?>();
            }
            return false;
        }

        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.Exception,
            OriginIp = task.CurrentInstructionIp,
            Frame = task.Frame,
            TargetHandler = target,
            ErrorValue = AttachSuppressedErrors(errorValue, suppressed),
            SuppressedErrors = suppressed
        };
        ContinuePendingUnwind(task);
        return true;
    }

    private static void DiscardSupersededCleanup(PCodeTask task)
    {
        if (task.CleanupStack.Count == 0)
            return;
        var cleanup = task.CleanupStack.Peek();
        if (task.CurrentInstructionIp < cleanup.FinallyIp || task.CurrentInstructionIp >= cleanup.EndIp)
            return;
        PopCleanup(task, cleanup);
    }

    private static object? AttachSuppressedErrors(object? errorValue, IReadOnlyList<object?> suppressed)
    {
        if (suppressed.Count == 0)
            return errorValue;
        if (errorValue is IDictionary<string, object?> errorInfo)
            errorInfo["Suppressed"] = suppressed.ToArray();
        return errorValue;
    }

    private static bool IsFrameInChain(PCodeCallFrame? current, PCodeCallFrame? target)
    {
        if (target == null)
            return true;
        for (var frame = current; frame != null; frame = frame.Previous)
            if (ReferenceEquals(frame, target))
                return true;
        return false;
    }

    /// <summary>Rimuove i TRY della frame corrente che un GOTO sta abbandonando.</summary>
    private static void UnwindTryRegionsForJump(PCodeTask task, int targetIp)
    {
        while (task.TryStack.Count > 0)
        {
            var activeTry = task.TryStack.Peek();
            if (!ReferenceEquals(activeTry.Frame, task.Frame))
                return;
            if (targetIp >= activeTry.ProtectedStartIp && targetIp < activeTry.HandlerIp)
                return;
            task.TryStack.Pop();
        }
    }

    private bool BeginReturnUnwind(PCodeTask task, int returnOpcodeIp)
    {
        if (task.PendingUnwind != null)
            return false;
        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.Return,
            ReturnOpcodeIp = returnOpcodeIp,
            OriginIp = returnOpcodeIp,
            Frame = task.Frame
        };
        return ContinuePendingUnwind(task);
    }

    private bool BeginGotoUnwind(PCodeTask task, int targetIp)
    {
        if (task.PendingUnwind != null)
            return false;
        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.Goto,
            TargetIp = targetIp,
            OriginIp = task.CurrentInstructionIp,
            Frame = task.Frame
        };
        return ContinuePendingUnwind(task);
    }

    private bool BeginGosubReturnUnwind(PCodeTask task, PCodeGosubContinuation continuation)
    {
        if (task.PendingUnwind != null)
            return false;
        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.ReturnGosub,
            OriginIp = task.CurrentInstructionIp,
            Frame = task.Frame,
            GosubContinuation = continuation
        };
        return ContinuePendingUnwind(task);
    }

    private bool BeginCancellationUnwind(PCodeTask task)
    {
        if (task.CancellationUnwindStarted)
            return task.PendingUnwind != null;
        task.CancellationUnwindStarted = true;
        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.Cancel,
            OriginIp = task.CurrentInstructionIp,
            Frame = task.Frame
        };
        return ContinuePendingUnwind(task);
    }

    private bool BeginGoExitUnwind(PCodeTask task)
    {
        if (task.PendingUnwind != null)
            return false;
        task.PendingUnwind = new PCodeUnwindState
        {
            Reason = PCodeUnwindReason.GoExit,
            OriginIp = task.CurrentInstructionIp,
            Frame = task.Frame
        };
        return ContinuePendingUnwind(task);
    }

    /// <summary>Avanza un RETURN/GOTO attraverso i FINALLY LIFO. Il FINALLY è normale
    /// bytecode: se parcheggia, PendingUnwind e CleanupStack restano nel PCB e l'unwind
    /// riprende quando Ip raggiunge EndIp.</summary>
    private bool ContinuePendingUnwind(PCodeTask task)
    {
        var unwind = task.PendingUnwind;
        if (unwind == null)
            return false;

        while (task.CleanupStack.Count > 0)
        {
            var cleanup = task.CleanupStack.Peek();
            if (unwind.Reason == PCodeUnwindReason.ReturnGosub &&
                task.CleanupStack.Count <= unwind.GosubContinuation!.CleanupDepth)
                break;
            if (unwind.Reason is not (PCodeUnwindReason.Exception or PCodeUnwindReason.Cancel or PCodeUnwindReason.GoExit) &&
                !ReferenceEquals(cleanup.Frame, unwind.Frame))
                break;

            if (unwind.Reason == PCodeUnwindReason.Exception &&
                ReferenceEquals(cleanup, unwind.TargetHandler))
                break;

            var targetStaysInside = unwind.Reason == PCodeUnwindReason.Goto &&
                unwind.TargetIp >= cleanup.ProtectedStartIp && unwind.TargetIp < cleanup.EndIp;
            if (targetStaysInside)
                break;

            // Un RETURN/GOTO emesso dal FINALLY corrente lo sostituisce: non deve rieseguire
            // lo stesso cleanup dall'inizio, ma deve ancora attraversare gli eventuali outer.
            if (unwind.OriginIp >= cleanup.FinallyIp && unwind.OriginIp < cleanup.EndIp)
            {
                PopCleanup(task, cleanup);
                continue;
            }

            task.Frame = cleanup.Frame;
            task.Ip = cleanup.FinallyIp;
            return true;
        }

        // Parità Go defer: prima di completare il RETURN ordinario di una procedura, esegue
        // i body DEFER registrati nel frame (LIFO). PendingUnwind resta attivo; OP_DEFER_RET
        // lo completa quando i deferred sono esauriti.
        if (unwind.Reason == PCodeUnwindReason.Return &&
            unwind.Frame?.DeferredIps is { Count: > 0 } deferred)
        {
            task.PendingDeferReturn = true;
            task.Ip = deferred.Pop();
            return true;
        }

        task.PendingUnwind = null;
        if (unwind.Reason == PCodeUnwindReason.Exception)
        {
            task.LastSuppressedErrors = unwind.SuppressedErrors;
            if (unwind.TargetHandler is null)
            {
                DeliverExceptionAfterFinally(task, unwind.ErrorValue);
                return false;
            }
            EnterSelectedExceptionHandler(task, unwind.TargetHandler, unwind.ErrorValue);
            return false;
        }
        if (unwind.Reason == PCodeUnwindReason.Cancel)
        {
            task.PendingException = new OperationCanceledException("P-Code task cancelled.");
            task.State = PCodeTaskState.Finished;
            return false;
        }
        if (unwind.Reason == PCodeUnwindReason.GoExit)
        {
            task.State = PCodeTaskState.Finished;
            return false;
        }
        if (unwind.Reason == PCodeUnwindReason.ReturnGosub)
        {
            var continuation = unwind.GosubContinuation!;
            if (task.GosubContinuations.Count == 0 ||
                !ReferenceEquals(task.GosubContinuations.Peek(), continuation))
                throw new InvalidOperationException("P-Code GOSUB continuation disappeared during unwind.");
            task.GosubContinuations.Pop();
            while (task.TryStack.Count > continuation.TryDepth)
                task.TryStack.Pop();
            while (task.CleanupStack.Count > continuation.CleanupDepth)
                task.CleanupStack.Pop();
            task.Sp = Math.Clamp(continuation.StackPointer, 0, task.Stack.Length);
            task.Ip = continuation.ReturnIp;
            return false;
        }
        task.Ip = unwind.Reason == PCodeUnwindReason.Goto ? unwind.TargetIp : unwind.ReturnOpcodeIp;
        return false;
    }

    /// <summary>Consegna un'eccezione la cui catena di FINALLY è stata attraversata e per cui
    /// non esiste alcun CATCH attivo: stesso schema a 3 livelli del ramo "nessun target"
    /// (ON ERROR RESUME NEXT → ON ERROR GOTO/GOSUB → errore fatale), ma DOPO i FINALLY.</summary>
    private void DeliverExceptionAfterFinally(PCodeTask task, object? errorValue)
    {
        var msg = "Unspecified runtime error";
        if (errorValue is System.Collections.Generic.IDictionary<string, object?> d &&
            d.TryGetValue("Description", out var dv) && dv is string ds && ds.Length > 0)
            msg = ds;
        if (task.ErrorMode == PCodeErrorMode.ResumeNext)
        {
            task.Ip = task.ErrorResumeNextIp;
            return;
        }
        UnwindProcedureFramesToErrorHandler(task);
        if (task.ErrorHandlerIp > 0)
        {
            if (task.ErrorHandlerIsGosub)
            {
                task.IpStack ??= new Stack<int>();
                task.IpStack.Push(task.Ip);
            }
            task.Ip = task.ErrorHandlerIp.Value;
            return;
        }
        throw new InvalidOperationException("BASIC Runtime Error: " + msg);
    }

    private static void EnterSelectedExceptionHandler(PCodeTask task, TryInfo target, object? errorValue)
    {
        while (task.TryStack.Count > 0)
        {
            var current = task.TryStack.Pop();
            if (!ReferenceEquals(current, target))
                continue;
            task.Frame = target.Frame;
            task.Sp = Math.Clamp(target.StackPointer, 0, task.Stack.Length);
            task.PushRef(errorValue);
            task.Ip = target.HandlerIp;
            return;
        }
        throw new InvalidOperationException("P-Code exception handler disappeared during unwind.");
    }

    /// <summary>Chiamato prima del fetch: EndIp non è un opcode aggiuntivo, ma il confine
    /// registrato nella side-table. Questo conserva il bytecode legacy e rende comunque il
    /// FINALLY sospendibile per un numero arbitrario di tick.</summary>
    private bool CompleteFinallyIfNeeded(PCodeTask task)
    {
        if (task.CleanupStack.Count == 0)
            return false;
        var cleanup = task.CleanupStack.Peek();
        if (task.Ip != cleanup.EndIp)
            return false;

        PopCleanup(task, cleanup);
        return task.PendingUnwind != null && ContinuePendingUnwind(task);
    }

    private static void PopCleanup(PCodeTask task, TryInfo cleanup)
    {
        if (task.CleanupStack.Count > 0 && ReferenceEquals(task.CleanupStack.Peek(), cleanup))
            task.CleanupStack.Pop();
        if (task.TryStack.Count > 0 && ReferenceEquals(task.TryStack.Peek(), cleanup))
            task.TryStack.Pop();
    }

    /// <summary>
    /// Implicit-await INLINE per un future usato come istanza direttamente dallo stack
    /// (es. `FETCH(url).StatusCode` / `FETCH(url).Headers["x"]` senza passare da una variabile).
    /// Prima l'await esisteva solo su OP_LOAD_LOCAL/OP_LOAD_GLOBAL e OP_RESOLVE_FUTURE: un
    /// PCodeFutureValue consumato da OP_LOAD_FIELD/OP_LOAD_MEMBER/OP_LOAD_ELEMENT/OP_CALL_MEMBER
    /// non veniva mai atteso (campo/index/membro su un future → null o no-op). Qui si lavora SOLO
    /// in Peek (lo stack resta intatto): se alla posizione <paramref name="stackPos"/> c'è un
    /// future non risolto, si riavvolge l'Ip sull'opcode corrente (opcodeSize) e si parcheggia il
    /// task (RegisterWaiter) — al risveglio l'opcode ri-esegue e ritrova lo stesso stato di stack.
    /// Se il future è già risolto lo si sostituisce IN PLACE col risultato (così l'opcode prosegue
    /// normalmente). Ritorna true se il task si è parcheggiato: il chiamante deve terminare subito
    /// il tick senza consumare nulla.</summary>
    private bool AwaitInlineFutureAt(PCodeTask task, int opcodeSize, int stackPos)
    {
        if (stackPos < 0 || stackPos >= task.Sp || task.Stack[stackPos].Type != 2) return false;
        if (task.Stack[stackPos].Ref is not PCodeFutureValue fut) return false;
        task.Ip -= opcodeSize; // ri-esegue l'opcode corrente al risveglio (rewind PRIMA di RegisterWaiter, vedi OP_LOAD_GLOBAL)
        if (fut.RegisterWaiter(task))
        {
            return true; // parcheggiato
        }
        task.Ip += opcodeSize; // già risolto: annulla il rewind
        if (fut.FailureException != null) { PropagateFutureFailure(task, fut); return true; }
        task.Stack[stackPos].Ref = fut.Result;
        task.Stack[stackPos].Type = 2;
        task.Stack[stackPos].BasicType = fut.Result switch
        {
            null => StackBasicType.Null,
            string => StackBasicType.String,
            bool => StackBasicType.Bool,
            double or int or long or float or decimal or short or byte => StackBasicType.Number,
            _ => StackBasicType.Object
        };
        return false;
    }

    /// <summary>Variante "istanza in cima allo stack" di <see cref="AwaitInlineFutureAt"/>.</summary>
    private bool AwaitInlineFuture(PCodeTask task, int opcodeSize) => AwaitInlineFutureAt(task, opcodeSize, task.Sp - 1);

    /// <summary>
    /// L'operatore & (macro Clipper-style: esegue una stringa come codice BASIC compilato al
    /// volo — NON è un operatore di concatenazione, nonostante l'aspetto superficiale di
    /// `PRINT "a" & "b"`, che in realtà produce due argomenti PRINT distinti senza separatore,
    /// il secondo dei quali è la macro) non è implementato nella VM P-CODE — solo nell'AST
    /// executor (Gpm.Macros.cs, compilazione JIT + esecuzione a 3 stati). Prima di questo fix
    /// l'opcode scartava silenziosamente l'operando e pushava null: un programma che si
    /// aspettava un valore vedeva sparire dati senza alcun errore. Meglio un errore BASIC
    /// chiaro a runtime (stesso schema a 3 livelli di SysCall.THROW_ERROR: TRY/CATCH attivo →
    /// ON ERROR GOTO → eccezione .NET) che un dato perso in silenzio.
    /// </summary>

    private int PopInt(PCodeTask task) { return (int)task.PopNum(); }
    /// <summary>Coordinata grafica: lround away-from-zero come gmb_gfx_*_f del runtime C
    /// (il cast (int) troncava e divergeva da C99Native su coordinate frazionarie).</summary>
    private int PopCoord(PCodeTask task) { return (int)Math.Round(task.PopNum(), MidpointRounding.AwayFromZero); }

    void PushF(PCodeTask t, object? v) { if (t.Frame != null) t.Frame.PushFrameValue(v); else t.PushRef(v); }

    /// <summary>Stato dei cicli FOR del frame CORRENTE. Vive sul frame e non sul task: il callee
    /// riusa gli stessi numeri di slot del chiamante, quindi uno stato indicizzato solo dallo slot
    /// veniva sovrascritto (OP_FOR_PREPARE) e poi rimosso (OP_FOR_LOOP) dal ciclo annidato, e al
    /// ritorno il FOR del chiamante si fermava. Senza frame (goroutine radice) resta sul task.
    /// Verificato da shared/minitests/loops/for_recursion_state.bas.</summary>
    static System.Collections.Generic.Dictionary<int, (double Counter, double End, double Step)> ForStateOf(PCodeTask t)
    {
        var f = t.Frame;
        if (f == null) return t.ForState;
        return f.ForState ??= new System.Collections.Generic.Dictionary<int, (double Counter, double End, double Step)>();
    }
    /// <summary>Pop di un riferimento (array/stringa) dal TASK stack: gli argomenti dei
    /// syscall sono push-ati da OP_LOAD_LOCAL/OP_PUSH_CONST su `task.Stack` (Sp), NON sul
    /// frame stack (che serve ai risultati multi-valore della rete via PushF).
    /// Prima leggeva il frame stack: con un frame attivo (dentro una SUB) ritornava
    /// SEMPRE null → MMAPWRITEBYTES e PALETTE_DEFINE no-op silenziosi (e i pop numerici
    /// successivi scalavano di uno: handle=0).</summary>
    object? PopF(PCodeTask t) => t.Sp > 0 ? t.PopRef() : null;

    private byte ReadU8(PCodeTask task)
    {
        var ip = task.Ip++;
        return ip < _code.Length ? _code[ip] : (byte)0;
    }

    private ushort ReadU16(PCodeTask task)
    {
        var hi = ReadU8(task);
        var lo = ReadU8(task);
        return (ushort)((hi << 8) | lo);
    }

    /// <summary>Builtin matematiche pure della fusione nativa (OP_NATIVE_LOOP_ACC /
    /// OP_NATIVE_BUILTIN_ASSIGN): stesso mapping dei codici BuiltInFunction della VM/C
    /// (gmb_vm_core.c) — FIX=9, INT=10, ROUND=12, ABS=19, ATN=21, COS=23, EXP=24, LOG=26,
    /// SIN=29, SQR=30, TAN=32, LOG10=35.</summary>
    private static double NativeLoopMath(int fCode, double i) => fCode switch
    {
        (int)BuiltInFunction.SQR => Math.Sqrt(i),
        (int)BuiltInFunction.ABS => Math.Abs(i),
        (int)BuiltInFunction.EXP => Math.Exp(i),
        (int)BuiltInFunction.LOG => Math.Log(i),
        (int)BuiltInFunction.LOG10 => Math.Log10(i),
        (int)BuiltInFunction.SIN => Math.Sin(i),
        (int)BuiltInFunction.COS => Math.Cos(i),
        (int)BuiltInFunction.TAN => Math.Tan(i),
        (int)BuiltInFunction.ATN => Math.Atan(i),
        (int)BuiltInFunction.INT => Math.Floor(i),
        (int)BuiltInFunction.ROUND => Math.Round(i),
        (int)BuiltInFunction.FIX => Math.Truncate(i),
        _ => 0,
    };

    /// <summary>Stesso formato del builtin CSTR (PCodeProcessor.Builtins.cs): InvariantCulture
    /// + spazio iniziale per i non negativi. Usato dalla fusione OP_NATIVE_STRING_APPEND_LOOP.</summary>
    private static string CstrInvariant(double value)
    {
        var cs = value.ToString("G15", CultureInfo.InvariantCulture);
        if (!cs.StartsWith("-")) cs = " " + cs;
        return cs;
    }

    /// <summary>Valuta un'espressione aritmetica semplice a runtime (macro compiler).</summary>
    private static object? EvaluateSimpleExpression(string expr)
    {
        expr = expr.Trim();
        // Numeric literal
        if (double.TryParse(expr, System.Globalization.NumberStyles.Any,
            System.Globalization.CultureInfo.InvariantCulture, out var d))
            return d;
        // String literal
        if ((expr.StartsWith("'") && expr.EndsWith("'")) || (expr.StartsWith("\"") && expr.EndsWith("\"")))
            return expr[1..^1];
        // Boolean
        if (expr.Equals("TRUE", StringComparison.OrdinalIgnoreCase)) return true;
        if (expr.Equals("FALSE", StringComparison.OrdinalIgnoreCase)) return false;
        if (expr.Equals("NIL", StringComparison.OrdinalIgnoreCase)) return null;
        // Simple arithmetic: a+b, a-b, a*b, a/b
        foreach (var op in new[] { '+', '-', '*', '/' })
        {
            var idx = expr.IndexOf(op);
            if (idx > 0)
            {
                var left = expr[..idx].Trim();
                var right = expr[(idx + 1)..].Trim();
                var lv = EvaluateSimpleExpression(left);
                var rv = EvaluateSimpleExpression(right);
                if (lv is double ld && rv is double rd)
                    return op switch { '+' => ld + rd, '-' => ld - rd, '*' => ld * rd, '/' => rd != 0 ? ld / rd : 0, _ => 0.0 };
                if (lv is string || rv is string)
                    return (lv?.ToString() ?? "") + (rv?.ToString() ?? "");
            }
        }
        return expr; // stringa non riconosciuta
    }

    private string? PopString(PCodeTask task) { return task.PopRef()?.ToString(); }
    private int? PopOptInt(PCodeTask task) { if (task.Sp <= 0) return null; return (int)task.PopNum(); }

    /// <summary>Pop di un valore numerico opzionale che distingue PUSH_NIL (null) da 0:
    /// legge il valore RAW (PopRef) e restituisce null se è NIL. Necessario per gli
    /// argomenti grafici opzionali (CIRCLE start/end/aspect, PAINT fill/border, PSET/PRESET
    /// color): PopNum su PUSH_NIL dava 0 → es. CIRCLE senza Start/End disegnava un solo
    /// punto (loop da 0 a 0).</summary>
    private double? PopOptDouble(PCodeTask task)
    {
        if (task.Sp <= 0) return null;
        var v = task.PopRef();
        return v == null ? null : SafeDouble(v);
    }

    /// <summary>Pop di un int opzionale che distingue PUSH_NIL (null) da 0 — vedi PopOptDouble.</summary>
    private int? PopOptCoordValue(PCodeTask task)
    {
        if (task.Sp <= 0) return null;
        var v = task.PopRef();
        return v == null ? null : (int)Math.Round(SafeDouble(v), MidpointRounding.AwayFromZero);
    }
    private int? PopOptIntValue(PCodeTask task)
    {
        if (task.Sp <= 0) return null;
        var v = task.PopRef();
        return v == null ? null : (int)SafeDouble(v);
    }

    private static readonly object ParkedMatrixResult = new();

    private object? ExecuteMatrixOp(PCodeTask task, byte subCode, object?[] args)
    {
        return ExecuteLinAlgMatrixCommand(task, (MatrixSubOpCode)subCode, args);
    }

    private object? ExecuteVectorOp(byte subCode, PCodeTask task, int argCount)
    {
        var args = new object?[argCount];
        for (var i = argCount - 1; i >= 0; i--)
            args[i] = task.Sp > 0 ? task.PopRef() : null;
        return ExecuteLinAlgVectorCommand((VectorSubOpCode)subCode, args);
    }

    private static readonly object ParkedNetResult = new();

    private static int NetworkTimeoutMs(object? value)
    {
        if (value == null) return 0;
        var timeout = Convert.ToInt64(value, CultureInfo.InvariantCulture);
        return timeout <= 0 ? 0 : (int)Math.Min(timeout, int.MaxValue);
    }

}
