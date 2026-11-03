' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: AI/TypeInfoFullCoverage.bas
' =================================================================================================
'   COPERTURA COMPLETA — riflessione (TYPEINFO) + tipi nullable
' =================================================================================================
' Funzioni/comandi esercitati:
'   - TYPEINFO(x)              nome del tipo di un valore (scalare, UDT, nullable)
'   - TYPEINFO.HAS(x)          1 se il valore non è NIL, 0 se NIL
'   - TYPEINFO.METHODINFO(t)   JSON dei metodi del tipo
'   - TYPEINFO.PROPERTYINFO(t) JSON delle proprietà del tipo
'   - DIM x AS <scalare>?      dichiarazione nullable
'   - x = NIL / x = <valore>   assegnazione valore ↔ NIL
'   - x HAS VALUE              presenza del valore
'   - x IS NIL                 assenza del valore
'   - x.VALUE                  accesso al valore sottostante
' Output identico sui 4 backend (IL, PCode, VM/C, C99Native).
' =================================================================================================

IMPORT TYPEINFO

TYPE Person
  Name AS STRING * 32
  Age AS INTEGER
END TYPE

SUB Main()
  PRINT "=== 1. TYPEINFO su scalari ==="
  PRINT "TYPEINFO(42)       = "; TYPEINFO(42)
  PRINT "TYPEINFO(3.5)      = "; TYPEINFO(3.5)
  PRINT "TYPEINFO(stringa)  = "; TYPEINFO("ciao")

  PRINT
  PRINT "=== 2. TYPEINFO su UDT ==="
  DIM p AS Person
  PRINT "TYPEINFO(Person)   = "; TYPEINFO(p)

  PRINT
  PRINT "=== 3. Nullable con valore ==="
  DIM n AS INTEGER?
  DIM d AS DOUBLE?
  DIM s AS STRING?
  DIM x AS INTEGER? = 100
  n = 42
  d = 3.14
  s = "Hello Nullable"
  PRINT "TYPEINFO(INTEGER?) = "; TYPEINFO(n)
  PRINT "TYPEINFO.HAS(n)    = "; TYPEINFO.HAS(n)
  PRINT "n.VALUE            = "; n.VALUE
  PRINT "d.VALUE            = "; d.VALUE
  PRINT "s.VALUE            = "; s.VALUE
  PRINT "x.VALUE (da DIM)   = "; x.VALUE
  IF n HAS VALUE AND x HAS VALUE THEN PRINT "n,x HAS VALUE      = vero"

  PRINT
  PRINT "=== 4. Nullable rilasciati (NIL) ==="
  n = NIL
  d = NIL
  s = NIL
  PRINT "TYPEINFO(nint)     = "; TYPEINFO(n)
  PRINT "TYPEINFO.HAS(nint) = "; TYPEINFO.HAS(n)
  PRINT "TYPEINFO(nstr)     = "; TYPEINFO(s)
  PRINT "TYPEINFO.HAS(nstr) = "; TYPEINFO.HAS(s)
  IF n IS NIL THEN PRINT "n IS NIL           = vero"
  IF NOT (n HAS VALUE) THEN PRINT "n HAS VALUE        = falso"

  PRINT
  PRINT "=== 5. METHODINFO / PROPERTYINFO ==="
  PRINT "METHODINFO(Person)   = "; METHODINFO("Person")
  PRINT "PROPERTYINFO(Person) = "; PROPERTYINFO("Person")

  PRINT
  PRINT "=== Copertura completa terminata ==="
END SUB
