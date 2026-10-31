' FILE: LexerStress.bas
' ======================================================================================================================
' GUIDA TEORICA: STRESS TEST DEL LEXER E SOURCE MAPPING
' ======================================================================================================================
' Questo test è progettato per verificare la robustezza del Lexer e, in particolare, la correttezza della
' mappatura delle righe sorgente (Source Map) in presenza di costrutti complessi.
'
' 1. MAPPATURA SORGENTE (SOURCE MAP)
'    - Il compilatore deve mantenere un collegamento preciso tra l'istruzione eseguita e il numero di riga
'      originale nel file sorgente, anche quando il codice viene trasformato dal pre-processore.
'    - Questo è fondamentale per il reporting degli errori e per il debugging.
'
' 2. COMMENTI MISTI
'    - Supporto per REM, apostrofo ('), doppio slash (//) e blocchi C-style (/* ... */).
'    - I commenti devono essere ignorati dal parser ma contati correttamente nella mappa delle righe.
'
' 3. CONTINUAZIONE DI RIGA COMPLESSA
'    - Underscore (_): Unisce la riga successiva alla corrente.
'    - Colon iniziali (:): Unisce la riga alla precedente come nuova istruzione.
'
' 4. RAW STRINGS MULTI-RIGA
'    - Le stringhe delimitate da tripli apici (""") possono contenere newline. Il Lexer deve contare
'      queste righe fisiche per mantenere sincronizzata la mappa delle righe per il codice che segue.
' ======================================================================================================================

10 REM File: test_lexer.bas
20 REM Questo è un test per la mappatura delle righe del Lexer.

' Definizione simbolo per compilazione condizionale
#DEF DEBUG_MODE

50 PRINT "Inizio"  ' Riga 5 originale

' Test commento a linea singola stile C
70 // Riga 7 è un commento

' Test commento multiriga stile C
/* 
    Riga 8 e 9
    sono un commento multiriga 
*/

' Test Pre-processore
#IFDEF DEBUG_MODE
    110 REM Questo blocco è incluso.
    120 PRINT "Modalità Debug Attiva"
#ELSE
    140 REM Questo blocco viene scartato dal pre-processore.
    150 PRINT "Questo non dovrebbe apparire"
#ENDIF

170 ' Riga 17

' Test Continuazione di riga esplicita con espressione matematica
190 PRINT "Valore="; _ ' Continuazione esplicita: RIGA LOGICA 190 CONTINUA
  200 + 42 / _    ' RIGA FISICA SENZA NUMERO DI RIGA
  + 700           ' RIGA FISICA SENZA NUMERO DI RIGA

' Test Raw String che consuma righe fisiche
300 DIM RAW_STR = `
Questa è la prima linea di una raw string.
L'underscore _ non è una continuazione qui.
`

' Test Continuazione su assegnazione variabile
400 DIM Calcolo = 5 + _
    6 - _
    7

' Test Continuazione logica con due punti a capo
450 DIM Risultato
    : PRINT "Test Colon Continuation" ' CONTINUAZIONE CON COLON SULLA RIGA 460
    : PRINT "FATTO"

' Verifica finale per assicurare che il flusso sia arrivato in fondo senza errori di parsing
999 PRINT "Stress Test Completato."