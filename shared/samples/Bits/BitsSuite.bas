' FILE: Bits/BitsSuite.bas
' =================================================================================================
' BITS — manipolazione bit a 64 bit (lib `bits` via FFI)
' =================================================================================================
' Esempio completo del dominio: usa TUTTE le 21 funzioni pubbliche del namespace BITS, una
' sezione per area. Indici 0-based (0 = LSB); i predicati sono BIT (-1 vero / 0 falso).
'
' Le sezioni sono pensate come un piccolo riferimento operativo: maschere di permessi,
' campi di protocollo, rotazioni, codici di Gray, interleaving (Morton), allineamenti.
' Output deterministico (nessun input esterno).
' =================================================================================================

IMPORT BITS

SUB Main()
    ' ── 1. MASCHERE DI BIT ─────────────────────────────────────────────────────────────────────
    ' Partiamo da 0 e accendiamo i bit 0, 3, 7: 1 + 8 + 128 = 137.
    PRINT "== maschere =="
    DIM m AS LONG
    m = BITSET(0, 0)
    m = BITSET(m, 3)
    m = BITSET(m, 7)
    PRINT "mask        = "; m
    PRINT "bit0/3/7    = "; BITGET(m, 0); "/"; BITGET(m, 3); "/"; BITGET(m, 7)
    PRINT "test bit3   = "; BITTEST(m, 3)          ' -1
    PRINT "test bit4   = "; BITTEST(m, 4)          ' 0
    PRINT "count       = "; BITCOUNT(m)             ' 3

    ' Spegnere, invertire, riaccendere.
    m = BITCLEAR(m, 7)
    m = BITTOGGLE(m, 4)
    PRINT "clear7+tog4 = "; m

    ' ── 2. SCANSIONE ──────────────────────────────────────────────────────────────────────────
    ' Indice del bit 1 più basso e più alto (-1 se il numero è 0).
    PRINT "== scansione =="
    DIM v AS LONG
    v = BITSET(BITSET(0, 2), 9)                   ' bit 2 e 9
    PRINT "v           = "; v
    PRINT "scanfwd     = "; BITSCANFWD(v)           ' 2
    PRINT "scanrev     = "; BITSCANREV(v)           ' 9

    ' ── 3. CAMPI DI BIT ───────────────────────────────────────────────────────────────────────
    ' BITFIELD estrae i bit lo..hi inclusi: da 0x3C (0011 1100) i bit 2..3 valgono 3.
    PRINT "== campi =="
    PRINT "field 2..3  = "; BITFIELD(60, 2, 3)        ' 3
    PRINT "field 2..5  = "; BITFIELD(60, 2, 5)        ' 15

    ' ── 4. ROTAZIONI ──────────────────────────────────────────────────────────────────────────
    ' Rotazione circolare su `width` bit: 0b1001 su 4 bit ruotata a sinistra di 1 → 0b0011.
    PRINT "== rotazioni =="
    PRINT "rotl        = "; BITROTATEL(9, 1, 4)        ' 3
    PRINT "rotr        = "; BITROTATER(9, 1, 4)        ' 12
    PRINT "reverse 4   = "; BITREVERSE(9, 4)          ' 9 (1001 invertito è 1001)

    ' ── 5. GRAY E PARITÀ ──────────────────────────────────────────────────────────────────────
    ' Il codice di Gray cambia un solo bit tra valori consecutivi; FROMGRAY lo inverte.
    PRINT "== gray =="
    PRINT "gray(5)     = "; GRAYCODE(5)               ' 7
    PRINT "fromgray(7) = "; FROMGRAY(7)               ' 5
    PRINT "parity(7)   = "; PARITY(7)                 ' 1 (tre bit a 1 → dispari)

    ' ── 6. BYTE E DISTANZA ────────────────────────────────────────────────────────────────────
    ' BYTESWAP inverte l'ordine degli 8 byte (endianness).
    PRINT "== byte =="
    PRINT "byteswap(1) = "; BYTESWAP(1)               ' 0x0100000000000000
    PRINT "hamming     = "; HAMMINGDIST(10, 8)         ' 1 (1010 vs 1000)

    ' ── 7. INTERLEAVING (MORTON) ──────────────────────────────────────────────────────────────
    ' BITINTERLEAVE alterna i bit di a e b: (1,1) → 0b11 = 3; BITDEINTERLEAVE li separa.
    PRINT "== morton =="
    PRINT "interleave  = "; BITINTERLEAVE(1, 1)        ' 3
    PRINT "deint 0     = "; BITDEINTERLEAVE(3, 0)      ' 1
    PRINT "deint 1     = "; BITDEINTERLEAVE(3, 1)      ' 1

    ' ── 8. POTENZE DI 2 ───────────────────────────────────────────────────────────────────────
    ' NEXTPOW2 = più piccola potenza di 2 ≥ x; ISPOW2 = test.
    PRINT "== potenze =="
    PRINT "nextpow2(5) = "; NEXTPOW2(5)               ' 8
    PRINT "ispow2(8)   = "; ISPOW2(8)                 ' -1
    PRINT "ispow2(9)   = "; ISPOW2(9)                 ' 0
END SUB
