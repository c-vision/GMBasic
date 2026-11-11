// FILE: Ast.TypeLayout.cs

namespace Basic;

/// <summary>
/// SORGENTE UNICA del canone "dimensione in byte di un tipo", usata da SYSTEM.SIZEOF e dal
/// layout degli UDT (PACKED TYPE). La condividono i backend gestiti: IL
/// (IlProgramCompiler.EmitSizeOfCall) (emissione della costante) e il compilatore
/// bytecode (fold di SIZEOF + chiave "Size" del descrittore di classe, quindi PCodeVM e VM/C
/// leggono lo STESSO numero calcolato qui).
///
/// CANONE (vedi AiWiki Guide/utente/System-Guida-Utente.md, sezione SIZEOF):
///  - BIT/BYTE/SBYTE = 1, INTEGER/UINT = 2, SINGLE = 4,
///    LONG/QUAD/ULONG/UQUAD/DOUBLE/DATETIME/TIMESPAN = 8, CURRENCY = 16;
///    DATE/TIME valgono 8 SOLO come TIPO DICHIARATO (<c>DIM v AS DATE</c>): come nudi
///    identificatori sono anche le funzioni a zero argomenti <c>DATE()</c>/<c>TIME()</c> e
///    cadono nella regola 5 → -1 (vedi <see cref="TrySizeOfTypeName"/>);
///  - STRING * n (lunghezza fissa) = n + 1 (terminatore, come i buffer C di record/FFI);
///  - UDT: PACKED TYPE = somma dei campi senza padding; TYPE ordinario = ogni campo parte
///    all'offset multiplo della propria dimensione e la struct chiude a multiplo del massimo
///    allineamento; UDT annidata = layout ricorsivo col PROPRIO flag packed;
///  - campo array (DIM ..() nel TYPE) = 8 (riferimento: gli elementi non sono inline);
///  - argomento di SIZEOF senza layout statico (stringa dinamica, array, collezione, CLASS,
///    oggetto) = -1: è il modo canonico di dire "non è una dimensione in byte nota".
///
/// ARGOMENTO di SIZEOF (canone di risoluzione, identico su tutte e cinque le gambe):
///  1. variabile DICHIARATA (DIM/parametro/campo) → dimensione del suo tipo statico;
///  2. nome di TYPE → layout dello UDT; nome di tipo scalare (keyword del lessico, es.
///     SIZEOF(INTEGER) = 2) → sua larghezza, <c>STRING</c> senza lunghezza = -1
///     (tabella in <see cref="TrySizeOfTypeName"/>, replicata solo in C99Native);
///  3. letterale numerico = 8 (il double di default del linguaggio);
///  4. variabile IMPLICITA (senza DIM) = il tipo che il backend le riconosce: stringa dinamica
///     o collezione → -1, altrimenti DOUBLE = 8. I suffissi <c>% ! #</c> NON cambiano il tipo
///     statico (resta il double di default), il <c>$</c> sì perché dichiara una stringa;
///  5. QUALSIASI ALTRA FORMA (espressione aritmetica, chiamata di funzione, accesso a membro o
///     indice, letterale di stringa o booleano) = -1: nessun backend ha un tipo statico da
///     misurare — l'inferimento sul flusso dati esiste solo nelle gambe gestite, quindi è
///     deliberatamente escluso dal canone. Ricade qui anche un identificatore il cui token è una
///     KEYWORD che non è nome di tipo né TYPE utente (<c>DATE</c>, <c>TIME</c>, <c>EMPTY</c>, …
///     = funzioni built-in a zero argomenti): il test è sul TokenKind, mai sul lexeme
///     (<see cref="IsBuiltinFunctionKeyword"/>).
///
/// Il canone è la dimensione LOGICA (byte del record/scambio FFI), NON la dimensione della
/// struct C o dell'oggetto CLR: le gambe C salvano ogni numerico in un double (8 byte) e il
/// backend managed mappa INTEGER su Int64 e SINGLE scalare su double — la tabella logica
/// (INTEGER = 2, SINGLE = 4) resta la STESSA su tutti e cinque i backend, perché descrive il
/// layout di record/FFI, non lo storage del valore. Vedi la nota AiWiki
/// `Knowledge/SizeOf-Forma-e-Folding.md`.
/// C99Native (compilatore C puro, deliberatamente indipendente da C# e dal runtime) ha la
/// stessa tabella replicata in native/gmb_compiler_c (gmb_cls_field_bytes): è l'unico posto
/// dove la duplicazione è ammessa, perché quel mondo non può chiamare il C#.
/// </summary>
public static class TypeLayout
{
    /// <summary>Occupazione di un campo a layout dinamico (riferimento) dentro una struct.</summary>
    public const long ReferenceFieldSize = 8;

    /// <summary>Valore di SIZEOF quando il tipo non ha una dimensione statica in byte.</summary>
    public const long NoStaticSize = -1;

    public static DataType Normalize(DataType type) => type is InferredType ? new DoubleType() : type;

    /// <summary>Dimensione in byte di uno scalare del linguaggio, 0 se non è uno scalare.</summary>
    public static long ScalarByteSize(DataType type) => Normalize(type) switch
    {
        BitType or ByteType or SByteType => 1,
        IntegerType or UIntegerType => 2,
        SingleType => 4,
        LongType or QuadType or ULongType or UQuadType or DoubleType
            or DateType or TimeType or DateTimeType or TimeSpanType => 8,
        CurrencyType => 16,
        _ => 0,
    };

    /// <summary>
    /// Dimensione che il tipo occupa come CAMPO di una struct: scalare, STRING*n = n+1,
    /// UDT annidata = proprio layout, tutto il resto = riferimento da 8 byte.
    /// <paramref name="resolve"/> (opzionale) mappa un riferimento di tipo ANNIDATO il cui
    /// <see cref="UserDefinedType.Fields"/> NON è popolato (le gambe C, es. vedono il
    /// campo come un semplice nome) verso la dichiarazione COMPLETA registrata, così il layout del
    /// campo annidato è identico a quello della gamba gestita. Con resolve null (IL/PCodeVM/VM-C,
    /// il cui grafo è già linkato) il comportamento è invariato.
    /// </summary>
    public static long FieldByteSize(DataType type, Func<UserDefinedType, UserDefinedType?>? resolve = null)
        => FieldByteSize(type, new HashSet<int>(), resolve);

    public static long FieldByteSize(DataType type, HashSet<int> visited, Func<UserDefinedType, UserDefinedType?>? resolve = null)
    {
        var scalar = ScalarByteSize(type);
        if (scalar > 0) return scalar;
        switch (Normalize(type))
        {
            case StringType { FixedLength: > 0 } fixedStr:
                return fixedStr.FixedLength!.Value + 1;
            case UserDefinedType udt:
                return ComputeLayout(ResolveNested(udt, resolve), visited, resolve).Size;
            default:
                return ReferenceFieldSize;
        }
    }

    /// <summary>
    /// Dimensione in byte del layout di uno UDT: packed = somma dei campi, altrimenti
    /// allineamento naturale di ogni campo e taglia finale multipla del massimo allineamento.
    /// UNION (2026-11-11): tutti i campi partono da 0 → taglia = massimo dei campi, allineamento
    /// = massimo. BITFIELD (`... WIDTH n`): i bit si impacchettano LSB-first nell'unità di
    /// storage del tipo dichiarato (regole C: nuova unità quando il campo non ci sta, il campo
    /// successivo riparte dopo l'unità intera).
    /// </summary>
    public static long UdtByteSize(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve = null)
        => ComputeLayout(udt, new HashSet<int>(), resolve).Size;

    /// <summary>Posizione di un campo nel layout (offset in byte + posizione dei bit per i
    /// bitfield). <see cref="BitWidth"/>=0 → campo normale.</summary>
    public readonly record struct FieldPlacement(int Index, long Offset, int BitOffset, int BitWidth);

    /// <summary>Layout completo di un UDT: taglia totale + posizione di ogni campo.</summary>
    public readonly record struct UdtLayout(long Size, FieldPlacement[] Fields);

    /// <summary>Calcola il layout C di un UDT (unione, bitfield, packed, allineamento
    /// naturale). SORGENTE UNICA: la usano SIZEOF, il descrittore di classe (PCodeVM/VM-C),
    /// il backend IL e l'emitter C99.</summary>
    public static UdtLayout ComputeLayout(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve = null)
        => ComputeLayout(udt, new HashSet<int>(), resolve);

    public static UdtLayout ComputeLayout(UserDefinedType udt, HashSet<int> visited, Func<UserDefinedType, UserDefinedType?>? resolve = null)
    {
        udt = ResolveNested(udt, resolve);
        var fields = udt.Fields.IsDefaultOrEmpty ? Array.Empty<TypeField>() : udt.Fields.ToArray();
        var placements = new FieldPlacement[fields.Length];
        if (fields.Length == 0 || !visited.Add(udt.SymbolId))
            return new UdtLayout(0, placements);

        // UNION: tutti i campi a offset 0; taglia = massimo, allineamento = massimo.
        if (udt.IsUnion)
        {
            long size = 0, maxAlignU = 1;
            for (var i = 0; i < fields.Length; i++)
            {
                var f = fields[i];
                var isBit = f.BitWidth > 0;
                var sz = isBit
                    ? Math.Max(1, ScalarByteSize(f.DataType))
                    : (f.Dimensions.IsDefaultOrEmpty || f.Dimensions.Length == 0
                        ? FieldByteSize(f.DataType, visited, resolve)
                        : ReferenceFieldSize);
                if (sz < 1) sz = 1;
                placements[i] = new FieldPlacement(i, 0, 0, f.BitWidth);
                if (sz > size) size = sz;
                if (sz > maxAlignU) maxAlignU = sz;
            }
            return new UdtLayout(Math.Max(size, (size + maxAlignU - 1) / maxAlignU * maxAlignU), placements);
        }

        long offset = 0, maxAlign = 1;
        long unitStart = -1;      // inizio dell'unità di storage del run di bitfield corrente
        long unitBytes = 0;       // dimensione dell'unità (byte)
        long usedBits = 0;        // bit già occupati nell'unità
        for (var i = 0; i < fields.Length; i++)
        {
            var f = fields[i];
            if (f.BitWidth > 0)
            {
                var unit = ScalarByteSize(f.DataType);
                if (unit < 1) unit = 1;
                var unitBits = unit * 8;
                // Nuova unità: primo bitfield, cambio di unità (tipo diverso), packed, o il
                // campo non ci sta nei bit rimasti.
                if (unitStart < 0 || unit != unitBytes || usedBits + f.BitWidth > unitBits)
                {
                    if (!udt.IsPacked) offset = (offset + unit - 1) / unit * unit;
                    unitStart = offset;
                    unitBytes = unit;
                    usedBits = 0;
                    offset += unit;
                    if (unit > maxAlign) maxAlign = unit;
                }
                placements[i] = new FieldPlacement(i, unitStart, (int)usedBits, f.BitWidth);
                usedBits += f.BitWidth;
                continue;
            }

            unitStart = -1; unitBytes = 0; usedBits = 0;
            // Campo array in un TYPE: gli elementi vivono sull'heap, il campo è un riferimento.
            var size = f.Dimensions.IsDefaultOrEmpty || f.Dimensions.Length == 0
                ? FieldByteSize(f.DataType, visited, resolve)
                : ReferenceFieldSize;
            if (size < 1) size = 1;
            if (!udt.IsPacked) offset = (offset + size - 1) / size * size;
            placements[i] = new FieldPlacement(i, offset, 0, 0);
            offset += size;
            if (size > maxAlign) maxAlign = size;
        }
        return new UdtLayout(udt.IsPacked ? offset : (offset + maxAlign - 1) / maxAlign * maxAlign, placements);
    }

    /// <summary>Codice tipo FFI di un campo scalare (0=double 1=int64 2=int8 3=uint8 4=int16
    /// 5=uint16 6=int32 7=uint32 8=float32 9=string 10=uint64), -1 se non è scalare.
    /// ATTENZIONE: è il mapping del CONFINE FFI/C (dove LONG = int32). Per il layout del
    /// LINGUAGGIO (SIZEOF, UNION/bitfield) si usa <see cref="RuntimeCode"/>.</summary>
    public static int FfiCode(DataType t) => Normalize(t) switch
    {
        DoubleType => 0, SingleType => 8, SByteType => 2, ByteType => 3,
        IntegerType => 4, UIntegerType => 5, LongType => 6, ULongType => 7,
        QuadType => 1, UQuadType => 10, StringType => 9,
        _ => -1,
    };

    /// <summary>Codice tipo per il layout del LINGUAGGIO (UNION/bitfield): stesse larghezze di
    /// <see cref="ScalarByteSize"/> — LONG/QUAD/ULONG/UQUAD = intero a 8 byte, INTEGER = 2,
    /// BYTE = 1, SINGLE = 4, DOUBLE = double. (Il confine FFI ha un mapping diverso: LONG =
    /// int32 — vedi <see cref="FfiCode"/>.)</summary>
    public static int RuntimeCode(DataType t) => Normalize(t) switch
    {
        DoubleType => 0, SingleType => 8, SByteType => 2, ByteType => 3,
        IntegerType => 4, UIntegerType => 5, LongType or QuadType => 1, ULongType or UQuadType => 10,
        _ => -1,
    };

    /// <summary>Byte occupati da un codice tipo FFI.</summary>
    public static int FfiCodeBytes(int code) => code switch
    {
        0 or 1 or 9 or 10 => 8, 6 or 7 or 8 => 4, 4 or 5 => 2, _ => 1,
    };

    /// <summary>Gruppi di ALIASING per il runtime (PCodeVM/VM-C): un gruppo è un blocco di byte
    /// condiviso da più campi.
    /// <list type="bullet">
    /// <item>UNION: un gruppo con TUTTI i campi a offset 0 (taglia = taglia dell'unione);</item>
    /// <item>bitfield (`WIDTH n`): un gruppo per ogni unità di storage (taglia = byte del tipo
    /// dichiarato), con i campi a (offset unità, bit offset, larghezza).</item>
    /// </list>
    /// Formato di ogni gruppo (lista di double, serializzabile nei chunk senza chiavi):
    /// <c>[size, n, (symId, byteOff, bitOff, bitWidth, typeCode) × n]</c>.</summary>
    public static List<double[]> AliasGroups(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve = null)
    {
        var groups = new List<double[]>();
        var layout = ComputeLayout(udt, resolve);
        var fields = udt.Fields.IsDefaultOrEmpty ? Array.Empty<TypeField>() : udt.Fields.ToArray();
        if (fields.Length == 0) return groups;

        if (udt.IsUnion)
        {
            var g = new List<double> { layout.Size, fields.Length };
            foreach (var f in fields)
            {
                var code = RuntimeCode(f.DataType);
                if (code < 0) continue;
                g.Add(f.SymbolId); g.Add(0); g.Add(0); g.Add(f.BitWidth); g.Add(code);
            }
            if (g.Count > 2) groups.Add(g.ToArray());
            return groups;
        }

        // Bitfield: raggruppa i campi che condividono la STESSA unità di storage.
        var byUnit = new Dictionary<(long Off, int Unit), List<(int Sym, int BitOff, int Width, int Code)>>();
        var order = new List<(long Off, int Unit)>();
        for (var i = 0; i < fields.Length && i < layout.Fields.Length; i++)
        {
            var f = fields[i];
            if (f.BitWidth <= 0) continue;
            var code = RuntimeCode(f.DataType);
            if (code < 0) continue;
            var p = layout.Fields[i];
            var key = (p.Offset, FfiCodeBytes(code));
            if (!byUnit.TryGetValue(key, out var list)) { list = new(); byUnit[key] = list; order.Add(key); }
            list.Add((f.SymbolId, p.BitOffset, p.BitWidth, code));
        }
        foreach (var key in order)
        {
            var list = byUnit[key];
            var g = new List<double> { key.Unit, list.Count };
            foreach (var m in list) { g.Add(m.Sym); g.Add(key.Off); g.Add(m.BitOff); g.Add(m.Width); g.Add(m.Code); }
            groups.Add(g.ToArray());
        }
        return groups;
    }

    /// <summary>Se <paramref name="resolve"/> è fornito e il tipo annidato ha i campi vuoti
    /// (riferimento per solo nome, tipico delle gambe C), ritorna la dichiarazione completa;
    /// altrimenti il tipo così com'è (resolve null o già popolato).</summary>
    static UserDefinedType ResolveNested(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve)
    {
        if (resolve is null || !udt.Fields.IsDefaultOrEmpty) return udt;
        return resolve(udt) ?? udt;
    }

    /// <summary>
    /// Valore di SIZEOF per un tipo: dimensione statica in byte, o -1 quando statica non c'è
    /// (stringa dinamica, array, collezione, CLASS, oggetto).
    /// </summary>
    public static long ByteSize(DataType type, Func<UserDefinedType, UserDefinedType?>? resolve = null)
    {
        var scalar = ScalarByteSize(type);
        if (scalar > 0) return scalar;
        return Normalize(type) switch
        {
            StringType { FixedLength: > 0 } fixedStr => fixedStr.FixedLength!.Value + 1,
            UserDefinedType udt => UdtByteSize(udt, resolve),
            _ => NoStaticSize,
        };
    }

    /// <summary>
    /// Token del campo nel RECORD di un file RANDOM (PUT/GET #n, rec, udt), o null se il tipo non
    /// è ammesso nel record. Layout packed little-endian, stesse larghezze di <see cref="FieldByteSize"/>:
    /// <c>t</c> BIT (1), <c>y</c> BYTE (1), <c>Y</c> SBYTE (1), <c>i</c> INTEGER (2), <c>I</c> UINT (2),
    /// <c>f</c> SINGLE (4), <c>l</c> LONG/QUAD/ULONG/UQUAD (8, int64), <c>d</c> DOUBLE (8),
    /// <c>c</c> CURRENCY (16: int64 valore*10000 + 8 byte a zero), <c>sN</c> STRING * N (N byte
    /// riempiti con spazi + 1 terminatore NUL). Stesso codice nel runtime C (gmb_file.c,
    /// gmb_file_put_record) e in <c>CommonLib.RandomRecordCodec</c>.
    /// </summary>
    public static string? RandomRecordToken(DataType type) => Normalize(type) switch
    {
        BitType => "t",
        ByteType => "y",
        SByteType => "Y",
        IntegerType => "i",
        UIntegerType => "I",
        SingleType => "f",
        LongType or QuadType or ULongType or UQuadType => "l",
        DoubleType => "d",
        CurrencyType => "c",
        StringType { FixedLength: > 0 } fixedStr => "s" + fixedStr.FixedLength!.Value,
        _ => null,
    };

    /// <summary>
    /// Descrittore RANDOM di uno UDT per i backend gestiti: <c>nome:token;nome:token;…</c> in ordine di
    /// dichiarazione (i nomi servono a PCode/VM-C, dove l'istanza è un dizionario per nome).
    /// False se un campo non è ammesso nel record (array, STRING dinamica, date, UDT annidata, …):
    /// in tal caso PUT/GET restano sul percorso legacy invariato.
    /// </summary>
    public static bool TryRandomRecordDescriptor(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve, out string descriptor)
    {
        descriptor = "";
        udt = ResolveNested(udt, resolve);
        if (udt.Fields.IsDefaultOrEmpty) return false;
        var sb = new System.Text.StringBuilder();
        foreach (var field in udt.Fields)
        {
            if (!(field.Dimensions.IsDefaultOrEmpty || field.Dimensions.Length == 0)) return false;
            var token = RandomRecordToken(field.DataType);
            if (token is null) return false;
            if (sb.Length > 0) sb.Append(';');
            sb.Append(field.Name.Lexeme).Append(':').Append(token);
        }
        descriptor = sb.ToString();
        return true;
    }

    /// <summary>
    /// SIZEOF di un NOME DI TIPO scalare scritto come identificatore (<c>SIZEOF(INTEGER)</c>,
    /// <c>SIZEOF(CURRENCY)</c>): le GRAFIE sono esattamente le keyword del lessico C#
    /// (<c>Lexer.Keywords.cs</c>) — ciò che NON è keyword non è un nome di tipo e il chiamante
    /// decide (nome di TYPE → <see cref="UdtByteSize"/>, altrimenti nessun tipo statico → -1).
    /// <c>STRING</c> senza <c>* n</c> è una parola del linguaggio senza layout statico: -1.
    /// DATE e TIME sono ESCLUSI: sono anche le funzioni a zero argomenti <c>DATE()</c>/
    /// <c>TIME()</c>, quindi il parser gestito le risolve come chiamata → -1 (regola 5 del
    /// canone); come tipo dichiarato (<c>DIM v AS DATE</c>) valgono 8 su tutte le gambe.
    /// Ritorna false se il nome non è un tipo scalare. C99Native replica la STESSA tabella in
    /// <c>gmb_sizeof_type_name</c> (unica duplicazione ammessa, §Canone).
    /// </summary>
    public static bool TrySizeOfTypeName(string name, out long bytes)
    {
        bytes = name?.ToUpperInvariant() switch
        {
            "BIT" or "BOOL" or "BOOLEAN" or "BYTE" or "SBYTE" => 1,
            "INTEGER" or "SHORT" or "INT" or "UINTEGER" or "USHORT" or "UINT" => 2,
            "SINGLE" => 4,
            "LONG" or "ULONG" or "QUAD" or "UQUAD" or "DOUBLE"
                or "DATETIME" or "TIMESPAN" => 8,
            "CURRENCY" => 16,
            "STRING" => NoStaticSize,
            _ => 0,
        };
        return bytes != 0;
    }

    /// <summary>
    /// True se il token dell'identificatore argomento di SIZEOF è una KEYWORD: non è un nome di
    /// tipo scalare (già risolto da <see cref="TrySizeOfTypeName"/>) e non è un TYPE utente (già
    /// risolto dal chiamante), quindi è una FUNZIONE built-in a zero argomenti — <c>DATE()</c>,
    /// <c>TIME()</c>, <c>EMPTY()</c>, … — e l'argomento non ha tipo statico: -1 (regola 5 del
    /// canone). Il test è sulla CHIAVE canonica (TokenKind), mai sul lexeme: una variabile reale
    /// ha sempre <see cref="TokenKind.Identifier"/>, e un TYPE utente che collide con una keyword
    /// (<c>TYPE Empty</c>) arriva qui già risolto dal chiamante. C99Native applica lo stesso test
    /// sul suo TokenKind in <c>gmb_sizeof_arg</c>.
    /// </summary>
    public static bool IsBuiltinFunctionKeyword(TokenKind kind)
        => kind != TokenKind.Identifier && kind != TokenKind.Unknown;
}
