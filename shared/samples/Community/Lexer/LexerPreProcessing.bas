' FILE: Lexer/LexerPreProcessing.bas
' ======================================================================================================================
' GUIDA TEORICA: IL PRE-PROCESSORE, COMPILAZIONE CONDIZIONALE E MACRO
' ======================================================================================================================
' Il Lexer di GM-BASIC esegue passate di pre-elaborazione prima di generare i token.
' Queste fasi gestiscono le direttive che iniziano con '#'.
'
' 1. DEFINIZIONE SIMBOLI (Per Compilazione Condizionale)
'    - #DEF <simbolo>: Definisce un simbolo di compilazione. I simboli non hanno valore, esistono o non esistono.
'    - #UNDEF <simbolo>: Rimuove la definizione di un simbolo.
'
' 2. BLOCCHI CONDIZIONALI
'    - #IFDEF <simbolo>: Include il blocco di codice seguente se il simbolo è definito.
'    - #IFUNDEF <simbolo>: Include il blocco se il simbolo NON è definito.
'    - #ELSE: Alterna la condizione di inclusione all'interno di un blocco #IFDEF/#IFUNDEF.
'    - #ENDIF: Termina un blocco condizionale.
'
' 3. MACRO CON PARAMETRI (Sostituzione Testuale a livello di Token)
'    - #DEFINE <NOME_MACRO>(<param1>, <param2>, ...): Definisce una macro che esegue una sostituzione di testo.
'    - Funzionamento: Durante una passata speciale, il pre-processore cerca le chiamate alla macro
'      (es. `NOME_MACRO(val1, val2)`) e sostituisce l'intera chiamata con il corpo della macro,
'      rimpiazzando i parametri (A, B) con i token degli argomenti forniti (val1, val2).
'    - Attenzione: È una sostituzione testuale/token, non una chiamata a funzione. L'espressione
'      risultante deve essere sintatticamente valida nel punto in cui viene inserita.
'
' 4. GESTIONE DEL CODICE ESCLUSO E DELLE MACRO
'    - Il codice all'interno di un blocco condizionale escluso viene rimosso fisicamente dal flusso
'      sorgente visto dal parser.
'    - Anche le definizioni di macro e le stringhe complesse vengono ignorate se si trovano
'      in un blocco escluso.
' ======================================================================================================================

' --- PARTE 1: TEST COMPILAZIONE CONDIZIONALE ---

' Definisce il simbolo DEBUG per questo modulo
#DEF DEBUG

' '#UNDEF DEBUG ' Questa riga è commentata, quindi DEBUG rimane definito

' Verifica se DEBUG NON è definito. Poiché è definito, questo blocco verrà saltato.
#IFUNDEF DEBUG
    5 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    6 PRINT "║ LexerPreProcessing - Compilazione condizionale                              ║"
    7 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    20 REM Inizio blocco escluso
    ' Questo codice contiene sintassi complessa (doppi apici escaped) che viene ignorata
    30 DIM A$ = "Questa stringa ha un ""escaped quote"" e un newline in raw string non supportato dal parser"
    
    ' Questo è un test critico per il Lexer: una raw string con backtick che si estende su più righe.
    ' Il pre-processore deve essere in grado di saltare correttamente questo blocco senza confondersi
    ' sui numeri di riga o sulle direttive.
    40 DIM B$ = `Questa raw string
    50 ha molti a capo
    60 e un altro apice: "`
#ELSE
    ' Questo ramo viene compilato perché DEBUG è definito.
    80 PRINT "DEBUG mode is ON"
#ENDIF
PRINT

' --- PARTE 2: TEST ESPANSIONE MACRO ---
PRINT "--- Test Macro con Parametri ---"

/*
' Definisce una macro semplice per la somma. Il corpo è (A + B).
#DEFINE SOMMA(A, B) (A + B)

' Definisce una macro per calcolare il quadrato.
#DEFINE SQUARE(X) (X * X)

' 1. Uso semplice della macro
DIM Risultato = SOMMA(10, 20)
PRINT "Risultato di SOMMA(10, 20): "; Risultato; " (Atteso: 30)"
' L'espansione trasforma la riga in: DIM Risultato = (10 + 20)

' 2. Uso con variabili ed espressioni complesse
DIM VarA = 5
Risultato = SOMMA(VarA * 2, 3)
PRINT "Risultato di SOMMA(VarA * 2, 3): "; Risultato; " (Atteso: 13)"
' L'espansione trasforma la riga in: Risultato = ((VarA * 2) + 3)

' 3. Uso annidato di macro
Risultato = SQUARE(SOMMA(2, 3))
PRINT "Risultato di SQUARE(SOMMA(2, 3)): "; Risultato; " (Atteso: 25)"
' Passaggi di espansione:
'   - SQUARE((2 + 3))
'   - ((2 + 3) * (2 + 3))
*/

' 4. Macro definita all'interno di un blocco #IFDEF
#IFDEF DEBUG
    #DEF MESSAGGIO(T) PRINT "DEBUG: " + T
#ELSE
    #DEF MESSAGGIO(T) REM
#ENDIF

' Questa chiamata verrà espansa in PRINT "DEBUG: Test macro" perché DEBUG è definito.
//MESSAGGIO("Test macro")

PRINT
PRINT "Test completato."
100 END