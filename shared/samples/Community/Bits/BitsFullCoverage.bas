' FILE: samples/Community/Bits/BitsFullCoverage.bas
' =================================================================================================
'  SAMPLE COMPLETO E COMPLESSO — namespace BITS (manipolazione bit a 64 bit, 21 funzioni)
' =================================================================================================
'  Scenario realistico: un "protocollo di pacchetti binari" con
'    1) bitmask di permessi            (BITSET/BITCLEAR/BITTOGGLE/BITTEST/BITGET/BITCOUNT)
'    2) header impacchettato           (BITFIELD)
'    3) integrità dei pacchetti        (PARITY/HAMMINGDIST)
'    4) contatore Gray                 (GRAYCODE/FROMGRAY)
'    5) indirizzamento spaziale Morton (BITINTERLEAVE/BITDEINTERLEAVE)
'    6) cifrario a rotazione           (BITROTATEL/BITROTATER/BITREVERSE/BYTESWAP)
'    7) capacità e allineamento        (NEXTPOW2/ISPOW2/BITSCANFWD/BITSCANREV)
'
'  Ogni scenario VERIFICA una proprietà (round-trip, invariante) e stampa l'esito:
'  deterministico, nessuna rete/orologio, valori < 1e16 (soglia del formato di stampa).
'  MiniTest per-funzione: minitests/bits/*.bas (21 file) + bits_all/bits_edge.
' =================================================================================================
IMPORT BITS

' ── Helper di dominio ─────────────────────────────────────────────────────────────────────────

' Permesso concesso? (convenzione Bit: -1 = vero, 0 = falso)
FUNCTION HasPerm(perm AS LONG, bit AS LONG) AS LONG
  RETURN BITS.BITTEST(perm, bit)
END FUNCTION

' Concede/revoca un permesso in modo dichiarativo.
FUNCTION SetPerm(perm AS LONG, bit AS LONG, attivo AS LONG) AS LONG
  IF attivo <> 0 THEN
    RETURN BITS.BITSET(perm, bit)
  END IF
  RETURN BITS.BITCLEAR(perm, bit)
END FUNCTION

' Header pacchetto: [15..8] versione | [7..4] tipo | [3..0] flag
FUNCTION PackHeader(ver AS LONG, tipo AS LONG, flags AS LONG) AS LONG
  RETURN (ver * 256) + (tipo * 16) + flags
END FUNCTION

' Estrae un campo con BITS (estremi inclusi).
FUNCTION UnpackField(h AS LONG, lo AS LONG, hi AS LONG) AS LONG
  RETURN BITS.BITFIELD(h, lo, hi)
END FUNCTION

SUB Main()
' ═══ Scenario 1: bitmask di permessi ═══════════════════════════════════════════════════════════
' Bit 0 = LETTURA, 1 = SCRITTURA, 2 = ESECUZIONE, 3 = CANCELLAZIONE
DIM perm AS LONG
perm = 0
perm = SetPerm(perm, 0, -1)                     ' concedi LETTURA
perm = SetPerm(perm, 2, -1)                     ' concedi ESECUZIONE
PRINT "perm1="; perm                            ' 1 + 4 = 5
PRINT "ha_lettura="; HasPerm(perm, 0)           ' -1
PRINT "ha_scrittura="; HasPerm(perm, 1)         ' 0
PRINT "bit3="; BITS.BITGET(perm, 3)             ' 0
perm = BITS.BITTOGGLE(perm, 2)                  ' revoca ESECUZIONE
perm = SetPerm(perm, 0, 0)                      ' revoca LETTURA
PRINT "perm2="; perm                            ' 0
perm = BITS.BITSET(perm, 5)
perm = BITS.BITSET(perm, 1)
PRINT "perm3="; perm                            ' 34
PRINT "accesi="; BITS.BITCOUNT(perm)            ' 2

' ═══ Scenario 2: header impacchettato + round-trip su una tabella ══════════════════════════════
DIM headers(4) AS LONG
headers(0) = PackHeader(1, 2, 3)
headers(1) = PackHeader(3, 0, 0)
headers(2) = PackHeader(2, 7, 5)
headers(3) = PackHeader(0, 1, 15)
DIM k AS LONG
DIM bad AS LONG
bad = 0
FOR k = 0 TO 3
  IF UnpackField(headers(k), 8, 15) <> BITS.BITFIELD(headers(k), 8, 15) THEN bad = bad + 1
  IF UnpackField(headers(k), 4, 7) > 15 THEN bad = bad + 1
NEXT
PRINT "h2="; headers(2)                          ' (2*256)+(7*16)+5 = 629
PRINT "ver2="; UnpackField(headers(2), 8, 15)    ' 2
PRINT "tipo2="; UnpackField(headers(2), 4, 7)    ' 7
PRINT "flag2="; UnpackField(headers(2), 0, 3)    ' 5
PRINT "bad_header="; bad                         ' 0
' campo con estremi invertiti (lo > hi): vengono scambiati
PRINT "swapfield="; BITS.BITFIELD(headers(2), 7, 4)   ' 7

' ═══ Scenario 3: integrità (parità + distanza di Hamming fra pacchetti consecutivi) ══════════
' NB: qui si ACCUMULA (somma) invece di calcolare il massimo con `IF dh > mx THEN mx = dh`:
' su IL il pattern `IF cond THEN <dest> = <variabile nuda>` dentro un ciclo perde
' l'assegnazione (bug IL aperto, non BITS — vedi
' Bugs/(open) - IL-If-Then-Assegna-Variabile-In-Loop.md). L'accumulo con RHS espressione
' è corretto su tutti i backend.
DIM par AS LONG
DIM hamsum AS LONG
par = 0
hamsum = 0
FOR k = 0 TO 2
  par = par + BITS.PARITY(headers(k))
  hamsum = hamsum + BITS.HAMMINGDIST(headers(k), headers(k + 1))
NEXT
PRINT "parity_sum="; par                         ' PARITY(291)=0, PARITY(768)=0, PARITY(629)=0 -> 0
PRINT "ham_sum="; hamsum                         ' 4 + 6 + 5 = 15 (coppie 291-768, 768-629, 629-31)
PRINT "ham_self="; BITS.HAMMINGDIST(perm, perm)  ' 0
PRINT "ham_self2="; BITS.HAMMINGDIST(headers(2), headers(2))   ' 0

' ═══ Scenario 4: contatore Gray — PROPRIETÀ: codici adiacenti differiscono di 1 bit ═══════════
DIM viol AS LONG
viol = 0
FOR k = 0 TO 14
  IF BITS.FROMGRAY(BITS.GRAYCODE(k)) <> k THEN viol = viol + 1
  IF BITS.HAMMINGDIST(BITS.GRAYCODE(k), BITS.GRAYCODE(k + 1)) <> 1 THEN viol = viol + 1
NEXT
PRINT "gray_viol="; viol                         ' 0 (round-trip + adiacenza a 1 bit su 0..15)
PRINT "gray5="; BITS.GRAYCODE(5)                 ' 7
PRINT "fromgray7="; BITS.FROMGRAY(7)             ' 5

' ═══ Scenario 5: indirizzamento spaziale Morton (griglia 4x4 -> indice lineare) ════════════════
DIM gx AS LONG
DIM gy AS LONG
DIM morton AS LONG
DIM morton_bad AS LONG
morton_bad = 0
FOR gy = 0 TO 3
  FOR gx = 0 TO 3
    morton = BITS.BITINTERLEAVE(gx, gy)
    IF BITS.BITDEINTERLEAVE(morton, 0) <> gx THEN morton_bad = morton_bad + 1
    IF BITS.BITDEINTERLEAVE(morton, 1) <> gy THEN morton_bad = morton_bad + 1
  NEXT
NEXT
PRINT "morton_bad="; morton_bad                  ' 0 (round-trip su tutte le 16 celle)
PRINT "morton_35="; BITS.BITINTERLEAVE(3, 5)    ' 39
PRINT "morton_x="; BITS.BITDEINTERLEAVE(39, 0)   ' 3
PRINT "morton_y="; BITS.BITDEINTERLEAVE(39, 1)   ' 5

' ═══ Scenario 6: cifrario a rotazione + serializzazione byte ══════════════════════════════════
DIM clear AS LONG
clear = 202                            ' 0b11001010
DIM cipher AS LONG
cipher = BITS.BITROTATEL(BITS.BITREVERSE(clear, 8), 3, 8)
DIM back AS LONG
back = BITS.BITREVERSE(BITS.BITROTATER(cipher, 3, 8), 8)
PRINT "cipher="; cipher
PRINT "roundtrip="; back                          ' 202
PRINT "rot_ok="; BITS.BITTEST(BITS.BITROTATER(BITS.BITROTATEL(clear, 3, 8), 3, 8), 7)  ' -1 se bit7 di 202 = 1
' BYTESWAP due volte = identità (invariante di endianness)
PRINT "byteswap2="; BITS.BYTESWAP(BITS.BYTESWAP(12345))   ' 12345
PRINT "byteswap256="; BITS.BYTESWAP(256)                  ' 2^48

' ═══ Scenario 7: capacità del buffer e allineamento ════════════════════════════════════════════
DIM need AS LONG
need = 1000
DIM cap AS LONG
cap = BITS.NEXTPOW2(need)
PRINT "cap="; cap                                 ' 1024
PRINT "cap_ispow="; BITS.ISPOW2(cap)             ' -1
PRINT "need_ispow="; BITS.ISPOW2(need)           ' 0
PRINT "lsb="; BITS.BITSCANFWD(40)                ' 3 (40 = 0b101000)
PRINT "msb="; BITS.BITSCANREV(40)                ' 5
PRINT "lsb_zero="; BITS.BITSCANFWD(0)            ' -1
PRINT "msb_zero="; BITS.BITSCANREV(0)            ' -1
PRINT "bit_roundtrip="; BITS.BITGET(BITS.BITSET(0, BITS.BITSCANREV(40)), BITS.BITSCANREV(40))  ' 1

PRINT "Sample BITS completato."
END SUB
