' FILE: Console/ConsoleKeyboardInput.bas
' =================================================================================================
'       GUIDA TEORICA: INPUT DA TASTIERA E INTERAZIONE UTENTE
' =================================================================================================
'
' Questo test copre l'intero spettro delle modalità di input da tastiera offerte da GM-BASIC,
' dalle primitive a basso livello per singoli tasti all'input di campi strutturati.
'
' 1. `GET` (Input Singolo Bloccante)
'    - Sintassi: `GET variabile$`
'    - Comportamento: Ferma l'esecuzione finché l'utente non preme un tasto.
'      Il carattere corrispondente viene memorizzato nella variabile.
'    - Uso tipico: "Premi un tasto per continuare", menu a scelta singola (A/B/C).
'
' 2. `INKEY` (Input Singolo Non-Bloccante)
'    - Sintassi: `INKEY variabile$`
'    - Comportamento: Controlla istantaneamente il buffer della tastiera.
'      Se c'è un tasto premuto, lo restituisce subito. Se no, restituisce una stringa vuota ("").
'      L'esecuzione NON si ferma.
'    - Uso tipico: Cicli di gioco, animazioni interattive, polling continuo.
'
' 3. `WAITKEY` (Pausa)
'    - Sintassi: `WAITKEY`
'    - Comportamento: Simile a `GET`, ma scarta il risultato. Serve solo a mettere in pausa
'      il programma finché non avviene un evento di tastiera.
'
' 4. `INPUT` (Input Strutturato)
'    - Sintassi: `INPUT "Prompt"; var1, var2, ...`
'    - Comportamento: Stampa il prompt (opzionale), aspetta che l'utente scriva e prema Invio.
'      Divide l'input in base alle virgole e assegna i valori alle variabili, effettuando
'      il casting automatico (es. stringa -> numero).
'
' 5. `LINE INPUT` (Input Riga Intera)
'    - Sintassi: `LINE INPUT "Prompt"; variabile$`
'    - Comportamento: Legge tutto ciò che l'utente scrive fino alla pressione di Invio,
'      inclusi spazi, virgole e caratteri speciali, assegnandolo a un'unica variabile stringa.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleKeyboardInput - GET/INKEY/WAITKEY/LINE INPUT/INPUT                ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
100 REM === TEST COMPLETO INPUT DA TASTIERA ===
110
120 PRINT "[130] --- TEST 1: GET (bloccante) ---"
130 PRINT "[140] Premi un tasto qualsiasi...";
' GET ferma il programma. Utile per pause o scelte obbligatorie.
140 GET A$
150 PRINT : PRINT "[160] GET ha ricevuto: '"; A$; "'"
160 PRINT
170
180 PRINT "[190] --- TEST 2: INKEY (non bloccante in un loop) ---"
190 PRINT "[200] Premi 'Q' per continuare...";
200 I = 0
inkey_loop:
' INKEY non ferma il programma. Permette al contatore I di incrementare mentre si aspetta.
210 INKEY B$
' Verifica se è stato premuto 'Q' (case-insensitive manuale qui).
220 IF B$ = "Q" OR B$ = "q" THEN GOTO quit_inkey
225 IF I > 100 THEN GOTO quit_inkey  ' Uscita di sicurezza in headless (INKEY non riceve input: ~1s con SLEEP 10ms)
230 LOCATE 11, 1
240 PRINT "[250] In attesa... Contatore: "; I; " (Premi 'Q')";
250 I = I + 1
' SLEEP è fondamentale nei loop INKEY per non saturare la CPU (busy waiting).
260 SLEEP 10 //Rende il loop meno intensivo per la CPU
270 GOTO inkey_loop
quit_inkey:
280 PRINT : PRINT "[290] INKEY ha ricevuto 'Q'." : PRINT
290
300 PRINT "[300] --- TEST 3: INPUT con prompt e variabili multiple ---"
' INPUT gestisce la conversione di tipo. L'utente deve inserire "Nome, Età".
310 INPUT "Inserisci nome e età (es. Mario, 30): "; NOME$, ETA
320 PRINT "[330] Ciao "; NOME$; ", hai "; ETA; " anni."
330 PRINT
340
350 PRINT "[350] --- TEST 4: LINE INPUT con prompt ---"
' LINE INPUT cattura l'intera riga, utile per frasi con punteggiatura.
360 LINE INPUT "Scrivi la tua frase preferita: "; FRASE$
370 PRINT "[380] La tua frase è: "; FRASE$
380 PRINT
390
400 PRINT "[400] --- TEST 5: WAITKEY (bloccante) ---"
410 PRINT "[420] Premi un tasto qualsiasi per terminare..."
' WAITKEY è una scorciatoia per GET dummy$.
420 WAITKEY
430 PRINT "[440] Test di input terminato."
440 END