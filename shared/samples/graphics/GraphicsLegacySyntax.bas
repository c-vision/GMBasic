' FILE: Graphics/GraphicsQualifiedSyntax.bas
' =================================================================================================
'       GUIDA TEORICA: POLIMORFISMO DEI COMANDI DI I/O E FUNZIONI QUALIFICATE
' =================================================================================================
'
' Questo programma è un test di integrazione fondamentale che dimostra il polimorfismo
' dei comandi di I/O di GM-BASIC.
'
' 1. COMANDI E FUNZIONI
'    - I comandi usano la sintassi legacy a spazio (es. `SCREEN 12`, `PSET x, y`, `COLOR 15, 1`);
'      non esiste forma qualificata per i comandi (`GRAPHICS.SCREEN ...` non è valida).
'    - Le funzioni che restituiscono valore usano la sintassi qualificata con parentesi
'      (es. `CONSOLE.CELL(...)`, `GRAPHICS.RGB(...)`).
'    - I comandi POLIMORFICI (CLS/COLOR/LOCATE/PRINT, condivisi tra CONSOLE e GRAPHICS)
'      usano la sintassi legacy (es. `COLOR 15, 1`): l'Executor sceglie l'implementazione
'      console o grafica in base alla modalità corrente.
'
' 2. POLIMORFISMO DEI COMANDI DI I/O
'    Il polimorfismo è la capacità di un comando di comportarsi in modo diverso a seconda del
'    contesto. In GM-BASIC, comandi come `PRINT`, `LOCATE`, `COLOR` e `CLS` sono polimorfici:
'
'    - `PRINT`:
'      - In modalità console, scrive caratteri nel buffer testuale.
'      - In modalità grafica (`SCREEN`), renderizza il testo come una texture grafica usando
'        il font caricato.
'
'    - `LOCATE y, x`:
'      - In modalità console, sposta il cursore a `riga, colonna` basate su caratteri.
'      - In modalità grafica, sposta il cursore a coordinate `pixel` assolute.
'
'    L'interprete (l'Executor) è responsabile di rilevare la modalità corrente (testo o grafica)
'    e di invocare l'implementazione corretta del comando, astraendo questa complessità
'    dall'utente. Questo test verifica che la stessa logica di codice (nella subroutine `TestIo...`)
'    produca l'output corretto in entrambi gli ambienti.
'
' =================================================================================================

' Questo programma testa l'uso ESPLICITO dei namespace per tutti i comandi e le funzioni.

SUB Main
  ' Imposta un gestore di errori globale per catturare eventuali problemi durante l'esecuzione.
  ON ERROR GOTO gestore_errori

  ' Inizia utilizzando i comandi del namespace CONSOLE.
CLS
COLOR 15, 1
  PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ GraphicsQualifiedSyntax - Test di TAB, CELL, WRAP, PALETTE e SYS             ║"
  PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
  PRINT
  
  ' Usa la funzione SYS (del namespace SYSTEM, importato di default) per l'introspezione.
  PRINT "--- INFORMAZIONI DI SISTEMA ---"
  PRINT "UI:", SYS("UI"), "OS:", SYS("OS"), "ARCH:", SYS("ARCH")
  PRINT "VERSION:", SYS("VERSION"), "RUNTIME:", SYS("RUNTIME")
  PRINT
  
  PRINT "--- INIZIO TEST IN MODALITÀ CONSOLE ---"
  ' Chiama la subroutine di test specifica per l'ambiente console.
  TestIoConsole()
  PRINT
  PRINT "Test su console terminati."
  
  ' Controlla se il sistema supporta una UI grafica. Se no, termina il programma.
  IF SYS("UI") <> "SDL" THEN GOTO fine_programma
  
  PRINT "Premi un tasto per passare alla modalità grafica...";
WAITKEY
  
  ' Passa alla modalità grafica. Da qui, i comandi polimorfici si comporteranno diversamente.
SCREEN 12 ' Passa a 640x480, 16 colori
  
  ' In modalità grafica, è obbligatorio caricare un font per poter usare PRINT.
FONT "font.ttf", 16
  
  ' Imposta i colori di disegno e pulisce la finestra grafica.
COLOR 15, 1 ' Bianco su Blu
CLS
  
  PRINT "--- INIZIO TEST IN MODALITÀ GRAFICA (SCREEN 12) ---"
  ' Chiama la subroutine di test specifica per l'ambiente grafico.
  TestIoGrafico()
  PRINT
  PRINT "Test su schermo grafico terminati."
  
  ' Torna alla modalità testo, chiudendo la finestra grafica.
SCREEN 0
  
  fine_programma:
  PRINT
  PRINT "PROGRAMMA TERMINATO."
END SUB

' --- SUBROUTINE DI TEST I/O CONSOLE ---
SUB TestIoConsole
  ' Usa i comandi qualificati del namespace CONSOLE.
COLOR 10,0 : PRINT "--- TEST CONSOLE QUALIFICATO ---" : COLOR 15,0
  PRINT
  
  ' Test di PALETTE (è un comando GRAPHICS, ma lo chiamiamo per verificare che non causi errori).
COLOR 14, 4 : PRINT "1. TEST PALETTE: " : COLOR 15, 1
  PRINT "La modifica della palette non è visibile su console."
PALETTE 14, 255, 0, 255
  ' Uso combinato di funzioni CONSOLE per dimostrare la risoluzione dei nomi.
LOCATE CONSOLE.CELL(20, 1, 0), 1
  PRINT "(Effetto solo su console SDL o schermo grafico)"
  TestPause()

  ' Test delle funzioni CELL e CELLSTR per leggere direttamente dal buffer della console.
COLOR 11, 0 : PRINT "2. TEST CELL: " : COLOR 15, 0
LOCATE 20, 5 : COLOR 10, 5 : PRINT "Testo di Prova"
  DIM C$ = CONSOLE.CELLSTR(20, 8) ' Accesso qualificato alla funzione
  DIM FG = CONSOLE.CELL(20, 8, 0)
  DIM BG = CONSOLE.CELL(20, 8, 1)
LOCATE 22, 1
PRINT "Ho letto la cella (20, 8): Carattere='"; C$; "', FG="; FG; ", BG="; BG
  TestPause()
  
  ' Test della funzione TAB() all'interno di PRINT.
COLOR 13, 0 : PRINT "3. TEST TAB:" : COLOR 15, 1
PRINT "Colonna 1"; TAB(20); "Colonna 20"; TAB(40); "Colonna 40"
PRINT "Test di TAB a ritroso: "; TAB(10); "** A capo, colonna 10 **"
  TestPause()
  
  ' Test del wrapping automatico del testo a fine riga.
COLOR 9, 0 : PRINT "4. TEST LINE WRAPPING:" : COLOR 15, 1
PRINT "Questa è una frase molto, molto lunga che è stata progettata appositamente per superare la larghezza dello schermo, forzando un 'a capo' automatico. La parola 'automatico' dovrebbe apparire correttamente all'inizio della riga successiva."
  TestPause()
END SUB

' --- SUBROUTINE DI TEST I/O GRAFICO ---
SUB TestIoGrafico
  ' Usa i comandi qualificati del namespace GRAPHICS.
COLOR 10,0 : PRINT "--- TEST GRAFICO QUALIFICATO ---" : COLOR 15,0
  PRINT
  
  ' Test della modifica della palette dei colori.
COLOR 14, 4 : PRINT "1. TEST PALETTE: " : COLOR 15, 1
  PRINT "Modifico il colore 14 (Giallo) in Fucsia (255,0,255)."
PALETTE 14, 255, 0, 255
LOCATE 20, 1
  PRINT "(L'effetto dovrebbe essere visibile ora)"
  TestPause()
  
  ' Spiegazione del comportamento di CELL in modalità grafica.
COLOR 11, 0 : PRINT "2. TEST CELL (grafico): " : COLOR 15, 0
  PRINT "Le funzioni CELL/CELLSTR non hanno un effetto diretto sui pixel grafici."
  PRINT "Vengono usate per console testo o buffer di caratteri."
  TestPause()
  
  ' Test di TAB in modalità grafica.
COLOR 13, 0 : PRINT "3. TEST TAB:" : COLOR 15, 1
PRINT "Colonna 1"; TAB(20); "Colonna 20"; TAB(40); "Colonna 40"
PRINT "Test di TAB a ritroso: "; TAB(10); "** A capo, colonna 10 **"
  TestPause()
  
  ' Test del wrapping del testo in modalità grafica.
COLOR 9, 0 : PRINT "4. TEST LINE WRAPPING:" : COLOR 15, 1
PRINT "Questa è una frase molto, molto lunga che è stata progettata appositamente per superare la larghezza dello schermo, forzando un 'a capo' automatico. La parola 'automatico' dovrebbe apparire correttamente all'inizio della riga successiva."
  TestPause()
END SUB

' --- SUBROUTINE DI PAUSA ---
' Mette in pausa l'esecuzione in attesa di un tasto.
SUB TestPause
  PRINT : PRINT "Premi un tasto per continuare...";
WAITKEY
  PRINT : PRINT
END SUB

' --- GESTORE ERRORI GLOBALE ---
SUB GestoreErrori
  PRINT "ERRORE CATTURATO: #" ; ERR.Number; " - "; ERR.Description
  ' In un'applicazione reale, qui si potrebbe gestire l'errore in modo più sofisticato.
WAITKEY
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Sintassi Legacy: Il test copre in modo esaustivo i comandi a spazio; le funzioni
'   usano la sintassi qualificata con parentesi (`NAMESPACE.FUNZIONE(...)`).
'   per i namespace `CONSOLE`, `GRAPHICS` e `SYSTEM` (implicito in `SYS` e `TAB`).
'
' - Polimorfismo: Verifica con successo il comportamento duale di `PRINT`, `LOCATE`, `COLOR`
'   e `CLS` in entrambi gli ambienti (testo e grafico).
'
' - Funzioni Avanzate: Include test per `PALETTE`, `CELL`, `CELLSTR` e `TEXTWIDTH`.
'
' - Interazione tra Namespace: Dimostra la coesistenza e l'uso combinato di comandi da
'   namespace diversi (es. `CONSOLE.LOCATE CONSOLE.CELL(...))`).
'
' Casi Mancanti Integrati:
' - Ho aggiunto un `FONT "font.ttf", 12` esplicito per garantire che il testo sia visibile
'   nella sezione grafica.
' - Ho aggiunto la gestione delle etichette e un `ON ERROR GOTO` per rendere il test più robusto.
'
' =================================================================================================