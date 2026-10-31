' FILE: Console/ConsoleOnKeyClipboard.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE EVENTI (ON KEY), INTERRUPT E CLIPBOARD
' =================================================================================================
'
' Questo test verifica il sistema di gestione degli eventi da tastiera ("Hook") e la loro
' interazione con il flusso di esecuzione, specialmente durante le operazioni bloccanti.
'
' 1. IL COMANDO `ON KEY`
'    Permette di registrare un "Interrupt Handler" che viene invocato automaticamente quando
'    l'utente preme una specifica combinazione di tasti, indipendentemente da cosa sta
'    facendo il programma principale.
'    - Sintassi: `ON KEY (codice_tasto) AZIONE`
'    - Codici Tasto: costanti stile PowerBASIC, sempre disponibili senza IMPORT. Supporta tasti
'      singoli (`%K_F1`) e combinazioni con modificatori tramite l'helper `KEY.MAKE(%K_X, %K_CTRL)`.
'
' 2. TIPI DI AZIONI
'    - `CALL Subroutine()`: Mette in pausa l'esecuzione corrente, esegue la subroutine e poi
'      riprende dal punto esatto in cui si era interrotta (comportamento da interrupt classico).
'    - `GOTO Etichetta`: Interrompe l'esecuzione corrente e salta incondizionatamente all'etichetta.
'      Lo stack di esecuzione precedente viene scartato (o gestito manualmente).
'    - `STOP`: Deregistra l'hook per quel tasto, ripristinando il comportamento normale.
'
' 3. INTERAZIONE CON `INPUT` (IO BLOCCANTE)
'    Un aspetto critico di GM-BASIC è che gli hook `ON KEY` devono funzionare anche mentre
'    l'interprete è bloccato in un'istruzione `INPUT` o `WAITKEY`. Questo permette di creare
'    menu di aiuto, tasti di uscita rapida o macro anche durante l'attesa di input utente.
'
' 4. CLIPBOARD E INPUT
'    L'istruzione `INPUT` supporta nativamente le operazioni di Clipboard del sistema operativo.
'    Incollando testo (es. Ctrl+V o Ctrl+Shift+V a seconda del terminale) durante un `INPUT`,
'    il testo viene inserito nel buffer di editing come se fosse stato digitato.
'
' =================================================================================================

IMPORT ONKEY

' Subroutine gestore per l'evento F1.
' Essendo chiamata con la sintassi CALL (implicta nelle parentesi), ritornerà al chiamante.
SUB GestoreF1
  PRINT "[Evento] Tasto F1 premuto! Eseguo CALL a GestoreF1."
END SUB

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleOnKeyClipboard - ON KEY, INPUT e Clipboard                       ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT

10 PRINT "--- SETUP DEGLI HOOK DA TASTIERA ---"
' Registra un hook di tipo CALL per F1.
20 ON KEY (%K_F1) GestoreF1()
' Registra un hook di tipo GOTO per Freccia Su.
30 ON KEY (%K_UP) GOTO FrecciaSu
' Registra un hook per una combinazione complessa (CTRL+X) usando la funzione helper MAKE.
40 ON KEY (KEY.MAKE(%K_X, %K_CTRL)) GOTO CombinazioneCtrlX
50 PRINT "Hook registrati per F1, Freccia Su, e CTRL+X."
60 PRINT

100 PRINT "--- TEST INTERCETTAZIONE CTRL+C (MODALITÀ NON BLOCCANTE) ---"
' Sovrascrive il comportamento di default di terminazione del programma.
110 ON KEY (%K_CTRL_C) GOTO GestoreCtrlC
120 PRINT "Hook per CTRL+C registrato."
130 PRINT "Ora eseguo un loop. Premi F1, Freccia Su, CTRL+X, o CTRL+C."
140 PRINT "Premi ESC per continuare con il test dell'input bloccante."
150 PRINT

' Salta le etichette degli handler per entrare nel loop principale.
155 GOTO loop_attesa

//--- ETICHETTE DI DESTINAZIONE PER I GOTO DEGLI HOOK ---
FrecciaSu:
156 PRINT "[Evento] Tasto Freccia Su premuto!"
158 GOTO loop_attesa

CombinazioneCtrlX:
159 PRINT "[Evento] Combinazione CTRL+X premuta!"
160 GOTO loop_attesa

GestoreCtrlC:
162 PRINT "[Evento] CTRL+C intercettato! Il programma non si ferma."
164 GOTO loop_attesa

//--- Loop di attesa non bloccante ---
loop_attesa:
180 DIM T$ AS STRING
' INKEY non blocca, permettendo al loop di girare. Gli hook ON KEY vengono controllati
' automaticamente dall'interprete ad ogni ciclo o istruzione.
190 INKEY T$
200 IF T$ = CHR(27) THEN GOTO fine_loop
205 SLEEP 1
210 GOTO loop_attesa

fine_loop:
220 PRINT : PRINT "Uscito dal loop. Ora testo l'input bloccante." : PRINT

//### TEST CHIAVE: HOOK DURANTE INPUT BLOCCANTE ###
221 PRINT "--- TEST HOOK DURANTE INPUT BLOCCANTE ---"
222 PRINT "Tutti gli hook (F1, Freccia Su, CTRL+X, CTRL+C) sono ATTIVI."
223 PRINT "Puoi anche provare a incollare del testo con [Ctrl+Shift+V]."
224 DIM RISPOSTA$ AS STRING
' Durante questo INPUT, se l'utente preme F1, deve partire GestoreF1 e poi tornare all'input.
' Se preme CTRL+C, deve andare a GestoreCtrlC.
225 INPUT "PROVA GLI HOOK E POI SCRIVI QUALCOSA E PREMI INVIO: ", RISPOSTA$
226 PRINT "Hai scritto: '"; RISPOSTA$; "'"
227 PRINT

230 PRINT "--- DISABILITAZIONE HOOK ---"
' Usa l'azione STOP per rimuovere gli hook.
240 ON KEY (%K_F1) STOP
250 ON KEY (%K_UP) STOP
260 PRINT "Hook per F1 e Freccia Su disabilitati."
270 PRINT "Provali ora durante il prossimo INPUT, non dovrebbero funzionare."
280 PRINT
285 INPUT "Secondo INPUT (F1 non dovrebbe fare nulla): ", RISPOSTA$
286 PRINT "Hai scritto: '"; RISPOSTA$; "'"
287 PRINT

300 PRINT "--- RIPRISTINO COMPORTAMENTO CTRL+C ---"
310 ON KEY (%K_CTRL_C) STOP
320 PRINT "Hook per CTRL+C disabilitato."
330 PRINT "Ora premendo CTRL+C il programma si interromperà come di default."
340 PRINT "Prova a premere CTRL+C ora per terminare..."

loop_infinito:
350 GOTO loop_infinito