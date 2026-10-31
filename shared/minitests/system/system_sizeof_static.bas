' MiniTest: SIZEOF = dimensione in byte del TIPO STATICO (canone unico, common/Ast/TypeLayout.cs)
'  — parità 4 backend (IL, PCodeVM, VM/C, C99Native).
'  scalari: DOUBLE 8, INTEGER 2; STRING * 10 → 11 (buffer + terminatore); UDT ordinario (2 INTEGER)
'  → 4; PACKED TYPE (LONG+BIT+DOUBLE) → 17 (somma senza padding); lo stesso TYPE non packed → 24
'  (allineamento naturale); stringa dinamica → -1 (nessun layout statico).
' EXPECT: 8|2|11|4|17|24|-1
IMPORT SYSTEM
PACKED TYPE PackedRecord
  id AS LONG
  flag AS BIT
  value AS DOUBLE
END TYPE
TYPE UnpackedRecord
  id AS LONG
  flag AS BIT
  value AS DOUBLE
END TYPE
TYPE Points
  x AS INTEGER
  y AS INTEGER
END TYPE
SUB Main()
  DIM d AS DOUBLE
  DIM i AS INTEGER
  DIM s AS STRING * 10
  DIM dyn AS STRING
  dyn = "hello"
  PRINT SIZEOF(d)
  PRINT SIZEOF(i)
  PRINT SIZEOF(s)
  PRINT SIZEOF(Points)
  PRINT SIZEOF(PackedRecord)
  PRINT SIZEOF(UnpackedRecord)
  PRINT SIZEOF(dyn)
END SUB
