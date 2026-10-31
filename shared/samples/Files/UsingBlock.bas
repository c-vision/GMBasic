' ==============================================================================
' Sample: USING Block — Automatic resource management
' ==============================================================================
' Questo sample dimostra il costrutto USING...END USING per la gestione
' automatica delle risorse "disposable" (es. file aperti).
'
' Sintassi:
'   USING <var> = <expression>
'     ' corpo del blocco
'   END USING
'
' La risorsa viene rilasciata automaticamente (Dispose) all'uscita del blocco,
' anche in caso di errore.
' ==============================================================================

TYPE Persona
  Nome AS STRING * 20
  Eta AS INTEGER
END TYPE

DIM P AS Persona
P.Nome = "Mario Rossi"
P.Eta = 30

' --- USING con OPEN per scrittura ---
PRINT "Scrittura file con USING..."
USING F = OPEN("test_using_write.txt", OUTPUT)
  PRINT #F, "Record 1: " + P.Nome
  PRINT #F, "Record 2: Eta = " + STR$(P.Eta)
  PRINT #F, "Record 3: Fine test"
END USING
PRINT "File scritto e chiuso automaticamente."

' --- USING con OPEN per lettura ---
PRINT ""
PRINT "Lettura file con USING..."
USING F = OPEN("test_using_write.txt", INPUT)
  DIM Linea$
  DO WHILE NOT EOF(F)
    INPUT #F, Linea$
    PRINT "Letto: "; Linea$
  LOOP
END USING
PRINT "File letto e chiuso automaticamente."

PRINT ""
PRINT "Usando KILL per pulire..."
KILL "test_using_write.txt"
PRINT "File eliminato."

' --- Verifica che la variabile NON esista più fuori dal USING ---
' Se la prossima riga fosse: PRINT F, causerebbe un errore
' perché F esiste solo dentro il blocco USING.

PRINT ""
PRINT "Test USING completato con successo."
