// FILE: IlBackendRuntime/PCode/PCodeProcessor.UdtGroups.cs
// PARTIAL — UNION e BITFIELD (2026-11-11) per la gamba PCodeVM.
//
// Su PCodeVM (e VM/C) un'istanza UDT è un DIZIONARIO campo→valore: non c'è una struct in
// memoria, quindi l'aliasing C (scrivere `u.I` e leggere `u.F`) va EMULATO. Il compilatore
// emette nel descrittore `__class__` la voce "Groups": una lista di gruppi, ognuno un blocco di
// byte condiviso da più campi:
//   UNION        → un gruppo con tutti i campi a offset 0 (size = taglia dell'unione)
//   BITFIELD     → un gruppo per unità di storage (size = byte del tipo dichiarato)
// Formato di ogni gruppo (lista di double, serializzabile nei chunk):
//   [size, n, (symId, byteOff, bitOff, bitWidth, typeCode) × n]
// Alla SCRITTURA di un campo membro: si aggiornano i bit nel buffer del gruppo e si riscrivono
// TUTTI i fratelli nel dizionario. In lettura i valori sono già allineati.

using System.Collections;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Codici tipo (stessi del descrittore/FFI): 0=double 1=int64 2=int8 3=uint8
    /// 4=int16 5=uint16 6=int32 7=uint32 8=float32 9=string 10=uint64.</summary>
    private static int GroupCodeBytes(int code) => code switch
    {
        0 or 1 or 9 or 10 => 8, 6 or 7 or 8 => 4, 4 or 5 => 2, _ => 1,
    };

    private static bool GroupCodeSigned(int code) => code is 1 or 2 or 4 or 6;

    /// <summary>Converte un valore BASIC (double) nei bit grezzi del tipo C.</summary>
    private static ulong GroupValueToBits(object? v, int code, int width)
    {
        var d = v is null ? 0.0 : Convert.ToDouble(v);
        ulong raw = code switch
        {
            0 => (ulong)BitConverter.DoubleToInt64Bits(d),                  // double
            8 => BitConverter.SingleToUInt32Bits((float)d),                // float
            _ => unchecked((ulong)(long)d)                                  // interi
        };
        return width is > 0 and < 64 ? raw & ((1UL << width) - 1) : raw;
    }

    /// <summary>Converte i bit grezzi nel valore BASIC (double), con estensione del segno
    /// quando il tipo è signed e la larghezza è un bitfield.</summary>
    private static double GroupBitsToValue(ulong bits, int code, int width)
    {
        var w = width > 0 ? width : GroupCodeBytes(code) * 8;
        if (w < 64) bits &= (1UL << w) - 1;
        if (code == 0) return BitConverter.Int64BitsToDouble(unchecked((long)bits));
        if (code == 8) return BitConverter.UInt32BitsToSingle((uint)bits);
        if (GroupCodeSigned(code) && w < 64 && ((bits >> (w - 1)) & 1) != 0)
            return (double)(long)(bits | (~0UL << w));                     // segno esteso
        return (double)bits;
    }

    private static ulong GroupReadBits(byte[] buf, int byteOff, int bitOff, int bitCount)
    {
        ulong r = 0;
        for (var b = 0; b < bitCount; b++)
        {
            var bit = bitOff + b;
            var pos = byteOff + bit / 8;
            if (pos >= buf.Length) break;
            if (((buf[pos] >> (bit % 8)) & 1) != 0) r |= 1UL << b;
        }
        return r;
    }

    private static void GroupWriteBits(byte[] buf, int byteOff, int bitOff, int bitCount, ulong v)
    {
        for (var b = 0; b < bitCount; b++)
        {
            var bit = bitOff + b;
            var pos = byteOff + bit / 8;
            if (pos >= buf.Length) break;
            var mask = (byte)(1 << (bit % 8));
            if (((v >> b) & 1) != 0) buf[pos] |= mask;
            else buf[pos] &= (byte)~mask;
        }
    }

    /// <summary>Hook di scrittura: se <paramref name="symbolId"/> appartiene a un gruppo di
    /// aliasing del tipo, aggiorna il buffer condiviso e riscrive i fratelli. Chiamato DOPO la
    /// scrittura normale del campo (che resta valida: il campo scritto è già quello giusto).</summary>
    private static void ApplyUdtGroupWrite(IDictionary inst, IDictionary classDesc, int symbolId, object? value)
    {
        if (classDesc["Groups"] is not IEnumerable groupsList) return;
        var fieldNames = classDesc["FieldNames"] as IDictionary;
        var gi = 0;
        foreach (var g in groupsList)
        {
            if (g is not IEnumerable nums) { gi++; continue; }
            var vals = nums.Cast<object?>().Select(x => x is null ? 0.0 : Convert.ToDouble(x)).ToList();
            if (vals.Count < 2) { gi++; continue; }
            var size = (int)vals[0];
            var n = (int)vals[1];
            var k = 2;
            var hitIndex = -1;
            for (var m = 0; m < n && k + 4 < vals.Count; m++, k += 5)
                if ((int)vals[k] == symbolId) hitIndex = m;
            if (hitIndex < 0) { gi++; continue; }

            // Buffer del gruppo (creato alla prima scrittura), conservato nell'istanza.
            var key = "__grp" + gi;
            if (inst[key] is not byte[] buf || buf.Length != Math.Max(1, size))
                buf = new byte[Math.Max(1, size)];

            var baseK = 2 + hitIndex * 5;
            var byteOff = (int)vals[baseK + 1];
            var bitOff = (int)vals[baseK + 2];
            var width = (int)vals[baseK + 3];
            var code = (int)vals[baseK + 4];
            var bitCount = width > 0 ? width : GroupCodeBytes(code) * 8;
            var raw = GroupValueToBits(value, code, width);
            // Scrittura per GRUPPO: prima riscrive il valore del campo scritto se il campo NON è
            // un bitfield (larghezza 0) — nel caso union l'utente ha scritto il valore intero.
            GroupWriteBits(buf, byteOff, bitOff, bitCount, raw);
            inst[key] = buf;

            // Riscrive TUTTI i fratelli dal buffer condiviso.
            k = 2;
            for (var m = 0; m < n && k + 4 < vals.Count; m++, k += 5)
            {
                var sym = (int)vals[k];
                var fname = fieldNames != null ? FieldNameOf(fieldNames, sym) : null;
                if (fname == null) continue;
                // Anche il campo appena scritto viene riletto dal buffer: per un BITFIELD è così
                // che si applica la maschera (il valore scritto era intero), per una union il
                // valore riletto è identico a quello scritto.
                var bOff = (int)vals[k + 1];
                var btOff = (int)vals[k + 2];
                var w = (int)vals[k + 3];
                var cd = (int)vals[k + 4];
                var cnt = w > 0 ? w : GroupCodeBytes(cd) * 8;
                inst[fname] = GroupBitsToValue(GroupReadBits(buf, bOff, btOff, cnt), cd, w);
            }
            return;
        }
    }

    /// <summary>Nome del campo dal descrittore FieldNames (chiavi int in-process, stringhe nei
    /// chunk deserializzati).</summary>
    private static string? FieldNameOf(IDictionary fieldNames, int symbolId)
    {
        if (fieldNames.Contains(symbolId)) return fieldNames[symbolId]?.ToString();
        return fieldNames[symbolId.ToString()]?.ToString();
    }
}
