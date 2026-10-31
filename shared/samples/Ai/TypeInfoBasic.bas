' FILE: AI/TypeInfoBasic.bas
' TYPEINFO reflection MVP — nome del tipo di un valore.
'
' Copre: TYPEINFO (scalari, UDT), TYPEINFO.HAS (non-nil), METHODINFO, PROPERTYINFO.
' I 4 backend (IL, PCode, VM/C, C99, C99Native) producono lo STESSO output — parità
' verificata da Sample_Parity_Sweep. Vedi Guide/utente/TYPEINFO-Riflessione-Guida-Utente.md.

IMPORT TYPEINFO WITH ALIAS

TYPE Person
  Name AS STRING * 32
  Age AS INTEGER
END TYPE

SUB Main()
  DIM nval AS INTEGER?
  DIM empty AS INTEGER?
  DIM p AS Person

  nval = 7
  empty = NIL

  PRINT "TYPEINFO(42)="; TYPEINFO(42)
  PRINT "TYPEINFO(string)="; TYPEINFO("hello")
  PRINT "TYPEINFO(integer?)="; TYPEINFO(nval)
  PRINT "TYPEINFO.HAS(7)="; TYPEINFO.HAS(nval)
  PRINT "TYPEINFO(nil)="; TYPEINFO(empty)
  PRINT "TYPEINFO.HAS(nil)="; TYPEINFO.HAS(empty)
  PRINT "TYPEINFO(udt)="; TYPEINFO(p)
  PRINT "METHODINFO(Person)="; METHODINFO("Person")
  PRINT "PROPERTYINFO(Person)="; PROPERTYINFO("Person")
END SUB
