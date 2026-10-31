' ==============================================================================
' SystemSizeOfUdtLayout.bas — canone di SIZEOF / SYSTEM.UDTPACKED / SYSTEM.UDTALIGNED
'  e riflesso dei tipi (SYSTEM.VALTYPE / SYSTEM.TYPENAME / SYSTEM.CTYPE).
'  Parità 4 backend: IL, PCodeVM, VM/C, C99Native.
'
' CASO D'USO: costruire la MAPPA DI UN RECORD BINARIO (offset e lunghezze dei campi)
'  con il solo layout statico del linguaggio, e verificarla con asserzioni.
'
' Copre OGNI classe di argomento ammessa dal canone (common/Ast/TypeLayout.cs):
'  1. variabile DICHIARATA               → SIZEOF del suo tipo statico
'  2. nome di TYPE                        → layout dello UDT (packed o allineato)
'     nome di tipo scalare (keyword)      → sua larghezza; STRING senza "* n" → -1
'  3. letterale numerico                  → 8 (il DOUBLE di default del linguaggio)
'  4. variabile IMPLICITA (senza DIM)     → 8; con suffisso $ → -1, con % ! # → 8
'  5. qualsiasi ALTRA FORMA (espressione  → -1 (nessun layout statico da misurare)
'     aritmetica, chiamata, membro,
'     indice, nome di funzione built-in)
' più i contatori di CAMPI (UDTPACKED/UDTALIGNED) e la lettura dei tipi
' (VALTYPE = codice, TYPENAME = nome).
' ==============================================================================
IMPORT SYSTEM

PACKED TYPE Header
  magic AS INTEGER
  version AS INTEGER
  flags AS BIT
END TYPE

TYPE Linea
  id AS LONG
  label AS STRING * 8
  amount AS DOUBLE
END TYPE

TYPE Record
  h AS Header
  l AS Linea
  total AS CURRENCY
  tag AS STRING * 4
END TYPE

PACKED TYPE RecordPacked
  h AS Header
  id AS LONG
  amount AS DOUBLE
  tag AS STRING * 4
END TYPE

SUB Main()
  PRINT "=== 1. NOMI DI TIPO SCALARE (keyword) ==="
  PRINT SIZEOF(BIT)
  PRINT SIZEOF(INTEGER)
  PRINT SIZEOF(SINGLE)
  PRINT SIZEOF(LONG)
  PRINT SIZEOF(DOUBLE)
  PRINT SIZEOF(CURRENCY)
  PRINT SIZEOF(STRING)

  PRINT "=== 2. VARIABILI DICHIARATE ==="
  DIM i AS INTEGER
  DIM l AS LONG
  DIM d AS DOUBLE
  DIM f10 AS STRING * 10
  DIM dyn AS STRING
  dyn = "ciao"
  PRINT SIZEOF(i)
  PRINT SIZEOF(l)
  PRINT SIZEOF(d)
  PRINT SIZEOF(f10)
  PRINT SIZEOF(dyn)

  PRINT "=== 3. LETTERALI ==="
  PRINT SIZEOF(42)
  PRINT SIZEOF("abc")

  PRINT "=== 4. VARIABILI IMPLICITE E SUFFISI ==="
  iv = 5
  sv = "abc"
  a$ = "x"
  b% = 3
  PRINT SIZEOF(iv)
  PRINT SIZEOF(sv)
  PRINT SIZEOF(a$)
  PRINT SIZEOF(b%)

  PRINT "=== 5. ESPRESSIONI E NOMI DI FUNZIONE BUILT-IN ==="
  DIM n AS LONG
  n = 3
  PRINT SIZEOF(n + 1)
  PRINT SIZEOF(LEN("hello"))
  PRINT SIZEOF(DATE)
  PRINT SIZEOF(PI)

  PRINT "=== 6. COLLEZIONI, ARRAY, CLASS ==="
  DIM arr(5) AS LONG
  DIM lst AS LIST
  PRINT SIZEOF(arr)
  PRINT SIZEOF(lst)

  PRINT "=== 7. LAYOUT DEGLI UDT ==="
  PRINT SIZEOF(Header)
  PRINT SIZEOF(Linea)
  PRINT SIZEOF(Record)
  PRINT SIZEOF(RecordPacked)

  PRINT "=== 8. MAPPA DEL RECORD (offset calcolati con SIZEOF) ==="
  DIM hh AS Header
  DIM idf AS LONG
  DIM amf AS DOUBLE
  DIM tgf AS STRING * 4
  DIM off AS LONG
  DIM size AS LONG
  off = 0
  size = SIZEOF(hh)
  PRINT "header     size="; size; " offset="; off
  off = off + size
  size = SIZEOF(idf)
  PRINT "id         size="; size; " offset="; off
  off = off + size
  size = SIZEOF(amf)
  PRINT "amount     size="; size; " offset="; off
  off = off + size
  size = SIZEOF(tgf)
  PRINT "tag        size="; size; " offset="; off
  off = off + size
  PRINT "packed tot ="; off
  PRINT "allineato  ="; SIZEOF(RecordPacked)

  PRINT "=== 9. CONTATORI DI CAMPI (UDTPACKED / UDTALIGNED) ==="
  DIM r AS Record
  DIM rp AS RecordPacked
  DIM hdr AS Header
  PRINT SYSTEM.UDTPACKED(r)
  PRINT SYSTEM.UDTALIGNED(r)
  PRINT SYSTEM.UDTPACKED(hdr)
  PRINT SYSTEM.UDTALIGNED(hdr)

  PRINT "=== 10. LETTURA DEI TIPI (VALTYPE / TYPENAME) ==="
  PRINT SYSTEM.VALTYPE(r)
  PRINT SYSTEM.TYPENAME(r)
  PRINT SYSTEM.VALTYPE(l)
  PRINT SYSTEM.VALTYPE(dyn)

  PRINT "=== 11. COERCIZIONE (CTYPE) ==="
  PRINT SYSTEM.CTYPE(42.9, "INTEGER")
  PRINT SYSTEM.CTYPE(3.5, "DOUBLE")
  PRINT SYSTEM.CTYPE(42, "STRING")

  PRINT "=== 12. ASSERZIONI SUL LAYOUT ==="
  IF SIZEOF(Record) >= SIZEOF(RecordPacked) THEN
    PRINT "A1 OK (allineato >= packed)"
  ELSE
    PRINT "A1 FAIL"
  END IF
  IF SIZEOF(RecordPacked) = SIZEOF(hh) + SIZEOF(idf) + SIZEOF(amf) + SIZEOF(tgf) THEN
    PRINT "A2 OK (packed = somma dei campi)"
  ELSE
    PRINT "A2 FAIL"
  END IF
  IF SIZEOF(LONG) = 8 THEN
    PRINT "A3 OK (LONG = 8, canone)"
  ELSE
    PRINT "A3 FAIL"
  END IF
  IF SIZEOF(dyn) = -1 THEN
    PRINT "A4 OK (stringa dinamica = -1)"
  ELSE
    PRINT "A4 FAIL"
  END IF
  IF SIZEOF(n + 1) = -1 THEN
    PRINT "A5 OK (espressione = -1)"
  ELSE
    PRINT "A5 FAIL"
  END IF
  PRINT "FINE"
END SUB
