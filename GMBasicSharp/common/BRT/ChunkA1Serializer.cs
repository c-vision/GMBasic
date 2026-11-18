// FILE: Basic.Runtime.ChunkA1Serializer.cs
// Serializzatore del CompiledChunk nel formato A1 (magic "GMBC" + version +
// big-endian, vedi native/gmb_vm_c/gmb_chunk.c).
// Utilizzato per ispezionare/trasferire un chunk P-Code (formato A1/GMBC).

using System.Text;
using Compiler;

namespace Basic.Runtime;

public static class ChunkA1Serializer
{
    public const uint GMB_CONST_NIL = 0;
    public const uint GMB_CONST_DOUBLE = 1;
    public const uint GMB_CONST_STRING = 2;
    public const uint GMB_CONST_DICT = 3;
    public const uint GMB_CONST_LIST = 4;
    public const uint GMB_CONST_INT = 5;
    public const uint GMB_CONST_DATATYPE = 6;

    public static byte[] Serialize(CompiledChunk chunk, bool loadLines = true)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms);

        // MAGIC "GMBC" + VERSION (2 = descrittore FFI per argomento, 2026-11-06)
        w.Write((byte)'G'); w.Write((byte)'M'); w.Write((byte)'B'); w.Write((byte)'C');
        w.Write((byte)2);

        // HEAD: entry_point, global_count, data_count, code_len, consts_len
        WriteU32(w, (uint)chunk.EntryPoint);
        WriteU32(w, (uint)chunk.GlobalCount);
        WriteU32(w, (uint)chunk.Data.Count);
        WriteU32(w, (uint)chunk.Code.Count);
        WriteU32(w, (uint)chunk.Constants.Count);

        // CODE
        foreach (var b in chunk.Code)
            w.Write(b);

        // LINES (u32 per byte di codice)
        for (int i = 0; i < chunk.Code.Count; i++)
            WriteU32(w, (uint)(i < chunk.Lines.Count ? chunk.Lines[i] : 0));

        // CONSTANTS
        foreach (var c in chunk.Constants)
            WriteValue(w, c);

        // DATA
        foreach (var d in chunk.Data)
            WriteValue(w, d);

        // PROCS: u32 count → (string name, u32 addr)
        WriteU32(w, (uint)chunk.ProcedureAddresses.Count);
        foreach (var (name, addr) in chunk.ProcedureAddresses)
        {
            WriteString(w, name);
            WriteU32(w, (uint)addr);
        }

        // ARITYS: u32 count → (u32 addr, u32 arity)
        WriteU32(w, (uint)chunk.ProcedureArities.Count);
        foreach (var (addr, arity) in chunk.ProcedureArities)
        {
            WriteU32(w, (uint)addr);
            WriteU32(w, (uint)arity);
        }

        // GLOBAL NAMES (coda, backward-compat: i loader vecchi ignorano i byte extra)
        var names = chunk.GlobalNames ?? [];
        WriteU32(w, (uint)names.Length);
        foreach (var n in names)
            WriteString(w, n ?? "");

        // LOCAL COUNT (coda opzionale, ignorata dai loader legacy e dalla VM/C).
        WriteU32(w, (uint)chunk.LocalCount);

        WriteU32(w, (uint)chunk.TryRegions.Count);
        foreach (var (tryIp, region) in chunk.TryRegions)
        {
            WriteU32(w, (uint)tryIp);
            WriteU32(w, (uint)region.FinallyIp);
            WriteU32(w, (uint)region.EndIp);
            WriteU32(w, region.HasCatch ? 1u : 0u);
        }

        // PROC LOCAL NAMES (coda opzionale S_eval, ignorata dai loader legacy:
        // C gmb_chunk_load tollera la coda corta, C# Deserialize ha catch EndOfStream).
        WriteU32(w, (uint)chunk.ProcedureLocalNames.Count);
        foreach (var (addr, locNames) in chunk.ProcedureLocalNames)
        {
            WriteU32(w, (uint)addr);
            WriteU32(w, (uint)locNames.Length);
            foreach (var n in locNames)
                WriteString(w, n ?? "");
        }

        // STATEMENT RANGES (coda FINALE, opzionale: i loader vecchi ignorano i byte extra):
        // intervalli [start,end) delle istruzioni per RESUME NEXT a granularità corretta.
        WriteU32(w, (uint)chunk.StatementRanges.Count);
        foreach (var (st, en) in chunk.StatementRanges)
        {
            WriteU32(w, (uint)st);
            WriteU32(w, (uint)en);
        }


        // LOCAL NUMERIC (coda FINALE opzionale): per procedura, quali slot locali sono numerici
        // (default 0). u32 count → per voce: u32 addr, u32 count, count byte (0/1).
        WriteU32(w, (uint)chunk.ProcedureLocalNumeric.Count);
        foreach (var (addr, flags) in chunk.ProcedureLocalNumeric)
        {
            WriteU32(w, (uint)addr);
            WriteU32(w, (uint)flags.Length);
            foreach (var f in flags) w.Write(f ? (byte)1 : (byte)0);
        }

        // USES TRAY (coda FINALE opzionale): 1 byte 0/1 — il programma usa il namespace TRAY.
        w.Write(chunk.UsesTray ? (byte)1 : (byte)0);

        // PROC PARAM DEFAULTS (coda FINALE opzionale): per procedura, i default LETTERALI dei
        // parametri OPTIONAL, così la VM/C può completare gli argomenti omessi da una macro
        // (il chunk non porta l'AST). u32 count → per voce: u32 addr, u32 nparams,
        // poi per parametro u8 kind (0 = nessun default, 1 = numero, 2 = stringa).
        WriteU32(w, (uint)chunk.ProcedureParamDefaults.Count);
        foreach (var (addr, defs) in chunk.ProcedureParamDefaults)
        {
            WriteU32(w, (uint)addr);
            WriteU32(w, (uint)defs.Length);
            foreach (var d in defs)
            {
                switch (d)
                {
                    case double dn: w.Write((byte)1); WriteF64(w, dn); break;
                    case string ds: w.Write((byte)2); WriteString(w, ds); break;
                    default: w.Write((byte)0); break;
                }
            }
        }

        // SOURCE TEXTS (coda FINALE opzionale): testo delle righe sorgente per
        // ERR.SourceLineText. u32 count → count stringhe (indice = riga 0-based).
        {
            var texts = chunk.SourceTexts;
            WriteU32(w, (uint)texts.Length);
            foreach (var t in texts) WriteString(w, t ?? string.Empty);
        }

        return ms.ToArray();
    }

    /// <summary>Deserializza un chunk A1/GMBC (magic "GMBC" + version 1, big-endian) nel
    /// CompiledChunk C#. Specchio di <see cref="Serialize"/>. Serve al consumatore dei
    /// vm-chunk di pacchetto (ChunkMerger) e al managed PCodeVm per eseguire .chunk/.prg.</summary>
    public static CompiledChunk Deserialize(byte[] bytes)
    {
        using var ms = new MemoryStream(bytes);
        using var r = new BinaryReader(ms);

        for (var i = 0; i < 4; i++)
            if (r.ReadByte() != (byte)"GMBC"[i])
                throw new InvalidDataException("Magic A1/GMBC mancante nel chunk.");
        var version = r.ReadByte();
        // 1 = formato storico (maschere FFI): leggibile SOLO se il chunk non usa FFI
        //     (PCodeVM rifiuta la chiamata con un errore esplicito); 2 = descrittore per argomento.
        if (version != 1 && version != 2)
            throw new InvalidDataException($"Versione chunk A1/GMBC non supportata: {version}.");

        var chunk = new CompiledChunk
        {
            FormatVersion = version,
            EntryPoint = unchecked((int)ReadU32(r)),
            GlobalCount = unchecked((int)ReadU32(r)),
        };
        var dataCount = ReadU32(r);
        var codeLen = ReadU32(r);
        var constsLen = ReadU32(r);

        for (var i = 0; i < codeLen; i++) chunk.Code.Add(r.ReadByte());
        for (var i = 0; i < codeLen; i++) chunk.Lines.Add(unchecked((int)ReadU32(r)));
        for (var i = 0; i < constsLen; i++) chunk.Constants.Add(ReadValue(r));
        for (var i = 0; i < dataCount; i++) chunk.Data.Add(ReadValue(r) ?? "");

        var procCount = ReadU32(r);
        for (var i = 0; i < procCount; i++)
        {
            var name = ReadString(r);
            var addr = unchecked((int)ReadU32(r));
            chunk.ProcedureAddresses[name] = addr;
        }

        var arityCount = ReadU32(r);
        for (var i = 0; i < arityCount; i++)
        {
            var addr = unchecked((int)ReadU32(r));
            var arity = unchecked((int)ReadU32(r));
            chunk.ProcedureArities[addr] = arity;
        }

        // GLOBAL NAMES (coda; se il file è più corto perché scritto da un loader vecchio,
        // il reader lancia EndOfStream → ignorato).
        try
        {
            var namesCount = ReadU32(r);
            var names = new string[namesCount];
            for (var i = 0; i < namesCount; i++) names[i] = ReadString(r);
            chunk.GlobalNames = names;
            chunk.LocalCount = unchecked((int)ReadU32(r));
            var tryRegionCount = ReadU32(r);
            for (var i = 0; i < tryRegionCount; i++)
            {
                var tryIp = unchecked((int)ReadU32(r));
                var finallyIp = unchecked((int)ReadU32(r));
                var endIp = unchecked((int)ReadU32(r));
                var hasCatch = ReadU32(r) != 0;
                chunk.TryRegions[tryIp] = new TryRegionMetadata(finallyIp, endIp, hasCatch);
            }
            // PROC LOCAL NAMES (coda opzionale S_eval; "" = slot anonimo → null).
            var plocCount = ReadU32(r);
            for (var i = 0; i < plocCount; i++)
            {
                var addr = unchecked((int)ReadU32(r));
                var nameCount = ReadU32(r);
                var locNames = new string?[nameCount];
                for (var j = 0; j < nameCount; j++)
                {
                    var s = ReadString(r);
                    locNames[j] = string.IsNullOrEmpty(s) ? null : s;
                }
                chunk.ProcedureLocalNames[addr] = locNames;
            }
            // STATEMENT RANGES (coda FINALE, opzionale: i chunk vecchi finiscono prima).
            var stmtRangeCount = ReadU32(r);
            for (var i = 0; i < stmtRangeCount; i++)
            {
                var st = unchecked((int)ReadU32(r));
                var en = unchecked((int)ReadU32(r));
                chunk.StatementRanges.Add((st, en));
            }
            // LOCAL NUMERIC (coda FINALE, opzionale).
            var plnumCount = ReadU32(r);
            for (var i = 0; i < plnumCount; i++)
            {
                var addr = unchecked((int)ReadU32(r));
                var n = (int)ReadU32(r);
                var flags = new bool[n];
                for (var j = 0; j < n; j++) flags[j] = r.ReadByte() != 0;
                chunk.ProcedureLocalNumeric[addr] = flags;
            }
            // USES TRAY (coda FINALE, opzionale): byte 0/1.
            if (r.BaseStream.Position < r.BaseStream.Length)
                chunk.UsesTray = r.ReadByte() != 0;
            // PROC PARAM DEFAULTS (coda FINALE, opzionale): default letterali dei parametri
            // OPTIONAL per procedura (la VM/C li usa per completare una chiamata da macro).
            if (r.BaseStream.Position < r.BaseStream.Length)
            {
                var pdefCount = ReadU32(r);
                for (var i = 0; i < pdefCount; i++)
                {
                    var addr = unchecked((int)ReadU32(r));
                    var n = (int)ReadU32(r);
                    var defs = new object?[n];
                    for (var j = 0; j < n; j++)
                    {
                        var kind = r.ReadByte();
                        defs[j] = kind switch
                        {
                            1 => (object)ReadF64(r),
                            2 => ReadString(r),
                            _ => null
                        };
                    }
                    chunk.ProcedureParamDefaults[addr] = defs;
                }
            }
            // SOURCE TEXTS (coda FINALE, opzionale): testo delle righe per ERR.SourceLineText.
            if (r.BaseStream.Position < r.BaseStream.Length)
            {
                var stCount = ReadU32(r);
                var texts = new string[stCount];
                for (var i = 0; i < stCount; i++) texts[i] = ReadString(r);
                chunk.SourceTexts = texts;
            }
        }
        catch (EndOfStreamException) { }

        return chunk;
    }

    private static object? ReadValue(BinaryReader r)
    {
        var tag = (uint)r.ReadByte();
        switch (tag)
        {
            case GMB_CONST_NIL: return null;
            case GMB_CONST_DOUBLE: return ReadF64(r);
            case GMB_CONST_INT: return unchecked((int)ReadU32(r));
            case GMB_CONST_STRING: return ReadString(r);
            case GMB_CONST_DICT:
            {
                var dict = new Dictionary<object, object?>();
                var count = ReadU32(r);
                for (var i = 0; i < count; i++)
                {
                    var ktag = r.ReadByte();
                    var key = ktag == 0 ? ReadString(r) : ReadValue(r)?.ToString() ?? "";
                    dict[key] = ReadValue(r);
                }
                return dict;
            }
            case GMB_CONST_LIST:
            {
                var list = new List<object?>();
                var count = ReadU32(r);
                for (var i = 0; i < count; i++)
                {
                    r.ReadByte();       // ktag (0 = chiave stringa)
                    var keyLen = ReadU32(r);
                    if (keyLen > 0) r.ReadBytes((int)keyLen);
                    list.Add(ReadValue(r));
                }
                return list;
            }
            default:
                throw new InvalidDataException($"Tag costante A1/GMBC sconosciuto: {tag}.");
        }
    }

    private static string ReadString(BinaryReader r)
    {
        var len = ReadU32(r);
        if (len > r.BaseStream.Length) throw new InvalidDataException("Stringa A1/GMBC non valida.");
        var bytes = r.ReadBytes((int)len);
        return Encoding.UTF8.GetString(bytes);
    }

    private static uint ReadU32(BinaryReader r)
    {
        var b = r.ReadBytes(4);
        return ((uint)b[0] << 24) | ((uint)b[1] << 16) | ((uint)b[2] << 8) | b[3];
    }

    private static double ReadF64(BinaryReader r)
    {
        var hi = ReadU32(r);
        var lo = ReadU32(r);
        ulong bits = ((ulong)hi << 32) | lo;
        return BitConverter.Int64BitsToDouble(unchecked((long)bits));
    }

    private static void WriteValue(BinaryWriter w, object? value)
    {
        switch (value)
        {
            case null:
                w.Write((byte)GMB_CONST_NIL);
                break;
            case double d:
                w.Write((byte)GMB_CONST_DOUBLE);
                WriteF64(w, d);
                break;
            case float f:
                w.Write((byte)GMB_CONST_DOUBLE);
                WriteF64(w, f);
                break;
            case int i:
                w.Write((byte)GMB_CONST_INT);
                WriteU32(w, unchecked((uint)i));
                break;
            case long l:
                w.Write((byte)GMB_CONST_INT);
                WriteU32(w, unchecked((uint)(int)l));
                break;
            case short s:
                w.Write((byte)GMB_CONST_INT);
                WriteU32(w, unchecked((uint)(int)s));
                break;
            case byte b:
                w.Write((byte)GMB_CONST_INT);
                WriteU32(w, b);
                break;
            case bool bv:
                w.Write((byte)GMB_CONST_INT);
                WriteU32(w, bv ? 1u : 0u);
                break;
            case string s:
                w.Write((byte)GMB_CONST_STRING);
                WriteString(w, s);
                break;
            case StringExpr se:
                // SET FILTER/SET KEY ecc. (Compiler.Speed.cs/Compiler.Isam.cs) emettono la
                // condizione come EmitConstant(stmt.Condition) — l'AST grezzo, non una
                // stringa — per farlo leggere dal runtime C# in-process senza serializzare
                // (stesso StringExpr .NET, vedi PCodeProcessor.Syscalls.cs SPEED_SET_FILTER).
                // Qui invece la VM/C è un PROCESSO SEPARATO: l'oggetto va serializzato, e
                // cadeva nel default sotto (value.ToString()) — StringExpr.ToString() è
                // `"\"{Value}\""" (rappresentazione DEBUG con virgolette, mai pensata per la
                // serializzazione), quindi il costante-stringa nel chunk conteneva le
                // virgolette letterali (`"ID > 1"` invece di `ID > 1`). Il lexer C
                // (gmb_expr.c) lo leggeva come UN SOLO token stringa (`TK_STR`), sempre
                // truthy — il filtro non discriminava mai nessun record (parità col bug
                // ExprPlain.From già risolto sul lato IL, causa diversa stesso sintomo).
                w.Write((byte)GMB_CONST_STRING);
                WriteString(w, se.Value);
                break;
            case Expr ex:
                // Altre espressioni AST (SPEED SELECT/WHERE/JOIN/GROUP BY/ORDER BY,
                // SET RELATION, JOIN FOR, ISAM SET KEY, ...): la VM/C è un processo
                // separato e ri-parsa il testo a runtime. Expr.ToString() include i
                // suffissi `#SymbolId` (es. "Clienti.ID#305 = Ordini.ID_Cliente#412")
                // che il lexer C non sa interpretare → qui si serializza il testo
                // "pulito" con le qualifiche preservate (ExprPlain.FromQualified).
                w.Write((byte)GMB_CONST_STRING);
                WriteString(w, ExprPlain.FromQualified(ex));
                break;
            case UserDefinedType udt:
                // Campo UDT ANNIDATO in FieldTypes (2026-09-29, bug reale — vedi AiWiki
                // Bugs/(solved) - UDT-Campo-Annidato-Scrittura-Encadenata-Non-Persiste.md): il
                // fallback sotto (value.ToString()) produce per UserDefinedType un formato
                // DEBUG ("TYPE Inner [...]", vedi DataTypes.UdtClasses.cs) inutilizzabile dalla
                // VM/C, che si aspetta una STRINGA PULITA come per gli scalari (DoubleType.
                // ToString()=="DOUBLE" ecc. — quelli hanno un override esplicito, UserDefinedType
                // no). Serializzato invece come DICT nidificato {FieldNames,FieldTypes} — stesso
                // shape del descriptor "vero" del tipo, RICORSIVO per costruzione (un campo
                // DOPPIAMENTE annidato funziona automaticamente) — la VM/C (gmb_vm_core.c,
                // vm_populate_udt_default_fields) lo riconosce controllando GMB_TYPE_OBJECT
                // invece di GMB_TYPE_STRING sul valore di FieldTypes[i], e costruisce
                // un'istanza reale invece del default double 0.0 (che rendeva un campo
                // annidato un numero, mai un IDictionary — ogni scrittura sul suo interno
                // veniva silenziosamente ignorata).
                var udtDesc = new Dictionary<string, object?>
                {
                    ["FieldNames"] = udt.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.Name.Lexeme),
                    ["FieldTypes"] = udt.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.DataType),
                };
                WriteValue(w, udtDesc);
                break;
            case System.Collections.IDictionary dict:
                // DICT: count + per ogni voce: ktag(0=string) + key_str + vtag + valore
                w.Write((byte)GMB_CONST_DICT);
                WriteU32(w, (uint)dict.Count);
                foreach (System.Collections.DictionaryEntry de in dict)
                {
                    w.Write((byte)0); // ktag 0 = chiave stringa
                    WriteString(w, de.Key?.ToString() ?? "");
                    WriteValue(w, de.Value);
                }
                break;
            case System.Collections.IEnumerable list:
                // LIST (letto come rd_dict dalla VM: items=entries): per ogni elemento
                // ktag(0=string chiave vuota) + key_len=0 + vtag + valore
                var items = list.Cast<object?>().ToList();
                w.Write((byte)GMB_CONST_LIST);
                WriteU32(w, (uint)items.Count);
                foreach (var item in items)
                {
                    w.Write((byte)0); // ktag 0 = chiave stringa
                    WriteU32(w, 0);  // key vuota (len 0)
                    WriteValue(w, item);
                }
                break;
            default:
                // Fallback: tratta come stringa se possibile, altrimenti NIL
                w.Write((byte)GMB_CONST_STRING);
                WriteString(w, value.ToString() ?? "");
                break;
        }
    }

    private static void WriteString(BinaryWriter w, string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s);
        WriteU32(w, (uint)bytes.Length);
        w.Write(bytes);
    }

    private static void WriteU32(BinaryWriter w, uint v)
    {
        w.Write((byte)((v >> 24) & 0xFF));
        w.Write((byte)((v >> 16) & 0xFF));
        w.Write((byte)((v >> 8) & 0xFF));
        w.Write((byte)(v & 0xFF));
    }

    private static void WriteF64(BinaryWriter w, double v)
    {
        ulong bits = unchecked((ulong)BitConverter.DoubleToInt64Bits(v));
        WriteU32(w, (uint)((bits >> 32) & 0xFFFFFFFF));
        WriteU32(w, (uint)(bits & 0xFFFFFFFF));
    }
}
