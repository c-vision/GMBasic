' FILE: Console/ConsoleKeyboardSemantic.bas
' =================================================================================================
'       GUIDA TEORICA: SEMANTICA DI INPUT (SINCRONO VS ASINCRONO)
' =================================================================================================
'
' Questo test approfondisce le differenze semantiche tra i vari comandi di input,
' inclusa la gestione del tempo, del blocco dell'esecuzione e della simulazione di input.
'
' 1. INPUT SINCRONO (BLOCCANTE)
'    - `GET variabile$`: Arresta il "Program Counter". L'applicazione non consuma cicli CPU
'      inutili, ma l'interfaccia utente è congelata finché l'utente non agisce.
'    - `WAITKEY`: Identico a GET, ma senza memorizzare il risultato.
'    - `INPUT variabile`: Blocca l'esecuzione finché l'utente non preme Invio.
'
' 2. INPUT ASINCRONO (POLLING)
'    - `INKEY variabile$`: Non arresta mai il programma. È progettato per essere inserito in un
'      ciclo. Restituisce una stringa vuota se il buffer della tastiera è vuoto.
'
' 3. INPUT CON TIMEOUT (IBRIDO)
'    - `INKEY variabile$, millisecondi`: Si comporta come GET per un periodo limitato. Se il
'      tempo scade, ritorna stringa vuota.
'    - `CONSOLE.INPUT variabile, timeout_ms)`: Novità del TODO #49. Blocca l'esecuzione
'      fino all'Invio o fino alla scadenza del timeout. Se scade, la variabile non viene modificata.
'    - `CONSOLE.LINEINPUT variabile$, timeout_ms)`: Stesso comportamento di INPUT con timeout, ma per
'      intere righe di testo.
'
' 4. SIMULAZIONE DI INPUT (TESTING AUTOMATICO)
'    - `CONSOLE.SENDKEY stringa$)`: Novità del TODO #47. Inserisce una stringa nel buffer di
'      input della console. I comandi di input successivi (INPUT, INKEY, GET, etc.) leggeranno
'      da questo buffer prima di attendere l'input dell'utente. È fondamentale per i test automatici.
'      Per simulare la pressione del tasto Invio, concatenare `CHR(13)`.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleKeyboardSemantic - Semantica GET/INKEY/INPUT/SENDKEY              ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

10 REM === TEST SEMANTICA GET/INKEY/WAITKEY ===
30
40 PRINT "TEST 1 - GET (bloccante)"
50 PRINT "Attendo la pressione di un tasto..."
' L'esecuzione si ferma qui a tempo indeterminato.
60 GET A$
70 PRINT "GET ha ricevuto: '"; A$; "'"
80 PRINT

100 PRINT "TEST 2 - INKEY senza timeout (non bloccante)"
110 PRINT "Controllo istantaneo della tastiera..."
120 INKEY B$
130 PRINT "INKEY ha ricevuto: '"; B$; "' (stringa vuota se nessun tasto era premuto)"
140 PRINT

160 PRINT "TEST 3 - INKEY con timeout 4 secondi"
170 PRINT "Premi un tasto entro 4 secondi..."
180 INKEY C$, 4000
190 PRINT "INKEY con timeout ha ricevuto: '"; C$; "' (vuoto = scaduto)"
200 PRINT

210 PRINT "TEST 4 - SENDKEY per automazione INPUT"
220 PRINT "Simulazione della digitazione 'Risposta Automatica' + Invio..."
230 SENDKEY "Risposta Automatica" + CHR(13)
240 INPUT "Qual e' la risposta? (verra' compilata automaticamente) -> "; D$
250 PRINT "INPUT ha ricevuto (da SENDKEY): '"; D$; "'"
260 PRINT

270 PRINT "TEST 5 - SENDKEY per automazione LINE INPUT"
280 PRINT "Simulazione della digitazione 'Questa frase, con virgole.' + Invio..."
290 SENDKEY "Questa frase, con virgole." + CHR(13)
300 LINE INPUT "Scrivi una frase (verra' compilata automaticamente) -> "; E$
310 PRINT "LINE INPUT ha ricevuto (da SENDKEY): '"; E$; "'"
320 PRINT

330 PRINT "TEST 6 - INPUT con timeout di 5 secondi"
340 PRINT "Inserisci il tuo nome entro 5 secondi, altrimenti sara' 'Ospite'."
350 DIM Nome$ AS STRING = "Ospite"
360 INPUT "Nome: ", Nome$, 5000
370 IF Nome$ = "Ospite" THEN
380   PRINT "Timeout scaduto! Il nome rimane 'Ospite'."
390 ELSE
400   PRINT "Ciao, "; Nome$; "!"
410 END IF
420 PRINT

430 PRINT "TEST 7 - LINE INPUT con timeout di 5 secondi"
440 PRINT "Scrivi una frase qualsiasi entro 5 secondi..."
450 DIM Frase$ AS STRING = ""
460 LINE INPUT "Frase: ", Frase$, 5000
470 IF Frase$ = "" THEN
480   PRINT "Timeout scaduto! Nessuna frase inserita."
490 ELSE
500   PRINT "Hai scritto: '"; Frase$; "'"
510 END IF
520 PRINT

530 PRINT "TEST 8 - WAITKEY (bloccante)"
540 PRINT "Premi QUALSIASI tasto per terminare il programma..."
550 WAITKEY
560 PRINT "Tasto premuto - fine test."
570 END