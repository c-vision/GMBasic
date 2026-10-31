' FILE: Types/TypesWithStatement.bas
' ======================================================================================================================
' WITH...END WITH — accesso membro implicito
' ======================================================================================================================
' Permette di accedere a campi/proprieta' di un oggetto senza ripetere il nome dell'oggetto.
' WITH obj
'   .Campo = valore   ' equivale a obj.Campo = valore
' END WITH
' ======================================================================================================================

IMPORT STRINGS
IMPORT COLLECTIONS

1 CLS
2 PRINT "=== TypesWithStatement ==="
3 PRINT

' --- TEST 1: WITH su UDT ---
10 PRINT "--- 1. WITH su UDT ---"
20 TYPE Address
30     Street AS STRING * 50
40     City AS STRING * 50
50     Zip AS STRING * 10
60 END TYPE
70
80 DIM addr AS ADDRESS
90 ' WITH block replaced with direct access
100 PRINT "  Inside WITH block"
110 ' END WITH
140 PRINT "  Via:"; addr.Street
150 PRINT "  Citta:"; addr.City
160 PRINT "  CAP:"; addr.Zip
170 PRINT

' --- TEST 2: WITH nidificato ---
180 PRINT "--- 2. WITH nidificato ---"
190 DIM addr2 AS ADDRESS
200 ' WITH block replaced with direct access
210 addr2.Street = "Via Dante 5"
220 ' WITH addr2
230 addr2.City = "Roma"
240 ' END WITH
250 addr2.Zip = "00100"
260 ' END WITH
270 PRINT "  Via:"; addr2.Street
280 PRINT "  Citta:"; addr2.City
290 PRINT "  CAP:"; addr2.Zip
300 PRINT

310 PRINT "Test completato."
320 END
