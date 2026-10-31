' FILE: LexerRawStrings.bas
' ======================================================================================================================
' GUIDA TEORICA: COMPLESSITÀ DEL LEXER
' ======================================================================================================================
' Questo file di test mette alla prova la robustezza del Lexer in scenari complessi che combinano pre-processore,
' raw string, continuazione di riga e commenti.
'
' 1. CONTINUAZIONE DI RIGA
'    - Gestione dell'underscore (_) anche all'interno di espressioni complesse.
'    - Gestione della continuazione con due punti (:) all'inizio della riga.
'
' 2. RAW STRING AVANZATE (backtick `...` o triple quote """...""")
'    - Stringhe multi-riga che contengono caratteri speciali, inclusi gli stessi delimitatori di continuazione (_)
'      o di direttiva (#). Preferire backtick per JSON e strutture con virgolette interne.
'    - Il pre-processore deve essere in grado di distinguere tra un carattere # che inizia una direttiva reale
'      e un carattere # all'interno di una stringa.
'
' 3. DIRETTIVE ANNIDATE E CONTESTO
'    - Verifica che le direttive (#IFDEF) funzionino correttamente anche quando annidate.
'    - Verifica che le direttive all'interno di commenti (/* ... */) o stringhe vengano ignorate.
' ======================================================================================================================

10 REM File: test_lexer_ultimate_v3.bas
20 REM Test finale e completo per il pre-processore del Lexer.

' Definizione simboli per il pre-processore
#DEF DEBUG_MODE
#DEF PRODUCTION
#UNDEF LEGACY_MODE
#UNDEF BOGUS_SYMBOL

' --- SEZIONE 1: TEST ORIGINALI (CONTINUAZIONI, COMMENTI, #IF) ---

50 PRINT "Inizio"  ' Riga 5 originale

' Commento in stile C++
70 // Riga 7 è un commento

' Commento multiriga in stile C
/* 
    Riga 8 e 9
    sono un commento multiriga 
*/

' Test compilazione condizionale annidata
#IFDEF DEBUG_MODE
    110 REM Questo blocco è incluso.
    120 PRINT "Modalità Debug Attiva (Livello 1)"

    #IFDEF PRODUCTION
        125 PRINT "Annidato: Debug e Production sono attivi."
    #ELSE
        127 PRINT "Questo non dovrebbe mai apparire."
    #ENDIF

#ELSE
    140 REM Questo blocco viene scartato dal pre-processore.
    150 PRINT "Questo non dovrebbe apparire"
#ENDIF

170 ' Riga 17

' Test continuazione di riga con underscore (_)
190 PRINT "Valore="; _ ' Continuazione esplicita: RIGA LOGICA 190 CONTINUA
  200 + 42 / _    ' RIGA FISICA SENZA NUMERO DI RIGA
  + 700           ' RIGA FISICA SENZA NUMERO DI RIGA

' Altro test continuazione
300 DIM Calcolo = 5 + _
    6 - _
    7

' Test continuazione con due punti (:) all'inizio
350 DIM Risultato
    : PRINT "Test Colon Continuation" ' CONTINUAZIONE CON COLON
    : PRINT "FATTO"

' --- SEZIONE 2: CASI LIMITE PER RAW STRING ---

' Raw string su singola riga
400 DIM SINGLE_LINE = `Questo è tutto su una riga.`
410 DIM SINGLE_LINE_COLON = `Test: 1` : PRINT "OK"
' Assegnazioni multiple con raw string
420 DIM A = `Hello` : DIM B = `World`
' Raw string contenente doppi apici (senza escape)
430 DIM RAW_WITH_QUOTE = `Questa stringa contiene un " apice.`
' Stringa normale contenente delimitatore raw (caso limite parser)
440 DIM NORMAL_WITH_RAW_DELIMITER = "Il delimitatore è """
' Raw string multi-riga standard
450 DIM RAW_STR_ORIGINAL = `
Questa è la prima linea di una raw string.
L'underscore _ non è una continuazione qui.
`
460 DIM MULTI_LINE_1 = `
Linea 1
Linea 2
` : PRINT "Multi-line 1 Fatto"
' Raw string con continuazione di riga PRIMA dell'inizio
470 DIM MULTI_LINE_2 = _
`
Inizia su _
una nuova riga.
`
480 DIM C = `Fine` ' Commento qui

' Raw string dentro blocco condizionale
#IFDEF PRODUCTION
    490 PRINT `Questo viene stampato in produzione`
#ELSE
    495 PRINT `Questo è per il debug`
#ENDIF

#IFUNDEF LEGACY_MODE
    497 PRINT "Modalità Legacy NON definita. Corretto."
#ENDIF

' Raw string vuota e con soli spazi
500 DIM EMPTY_RAW = ``
510 DIM WHITESPACE_RAW = `

`
' Combinazione complessa: continuazione, raw string multi-riga e statement multipli
520 PRINT "Inizio complesso"; _
    : DIM COMPLEX_RAW = `
Riga A...
Riga B...
` : PRINT "Fine complesso"
530 PRINT "Test 13": DIM RAW_13 = `Inizio Raw 13
Fine Raw 13`

' --- SEZIONE 3: TEST SPECIFICI PER IL BUG DELLE DIRETTIVE IN STRINGHE/COMMENTI ---

700 PRINT "Inizio test direttive..."

' Caso limite 15: Direttiva #IFDEF all'interno di una raw string.
' Il pre-processore NON deve eseguirla, ma trattarla come testo.
710 DIM RAW_WITH_DIRECTIVE = `
Questo testo è parte della stringa.
#IFDEF BOGUS_SYMBOL
    Questo testo DEVE essere incluso, perché è dentro una stringa!
#ENDIF
...e anche questo.
`

' Caso limite 16: Direttiva #DEF all'interno di un commento multi-riga.
' Il pre-processore NON deve eseguirla.
/*
    Questo è un commento.
    #DEF SHOULD_NOT_EXIST = 1
*/

' Verifica che il simbolo del caso 16 non sia stato definito.
#IFDEF SHOULD_NOT_EXIST
    720 PRINT "ERRORE: Il simbolo 'SHOULD_NOT_EXIST' è stato definito illegalmente!"
#ELSE
    730 PRINT "OK: Il simbolo 'SHOULD_NOT_EXIST' non è stato definito."
#ENDIF


800 PRINT "--- TEST ULTIMATE V3 COMPLETATO ---"