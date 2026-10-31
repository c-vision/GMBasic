' MiniTest COLLECTIONS — `x = NEW LIST OF T {}` come ESPRESSIONE (assegnazione, non DIM).
' Copre il fix parser 2026-09-14: NEW LIST era riconosciuto solo come inizializzatore di
' `DIM x = ...`; in assegnazione a variabile già dichiarata dava "statement non
' riconosciuto". L'inizializzatore vuoto {} è supportato (parità con il resto del linguaggio).
' EXPECT: count= 2|first= 10|second= 20|empty= 0
IMPORT COLLECTIONS

SUB Main()
  DIM a AS LIST OF INTEGER
  a = NEW LIST OF INTEGER {}
  a.Add(10) : a.Add(20)
  PRINT "count="; a.Count()
  PRINT "first="; a(0)
  PRINT "second="; a(1)

  DIM b AS LIST OF STRING
  b = NEW LIST OF STRING {}
  PRINT "empty="; b.Count()
END SUB
