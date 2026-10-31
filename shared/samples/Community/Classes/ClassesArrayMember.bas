' FILE: Classes/ClassesArrayMember.bas
' =================================================================================================
'        GUIDA: ARRAY COME CAMPO (MEMBRO) DI UNA CLASSE — storage PER-ISTANZA
' =================================================================================================
'
' Un campo di una CLASS può essere un ARRAY: si dichiara `PUBLIC DIM nome(N) AS tipo`.
' L'array è PER-ISTANZA (ogni oggetto ha il proprio buffer), allocato automaticamente alla
' creazione dell'istanza. Si indicizza sia in forma NUDa (`nome(i)`) sia qualificata
' (`SELF.nome(i)`), in lettura e in scrittura.
'
' Prima: l'indicizzazione di un array-membro era rotta su TUTTI i backend (IL/PCode/VM-C
' davano vuoto o eccezione, i transpiler C generavano codice non compilabile). Ora c'è
' parità 4 backend.
'
' Nota: elementi SCALARI (INTEGER/DOUBLE/STRING). Per l'array di UDT a livello modulo si
' veda Arrays/UdtArrayFieldGlobal (il campo-elem si legge con `arr(i).campo`).

CLASS Punto
  PUBLIC DIM storico(4) AS INTEGER
  PUBLIC Dim somma AS INTEGER

  PUBLIC SUB NEW()
    SELF.somma = 0
  END SUB

  SUB Push(v AS INTEGER)
    ' Scrittura BARE (senza SELF): risolta come SELF.storico(...)
    SELF.storico(SELF.somma) = v
    SELF.somma = SELF.somma + 1
  END SUB

  SUB Mostra()
    DIM i AS INTEGER
    FOR i = 0 TO SELF.somma - 1
      ' Lettura qualificata via SELF
      PRINT "s(" + STR(i) + ")="; SELF.storico(i)
    NEXT i
  END SUB
END CLASS

SUB Main()
  DIM a AS Punto = NEW Punto()
  DIM b AS Punto = NEW Punto()
  ' Due istanze indipendenti: ciascuna ha il proprio array.
  a.Push(10)
  a.Push(20)
  b.Push(77)
  PRINT "a.somma="; a.somma
  PRINT "b.somma="; b.somma
  a.Mostra()
  b.Mostra()
END SUB
