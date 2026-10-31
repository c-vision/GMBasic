// FILE: CommonLib/Consoles/Tui/NativeValtype.cs
// Lettera di tipo VAL (U/L/N/C/D/A/O) dal runtime C: unica sorgente per ValType.
using System;
using System.Runtime.InteropServices;

namespace CommonLib.Consoles.Tui;

public static class NativeValtype
{
    private const string Lib = "gmb_gfx";

    // Valori di gmb_type_kind_t (include/gmb_runtime.h).
    public const int NIL = 0, INT = 1, DOUBLE = 2, BOOL = 3, STRING = 4, ARRAY = 5,
        OBJECT = 6, DATETIME = 7, CURRENCY = 8;

    [DllImport(Lib, CharSet = CharSet.Ansi)]
    private static extern IntPtr gmb_valtype_letter(int kind);

    /// <summary>Lettera VAL — mapping GESTITO, identico a <c>gmb_valtype_letter</c> in
    /// <c>gmb_core.c</c>: NIL→U, BOOL→L, INT/DOUBLE/CURRENCY→N, STRING→C, ARRAY→A,
    /// DATETIME→D, altro→O. Era una P/Invoke su <c>libgmb_gfx</c>: chiamata durante la
    /// registrazione dei tipi UDT all'avvio, caricava la dylib grafica (+SDL, ~8 MB) anche
    /// per un programma che stampa una riga. La tabella è fissa e verificata dai MiniTest
    /// di tipo (VALTYPE) su IL; il runtime C resta la sorgente per il mondo C.</summary>
    public static string Letter(int kind) => kind switch
    {
        NIL => "U",
        BOOL => "L",
        INT or DOUBLE or CURRENCY => "N",
        STRING => "C",
        ARRAY => "A",
        DATETIME => "D",
        _ => "O",
    };
}
