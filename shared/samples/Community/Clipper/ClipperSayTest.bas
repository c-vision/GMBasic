' =============================================================================
' ClipperSayTest.bas — Test automatico @...SAY (headless)
' =============================================================================

' Test 1: SAY base
@ 1, 1 SAY "Hello World"
PRINT "T1_OK"

' Test 2: SAY con PICTURE numerica
DIM prezzo AS DOUBLE
prezzo = 1234.56
@ 2, 1 SAY "Prezzo:"
@ 2, 10 SAY prezzo PICTURE "999,999.99"
PRINT "T2_OK"

' Test 3: SAY con espressione
DIM nome AS STRING
nome = "Mario"
@ 3, 1 SAY "Benvenuto " + nome
PRINT "T3_OK"

' Test 4: SAY multipli
@ 5, 1 SAY "Riga 1"
@ 6, 1 SAY "Riga 2"
@ 7, 1 SAY "Riga 3"
PRINT "T4_OK"

' Test 5: PICTURE su stringa
DIM codice AS STRING
codice = "ABC123"
@ 8, 1 SAY codice PICTURE "@!"
PRINT "T5_OK"

' Test 6: Date
DIM oggi AS STRING
oggi = DATE$()
@ 9, 1 SAY "Data: " + oggi
PRINT "T6_OK"

PRINT "ALL_SAY_TESTS_PASSED"
