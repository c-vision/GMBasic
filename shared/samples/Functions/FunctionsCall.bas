' FILE: Functions/FunctionsCall.bas
' =================================================================================================
'       GUIDA TEORICA: CHIAMATE A FUNZIONI, PASSAGGIO PARAMETRI E SCOPE
' =================================================================================================
'
' Questo test esplora i meccanismi fondamentali di definizione e chiamata di procedure
' (FUNCTION e SUB) in GM-BASIC.
'
' 1. `FUNCTION` vs `SUB`
'    - `FUNCTION`: Una procedura che restituisce un valore. Il valore di ritorno viene
'      assegnato al nome della funzione stessa (es. `Somma = A + B`) o tramite `RETURN valore`.
'    - `SUB`: Una procedura che esegue azioni ma non restituisce un valore.
'
' 2. PASSAGGIO PARAMETRI
'    GM-BASIC supporta due modalità di passaggio dei parametri:
'    - `BYVAL` (Per Valore - Default): Viene passata una *copia* del valore. Qualsiasi
'      modifica al parametro all'interno della funzione NON si riflette sulla variabile originale
'      del chiamante.
'    - `BYREF` (Per Riferimento): Viene passato un *riferimento* alla variabile originale.
'      Le modifiche al parametro all'interno della funzione modificano direttamente la
'      variabile del chiamante.
'
' 3. PARAMETRI OPZIONALI E VALORI DI DEFAULT
'    È possibile definire parametri che il chiamante può omettere.
'    - `OPTIONAL`: Rende il parametro non obbligatorio.
'    - `valore_default`: Si può specificare un valore di default (`param AS TIPO = valore`).
'      Se il parametro viene omesso, assumerà questo valore.
'
' 4. SCOPE DELLE FUNZIONI
'    - Funzioni Globali (Top-level): Definite a livello di modulo, sono visibili in tutto il file.
'    - `LOCAL FUNCTION`: Definite all'interno di un'altra procedura (`SUB` o `FUNCTION`).
'      Sono visibili solo all'interno della procedura che le contiene, promuovendo l'incapsulamento.
'
' =================================================================================================

' --- Esempio 1: Funzione Globale Semplice ---
' Questa funzione è definita a livello di modulo ed è visibile ovunque.
' I parametri A e B sono passati BYVAL (per valore) di default.
FUNCTION Somma(A, B) AS LONG
    PRINT "Dentro Somma: A="; A; ", B="; B
    ' Il valore di ritorno viene assegnato al nome della funzione.
    Somma = A + B
    PRINT "Dentro Somma: Ritorno "; Somma
END FUNCTION

' --- Esempio 2: Funzione con Parametro per Riferimento (BYREF) ---
' Questa funzione modifica la variabile originale passata dal chiamante.
FUNCTION ModificaByRef(BYREF Valore AS LONG) AS LONG
    PRINT "Dentro ModificaByRef - Valore iniziale:", Valore
    Valore = Valore * 10
    ModificaByRef = Valore ' Restituisce anche il nuovo valore
END FUNCTION

' --- Esempio 3: Funzione con Parametri Opzionali ---
FUNCTION Saluto(Nome AS STRING, OPTIONAL Titolo AS STRING = "Sig.") AS STRING
    Saluto = "Ciao, " + Titolo + " " + Nome
END FUNCTION

' --- Programma Principale ---
SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsCall - Chiamate a Funzioni, BYREF e Parametri Opzionali            ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT "Inizio Main"
    PRINT
    
    ' --- Test Chiamata Semplice ---
    PRINT "--- 1. Chiamata a Funzione Globale ---"
    DIM Result AS LONG
    Result = Somma(10, 5)
    PRINT "Fine chiamata a Somma. Risultato:"; Result
    PRINT
    
    ' --- Test Passaggio BYREF ---
    PRINT "--- 2. Chiamata con Parametro BYREF ---"
    DIM MyVar AS LONG = 7
    PRINT "Valore di MyVar prima della chiamata:", MyVar
    Result = ModificaByRef(MyVar)
    PRINT "Valore di MyVar DOPO la chiamata:", MyVar; "(Atteso: 70)"
    PRINT "Risultato di ModificaByRef:", Result
    PRINT
    
    ' --- Test Parametri Opzionali ---
    PRINT "--- 3. Chiamata con Parametri Opzionali ---"
    PRINT Saluto("Rossi") ' Usa il valore di default per Titolo
    PRINT Saluto("Verdi", "Dott.") ' Sovrascrive il valore di default
    PRINT
    
    ' --- Test Funzione Locale ---
    PRINT "--- 4. Chiamata a Funzione Locale ---"
    
    ' Questa funzione è definita DENTRO Main e visibile solo qui.
    FUNCTION Quadrato(N AS LONG) AS LONG
    Quadrato = N * N
    END FUNCTION
    
    Result = Quadrato(9)
    PRINT "Il quadrato di 9 è:"; Result
    PRINT
    
    PRINT "Fine Main."
END SUB
