' FILE: Console/ConsoleAdvanced.bas
' =================================================================================================
'       GUIDA TEORICA: I/O AVANZATO, GRAFICA E INTROSPEZIONE DI SISTEMA
' =================================================================================================
'
' Questo test verifica le capacità di Input/Output avanzate di GM-BASIC, dimostrando la
' portabilità del codice tra modalità Console (testuale) e modalità Grafica (SDL).
'
' 1. INTROSPEZIONE DI SISTEMA (`SYS`)
'    La funzione `SYS(chiave$)` permette al programma di adattarsi all'ambiente di esecuzione.
'    - `SYS("UI")`: Restituisce il backend grafico in uso ("CONSOLE" o "SDL").
'    - `SYS("OS")`: Restituisce il sistema operativo host.
'    - Utile per eseguire codice condizionale (es. saltare test grafici se si è in un terminale puro).
'
' 2. MODALITÀ IBRIDA (Console e Grafica)
'    GM-BASIC permette di passare dalla modalità testo alla modalità grafica a runtime.
'    - `SCREEN n`: Attiva una modalità grafica (es. 12 = 640x480).
'    - `FONT path, size`: In modalità grafica, è OBBLIGATORIO caricare un font per usare `PRINT`.
'      Senza font, i comandi di testo non producono output visibile.
'    - Polimorfismo di I/O: Comandi come `PRINT`, `LOCATE`, `COLOR` e `CLS` funzionano in
'      entrambe le modalità, astraendo la complessità sottostante.
'
' 3. MANIPOLAZIONE AVANZATA DEL TESTO
'    - `TAB(n)`: Funzione speciale usata dentro `PRINT` per spostare il cursore a una colonna assoluta.
'      Se l'indice è minore della posizione corrente, va a capo alla riga successiva.
'    - `CELL(row, col, attr)`: Legge direttamente dal buffer dello schermo (solo modalità testo).
'      attr 0 = carattere ASCII, attr 1 = attributo colore.
'    - `CELLSTR(row, len)`: Legge una stringa direttamente dal buffer dello schermo.
'    - Line Wrapping: Il sistema gestisce automaticamente l'a capo se il testo supera la larghezza.
'
' 4. GESTIONE DELLA TAVOLOZZA (`PALETTE`)
'    - `PALETTE index, r, g, b`: Ridefinisce un colore della console/schermo.
'    - Le modifiche sono immediate e retroattive (il testo già stampato con quell'indice cambia colore).
'
' =================================================================================================

1 REM SampleAdvancedIo - Test avanzato I/O su Console e Schermo Grafico
2 CLS
3 COLOR 15, 1
10 PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
11 PRINT "║ ConsoleAdvanced - Test di TAB, CELL, WRAP, PALETTE e SYS                     ║"
12 PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
13 PRINT

' Stampa informazioni sull'ambiente di esecuzione corrente.
15 PRINT "--- INFORMAZIONI DI SISTEMA ---"
16 PRINT "UI:", SYS("UI"), "OS:", SYS("OS"), "ARCH:", SYS("ARCH")
17 PRINT "VERSION:", SYS("VERSION"), "RUNTIME:", SYS("RUNTIME")
18 PRINT

20 REM --- ESECUZIONE TEST IN MODALITÀ CONSOLE ---
21 COLOR 10,0 : PRINT "--- INIZIO TEST IN MODALITÀ CONSOLE ---" : COLOR 15,0
22 PRINT
' Esegue la suite di test standard usando la console testuale predefinita.
23 GOSUB 1000 : REM Chiama la subroutine di test I/O
24 PRINT : PRINT "Test su console terminati."

28 REM Se la UI non è SDL (es. siamo in un terminale headless), il test finisce qui.
29 IF SYS("UI") <> "SDL" THEN GOTO 90

30 REM --- PASSAGGIO ALLA MODALITÀ GRAFICA ---
31 PRINT "Premi un tasto per passare alla modalità grafica..."; : WAITKEY
' Attiva la modalità grafica 640x480. Da qui in poi, l'output è renderizzato su una finestra SDL.
32 SCREEN 12 : REM Passa a 640x480, 16 colori

33 REM 1. Carica un FONT. Senza questo, nessun PRINT funzionerà in modalità grafica.
' In modalità grafica, i caratteri sono texture. Serve un file font valido.
34 FONT "font.ttf", 16

35 REM 2. Imposta i colori DESIDERATI per lo sfondo.
' COLOR , 1 imposta il blu come sfondo predefinito per i successivi CLS.
36 COLOR , 1 : REM Sfondo Blu

37 REM 3. Pulisci lo schermo ORA, usando il nuovo colore di sfondo.
38 CLS

39 COLOR 10 : PRINT "--- INIZIO TEST IN MODALITÀ GRAFICA (SCREEN 12) ---" : COLOR 15
40 PRINT

41 REM --- ESECUZIONE TEST IN MODALITÀ GRAFICA ---
' Riutilizza la STESSA subroutine. Dimostra che i comandi I/O sono polimorfici.
42 GOSUB 1000 : REM Chiama la STESSA subroutine di test I/O
43 PRINT : PRINT "Test su schermo grafico terminati."

90 PRINT : PRINT "PROGRAMMA TERMINATO."
99 END

1000 REM --- SUBROUTINE DI TEST I/O ---
1010 REM Questo codice viene eseguito sia su console che su schermo grafico.

1020 REM --- TEST DI PALETTE ---
1021 COLOR 14, 4 : PRINT "1. TEST PALETTE: " : COLOR 15, 1
1022 PRINT "Modifico il colore 14 (Giallo) in Fucsia (255,0,255)."
' Cambia la definizione del colore 14. Tutto il testo stampato con questo colore cambierà aspetto.
1023 PALETTE 14, 255, 0, 255
1024 PRINT "(L'effetto è visibile solo su console SDL o schermo grafico)"
1025 GOSUB 2000 : REM Pausa

1030 REM --- TEST DI CELL (solo se non in grafica) ---
' Le funzioni CELL/CELLSTR leggono il buffer testuale. In modalità grafica pura potrebbero non essere supportate.
' SCREENINFO().ModeID > 0 indica una modalità grafica.
1031 IF SCREENINFO().ModeID > 0 THEN GOTO 1050
1032 COLOR 11, 0 : PRINT "2. TEST CELL: " : COLOR 15, 0
1033 LOCATE 20, 5 : COLOR 10, 5 : PRINT "Testo di Prova"
' Legge i caratteri e gli attributi direttamente dalla memoria video della console.
1034 C$ = CELLSTR(20, 8) ' Legge 8 caratteri dalla riga 20
1035 FG = CELL(20, 8, 0) ' Legge il codice ASCII del carattere in 20,8
1036 BG = CELL(20, 8, 1) ' Legge l'attributo colore
1037 LOCATE 22, 1
1038 PRINT "Ho letto la cella (20, 8): Carattere='"; C$; "', FG="; FG; ", BG="; BG
1039 GOSUB 2000 : REM Pausa

1050 REM --- TEST DI TAB(x) ---
1051 COLOR 13, 0 : PRINT "3. TEST TAB:" : COLOR 15, 1
' TAB(n) sposta il cursore alla colonna n della riga corrente.
1052 PRINT "Colonna 1"; TAB(20); "Colonna 20"; TAB(40); "Colonna 40"
' Se n è minore della posizione corrente, TAB va a capo alla riga successiva.
1053 PRINT "Test di TAB a ritroso: "; TAB(10); "** A capo, colonna 10 **"
1054 GOSUB 2000 : REM Pausa

1060 REM --- TEST DI LINE WRAPPING ---
1061 COLOR 9, 0 : PRINT "4. TEST LINE WRAPPING:" : COLOR 15, 1
' Verifica che il testo lungo vada a capo automaticamente senza spezzare le parole impropriamente.
1062 PRINT "Questa è una frase molto, molto lunga che è stata progettata appositamente per superare la larghezza dello schermo, forzando un 'a capo' automatico. La parola 'automatico' dovrebbe apparire correttamente all'inizio della riga successiva."
1063 GOSUB 2000 : REM Pausa

1070 RETURN

2000 REM --- SUBROUTINE DI PAUSA ---
2010 PRINT : PRINT "Premi un tasto per continuare...";
2020 WAITKEY
2030 PRINT : PRINT
2040 RETURN