// FILE: Compiler/FfiCallFormat.cs
// Parser UNICO del layout di un'istruzione OP_CALL_FFI_T (bytecode condiviso PCodeVM/VM-C).
//
// Perché esiste: il formato è cresciuto per strati (descrittore per-argomento a 3 byte,
// coda a lunghezza variabile delle foglie UDT, tabella delle firme callback) e tre
// consumatori C# dovevano conoscerne la lunghezza esatta per non disallineare il resto del
// chunk: il disassembler, il merger dei chunk (REQUIRE) e i test sul bytecode. Una copia
// dell'aritmetica in ognuno è la classe di bug che ha già rotto il merge (2026-10) e il
// disassembler (formato a maschere, mai aggiornato al descrittore per-argomento).
//
// Formato corrente (offset relativo all'OPCODE):
//   u16 libIndex (INDICE COSTANTE)
//   u16 nameIndex (INDICE COSTANTE)
//   u16 argc
//   u8  kind            bit0 = kind omogeneo INT64, 0x20 = DECLARE NATIVE,
//                       0x40 = tabella firme callback in coda, 0x80 = ritorno UDT layout nuovo
//   u8  retIsString
//   u8  retLeafLo, u8 retLeafHi   (ritorno UDT layout nuovo: conteggio foglie u16;
//                                  storico: count u8 + maschera kind u8)
//   argc × [u8 flags, u8 aux1, u8 aux2]
//        flags: 1 int64/puntatore, 2 array, 4 string, 8 byref, 0x10 callback, 0x20 UDT,
//               0x40 array elem int, 0x80 array-di-UDT / UDT layout nuovo / byref elem int
//        callback: aux1 = arità (0-64 scalare; 128+ / 192+ forma speciale),
//                  aux2 bit0 = kind storico omogeneo, bit1 = ha ritorno
//        UDT: aux1|aux2<<8 = conteggio foglie (layout nuovo) oppure aux1 = campi (storico)
//        array di UDT: aux1|aux2<<8 = conteggio foglie dell'elemento
//   coda foglie UDT, per ogni parametro UDT/array-di-UDT (ordine di slot) poi per il ritorno:
//        foglia × (u16 pathNomiCost, u16 SymbolIdPathCost)   [INDICI COSTANTE]
//        layout nuovo: u16 size + foglia × (u8 tipo, u16 offset)
//   se kind & 0x40: per ogni parametro callback SCALARE (aux1 < 128), in ordine di slot:
//        u8 arità, u8 retKind (0=DOUBLE 1=INT64 2=nessun ritorno), arità × u8 kind

namespace Compiler;

internal static class FfiCallFormat
{
    internal sealed class Layout
    {
        public int Length;
        public int ArgCount;
        public int KindByte;
        public int RetIsString;
        public bool HasUdtLeaves;
        public bool HasCallbackTable;
        /// <summary>Offset ASSOLUTI (dal debutto del chunk) degli u16 che sono INDICI
        /// COSTANTE: lib, name, poi le coppie nome/SymbolId delle foglie UDT.</summary>
        public List<int> ConstOffsets = new();
    }

    internal static Layout Describe(IReadOnlyList<byte> code, int offset)
    {
        var lay = new Layout();
        var o = offset + 1;
        byte R8() => o < code.Count ? code[o++] : (byte)0;
        int R16()
        {
            if (o + 1 >= code.Count) { o += 2; return 0; }
            var v = (code[o] << 8) | code[o + 1];
            o += 2;
            return v;
        }
        void Skip(int n) => o += n;

        lay.ConstOffsets.Add(o); _ = R16();          // libIndex
        lay.ConstOffsets.Add(o); _ = R16();          // nameIndex
        lay.ArgCount = R16();
        if (lay.ArgCount < 0 || lay.ArgCount > 64) lay.ArgCount = Math.Max(0, Math.Min(lay.ArgCount, 64));
        lay.KindByte = R8();
        lay.RetIsString = R8();
        var retLo = R8();
        var retHi = R8();
        var retUdtNew = (lay.KindByte & 0x80) != 0;
        var retUdtCount = retUdtNew ? (retLo | (retHi << 8)) : retLo;

        var udtCount = new int[lay.ArgCount];
        var udtNew = new bool[lay.ArgCount];
        var cbArity = new int[lay.ArgCount];
        for (var i = 0; i < lay.ArgCount; i++)
        {
            var flags = R8(); var a1 = R8(); var a2 = R8();
            cbArity[i] = 255;
            if ((flags & 0x10) != 0) cbArity[i] = a1;
            if ((flags & 2) != 0 && (flags & 0x80) != 0) { udtCount[i] = a1 | (a2 << 8); udtNew[i] = true; }
            else if ((flags & 0x20) != 0)
            {
                udtNew[i] = (flags & 0x80) != 0;
                udtCount[i] = udtNew[i] ? (a1 | (a2 << 8)) : a1;
            }
        }
        if (lay.ArgCount > 0) lay.HasUdtLeaves = false;
        for (var i = 0; i < lay.ArgCount; i++)
            if (udtCount[i] > 0) { lay.HasUdtLeaves = true; break; }
        if (retUdtCount > 0) lay.HasUdtLeaves = true;

        for (var i = 0; i < lay.ArgCount; i++)
        {
            if (udtCount[i] <= 0) continue;
            for (var k = 0; k < udtCount[i]; k++)
            {
                lay.ConstOffsets.Add(o); _ = R16();
                lay.ConstOffsets.Add(o); _ = R16();
            }
            if (udtNew[i]) { _ = R16(); Skip(udtCount[i] * 3); }
        }
        if (retUdtCount > 0)
        {
            for (var k = 0; k < retUdtCount; k++)
            {
                lay.ConstOffsets.Add(o); _ = R16();
                lay.ConstOffsets.Add(o); _ = R16();
            }
            if (retUdtNew) { _ = R16(); Skip(retUdtCount * 3); }
        }
        lay.HasCallbackTable = (lay.KindByte & 0x40) != 0;
        if (lay.HasCallbackTable)
        {
            for (var i = 0; i < lay.ArgCount; i++)
            {
                if (cbArity[i] == 255 || cbArity[i] >= 128) continue;
                var n = R8(); _ = R8(); Skip(n);
            }
        }
        lay.Length = o - offset;
        return lay;
    }
}
