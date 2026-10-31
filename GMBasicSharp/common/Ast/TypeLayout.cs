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
/// struct C o dell'oggetto CLR: VM/C e C99 salvano ogni numerico in un double (8 byte) e IL
/// mappa INTEGER su Int16 — SIZEOF risponde allo stesso modo su tutti e cinque i backend.
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
                return UdtByteSize(ResolveNested(udt, resolve), visited, resolve);
            default:
                return ReferenceFieldSize;
        }
    }

    /// <summary>
    /// Dimensione in byte del layout di uno UDT: packed = somma dei campi, altrimenti
    /// allineamento naturale di ogni campo e taglia finale multipla del massimo allineamento.
    /// </summary>
    public static long UdtByteSize(UserDefinedType udt, Func<UserDefinedType, UserDefinedType?>? resolve = null)
        => UdtByteSize(udt, new HashSet<int>(), resolve);

    public static long UdtByteSize(UserDefinedType udt, HashSet<int> visited, Func<UserDefinedType, UserDefinedType?>? resolve = null)
    {
        udt = ResolveNested(udt, resolve);
        if (udt.Fields.IsDefaultOrEmpty || !visited.Add(udt.SymbolId)) return 0;
        long offset = 0, maxAlign = 1;
        foreach (var field in udt.Fields)
        {
            // Campo array in un TYPE: gli elementi vivono sull'heap, il campo è un riferimento.
            var size = field.Dimensions.IsDefaultOrEmpty || field.Dimensions.Length == 0
                ? FieldByteSize(field.DataType, visited, resolve)
                : ReferenceFieldSize;
            if (udt.IsPacked)
            {
                offset += size;
                continue;
            }
            // Un campo da 0 byte (TYPE vuoto annidato) non è un allineamento: si conta 1,
            // altrimenti la divisione sotto è per zero.
            if (size < 1) size = 1;
            offset = (offset + size - 1) / size * size + size;
            if (size > maxAlign) maxAlign = size;
        }
        return udt.IsPacked ? offset : (offset + maxAlign - 1) / maxAlign * maxAlign;
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
