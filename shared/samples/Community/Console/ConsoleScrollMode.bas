' FILE: Console/ConsoleScrollMode.bas
' =================================================================================================
'       GUIDA TEORICA: CONSOLE.SCROLLMODE - SCROLLBACK NATIVO VS BUFFER A GRIGLIA
' =================================================================================================
'
' StandardConsole si apre di default in modalita' NATIVE: l'output va scritto direttamente sul
' terminale reale, che gestisce da solo l'avanzamento riga e lo scrolling, popolando il suo
' scrollback nativo normalmente (come qualsiasi programma da riga di comando) - la console deve
' scrollare fin dall'avvio, non serve chiedere nulla.
'
' `CONSOLE.SCROLLMODE("BUFFERED")` attiva invece un buffer a griglia interno di dimensione fissa
' (larghezza x altezza del terminale rilevate all'avvio): quando l'output supera l'altezza del
' buffer, le righe piu' vecchie vengono scartate per sempre per far posto alle nuove - e' il
' prezzo da pagare per funzionalita' avanzate come VIEW PRINT (scroll region), SAVE SCREEN/
' RESTORE SCREEN/CSAVESCREEN/CRESTORESCREEN (catturano il buffer) e il rendering ottimizzato
' (ridisegna solo le celle cambiate). In modalita' NATIVE questi comandi generano un errore
' runtime, proprio perche' non esiste alcun buffer su cui operare.
'
' `CONSOLE.SCROLLMODE("NATIVE")` torna alla modalita' di default.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleScrollMode - Scrollback nativo del terminale vs buffer a griglia  ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

10 PRINT "TEST 1 - Modalita' NATIVE (default): tutte le righe restano nello scrollback del terminale"
20 FOR I = 1 TO 10
30   PRINT "Riga NATIVE #"; I
40 NEXT I
50 PRINT "Fine TEST 1 - scorri con la rotellina per rivedere tutte le righe di questo test."
60 PRINT

100 PRINT "TEST 2 - Passo a SCROLLMODE ""BUFFERED""..."
110 SCROLLMODE "BUFFERED"
120 FOR I = 1 TO 10
130   PRINT "Riga BUFFERED #"; I
140 NEXT I
150 PRINT "Fine TEST 2 - se il terminale e' piu' basso di quanto scritto, le righe piu' vecchie sono sparite per sempre (VIEW PRINT/SAVE SCREEN ora disponibili)."
160 PRINT

200 PRINT "TEST 3 - Torno a SCROLLMODE ""NATIVE""..."
210 SCROLLMODE "NATIVE"
220 PRINT "Di nuovo in modalita' scrollback nativo (il default)."
230 PRINT

300 PRINT "Test completato."
310 END
