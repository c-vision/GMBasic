' FILE: Functions/FunctionsRecursionAndByRef.bas
' =================================================================================================
'       GUIDA TEORICA: RICORSIONE, PASSAGGIO PARAMETRI E PARAMETRI OPZIONALI
' =================================================================================================
'
' Questo test esplora tre meccanismi fondamentali della programmazione procedurale in GM-BASIC:
' la ricorsione, le diverse modalità di passaggio dei parametri e i parametri opzionali.
'
' 1. RICORSIONE
'    Una funzione è "ricorsiva" quando chiama se stessa per risolvere un problema.
'    - Caso Base: Ogni funzione ricorsiva deve avere una "condizione di uscita" (il caso base)
'      che non effettua ulteriori chiamate ricorsive (es. `IF N <= 1 THEN ...`).
'    - Passo Ricorsivo: La funzione richiama se stessa con un input modificato che si avvicina
'      progressivamente al caso base (es. `Fattoriale(N - 1)`).
'    La funzione `Fattoriale` è un esempio classico di ricorsione.
'
' 2. PASSAGGIO PARAMETRI (`BYVAL` vs. `BYREF`)
'    - `BYVAL` (Per Valore - Default): Quando un parametro è dichiarato `BYVAL` (o senza specificatore),
'      la procedura riceve una *copia* del valore. Qualsiasi modifica apportata al parametro
'      all'interno della procedura è locale e non influisce sulla variabile originale del chiamante.
'      `CalcolaArea` usa parametri BYVAL.
'
'    - `BYREF` (Per Riferimento): Quando un parametro è dichiarato `BYREF`, la procedura riceve
'      un *riferimento* alla variabile originale. Qualsiasi modifica al parametro all'interno
'      della procedura si riflette direttamente sulla variabile del chiamante. La `SUB Scambia`
'      usa `BYREF` per scambiare i valori di due variabili esterne.
'
' 3. PARAMETRI OPZIONALI E VALORI DI DEFAULT
'    Una procedura può avere parametri che non è obbligatorio specificare durante la chiamata.
'    - `OPTIONAL`: Rende il parametro facoltativo.
'    - `parametro AS TIPO = valore`: Assegna un valore di default al parametro opzionale. Se il
'      chiamante omette l'argomento, verrà usato il valore di default.
'    La `SUB StampaLinea` e `TestOpzionali` dimostrano questa funzionalità. Tutti i parametri
'    obbligatori devono precedere quelli opzionali.
'
' =================================================================================================

REM --- DEFINIZIONI DI PROCEDURE ---

' Funzione che calcola l'area. I parametri sono BYVAL di default.
FUNCTION CalcolaArea(BYVAL Larghezza, BYVAL Altezza) AS DOUBLE
    Area = Larghezza * Altezza
    ' Utilizza la sintassi di ritorno esplicita con RETURN.
    RETURN Area
END FUNCTION

' Funzione ricorsiva che calcola il fattoriale di un numero.
FUNCTION Fattoriale(N) AS QUAD
    ' Caso base: la condizione di uscita per terminare la ricorsione.
    IF N <= 1 THEN
        ' Utilizza la sintassi di ritorno legacy assegnando al nome della funzione.
        Fattoriale = 1
    ELSE
        ' Passo ricorsivo: la funzione chiama se stessa con un argomento ridotto.
        Fattoriale = N * Fattoriale(N - 1)
    END IF
END FUNCTION

' Subroutine che scambia i valori di due variabili.
' I parametri sono dichiarati BYREF, quindi le modifiche si riflettono all'esterno.
SUB Scambia(BYREF A, BYREF B)
    DIM Temp
    Temp = A
    A = B
    B = Temp
END SUB

' Subroutine che stampa una linea di caratteri.
' Il secondo parametro è opzionale e ha un valore di default "-".
SUB StampaLinea(Lunghezza, OPTIONAL Char AS STRING = "-")
    PRINT DUP(Lunghezza, LEFT(Char, 1))
END SUB

' Subroutine per dimostrare in dettaglio i parametri opzionali.
SUB TestOpzionali(Obbligatorio AS INTEGER, OPTIONAL DefaultDieci AS INTEGER = 10, OPTIONAL Vuoto AS STRING)
    PRINT
    PRINT "--- Esecuzione TestOpzionali ---"
    PRINT "  Obbligatorio: "; Obbligatorio
    PRINT "  DefaultDieci: "; DefaultDieci
    ' Un parametro stringa opzionale senza valore di default viene inizializzato a stringa vuota ("").
    PRINT "  Vuoto (default di STRING): '"; Vuoto; "'"
    StampaLinea(30, ".")
END SUB

' Subroutine che orchestra il test del passaggio BYREF.
SUB TestByRef()
    PRINT
    PRINT "--- Test Passaggio BYREF ---"
    StampaLinea(30) ' Chiama StampaLinea usando il carattere di default.
    DIM X, Y
    X = 100
    Y = 200
    PRINT "Valori prima di Scambia: X="; X; ", Y="; Y
    ' Poiché i parametri di Scambia sono BYREF, X e Y verranno modificati.
    Scambia(X, Y)
    PRINT "Valori dopo Scambia:  X="; X; ", Y="; Y
    IF X = 200 AND Y = 100 THEN
        PRINT "Test BYREF superato."
    ELSE
        PRINT "Test BYREF fallito."
    END IF
END SUB

' Subroutine principale che orchestra tutti i test.
SUB TestCompleto()
    PRINT "--- Test Funzioni e Subroutine ---"
    StampaLinea(35)
    
    DIM A, B, AreaRettangolo
    A = 10
    B = 5
    AreaRettangolo = CalcolaArea(A, B)
    PRINT "Area di un rettangolo "; A; "x"; B; " = "; AreaRettangolo
    
    ' Chiama StampaLinea fornendo il secondo argomento opzionale.
    StampaLinea(15, "#")
    
    DIM NumFatt, RisultatoFatt
    NumFatt = 5
    RisultatoFatt = Fattoriale(NumFatt)
    PRINT "Fattoriale di "; NumFatt; " = "; RisultatoFatt
    
    ' Esegue il test specifico per BYREF.
    TestByRef()
    
    ' Test delle varie combinazioni di parametri opzionali.
    TestOpzionali(1)             ' Usa entrambi i default (10 e "").
    TestOpzionali(2, 20)         ' Sovrascrive il primo default, usa il secondo.
    TestOpzionali(3, 30, "Ciao") ' Fornisce tutti i parametri.
END SUB

' ============================
' --- PROGRAMMA PRINCIPALE ---
' ============================
10 REM Questo programma testa FUNCTION e SUB, inclusi i parametri opzionali
20 CLS
30 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
40 PRINT "║ FunctionsRecursionAndByRef - FUNZIONI RICORSIVE E BYREF                 ║"
50 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
60 PRINT
' L'esecuzione parte da qui e chiama la subroutine che orchestra i test.
70 TestCompleto()
80 PRINT
90 PRINT "Test terminato."
100 END