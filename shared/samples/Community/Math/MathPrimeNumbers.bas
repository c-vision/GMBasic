' FILE: Math/MathPrimeNumbers.bas
' ======================================================================================================================
' GUIDA TEORICA: SCOPE DELLE VARIABILI DI CICLO E OTTIMIZZAZIONE
' ======================================================================================================================
' Questo programma calcola i numeri primi utilizzando cicli `FOR...NEXT` annidati.
' È un eccellente caso di studio per due concetti chiave del compilatore GM-BASIC:
'
' 1. SCOPE DELLE VARIABILI
'    - GM-BASIC utilizza lo scope lessicale (statico). Una variabile è visibile solo all'interno del blocco
'      in cui è dichiarata (o in blocchi figli).
'    - Variabili di Ciclo (I, J): Nel BASIC tradizionale, la variabile di un ciclo FOR è spesso visibile
'      anche al di fuori del ciclo stesso. GM-BASIC, tramite l'ottimizzazione di "loop flattening",
'      promuove queste variabili allo scope della procedura o del modulo, mimando questo comportamento
'      ma garantendo che ogni ciclo annidato abbia il suo contesto corretto.
'    - Variabili Locali al Corpo (C): La variabile `C` viene dichiarata (implicitamente) all'interno del
'      ciclo esterno. Viene resettata a 0 ad ogni nuova iterazione di `I`.
'
' 2. OTTIMIZZAZIONE MATEMATICA E BUILT-IN
'    - `SQR(I)`: L'uso della radice quadrata (`SQR`) è un'ottimizzazione classica per i test di primalità.
'      Il ciclo interno deve controllare i divisori solo fino alla radice quadrata del numero.
'    - `CINT(expr)`: Questa funzione converte un'espressione in un intero, troncando la parte decimale.
'      L'espressione `I/J = CINT(I/J)` è un modo per verificare se `J` è un divisore esatto di `I`.
'
' 3. FLUSSO DI ESECUZIONE
'    - Il ciclo esterno (controllato da `I`) itera su tutti i numeri da 2 a N.
'    - Per ogni numero `I`, un flag `C` (per "Composto") viene resettato a 0.
'    - Il ciclo interno (controllato da `J`) itera sui possibili divisori di `I`.
'    - Se viene trovato un divisore, il flag `C` viene impostato a 1 e il ciclo interno prosegue
'      (un `EXIT FOR` potrebbe ottimizzarlo ulteriormente).
'    - Dopo il ciclo interno, se `C` è ancora 0, significa che non sono stati trovati divisori,
'      e quindi `I` è un numero primo e viene stampato.
' ======================================================================================================================

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ MathPrimeNumbers - Numeri Primi con Test di Scope                           ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' Calcolo dei numeri primi fino a N
10 REM INPUT "N: ";N
15 N = 50

' Ciclo esterno: itera su ogni numero da testare
20 FOR I=2 TO N
    ' Flag per indicare se il numero è composto (non primo). Resettato ad ogni iterazione.
30     C=0
    
    ' Ciclo interno: cerca un divisore.
    ' Ottimizzazione: si cerca solo fino alla radice quadrata di I.
40     FOR J=2 TO SQR(I)
        ' Se I/J è un numero intero, allora J è un divisore.
50         IF I/J=CINT(I/J) THEN C=1
60     NEXT J
    
    ' Se il flag C non è stato impostato, nessun divisore è stato trovato, quindi il numero è primo.
70     IF C=0 THEN PRINT I
80 NEXT I

90 END
