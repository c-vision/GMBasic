' FILE: Functions/FunctionsMain.bas
' =================================================================================================
'       GUIDA TEORICA: PUNTO DI INGRESSO E CODICE IBRIDO
' =================================================================================================
'
' Questo test dimostra una delle caratteristiche chiave di GM-BASIC per la retrocompatibilità:
' la gestione di programmi "ibridi" che contengono sia una procedura `SUB Main` che codice
' con numeri di riga.
'
' 1. PRIORITÀ DEL PUNTO DI INGRESSO (ENTRY POINT)
'    Quando un programma contiene sia una `SUB Main` che codice numerato, GM-BASIC dà
'    **priorità assoluta alla `SUB Main`**.
'    - `SUB Main()`: Viene eseguita per prima e agisce come punto di ingresso principale
'      dell'applicazione.
'    - Codice Numerato: Il codice con numeri di riga a livello di modulo (fuori da qualsiasi
'      `SUB` o `FUNCTION`) viene ignorato all'avvio del programma. Non viene eseguito
'      automaticamente.
'
' 2. INTERAZIONE TRA CODICE MODERNO E LEGACY
'    Nonostante il codice numerato non venga eseguito all'avvio, rimane parte del programma
'    e può essere invocato esplicitamente dal codice moderno:
'    - `GOSUB <numero_riga>`: Dalla `SUB Main` (o da qualsiasi altra procedura), è possibile
'      saltare a una subroutine legacy definita da numeri di riga.
'    - `GOTO <numero_riga>`: È possibile anche eseguire salti incondizionati verso il codice
'      numerato.
'
' Questo approccio permette una migrazione graduale da uno stile di programmazione legacy
' (basato su numeri di riga e GOTO/GOSUB) a uno stile moderno e strutturato (basato su
' procedure nominate come `SUB Main`), consentendo ai due stili di coesistere e interagire
' nello stesso file.
'
' =================================================================================================

' --- PUNTO DI INGRESSO PRINCIPALE ---
' La presenza di SUB Main fa sì che l'esecuzione del programma parta da qui.
SUB Main
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsMain - Programma Ibrido (SUB Main)                            ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Esecuzione iniziata da SUB Main."
    PRINT "Chiamo una funzione..."
    
    ' Chiama una funzione moderna definita a livello di modulo.
    RISULTATO = TestFunc()
    PRINT "TestFunc() ha restituito:"; RISULTATO
    
    PRINT "Ora eseguo un GOSUB verso una routine numerata..."
    ' Salta alla subroutine definita a partire dalla riga 100.
    GOSUB 100
    
    PRINT "Tornato da GOSUB. Esecuzione di Main terminata."
END SUB

' --- Funzione ausiliaria ---
FUNCTION TestFunc() AS INTEGER
    PRINT "  (Dentro TestFunc)"
    RETURN 42
END FUNCTION

' ======================================================================
' --- CODICE CON NUMERI DI RIGA (NON ESEGUITO ALL'AVVIO) ---
' Questo blocco viene ignorato all'avvio perché è presente una SUB Main.
' Viene eseguito solo se chiamato esplicitamente.
' ======================================================================

10 REM Questo codice non viene eseguito all'avvio.
20 PRINT "ERRORE: La riga 20 non doveva essere eseguita."
30 END

' --- Subroutine legacy accessibile tramite GOSUB ---
100 REM Routine GOSUB numerata
110 PRINT "  (Dentro la routine GOSUB alla linea 100)"
' Ritorna al chiamante (la SUB Main).
120 RETURN